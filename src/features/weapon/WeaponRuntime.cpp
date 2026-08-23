#include "WeaponRuntime.hpp"

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/PlayerNatives.hpp"
#include "../../game/native/NativeInvoker.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

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
        constexpr const char* WeaponNames[] = {
            "WEAPON_UNARMED", "WEAPON_KNIFE", "WEAPON_NIGHTSTICK", "WEAPON_HAMMER", "WEAPON_BAT",
            "WEAPON_GOLFCLUB", "WEAPON_CROWBAR", "WEAPON_PISTOL", "WEAPON_COMBATPISTOL", "WEAPON_APPISTOL",
            "WEAPON_PISTOL50", "WEAPON_MICROSMG", "WEAPON_SMG", "WEAPON_ASSAULTSMG", "WEAPON_ASSAULTRIFLE",
            "WEAPON_CARBINERIFLE", "WEAPON_ADVANCEDRIFLE", "WEAPON_MG", "WEAPON_COMBATMG", "WEAPON_PUMPSHOTGUN",
            "WEAPON_SAWNOFFSHOTGUN", "WEAPON_ASSAULTSHOTGUN", "WEAPON_BULLPUPSHOTGUN", "WEAPON_STUNGUN",
            "WEAPON_SNIPERRIFLE", "WEAPON_HEAVYSNIPER", "WEAPON_REMOTESNIPER", "WEAPON_GRENADELAUNCHER",
            "WEAPON_GRENADELAUNCHER_SMOKE", "WEAPON_RPG", "WEAPON_MINIGUN", "WEAPON_GRENADE", "WEAPON_STICKYBOMB",
            "WEAPON_SMOKEGRENADE", "WEAPON_BZGAS", "WEAPON_MOLOTOV", "WEAPON_FIREEXTINGUISHER", "WEAPON_PETROLCAN",
            "WEAPON_BALL", "WEAPON_FLARE", "WEAPON_BOTTLE", "WEAPON_SNSPISTOL", "WEAPON_HEAVYPISTOL",
            "WEAPON_BULLPUPRIFLE", "WEAPON_SPECIALCARBINE", "WEAPON_SNSPISTOL_MK2", "WEAPON_SPECIALCARBINE_MK2",
            "WEAPON_PUMPSHOTGUN_MK2", "WEAPON_BULLPUPRIFLE_MK2", "WEAPON_MARKSMANRIFLE_MK2", "WEAPON_CANDYCANE",
            "WEAPON_PISTOLXM3", "WEAPON_RAILGUNXM3", "WEAPON_ACIDPACKAGE", "WEAPON_HOMINGLAUNCHER", "WEAPON_PROXMINE",
            "WEAPON_SNOWBALL", "WEAPON_DOUBLEACTION", "WEAPON_REVOLVER_MK2", "WEAPON_RAYPISTOL", "WEAPON_RAYCARBINE",
            "WEAPON_RAYMINIGUN", "WEAPON_GUSENBERG", "WEAPON_DAGGER", "WEAPON_VINTAGEPISTOL", "WEAPON_FIREWORK",
            "WEAPON_MUSKET", "WEAPON_HATCHET", "WEAPON_RAILGUN", "WEAPON_MARKSMANRIFLE", "WEAPON_HEAVYSHOTGUN",
            "WEAPON_CERAMICPISTOL", "WEAPON_MILITARYRIFLE", "WEAPON_GADGETPISTOL", "WEAPON_HAZARDCAN",
            "WEAPON_COMBATSHOTGUN", "WEAPON_NAVYREVOLVER", "WEAPON_FLAREGUN", "WEAPON_KNUCKLE", "WEAPON_COMBATPDW",
            "WEAPON_MARKSMANPISTOL", "WEAPON_DBSHOTGUN", "WEAPON_COMPACTRIFLE", "WEAPON_MACHINEPISTOL", "WEAPON_MACHETE",
            "WEAPON_FLASHLIGHT", "WEAPON_SWITCHBLADE", "WEAPON_REVOLVER", "WEAPON_WRENCH", "WEAPON_POOLCUE",
            "WEAPON_MINISMG", "WEAPON_BATTLEAXE", "WEAPON_AUTOSHOTGUN", "WEAPON_COMPACTLAUNCHER", "WEAPON_PIPEBOMB",
            "WEAPON_SMG_MK2", "WEAPON_COMBATMG_MK2", "WEAPON_CARBINERIFLE_MK2", "WEAPON_ASSAULTRIFLE_MK2",
            "WEAPON_HEAVYSNIPER_MK2", "WEAPON_PISTOL_MK2", "WEAPON_STONE_HATCHET", "WEAPON_TACTICALRIFLE",
            "WEAPON_PRECISIONRIFLE", "WEAPON_HEAVYRIFLE", "WEAPON_FERTILIZERCAN", "WEAPON_EMPLAUNCHER",
            "WEAPON_STUNGUN_MP", "WEAPON_TECPISTOL", "WEAPON_SNOWLAUNCHER", "WEAPON_HACKINGDEVICE",
            "WEAPON_BATTLERIFLE", "WEAPON_STUNROD", "WEAPON_STRICKLER", "WEAPON_BRIEFCASE_03", "WEAPON_NEWSPAPER",
        };

        constexpr std::uint32_t Joaat(const char* text) noexcept
        {
            std::uint32_t hash{};
            while (text && *text)
            {
                char c = *text++;
                if (c >= 'A' && c <= 'Z')
                    c = static_cast<char>(c - 'A' + 'a');
                hash += static_cast<std::uint8_t>(c);
                hash += hash << 10;
                hash ^= hash >> 6;
            }
            hash += hash << 3;
            hash ^= hash >> 11;
            hash += hash << 15;
            return hash;
        }
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

        auto& pointers = GamePointers::Get();
        const bool shouldNotTargetConfigured = pointers.ShouldNotTargetEntityPatch().IsConfigured();
        const bool assistedAimTypeConfigured = pointers.GetAssistedAimTypePatch().IsConfigured();
        const bool lockOnPosConfigured = pointers.GetLockOnPosPatch().IsConfigured();
        const bool driverLockOnConfigured = pointers.ShouldAllowDriverLockOnPatch().IsConfigured();

        std::string patchState("Weapon patch self-check: pointers=");
        patchState += pointers.IsResolved() ? "resolved" : "not-resolved";
        patchState += ", ShouldNotTargetEntity=";
        patchState += shouldNotTargetConfigured ? "configured" : "missing";
        patchState += ", GetAssistedAimType=";
        patchState += assistedAimTypeConfigured ? "configured" : "missing";
        patchState += ", GetLockOnPos=";
        patchState += lockOnPosConfigured ? "configured" : "missing";
        patchState += ", ShouldAllowDriverLockOn=";
        patchState += driverLockOnConfigured ? "configured" : "missing";

        if (pointers.IsResolved() && shouldNotTargetConfigured && assistedAimTypeConfigured)
            TUTONES_LOG_INFO("weapon.runtime", patchState);
        else
            TUTONES_LOG_WARN("weapon.runtime", patchState);

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

        // Application saves the requested settings before Stop(), so clear the live
        // runtime state to prevent stale feature state if the runtime is started again.
        m_InfiniteAmmo.store(false, std::memory_order_release);
        m_InfiniteClip.store(false, std::memory_order_release);
        m_Aimbot.store(false, std::memory_order_release);
        m_AimForHead.store(true, std::memory_order_release);
        m_TargetDrivers.store(true, std::memory_order_release);
        m_ReleaseDeadPed.store(false, std::memory_order_release);
        m_ExplosiveAmmo.store(false, std::memory_order_release);

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
        snapshot.pointersResolved = pointers.IsResolved();
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

    bool WeaponRuntime::QueueGiveAllWeapons()
    {
        if (!Native::NativeRegistry::Get().IsReady())
            return false;

        return Runtime::GameRuntime::Get().Enqueue([] {
            const auto ped = PlayerNatives::PlayerPedId();
            if (!ped || *ped == 0)
                return;

            bool success = true;
            for (const char* name : WeaponNames)
            {
                success = Native::NativeInvoker::InvokeVoid(
                    Native::NativeId::GiveWeaponToPed,
                    *ped,
                    Joaat(name),
                    9999,
                    std::int32_t{0},
                    std::int32_t{0}) && success;
            }
            if (success)
                TUTONES_LOG_INFO("weapon.runtime", "Give All Weapons dispatched across the weapon catalog");
            else
                TUTONES_LOG_WARN("weapon.runtime", "One or more Give All Weapons native calls could not be dispatched");
        });
    }

    bool WeaponRuntime::QueueGiveMaxAmmo()
    {
        if (!Native::NativeRegistry::Get().IsReady())
            return false;

        return Runtime::GameRuntime::Get().Enqueue([] {
            const auto ped = PlayerNatives::PlayerPedId();
            if (!ped || *ped == 0)
                return;

            bool success = true;
            for (const char* name : WeaponNames)
            {
                const std::uint32_t weapon = Joaat(name);
                int maxAmmo{};
                const auto hasMax = Native::NativeInvoker::Invoke<std::int32_t>(
                    Native::NativeId::GetMaxAmmo, *ped, weapon, &maxAmmo);
                if (!hasMax || *hasMax == 0 || maxAmmo <= 0)
                    continue;
                success = Native::NativeInvoker::InvokeVoid(
                    Native::NativeId::SetPedAmmo,
                    *ped,
                    weapon,
                    maxAmmo,
                    std::int32_t{0}) && success;
            }
            if (success)
                TUTONES_LOG_INFO("weapon.runtime", "Give Max Ammo dispatched across available player weapons");
            else
                TUTONES_LOG_WARN("weapon.runtime", "One or more max-ammo writes could not be dispatched");
        });
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
            TUTONES_LOG_ERROR("weapon.runtime", "Weapon runtime lost its GTA script-thread scheduling slot and is restoring state");
            // Stop from this callback while the native/TLS context is still valid. This
            // guarantees infinite-ammo/clip and laser state are restored immediately.
            Stop();
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
        static_cast<void>(Native::NativeInvoker::InvokeVoid(
            Native::NativeId::EnableLaserSightRendering,
            m_Aimbot.load(std::memory_order_acquire) ? std::int32_t{1} : std::int32_t{0}));

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
        {
            m_InfiniteAmmoWasApplied = false;
            m_InfiniteClipWasApplied = false;
            return;
        }

        const auto cleanup = [this] {
            static_cast<void>(Native::NativeInvoker::InvokeVoid(
                Native::NativeId::EnableLaserSightRendering,
                std::int32_t{0}));

            const auto ped = PlayerNatives::PlayerPedId();
            if (ped && *ped != 0)
            {
                static_cast<void>(Native::NativeInvoker::InvokeVoid(
                    Native::NativeId::SetPedInfiniteAmmo,
                    *ped,
                    std::int32_t{0},
                    std::uint32_t{0}));
                static_cast<void>(Native::NativeInvoker::InvokeVoid(
                    Native::NativeId::SetPedInfiniteAmmoClip,
                    *ped,
                    std::int32_t{0}));
            }

            m_InfiniteAmmoWasApplied = false;
            m_InfiniteClipWasApplied = false;
        };

        if (runtime.IsOnGameThread())
        {
            cleanup();
            return;
        }

        const auto cleaned = std::make_shared<std::atomic<bool>>(false);
        if (!runtime.Enqueue([cleanup, cleaned] {
                cleanup();
                cleaned->store(true, std::memory_order_release);
            }))
        {
            return;
        }

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
        while (!cleaned->load(std::memory_order_acquire)
            && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}
