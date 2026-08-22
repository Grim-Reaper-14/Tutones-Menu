#pragma once

#include "../../game/GamePointers.hpp"
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
            {
                EnsureTicking();
                return;
            }

            // Release the protection we applied to the last vehicle as soon as the
            // pristine toggle is disabled, even if no other vehicle loop feature is on.
            static_cast<void>(Runtime::GameRuntime::Get().Enqueue([this] {
                if (!m_KeepVehicleClean.load(std::memory_order_acquire))
                    RestoreLastCleanVehicle();
            }));
        }

        void SetLoweredStance(bool enabled) noexcept
        {
            m_LoweredStance.store(enabled, std::memory_order_release);
            if (enabled)
            {
                EnsureTicking();
                return;
            }

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
            // Resolve the actual vehicle directly on GTA's script thread. includeEntering
            // is true so a seat transition does not briefly make the persistent feature
            // lose the vehicle it is supposed to protect.
            if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread())
                return 0;

            const auto ped = Natives::PlayerPedId();
            if (!ped || *ped == 0)
                return 0;

            const auto inVehicle = Natives::IsPedInAnyVehicle(*ped, true);
            if (!inVehicle || !*inVehicle)
                return 0;

            const auto vehicle = Natives::GetVehiclePedIsIn(*ped, true);
            if (!vehicle || *vehicle == 0)
                return 0;

            const auto exists = Natives::DoesEntityExist(*vehicle);
            return exists && *exists ? *vehicle : 0;
        }

        static Native::NativeHandler& ReducedSuspensionHandler() noexcept
        {
            static Native::NativeHandler handler{};
            return handler;
        }

        static Native::NativeHandler& RemoveDecalsHandler() noexcept
        {
            static Native::NativeHandler handler{};
            return handler;
        }

        static Native::NativeHandler& EntityInvincibleHandler() noexcept
        {
            static Native::NativeHandler handler{};
            return handler;
        }

        static Native::NativeHandler& DeformationFixedHandler() noexcept
        {
            static Native::NativeHandler handler{};
            return handler;
        }

        static Native::NativeHandler& TyreFixedHandler() noexcept
        {
            static Native::NativeHandler handler{};
            return handler;
        }

        [[nodiscard]] static bool ResolveSingleHandler(
            Native::NativeHandler& handler,
            std::uint64_t enhancedHash) noexcept
        {
            if (handler)
                return true;
            if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread())
                return false;

            const auto init = GamePointers::Get().InitNativeTables();
            if (!init)
                return false;

            std::uint64_t slot = enhancedHash;
            NativeProgram program{};
            program.nativeCount = 1;
            program.nativeEntrypoints = reinterpret_cast<Native::NativeHandler*>(&slot);
            init(&program);

            handler = reinterpret_cast<Native::NativeHandler>(static_cast<std::uintptr_t>(slot));
            return handler != nullptr;
        }

        [[nodiscard]] static bool ResolveReducedSuspensionHandler() noexcept
        {
            return ResolveSingleHandler(ReducedSuspensionHandler(), 0xCE2ADF354D3F97AEull);
        }

        [[nodiscard]] static bool ResolveRemoveDecalsHandler() noexcept
        {
            // REMOVE_DECALS_FROM_VEHICLE
            // legacy E91F1B65F2B48D57 -> Enhanced FEC8EAE457274AD3.
            return ResolveSingleHandler(RemoveDecalsHandler(), 0xFEC8EAE457274AD3ull);
        }

        [[nodiscard]] static bool ResolveEntityInvincibleHandler() noexcept
        {
            // SET_ENTITY_INVINCIBLE
            // legacy 3882114BDE571AD4 -> Enhanced 935364B4448CD584.
            return ResolveSingleHandler(EntityInvincibleHandler(), 0x935364B4448CD584ull);
        }

        [[nodiscard]] static bool ResolveDeformationFixedHandler() noexcept
        {
            // SET_VEHICLE_DEFORMATION_FIXED
            // legacy 953DA1E1B12C0491 -> Enhanced 1D1124C855316790.
            return ResolveSingleHandler(DeformationFixedHandler(), 0x1D1124C855316790ull);
        }

        [[nodiscard]] static bool ResolveTyreFixedHandler() noexcept
        {
            // SET_VEHICLE_TYRE_FIXED
            // legacy 6E13FC662B882D1D -> Enhanced F516E954BCB89C18.
            return ResolveSingleHandler(TyreFixedHandler(), 0xF516E954BCB89C18ull);
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

        [[nodiscard]] static bool RemoveVehicleDecals(Vehicle vehicle) noexcept
        {
            if (vehicle == 0 || !ResolveRemoveDecalsHandler())
                return false;

            Native::CallContext context;
            if (!context.PushArg(vehicle))
                return false;
            RemoveDecalsHandler()(&context);
            return true;
        }

        [[nodiscard]] static bool SetEntityInvincible(Entity entity, bool enabled) noexcept
        {
            if (entity == 0 || !ResolveEntityInvincibleHandler())
                return false;

            Native::CallContext context;
            if (!context.PushArg(entity)
                || !context.PushArg(static_cast<std::int32_t>(enabled)))
            {
                return false;
            }
            EntityInvincibleHandler()(&context);
            return true;
        }

        [[nodiscard]] static bool SetVehicleDeformationFixed(Vehicle vehicle) noexcept
        {
            if (vehicle == 0 || !ResolveDeformationFixedHandler())
                return false;

            Native::CallContext context;
            if (!context.PushArg(vehicle))
                return false;
            DeformationFixedHandler()(&context);
            return true;
        }

        [[nodiscard]] static bool SetVehicleTyreFixed(Vehicle vehicle, int tyreIndex) noexcept
        {
            if (vehicle == 0 || !ResolveTyreFixedHandler())
                return false;

            Native::CallContext context;
            if (!context.PushArg(vehicle) || !context.PushArg(tyreIndex))
                return false;
            TyreFixedHandler()(&context);
            return true;
        }

        static void ApplyPristineVehicle(Vehicle vehicle) noexcept
        {
            if (vehicle == 0)
                return;

            // "Keep Vehicle Clean" means pristine in Tutones: prevent new damage and
            // continuously remove/repair any visible or mechanical damage that slips in.
            static_cast<void>(SetEntityInvincible(vehicle, true));
            static_cast<void>(Natives::SetVehicleFixed(vehicle));
            static_cast<void>(SetVehicleDeformationFixed(vehicle));

            // Standard GTA wheel/tyre indices, including the two special rear-wheel slots.
            constexpr int TyreIndices[]{0, 1, 2, 3, 4, 5, 45, 47};
            for (const int tyreIndex : TyreIndices)
                static_cast<void>(SetVehicleTyreFixed(vehicle, tyreIndex));

            static_cast<void>(Natives::SetVehicleDirtLevel(vehicle, 0.0f));
            static_cast<void>(RemoveVehicleDecals(vehicle));
        }

        void RestoreLastCleanVehicle() noexcept
        {
            if (m_LastCleanVehicle == 0)
                return;

            const auto exists = Natives::DoesEntityExist(m_LastCleanVehicle);
            if (exists && *exists)
                static_cast<void>(SetEntityInvincible(m_LastCleanVehicle, false));
            m_LastCleanVehicle = 0;
        }

        void RestoreLastStance() noexcept
        {
            if (m_LastStancedVehicle == 0)
                return;

            const auto exists = Natives::DoesEntityExist(m_LastStancedVehicle);
            if (exists && *exists)
                static_cast<void>(SetReducedSuspensionForce(m_LastStancedVehicle, false));
            m_LastStancedVehicle = 0;
        }

        void TickOnGameThread() noexcept
        {
            const bool keepClean = m_KeepVehicleClean.load(std::memory_order_acquire);
            const bool lowered = m_LoweredStance.load(std::memory_order_acquire);
            const Vehicle vehicle = CurrentVehicle();

            if (keepClean && vehicle != 0)
            {
                if (m_LastCleanVehicle != 0 && m_LastCleanVehicle != vehicle)
                    RestoreLastCleanVehicle();

                ApplyPristineVehicle(vehicle);
                m_LastCleanVehicle = vehicle;
            }
            else
            {
                // Do not leave a vehicle permanently invincible after the player exits it
                // or disables the pristine feature.
                RestoreLastCleanVehicle();
            }

            if (lowered && vehicle != 0)
            {
                if (m_LastStancedVehicle != 0 && m_LastStancedVehicle != vehicle)
                    RestoreLastStance();

                if (SetReducedSuspensionForce(vehicle, true))
                    m_LastStancedVehicle = vehicle;
            }
            else if (!lowered)
            {
                RestoreLastStance();
            }

            if (AnyEnabled() && Runtime::GameRuntime::Get().Enqueue([this] { TickOnGameThread(); }))
                return;

            m_Ticking.store(false, std::memory_order_release);
            if (AnyEnabled())
                EnsureTicking();
        }

        std::atomic<bool> m_KeepVehicleClean{false};
        std::atomic<bool> m_LoweredStance{false};
        std::atomic<bool> m_Ticking{false};
        Vehicle m_LastCleanVehicle{};
        Vehicle m_LastStancedVehicle{};
    };
}
