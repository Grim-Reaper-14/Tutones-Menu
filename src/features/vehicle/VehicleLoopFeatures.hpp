#pragma once

#include "../../game/GamePointers.hpp"
#include "../../game/Natives.hpp"
#include "../../game/native/NativeCallContext.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <Windows.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>

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

        [[nodiscard]] bool VehicleGodMode() const noexcept
        {
            return m_VehicleGodMode.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool KeepVehicleClean() const noexcept
        {
            return m_KeepVehicleClean.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool LoweredStance() const noexcept
        {
            return m_LoweredStance.load(std::memory_order_acquire);
        }

        void SetVehicleGodMode(bool enabled) noexcept
        {
            m_VehicleGodMode.store(enabled, std::memory_order_release);
            if (enabled)
            {
                EnsureTicking();
                return;
            }

            auto& runtime = Runtime::GameRuntime::Get();
            if (runtime.IsOnGameThread())
            {
                RestoreLastGodVehicle();
                return;
            }

            static_cast<void>(runtime.Enqueue([this] {
                if (!m_VehicleGodMode.load(std::memory_order_acquire))
                    RestoreLastGodVehicle();
            }));
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

            auto& runtime = Runtime::GameRuntime::Get();
            if (runtime.IsOnGameThread())
            {
                RestoreLastStance();
                return;
            }

            static_cast<void>(runtime.Enqueue([this] {
                if (!m_LoweredStance.load(std::memory_order_acquire))
                    RestoreLastStance();
            }));
        }

        void Shutdown() noexcept
        {
            m_VehicleGodMode.store(false, std::memory_order_release);
            m_KeepVehicleClean.store(false, std::memory_order_release);
            m_LoweredStance.store(false, std::memory_order_release);

            const auto cleanup = [this] {
                RestoreLastGodVehicle();
                RestoreLastStance();
            };

            auto& runtime = Runtime::GameRuntime::Get();
            if (runtime.IsOnGameThread())
            {
                cleanup();
            }
            else if (runtime.IsInitialized())
            {
                const auto cleaned = std::make_shared<std::atomic<bool>>(false);
                if (runtime.Enqueue([cleanup, cleaned] {
                        cleanup();
                        cleaned->store(true, std::memory_order_release);
                    }))
                {
                    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
                    while (!cleaned->load(std::memory_order_acquire)
                        && std::chrono::steady_clock::now() < deadline)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                }
            }

            m_Ticking.store(false, std::memory_order_release);
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
            return m_VehicleGodMode.load(std::memory_order_acquire)
                || m_KeepVehicleClean.load(std::memory_order_acquire)
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
            if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread())
                return 0;

            const auto ped = Natives::PlayerPedId();
            if (!ped || *ped == 0)
                return 0;

            const auto inVehicle = Natives::IsPedInAnyVehicle(*ped, false);
            if (!inVehicle || !*inVehicle)
                return 0;

            const auto vehicle = Natives::GetVehiclePedIsIn(*ped, false);
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

        static Native::NativeHandler& DamageDecalsHandler() noexcept
        {
            static Native::NativeHandler handler{};
            return handler;
        }

        static Native::NativeHandler& VehicleEngineHealthHandler() noexcept
        {
            static Native::NativeHandler handler{};
            return handler;
        }

        static Native::NativeHandler& VehiclePetrolTankHealthHandler() noexcept
        {
            static Native::NativeHandler handler{};
            return handler;
        }

        static Native::NativeHandler& ForceEntityUpdateHandler() noexcept
        {
            static Native::NativeHandler handler{};
            return handler;
        }

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

            const auto address = static_cast<std::uintptr_t>(slot);
            if (!IsExecutableAddress(address))
            {
                handler = nullptr;
                return false;
            }

            handler = reinterpret_cast<Native::NativeHandler>(address);
            return true;
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

        [[nodiscard]] static bool ResolveDamageDecalsHandler() noexcept
        {
            // GET_DOES_VEHICLE_HAVE_DAMAGE_DECALS
            // legacy BCDC5017D3CE1E9E -> Enhanced B69AE16F62A14003.
            return ResolveSingleHandler(DamageDecalsHandler(), 0xB69AE16F62A14003ull);
        }

        [[nodiscard]] static bool ResolveVehicleEngineHealthHandler() noexcept
        {
            // SET_VEHICLE_ENGINE_HEALTH
            // legacy 45F6D8EEF34ABEF1 -> Enhanced 2AEBE39F6BF7D6BC.
            return ResolveSingleHandler(VehicleEngineHealthHandler(), 0x2AEBE39F6BF7D6BCull);
        }

        [[nodiscard]] static bool ResolveVehiclePetrolTankHealthHandler() noexcept
        {
            // SET_VEHICLE_PETROL_TANK_HEALTH
            // legacy 70DB57649FA8D0D8 -> Enhanced DF9DC0584881B7AF.
            return ResolveSingleHandler(VehiclePetrolTankHealthHandler(), 0xDF9DC0584881B7AFull);
        }

        [[nodiscard]] static bool ResolveForceEntityUpdateHandler() noexcept
        {
            // FORCE_ENTITY_AI_AND_ANIMATION_UPDATE
            // legacy 40FDEDB72F8293B2 -> Enhanced 2B2ECB6F6371E59E.
            return ResolveSingleHandler(ForceEntityUpdateHandler(), 0x2B2ECB6F6371E59Eull);
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
                || !context.PushArg(static_cast<std::int32_t>(enabled))
                || !context.PushArg(std::int32_t{1}))
            {
                return false;
            }

            EntityInvincibleHandler()(&context);
            return true;
        }

        [[nodiscard]] static std::optional<bool> HasVehicleDamageDecals(Vehicle vehicle) noexcept
        {
            if (vehicle == 0 || !ResolveDamageDecalsHandler())
                return std::nullopt;

            Native::CallContext context;
            if (!context.PushArg(vehicle))
                return std::nullopt;

            DamageDecalsHandler()(&context);
            return context.GetReturnValue<std::int32_t>() != 0;
        }

        [[nodiscard]] static bool SetVehicleEngineHealth(Vehicle vehicle, float health) noexcept
        {
            if (vehicle == 0 || !ResolveVehicleEngineHealthHandler())
                return false;

            Native::CallContext context;
            if (!context.PushArg(vehicle) || !context.PushArg(health))
                return false;

            VehicleEngineHealthHandler()(&context);
            return true;
        }

        [[nodiscard]] static bool SetVehiclePetrolTankHealth(Vehicle vehicle, float health) noexcept
        {
            if (vehicle == 0 || !ResolveVehiclePetrolTankHealthHandler())
                return false;

            Native::CallContext context;
            if (!context.PushArg(vehicle) || !context.PushArg(health))
                return false;

            VehiclePetrolTankHealthHandler()(&context);
            return true;
        }

        [[nodiscard]] static bool ForceEntityAiAndAnimationUpdate(Entity entity) noexcept
        {
            if (entity == 0 || !ResolveForceEntityUpdateHandler())
                return false;

            Native::CallContext context;
            if (!context.PushArg(entity))
                return false;

            ForceEntityUpdateHandler()(&context);
            return true;
        }

        static void ApplyVehicleGodMode(Vehicle vehicle) noexcept
        {
            if (vehicle != 0)
                static_cast<void>(SetEntityInvincible(vehicle, true));
        }

        static void ApplyVehicleClean(Vehicle vehicle) noexcept
        {
            if (vehicle == 0)
                return;

            // Mirror YimMenuV2's Vehicle::Fix repair path whenever GTA reports
            // visible vehicle damage. If the damage query cannot resolve, repair
            // anyway rather than silently degrading back to dirt-only cleanup.
            const auto hasDamageDecals = HasVehicleDamageDecals(vehicle);
            if (!hasDamageDecals || *hasDamageDecals)
            {
                static_cast<void>(Natives::SetVehicleFixed(vehicle));
                static_cast<void>(Native::NativeInvoker::InvokeVoid(
                    Native::NativeId::SetEntityHealth,
                    vehicle,
                    1000,
                    std::int32_t{0},
                    std::uint32_t{0}));
                static_cast<void>(SetVehicleEngineHealth(vehicle, 1000.0f));
                static_cast<void>(SetVehiclePetrolTankHealth(vehicle, 1000.0f));
            }

            // Keep the visible-clean portion active every tick. Yim's Fix() exits
            // when no damage decals are reported, but GTA can still accumulate dirt
            // without that flag; always clearing dirt keeps this toggle dependable.
            static_cast<void>(Natives::SetVehicleDirtLevel(vehicle, 0.0f));
            static_cast<void>(RemoveVehicleDecals(vehicle));
            static_cast<void>(ForceEntityAiAndAnimationUpdate(vehicle));
        }

        void RestoreLastGodVehicle() noexcept
        {
            if (m_LastGodVehicle == 0)
                return;

            const auto exists = Natives::DoesEntityExist(m_LastGodVehicle);
            if (exists && *exists)
                static_cast<void>(SetEntityInvincible(m_LastGodVehicle, false));
            m_LastGodVehicle = 0;
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
            const bool godMode = m_VehicleGodMode.load(std::memory_order_acquire);
            const bool keepClean = m_KeepVehicleClean.load(std::memory_order_acquire);
            const bool lowered = m_LoweredStance.load(std::memory_order_acquire);
            const Vehicle vehicle = CurrentVehicle();

            if (godMode && vehicle != 0)
            {
                if (m_LastGodVehicle != 0 && m_LastGodVehicle != vehicle)
                    RestoreLastGodVehicle();

                ApplyVehicleGodMode(vehicle);
                m_LastGodVehicle = vehicle;
            }
            else
            {
                // Do not leave a vehicle invincible after the player exits it,
                // changes vehicles, or disables the toggle.
                RestoreLastGodVehicle();
            }

            if (keepClean && vehicle != 0)
                ApplyVehicleClean(vehicle);

            if (lowered && vehicle != 0)
            {
                if (m_LastStancedVehicle != 0 && m_LastStancedVehicle != vehicle)
                    RestoreLastStance();

                if (SetReducedSuspensionForce(vehicle, true))
                    m_LastStancedVehicle = vehicle;
            }
            else
            {
                RestoreLastStance();
            }

            if (AnyEnabled() && Runtime::GameRuntime::Get().Enqueue([this] { TickOnGameThread(); }))
                return;

            // Never leave reversible state behind if scheduling disappears.
            RestoreLastGodVehicle();
            RestoreLastStance();
            m_Ticking.store(false, std::memory_order_release);
            if (AnyEnabled())
                EnsureTicking();
        }

        std::atomic<bool> m_VehicleGodMode{false};
        std::atomic<bool> m_KeepVehicleClean{false};
        std::atomic<bool> m_LoweredStance{false};
        std::atomic<bool> m_Ticking{false};
        Vehicle m_LastGodVehicle{};
        Vehicle m_LastStancedVehicle{};
    };
}
