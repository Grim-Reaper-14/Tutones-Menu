#pragma once

#include "../../game/EntityInspectorNatives.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/Natives.hpp"
#include "../../game/PlayerNatives.hpp"
#include "../../game/VehicleNatives.hpp"
#include "../../game/native/NativeCallContext.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
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

        [[nodiscard]] bool AutoRepair() const noexcept
        {
            return m_AutoRepair.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool BulletproofTyres() const noexcept
        {
            return m_BulletproofTyres.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool EngineAlwaysOn() const noexcept
        {
            return m_EngineAlwaysOn.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool AutoFlip() const noexcept
        {
            return m_AutoFlip.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool VehicleInvisible() const noexcept
        {
            return m_VehicleInvisible.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool NoCollision() const noexcept
        {
            return m_NoCollision.load(std::memory_order_acquire);
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

            QueueRestore([this] { RestoreLastGodVehicle(); });
        }

        void SetAutoRepair(bool enabled) noexcept
        {
            m_AutoRepair.store(enabled, std::memory_order_release);
            if (enabled)
                EnsureTicking();
        }

        void SetBulletproofTyres(bool enabled) noexcept
        {
            m_BulletproofTyres.store(enabled, std::memory_order_release);
            if (enabled)
            {
                EnsureTicking();
                return;
            }

            QueueRestore([this] { RestoreLastTyreVehicle(); });
        }

        void SetEngineAlwaysOn(bool enabled) noexcept
        {
            m_EngineAlwaysOn.store(enabled, std::memory_order_release);
            if (enabled)
                EnsureTicking();
        }

        void SetAutoFlip(bool enabled) noexcept
        {
            m_AutoFlip.store(enabled, std::memory_order_release);
            if (enabled)
                EnsureTicking();
        }

        void SetVehicleInvisible(bool enabled) noexcept
        {
            m_VehicleInvisible.store(enabled, std::memory_order_release);
            if (enabled)
            {
                EnsureTicking();
                return;
            }

            QueueRestore([this] { RestoreLastInvisibleVehicle(); });
        }

        void SetNoCollision(bool enabled) noexcept
        {
            m_NoCollision.store(enabled, std::memory_order_release);
            if (enabled)
            {
                EnsureTicking();
                return;
            }

            QueueRestore([this] { RestoreLastCollisionVehicle(); });
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

            QueueRestore([this] { RestoreLastStance(); });
        }

        [[nodiscard]] bool QueueVehicleJump(float upwardVelocity = 8.5f) noexcept
        {
            if (!Native::NativeRegistry::Get().IsReady())
                return false;

            upwardVelocity = std::clamp(upwardVelocity, 2.0f, 25.0f);
            return Runtime::GameRuntime::Get().Enqueue([this, upwardVelocity] {
                const Vehicle vehicle = CurrentVehicle();
                if (vehicle == 0)
                    return;

                const auto velocity = EntityInspectorNatives::GetEntityVelocity(vehicle);
                if (!velocity)
                    return;

                const float z = std::max(velocity->z, 0.0f) + upwardVelocity;
                static_cast<void>(SetEntityVelocity(vehicle, velocity->x, velocity->y, z));
            });
        }

        void Shutdown() noexcept
        {
            m_VehicleGodMode.store(false, std::memory_order_release);
            m_AutoRepair.store(false, std::memory_order_release);
            m_BulletproofTyres.store(false, std::memory_order_release);
            m_EngineAlwaysOn.store(false, std::memory_order_release);
            m_AutoFlip.store(false, std::memory_order_release);
            m_VehicleInvisible.store(false, std::memory_order_release);
            m_NoCollision.store(false, std::memory_order_release);
            m_KeepVehicleClean.store(false, std::memory_order_release);
            m_LoweredStance.store(false, std::memory_order_release);

            const auto cleanup = [this] {
                RestoreLastGodVehicle();
                RestoreLastTyreVehicle();
                RestoreLastInvisibleVehicle();
                RestoreLastCollisionVehicle();
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

        template<typename Fn>
        void QueueRestore(Fn&& fn) noexcept
        {
            auto& runtime = Runtime::GameRuntime::Get();
            if (runtime.IsOnGameThread())
            {
                fn();
                return;
            }

            if (runtime.IsInitialized())
                static_cast<void>(runtime.Enqueue(std::forward<Fn>(fn)));
        }

        [[nodiscard]] bool AnyEnabled() const noexcept
        {
            return m_VehicleGodMode.load(std::memory_order_acquire)
                || m_AutoRepair.load(std::memory_order_acquire)
                || m_BulletproofTyres.load(std::memory_order_acquire)
                || m_EngineAlwaysOn.load(std::memory_order_acquire)
                || m_AutoFlip.load(std::memory_order_acquire)
                || m_VehicleInvisible.load(std::memory_order_acquire)
                || m_NoCollision.load(std::memory_order_acquire)
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

        static Native::NativeHandler& VehicleEngineHandler() noexcept
        {
            static Native::NativeHandler handler{};
            return handler;
        }

        static Native::NativeHandler& EntityUpsidedownHandler() noexcept
        {
            static Native::NativeHandler handler{};
            return handler;
        }

        static Native::NativeHandler& EntityCollisionHandler() noexcept
        {
            static Native::NativeHandler handler{};
            return handler;
        }

        static Native::NativeHandler& EntityVelocityHandler() noexcept
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

        [[nodiscard]] static bool ResolveVehicleEngineHandler() noexcept
        {
            // SET_VEHICLE_ENGINE_ON - already used by EnhancedNativeToolkit.
            return ResolveSingleHandler(VehicleEngineHandler(), 0xC229299217554C78ull);
        }

        [[nodiscard]] static bool ResolveEntityUpsidedownHandler() noexcept
        {
            // IS_ENTITY_UPSIDEDOWN
            // legacy 1DBD58820FA61D71 -> Enhanced D1F1A906BA9226BE.
            return ResolveSingleHandler(EntityUpsidedownHandler(), 0xD1F1A906BA9226BEull);
        }

        [[nodiscard]] static bool ResolveEntityCollisionHandler() noexcept
        {
            // SET_ENTITY_COLLISION
            // legacy 1A9205C1B9EE827F -> Enhanced 44C48AC14D3C09ED.
            return ResolveSingleHandler(EntityCollisionHandler(), 0x44C48AC14D3C09EDull);
        }

        [[nodiscard]] static bool ResolveEntityVelocityHandler() noexcept
        {
            // SET_ENTITY_VELOCITY - already used by TeleportRuntime.
            return ResolveSingleHandler(EntityVelocityHandler(), 0x1AB7223AC0702871ull);
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

        [[nodiscard]] static bool SetVehicleEngineOn(Vehicle vehicle, bool enabled) noexcept
        {
            if (vehicle == 0 || !ResolveVehicleEngineHandler())
                return false;

            Native::CallContext context;
            if (!context.PushArg(vehicle)
                || !context.PushArg(static_cast<std::int32_t>(enabled))
                || !context.PushArg(std::int32_t{1})
                || !context.PushArg(std::int32_t{1}))
            {
                return false;
            }

            VehicleEngineHandler()(&context);
            return true;
        }

        [[nodiscard]] static bool IsEntityUpsidedown(Entity entity, bool& upsideDown) noexcept
        {
            if (entity == 0 || !ResolveEntityUpsidedownHandler())
                return false;

            Native::CallContext context;
            if (!context.PushArg(entity))
                return false;
            EntityUpsidedownHandler()(&context);
            upsideDown = context.GetReturnValue<std::int32_t>() != 0;
            return true;
        }

        [[nodiscard]] static bool SetEntityCollision(Entity entity, bool enabled) noexcept
        {
            if (entity == 0 || !ResolveEntityCollisionHandler())
                return false;

            Native::CallContext context;
            if (!context.PushArg(entity)
                || !context.PushArg(static_cast<std::int32_t>(enabled))
                || !context.PushArg(std::int32_t{1}))
            {
                return false;
            }
            EntityCollisionHandler()(&context);
            return true;
        }

        [[nodiscard]] static bool SetEntityVelocity(Entity entity, float x, float y, float z) noexcept
        {
            if (entity == 0 || !ResolveEntityVelocityHandler())
                return false;

            Native::CallContext context;
            if (!context.PushArg(entity)
                || !context.PushArg(x)
                || !context.PushArg(y)
                || !context.PushArg(z))
            {
                return false;
            }
            EntityVelocityHandler()(&context);
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

            static_cast<void>(Natives::SetVehicleDirtLevel(vehicle, 0.0f));
            static_cast<void>(RemoveVehicleDecals(vehicle));
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

        void RestoreLastTyreVehicle() noexcept
        {
            if (m_LastTyreVehicle == 0)
                return;

            const auto exists = Natives::DoesEntityExist(m_LastTyreVehicle);
            if (exists && *exists && m_HaveOriginalTyreState)
                static_cast<void>(VehicleNatives::SetVehicleTyresCanBurst(m_LastTyreVehicle, m_OriginalTyresCanBurst));

            m_LastTyreVehicle = 0;
            m_HaveOriginalTyreState = false;
            m_OriginalTyresCanBurst = true;
        }

        void RestoreLastInvisibleVehicle() noexcept
        {
            if (m_LastInvisibleVehicle == 0)
                return;

            const auto exists = Natives::DoesEntityExist(m_LastInvisibleVehicle);
            if (exists && *exists)
                static_cast<void>(PlayerNatives::SetEntityVisible(m_LastInvisibleVehicle, true, false));
            m_LastInvisibleVehicle = 0;
        }

        void RestoreLastCollisionVehicle() noexcept
        {
            if (m_LastCollisionVehicle == 0)
                return;

            const auto exists = Natives::DoesEntityExist(m_LastCollisionVehicle);
            if (exists && *exists)
                static_cast<void>(SetEntityCollision(m_LastCollisionVehicle, true));
            m_LastCollisionVehicle = 0;
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
            const bool autoRepair = m_AutoRepair.load(std::memory_order_acquire);
            const bool bulletproofTyres = m_BulletproofTyres.load(std::memory_order_acquire);
            const bool engineAlwaysOn = m_EngineAlwaysOn.load(std::memory_order_acquire);
            const bool autoFlip = m_AutoFlip.load(std::memory_order_acquire);
            const bool invisible = m_VehicleInvisible.load(std::memory_order_acquire);
            const bool noCollision = m_NoCollision.load(std::memory_order_acquire);
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
                RestoreLastGodVehicle();
            }

            if (autoRepair && vehicle != 0)
                static_cast<void>(Natives::SetVehicleFixed(vehicle));

            if (bulletproofTyres && vehicle != 0)
            {
                if (m_LastTyreVehicle != 0 && m_LastTyreVehicle != vehicle)
                    RestoreLastTyreVehicle();

                if (m_LastTyreVehicle == 0)
                {
                    const auto current = VehicleNatives::GetVehicleTyresCanBurst(vehicle);
                    if (current)
                    {
                        m_LastTyreVehicle = vehicle;
                        m_OriginalTyresCanBurst = *current;
                        m_HaveOriginalTyreState = true;
                    }
                }

                if (m_LastTyreVehicle == vehicle && m_HaveOriginalTyreState)
                    static_cast<void>(VehicleNatives::SetVehicleTyresCanBurst(vehicle, false));
            }
            else
            {
                RestoreLastTyreVehicle();
            }

            if (engineAlwaysOn && vehicle != 0)
                static_cast<void>(SetVehicleEngineOn(vehicle, true));

            if (autoFlip && vehicle != 0)
            {
                bool upsideDown{};
                const auto speed = EntityInspectorNatives::GetEntitySpeed(vehicle);
                if (speed && *speed < 6.0f && IsEntityUpsidedown(vehicle, upsideDown) && upsideDown)
                    static_cast<void>(Natives::SetVehicleOnGroundProperly(vehicle, 0.0f));
            }

            if (invisible && vehicle != 0)
            {
                if (m_LastInvisibleVehicle != 0 && m_LastInvisibleVehicle != vehicle)
                    RestoreLastInvisibleVehicle();

                if (PlayerNatives::SetEntityVisible(vehicle, false, false))
                    m_LastInvisibleVehicle = vehicle;
            }
            else
            {
                RestoreLastInvisibleVehicle();
            }

            if (noCollision && vehicle != 0)
            {
                if (m_LastCollisionVehicle != 0 && m_LastCollisionVehicle != vehicle)
                    RestoreLastCollisionVehicle();

                if (SetEntityCollision(vehicle, false))
                    m_LastCollisionVehicle = vehicle;
            }
            else
            {
                RestoreLastCollisionVehicle();
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
            RestoreLastTyreVehicle();
            RestoreLastInvisibleVehicle();
            RestoreLastCollisionVehicle();
            RestoreLastStance();
            m_Ticking.store(false, std::memory_order_release);
            if (AnyEnabled())
                EnsureTicking();
        }

        std::atomic<bool> m_VehicleGodMode{false};
        std::atomic<bool> m_AutoRepair{false};
        std::atomic<bool> m_BulletproofTyres{false};
        std::atomic<bool> m_EngineAlwaysOn{false};
        std::atomic<bool> m_AutoFlip{false};
        std::atomic<bool> m_VehicleInvisible{false};
        std::atomic<bool> m_NoCollision{false};
        std::atomic<bool> m_KeepVehicleClean{false};
        std::atomic<bool> m_LoweredStance{false};
        std::atomic<bool> m_Ticking{false};

        Vehicle m_LastGodVehicle{};
        Vehicle m_LastTyreVehicle{};
        Vehicle m_LastInvisibleVehicle{};
        Vehicle m_LastCollisionVehicle{};
        Vehicle m_LastStancedVehicle{};
        bool m_HaveOriginalTyreState{};
        bool m_OriginalTyresCanBurst{true};
    };
}
