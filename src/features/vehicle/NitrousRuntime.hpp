#pragma once

#include "../../game/GamePointers.hpp"
#include "../../game/Natives.hpp"
#include "../../game/native/NativeCallContext.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace Tutones::Game::Mods
{
    class NitrousRuntime final
    {
    public:
        static NitrousRuntime& Get() noexcept
        {
            static NitrousRuntime instance;
            return instance;
        }

        [[nodiscard]] bool Enabled() const noexcept
        {
            return m_Enabled.load(std::memory_order_acquire);
        }

        void SetEnabled(bool enabled) noexcept
        {
            m_Enabled.store(enabled, std::memory_order_release);
            if (enabled)
                EnsureTicking();
        }

        [[nodiscard]] bool Unlimited() const noexcept
        {
            return m_Unlimited.load(std::memory_order_acquire);
        }

        void SetUnlimited(bool unlimited) noexcept
        {
            m_Unlimited.store(unlimited, std::memory_order_release);
        }

        [[nodiscard]] float Level() const noexcept
        {
            return m_Level.load(std::memory_order_acquire);
        }

        void SetLevel(float level) noexcept
        {
            m_Level.store(std::clamp(level, MinimumLevel, MaximumLevel), std::memory_order_release);
        }

        [[nodiscard]] float Power() const noexcept
        {
            return m_Power.load(std::memory_order_acquire);
        }

        void SetPower(float power) noexcept
        {
            m_Power.store(std::clamp(power, MinimumPower, MaximumPower), std::memory_order_release);
        }

        void Shutdown() noexcept
        {
            m_Enabled.store(false, std::memory_order_release);
        }

    private:
        struct NativeProgram final
        {
            std::byte pad00[0x2C]{};
            std::uint32_t nativeCount{};
            std::byte pad30[0x10]{};
            Native::NativeHandler* nativeEntrypoints{};
            std::byte pad48[0x38]{};
        };

        static_assert(offsetof(NativeProgram, nativeCount) == 0x2C);
        static_assert(offsetof(NativeProgram, nativeEntrypoints) == 0x40);
        static_assert(sizeof(NativeProgram) == 0x80);

        enum HandlerIndex : std::size_t
        {
            IsControlPressed,
            SetOverrideNitrousLevel,
            SetNitrousIsActive,
            FullyChargeNitrous,
            HandlerCount,
        };

        // Current GTA V Enhanced mappings. The override mapping is crossmapped from
        // SET_OVERRIDE_NITROUS_LEVEL; the remaining nitrous entries are stable in the
        // current Enhanced crossmap family.
        static constexpr std::array<std::uint64_t, HandlerCount> HandlerHashes{
            0x6D05C5731A838CB3ull, // IS_CONTROL_PRESSED
            0x8D6D4EB2FFE77CB3ull, // SET_OVERRIDE_NITROUS_LEVEL
            0x465EEA70AF251045ull, // SET_NITROUS_IS_ACTIVE
            0x1A2BCC8C636F9226ull, // FULLY_CHARGE_NITROUS
        };

        static constexpr int SprintControl = 21;
        static constexpr int AccelerateControl = 71;
        static constexpr float MinimumLevel = 0.5f;
        static constexpr float MaximumLevel = 5.0f;
        static constexpr float MinimumPower = 0.5f;
        static constexpr float MaximumPower = 5.0f;
        static constexpr float RechargeTime = 0.25f;

        NitrousRuntime() = default;
        ~NitrousRuntime() = default;
        NitrousRuntime(const NitrousRuntime&) = delete;
        NitrousRuntime& operator=(const NitrousRuntime&) = delete;

        [[nodiscard]] static bool IsExecutableAddress(std::uintptr_t address) noexcept
        {
            if (address == 0)
                return false;

            MEMORY_BASIC_INFORMATION memory{};
            if (::VirtualQuery(reinterpret_cast<const void*>(address), &memory, sizeof(memory)) != sizeof(memory))
                return false;
            if (memory.State != MEM_COMMIT || (memory.Protect & PAGE_GUARD) != 0 || memory.Protect == PAGE_NOACCESS)
                return false;

            switch (memory.Protect & 0xFF)
            {
            case PAGE_EXECUTE:
            case PAGE_EXECUTE_READ:
            case PAGE_EXECUTE_READWRITE:
            case PAGE_EXECUTE_WRITECOPY:
                return true;
            default:
                return false;
            }
        }

        [[nodiscard]] bool ResolveHandlers() noexcept
        {
            bool ready = true;
            for (const auto handler : m_Handlers)
                ready = ready && handler != nullptr;
            if (ready)
                return true;

            if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread())
                return false;

            const auto init = GamePointers::Get().InitNativeTables();
            if (!init)
                return false;

            auto slots = HandlerHashes;
            NativeProgram program{};
            program.nativeCount = static_cast<std::uint32_t>(slots.size());
            program.nativeEntrypoints = reinterpret_cast<Native::NativeHandler*>(slots.data());
            init(&program);

            for (std::size_t index = 0; index < slots.size(); ++index)
            {
                const auto address = static_cast<std::uintptr_t>(slots[index]);
                if (!IsExecutableAddress(address))
                {
                    m_Handlers.fill(nullptr);
                    return false;
                }
                m_Handlers[index] = reinterpret_cast<Native::NativeHandler>(address);
            }

            return true;
        }

        [[nodiscard]] Vehicle CurrentVehicle() const noexcept
        {
            if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread())
                return 0;

            const auto ped = Natives::PlayerPedId();
            if (!ped || *ped == 0)
                return 0;

            const auto inVehicle = Natives::IsPedInAnyVehicle(*ped, false);
            if (!inVehicle || !*inVehicle)
                return 0;

            const auto vehicle = Natives::GetVehiclePedIsIn(*ped, false);
            return vehicle ? *vehicle : 0;
        }

        [[nodiscard]] bool ControlPressed(int control) noexcept
        {
            if (!ResolveHandlers())
                return false;

            Native::CallContext context;
            if (!context.PushArg(std::int32_t{0}) || !context.PushArg(std::int32_t{control}))
                return false;

            m_Handlers[IsControlPressed](&context);
            return context.GetReturnValue<std::int32_t>() != 0;
        }

        [[nodiscard]] bool ConfigureNitrous(Vehicle vehicle, bool enabled) noexcept
        {
            if (vehicle == 0 || !ResolveHandlers())
                return false;

            Native::CallContext context;
            if (!context.PushArg(vehicle)
                || !context.PushArg(std::int32_t{enabled ? 1 : 0})
                || !context.PushArg(Level())
                || !context.PushArg(Power())
                || !context.PushArg(RechargeTime)
                || !context.PushArg(std::int32_t{0}))
            {
                return false;
            }

            m_Handlers[SetOverrideNitrousLevel](&context);
            return true;
        }

        [[nodiscard]] bool SetActive(Vehicle vehicle, bool active) noexcept
        {
            if (vehicle == 0 || !ResolveHandlers())
                return false;

            Native::CallContext context;
            if (!context.PushArg(vehicle) || !context.PushArg(std::int32_t{active ? 1 : 0}))
                return false;

            m_Handlers[SetNitrousIsActive](&context);
            return true;
        }

        [[nodiscard]] bool FullyCharge(Vehicle vehicle) noexcept
        {
            if (vehicle == 0 || !ResolveHandlers())
                return false;

            Native::CallContext context;
            if (!context.PushArg(vehicle))
                return false;

            m_Handlers[FullyChargeNitrous](&context);
            return true;
        }

        void DisableVehicle(Vehicle vehicle) noexcept
        {
            if (vehicle == 0)
                return;

            static_cast<void>(SetActive(vehicle, false));
            static_cast<void>(ConfigureNitrous(vehicle, false));
        }

        void EnsureTicking() noexcept
        {
            bool expected = false;
            if (!m_Ticking.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return;

            if (!QueueNextTick())
                m_Ticking.store(false, std::memory_order_release);
        }

        [[nodiscard]] bool QueueNextTick() noexcept
        {
            return Runtime::GameRuntime::Get().Enqueue([this] { TickOnGameThread(); });
        }

        void TickOnGameThread() noexcept
        {
            if (!m_Enabled.load(std::memory_order_acquire))
            {
                DisableVehicle(m_LastVehicle);
                m_LastVehicle = 0;
                m_Ticking.store(false, std::memory_order_release);
                return;
            }

            const Vehicle vehicle = CurrentVehicle();
            if (vehicle != m_LastVehicle)
            {
                DisableVehicle(m_LastVehicle);
                m_LastVehicle = vehicle;
            }

            if (vehicle != 0 && ResolveHandlers())
            {
                static_cast<void>(ConfigureNitrous(vehicle, true));
                if (Unlimited())
                    static_cast<void>(FullyCharge(vehicle));

                const bool active = ControlPressed(SprintControl) && ControlPressed(AccelerateControl);
                static_cast<void>(SetActive(vehicle, active));
            }

            if (!QueueNextTick())
            {
                DisableVehicle(m_LastVehicle);
                m_LastVehicle = 0;
                m_Ticking.store(false, std::memory_order_release);
            }
        }

        std::atomic<bool> m_Enabled{false};
        std::atomic<bool> m_Unlimited{true};
        std::atomic<bool> m_Ticking{false};
        std::atomic<float> m_Level{2.5f};
        std::atomic<float> m_Power{2.0f};
        std::array<Native::NativeHandler, HandlerCount> m_Handlers{};
        Vehicle m_LastVehicle{};
    };
}
