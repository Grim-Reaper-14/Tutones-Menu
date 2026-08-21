#pragma once

#include "../../game/GamePointers.hpp"
#include "../../game/GameState.hpp"
#include "../../game/Natives.hpp"
#include "../../game/native/NativeCallContext.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace Tutones::Game::Mods
{
    class VehicleLoopFeatures final
    {
    public:
        static VehicleLoopFeatures& Get() noexcept
        {
            static VehicleLoopFeatures instance;
            return instance;
        }

        [[nodiscard]] bool KeepVehicleClean() const noexcept
        {
            return m_KeepVehicleClean.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool LoweredStance() const noexcept
        {
            return m_LoweredStance.load(std::memory_order_acquire);
        }

        void SetKeepVehicleClean(bool enabled) noexcept
        {
            m_KeepVehicleClean.store(enabled, std::memory_order_release);
            if (enabled)
                EnsureTicking();
        }

        void SetLoweredStance(bool enabled) noexcept
        {
            m_LoweredStance.store(enabled, std::memory_order_release);
            if (enabled)
            {
                EnsureTicking();
                return;
            }

            // Make disabling deterministic even if no loop tick is currently queued.
            static_cast<void>(Runtime::GameRuntime::Get().Enqueue([this] {
                if (!m_LoweredStance.load(std::memory_order_acquire))
                    RestoreLastStance();
            }));
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

        VehicleLoopFeatures() = default;
        ~VehicleLoopFeatures() = default;
        VehicleLoopFeatures(const VehicleLoopFeatures&) = delete;
        VehicleLoopFeatures& operator=(const VehicleLoopFeatures&) = delete;

        [[nodiscard]] bool AnyEnabled() const noexcept
        {
            return m_KeepVehicleClean.load(std::memory_order_acquire)
                || m_LoweredStance.load(std::memory_order_acquire);
        }

        void EnsureTicking() noexcept
        {
            if (!AnyEnabled())
                return;

            bool expected = false;
            if (!m_Ticking.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return;

            if (!Runtime::GameRuntime::Get().Enqueue([this] { TickOnGameThread(); }))
                m_Ticking.store(false, std::memory_order_release);
        }

        [[nodiscard]] Vehicle CurrentVehicle() const noexcept
        {
            const auto snapshot = GameState::Get().Snapshot();
            if (!snapshot.nativeRuntimeReady || !snapshot.inVehicle || snapshot.vehicle == 0)
                return 0;
            return snapshot.vehicle;
        }

        static Native::NativeHandler& ReducedSuspensionHandler() noexcept
        {
            static Native::NativeHandler handler{};
            return handler;
        }

        [[nodiscard]] static bool ResolveReducedSuspensionHandler() noexcept
        {
            auto& handler = ReducedSuspensionHandler();
            if (handler)
                return true;
            if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread())
                return false;

            const auto init = GamePointers::Get().InitNativeTables();
            if (!init)
                return false;

            // GTA V Enhanced mapping for SET_REDUCED_SUSPENSION_FORCE.
            std::uint64_t slot = 0xCE2ADF354D3F97AEull;
            NativeProgram program{};
            program.nativeCount = 1;
            program.nativeEntrypoints = reinterpret_cast<Native::NativeHandler*>(&slot);
            init(&program);

            handler = reinterpret_cast<Native::NativeHandler>(static_cast<std::uintptr_t>(slot));
            return handler != nullptr;
        }

        [[nodiscard]] static bool SetReducedSuspensionForce(Vehicle vehicle, bool enabled) noexcept
        {
            if (vehicle == 0 || !ResolveReducedSuspensionHandler())
                return false;

            Native::CallContext context;
            if (!context.PushArg(vehicle)
                || !context.PushArg(static_cast<std::int32_t>(enabled)))
            {
                return false;
            }

            ReducedSuspensionHandler()(&context);
            return true;
        }

        void RestoreLastStance() noexcept
        {
            if (m_LastStancedVehicle == 0)
                return;

            const auto exists = Natives::DoesEntityExist(m_LastStancedVehicle);
            if (!exists || *exists)
                static_cast<void>(SetReducedSuspensionForce(m_LastStancedVehicle, false));
            m_LastStancedVehicle = 0;
        }

        void TickOnGameThread() noexcept
        {
            const bool keepClean = m_KeepVehicleClean.load(std::memory_order_acquire);
            const bool lowered = m_LoweredStance.load(std::memory_order_acquire);
            const Vehicle vehicle = CurrentVehicle();

            if (keepClean && vehicle != 0)
                static_cast<void>(Natives::SetVehicleDirtLevel(vehicle, 0.0f));

            if (lowered)
            {
                if (vehicle != 0)
                {
                    if (m_LastStancedVehicle != 0 && m_LastStancedVehicle != vehicle)
                        RestoreLastStance();

                    if (SetReducedSuspensionForce(vehicle, true))
                        m_LastStancedVehicle = vehicle;
                }
            }
            else
            {
                RestoreLastStance();
            }

            if (AnyEnabled() && Runtime::GameRuntime::Get().Enqueue([this] { TickOnGameThread(); }))
                return;

            m_Ticking.store(false, std::memory_order_release);

            // Close the small enable/schedule race if a toggle changed while this tick completed.
            if (AnyEnabled())
                EnsureTicking();
        }

        std::atomic<bool> m_KeepVehicleClean{false};
        std::atomic<bool> m_LoweredStance{false};
        std::atomic<bool> m_Ticking{false};
        Vehicle m_LastStancedVehicle{};
    };
}
