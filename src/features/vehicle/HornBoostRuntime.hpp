#pragma once

#include "../../game/GamePointers.hpp"
#include "../../game/Natives.hpp"
#include "../../game/VehicleNatives.hpp"
#include "../../game/native/NativeCallContext.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace Tutones::Game::Mods
{
    class HornBoostRuntime final
    {
    public:
        static HornBoostRuntime& Get() noexcept
        {
            static HornBoostRuntime instance;
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
            GetEntitySpeed,
            SetEntityVelocity,
            HandlerCount,
        };

        // Current GTA V Enhanced mappings verified against YimMenuV2's enhanced crossmap.
        static constexpr std::array<std::uint64_t, HandlerCount> HandlerHashes{
            0x6D05C5731A838CB3ull, // IS_CONTROL_PRESSED
            0xDF93B3CFAC96698Full, // GET_ENTITY_SPEED
            0x1AB7223AC0702871ull, // SET_ENTITY_VELOCITY
        };

        static constexpr int VehicleHornControl = 86;
        static constexpr float DefaultBoostSpeed = 10.0f;
        static constexpr float MaximumBoostSpeed = 200.0f;
        static constexpr float BoostIncrement = 0.3f;

        HornBoostRuntime() = default;
        ~HornBoostRuntime() = default;
        HornBoostRuntime(const HornBoostRuntime&) = delete;
        HornBoostRuntime& operator=(const HornBoostRuntime&) = delete;

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

            for (std::size_t i = 0; i < slots.size(); ++i)
            {
                const auto address = static_cast<std::uintptr_t>(slots[i]);
                if (!IsExecutableAddress(address))
                {
                    m_Handlers.fill(nullptr);
                    return false;
                }
                m_Handlers[i] = reinterpret_cast<Native::NativeHandler>(address);
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

        [[nodiscard]] std::optional<bool> HornPressed() noexcept
        {
            if (!ResolveHandlers())
                return std::nullopt;

            Native::CallContext context;
            if (!context.PushArg(std::int32_t{0}) || !context.PushArg(std::int32_t{VehicleHornControl}))
                return std::nullopt;

            m_Handlers[IsControlPressed](&context);
            return context.GetReturnValue<std::int32_t>() != 0;
        }

        [[nodiscard]] std::optional<float> EntitySpeed(Entity entity) noexcept
        {
            if (entity == 0 || !ResolveHandlers())
                return std::nullopt;

            Native::CallContext context;
            if (!context.PushArg(entity))
                return std::nullopt;

            m_Handlers[GetEntitySpeed](&context);
            return context.GetReturnValue<float>();
        }

        [[nodiscard]] bool SetVelocity(Entity entity, const Vector3& velocity) noexcept
        {
            if (entity == 0 || !ResolveHandlers())
                return false;

            Native::CallContext context;
            if (!context.PushArg(entity)
                || !context.PushArg(velocity.x)
                || !context.PushArg(velocity.y)
                || !context.PushArg(velocity.z))
            {
                return false;
            }

            m_Handlers[SetEntityVelocity](&context);
            return true;
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

        void ResetBoost() noexcept
        {
            m_BoostSpeed = DefaultBoostSpeed;
            m_WasHornPressed = false;
        }

        void TickOnGameThread() noexcept
        {
            if (!m_Enabled.load(std::memory_order_acquire))
            {
                ResetBoost();
                m_Ticking.store(false, std::memory_order_release);
                return;
            }

            const Vehicle vehicle = CurrentVehicle();
            const auto horn = HornPressed();
            const bool pressed = vehicle != 0 && horn && *horn;

            if (!pressed)
            {
                ResetBoost();
            }
            else
            {
                if (!m_WasHornPressed)
                    m_BoostSpeed = std::max(DefaultBoostSpeed, EntitySpeed(vehicle).value_or(DefaultBoostSpeed));

                m_BoostSpeed = std::min(MaximumBoostSpeed, m_BoostSpeed + BoostIncrement);

                const auto position = VehicleNatives::GetEntityCoords(vehicle, false);
                const auto target = VehicleNatives::Detail::OffsetFromEntity(vehicle, 0.0f, m_BoostSpeed, 0.0f);
                if (position && target)
                {
                    const Vector3 velocity{
                        target->x - position->x,
                        target->y - position->y,
                        target->z - position->z,
                    };
                    static_cast<void>(SetVelocity(vehicle, velocity));
                }

                m_WasHornPressed = true;
            }

            if (!QueueNextTick())
                m_Ticking.store(false, std::memory_order_release);
        }

        std::atomic<bool> m_Enabled{false};
        std::atomic<bool> m_Ticking{false};
        std::array<Native::NativeHandler, HandlerCount> m_Handlers{};
        float m_BoostSpeed{DefaultBoostSpeed};
        bool m_WasHornPressed{};
    };
}
