#include "WeaponRuntime.hpp"

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/PlayerNatives.hpp"
#include "../../game/native/NativeInvoker.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <algorithm>
#include <cstdint>

namespace Tutones::Game::WeaponFeatures
{
    namespace
    {
        constexpr int MinExplosionType = -1;
        constexpr int MaxExplosionType = 83;
        constexpr float MinExplosionDamage = 0.0f;
        constexpr float MaxExplosionDamage = 1000.0f;
        constexpr float MinCameraShake = 0.0f;
        constexpr float MaxCameraShake = 10.0f;
    }

    WeaponRuntime& WeaponRuntime::Get() noexcept
    {
        static WeaponRuntime instance;
        return instance;
    }

    bool WeaponRuntime::Start()
    {
        bool expected = false;
        if (!m_Running.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            return true;

        Runtime::GameRuntime::Get().SetReleaseDeadTargetEnabled(m_ReleaseDeadPed.load(std::memory_order_acquire));

        if (QueueNextTick())
        {
            TUTONES_LOG_INFO("weapon.runtime", "Weapon runtime scheduled on the GTA script thread");
            return true;
        }

        m_Running.store(false, std::memory_order_release);
        Runtime::GameRuntime::Get().SetReleaseDeadTargetEnabled(false);
        TUTONES_LOG_ERROR("weapon.runtime", "Weapon runtime failed to queue its first GTA script-thread tick");
        return false;
    }

    void WeaponRuntime::Stop() noexcept
    {
        if (!m_Running.exchange(false, std::memory_order_acq_rel))
            return;

        Runtime::GameRuntime::Get().SetReleaseDeadTargetEnabled(false);
        RestorePatches();
        QueueNativeCleanup();
        TUTONES_LOG_INFO("weapon.runtime", "Weapon runtime stopped and reversible weapon state was restored");
    }

    bool WeaponRuntime::IsRunning() const noexcept
    {
        return m_Running.load(std::memory_order_acquire);
    }

    WeaponSnapshot WeaponRuntime::Snapshot() const noexcept
    {
        auto& pointers = GamePointers::Get();

        WeaponSnapshot snapshot{};
        snapshot.settings.infiniteAmmo = m_InfiniteAmmo.load(std::memory_order_acquire);
        snapshot.settings.infiniteClip = m_InfiniteClip.load(std::memory_order_acquire);
        snapshot.settings.aimbot = m_Aimbot.load(std::memory_order_acquire);
        snapshot.settings.aimForHead = m_AimForHead.load(std::memory_order_acquire);
        snapshot.settings.targetDrivers = m_TargetDrivers.load(std::memory_order_acquire);
        snapshot.settings.releaseDeadPed = m_ReleaseDeadPed.load(std::memory_order_acquire);
        snapshot.settings.explosiveAmmo = m_ExplosiveAmmo.load(std::memory_order_acquire);
        snapshot.settings.explosionType = m_ExplosionType.load(std::memory_order_acquire);
        snapshot.settings.explosionDamage = m_ExplosionDamage.load(std::memory_order_acquire);
        snapshot.settings.explosionCameraShake = m_ExplosionCameraShake.load(std::memory_order_acquire);
        snapshot.nativeReady = Native::NativeRegistry::Get().IsReady();
        snapshot.aimbotSupported = pointers.ShouldNotTargetEntityPatch().IsConfigured()
            && pointers.GetAssistedAimTypePatch().IsConfigured();
        snapshot.aimForHeadSupported = pointers.GetLockOnPosPatch().IsConfigured();
        snapshot.targetDriversSupported = pointers.ShouldAllowDriverLockOnPatch().IsConfigured();
        snapshot.releaseDeadTargetSupported = Runtime::GameRuntime::Get().ReleaseDeadTargetSupported();
        snapshot.running = IsRunning();
        return snapshot;
    }

    void WeaponRuntime::SetInfiniteAmmo(bool enabled) noexcept
    {
        m_InfiniteAmmo.store(enabled, std::memory_order_release);
    }

    void WeaponRuntime::SetInfiniteClip(bool enabled) noexcept
    {
        m_InfiniteClip.store(enabled, std::memory_order_release);
    }

    void WeaponRuntime::SetAimbot(bool enabled) noexcept
    {
        m_Aimbot.store(enabled, std::memory_order_release);
    }

    void WeaponRuntime::SetAimForHead(bool enabled) noexcept
    {
        m_AimForHead.store(enabled, std::memory_order_release);
    }

    void WeaponRuntime::SetTargetDrivers(bool enabled) noexcept
    {
        m_TargetDrivers.store(enabled, std::memory_order_release);
    }

    void WeaponRuntime::SetReleaseDeadPed(bool enabled) noexcept
    {
        m_ReleaseDeadPed.store(enabled, std::memory_order_release);
        Runtime::GameRuntime::Get().SetReleaseDeadTargetEnabled(enabled);
    }

    void WeaponRuntime::SetExplosiveAmmo(bool enabled) noexcept
    {
        m_ExplosiveAmmo.store(enabled, std::memory_order_release);
    }

    void WeaponRuntime::SetExplosionType(int type) noexcept
    {
        m_ExplosionType.store(std::clamp(type, MinExplosionType, MaxExplosionType), std::memory_order_release);
    }

    void WeaponRuntime::SetExplosionDamage(float damage) noexcept
    {
        m_ExplosionDamage.store(std::clamp(damage, MinExplosionDamage, MaxExplosionDamage), std::memory_order_release);
    }

    void WeaponRuntime::SetExplosionCameraShake(float shake) noexcept
    {
        m_ExplosionCameraShake.store(std::clamp(shake, MinCameraShake, MaxCameraShake), std::memory_order_release);
    }

    bool WeaponRuntime::QueueNextTick()
    {
        if (!IsRunning())
            return false;
        return Runtime::GameRuntime::Get().Enqueue([this] { TickOnGameThread(); });
    }

    void WeaponRuntime::TickOnGameThread() noexcept
    {
        if (!IsRunning())
            return;

        ApplyPatchState();
        Runtime::GameRuntime::Get().SetReleaseDeadTargetEnabled(m_ReleaseDeadPed.load(std::memory_order_acquire));

        if (Native::NativeRegistry::Get().IsReady())
            ApplyNativeState();

        if (IsRunning() && !QueueNextTick())
        {
            m_Running.store(false, std::memory_order_release);
            Runtime::GameRuntime::Get().SetReleaseDeadTargetEnabled(false);
            RestorePatches();
            TUTONES_LOG_ERROR("weapon.runtime", "Weapon runtime lost its GTA script-thread scheduling slot and stopped");
        }
    }

    void WeaponRuntime::ApplyPatchState() noexcept
    {
        auto& pointers = GamePointers::Get();
        const bool aimbot = m_Aimbot.load(std::memory_order_acquire);
        const bool aimForHead = m_AimForHead.load(std::memory_order_acquire);
        const bool targetDrivers = m_TargetDrivers.load(std::memory_order_acquire);

        auto& shouldNotTarget = pointers.ShouldNotTargetEntityPatch();
        auto& assistedAimType = pointers.GetAssistedAimTypePatch();
        auto& lockOnPos = pointers.GetLockOnPosPatch();
        auto& driverLockOn = pointers.ShouldAllowDriverLockOnPatch();

        if (aimbot)
        {
            if (shouldNotTarget.IsConfigured()) static_cast<void>(shouldNotTarget.Apply());
            if (assistedAimType.IsConfigured()) static_cast<void>(assistedAimType.Apply());
        }
        else
        {
            static_cast<void>(shouldNotTarget.Restore());
            static_cast<void>(assistedAimType.Restore());
        }

        if (aimbot && aimForHead)
        {
            if (lockOnPos.IsConfigured()) static_cast<void>(lockOnPos.Apply());
        }
        else
            static_cast<void>(lockOnPos.Restore());

        if (aimbot && targetDrivers)
        {
            if (driverLockOn.IsConfigured()) static_cast<void>(driverLockOn.Apply());
        }
        else
            static_cast<void>(driverLockOn.Restore());
    }

    void WeaponRuntime::ApplyNativeState() noexcept
    {
        const auto ped = PlayerNatives::PlayerPedId();
        if (!ped || *ped == 0)
            return;

        const bool infiniteAmmo = m_InfiniteAmmo.load(std::memory_order_acquire);
        if (infiniteAmmo)
        {
            if (Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetPedInfiniteAmmo,
                *ped,
                std::int32_t{1},
                std::uint32_t{0}))
                m_InfiniteAmmoWasApplied = true;
        }
        else if (m_InfiniteAmmoWasApplied)
        {
            if (Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetPedInfiniteAmmo,
                *ped,
                std::int32_t{0},
                std::uint32_t{0}))
                m_InfiniteAmmoWasApplied = false;
        }

        const bool infiniteClip = m_InfiniteClip.load(std::memory_order_acquire);
        if (infiniteClip)
        {
            if (Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetPedInfiniteAmmoClip,
                *ped,
                std::int32_t{1}))
                m_InfiniteClipWasApplied = true;
        }
        else if (m_InfiniteClipWasApplied)
        {
            if (Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetPedInfiniteAmmoClip,
                *ped,
                std::int32_t{0}))
                m_InfiniteClipWasApplied = false;
        }

        if (!m_ExplosiveAmmo.load(std::memory_order_acquire))
            return;

        const auto armed = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::IsPedArmed, *ped, std::int32_t{4});
        if (!armed || *armed == 0)
            return;

        const auto melee = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::IsPedPerformingMeleeAction, *ped);
        if (!melee || *melee != 0)
            return;

        Native::NativeVector3 impact{};
        const auto hasImpact = Native::NativeInvoker::Invoke<std::int32_t>(
            Native::NativeId::GetPedLastWeaponImpactCoord,
            *ped,
            &impact);
        if (!hasImpact || *hasImpact == 0)
            return;

        static_cast<void>(Native::NativeInvoker::InvokeVoid(
            Native::NativeId::AddOwnedExplosion,
            *ped,
            impact.x,
            impact.y,
            impact.z,
            m_ExplosionType.load(std::memory_order_acquire),
            m_ExplosionDamage.load(std::memory_order_acquire),
            std::int32_t{1},
            std::int32_t{0},
            m_ExplosionCameraShake.load(std::memory_order_acquire)));
    }

    void WeaponRuntime::RestorePatches() noexcept
    {
        auto& pointers = GamePointers::Get();
        static_cast<void>(pointers.ShouldNotTargetEntityPatch().Restore());
        static_cast<void>(pointers.GetAssistedAimTypePatch().Restore());
        static_cast<void>(pointers.GetLockOnPosPatch().Restore());
        static_cast<void>(pointers.ShouldAllowDriverLockOnPatch().Restore());
    }

    void WeaponRuntime::QueueNativeCleanup() noexcept
    {
        auto& runtime = Runtime::GameRuntime::Get();
        if (!runtime.IsInitialized())
            return;

        static_cast<void>(runtime.Enqueue([] {
            const auto ped = PlayerNatives::PlayerPedId();
            if (!ped || *ped == 0)
                return;

            static_cast<void>(Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetPedInfiniteAmmo,
                *ped,
                std::int32_t{0},
                std::uint32_t{0}));
            static_cast<void>(Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetPedInfiniteAmmoClip,
                *ped,
                std::int32_t{0}));
        }));
    }
}
