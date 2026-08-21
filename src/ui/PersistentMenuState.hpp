#pragma once

#include "MiscPanel.hpp"
#include "../core/config/MenuSettings.hpp"
#include "../features/network/NetworkRuntime.hpp"
#include "../features/player/OffRadarRuntime.hpp"
#include "../features/player/PlayerRuntime.hpp"
#include "../features/recovery/RecoveryRuntime.hpp"
#include "../features/vehicle/LscBypassRuntime.hpp"
#include "../features/weapon/WeaponRuntime.hpp"
#include "../features/world/TeleportRuntime.hpp"
#include "../features/world/WorldRuntime.hpp"

#include <atomic>
#include <chrono>

namespace Tutones::UI
{
    namespace PersistentMenuStateDetail
    {
        inline bool g_Staged{};
        inline std::chrono::steady_clock::time_point g_CaptureAfter{};

        inline void StageLoadedSettings() noexcept
        {
            auto& service = Core::Config::MenuSettingsService::Get();
            const auto settings = service.Current();

            auto& player = Game::PlayerFeatures::PlayerRuntime::Get();
            player.SetInvincible(settings.player.invincible);
            player.SetBulletproof(settings.player.bulletproof);
            player.SetAquaLungs(settings.player.aquaLungs);
            player.SetInfiniteOxygen(settings.player.infiniteOxygen);
            player.SetInvisible(settings.player.invisible);
            player.SetNoRagdoll(settings.player.noRagdoll);
            player.SetSuperJump(settings.player.superJump);
            player.SetInfiniteStamina(settings.player.infiniteStamina);
            player.SetKeepPlayerClean(settings.player.keepPlayerClean);
            player.SetDisableCriticalHits(settings.player.disableCriticalHits);
            player.SetStandOnVehicles(settings.player.standOnVehicles);
            player.SetDisableActionMode(settings.player.disableActionMode);
            player.SetInfiniteParachutes(settings.player.infiniteParachutes);
            player.SetMobileRadio(settings.player.mobileRadio);
            player.SetNeverWanted(settings.player.neverWanted);
            player.SetPoliceIgnore(settings.player.policeIgnore);
            player.SetEveryoneIgnore(settings.player.everyoneIgnore);
            player.SetRunMultiplier(settings.player.runMultiplier);
            player.SetSwimMultiplier(settings.player.swimMultiplier);

            Game::PlayerFeatures::OffRadarRuntime::Get().SetEnabled(settings.offRadar);
            Game::Mods::LscBypassRuntime::Get().SetEnabled(settings.vehicle.removeLscRestrictions);

            auto& weapons = Game::WeaponFeatures::WeaponRuntime::Get();
            weapons.SetInfiniteAmmo(settings.weapons.infiniteAmmo);
            weapons.SetInfiniteClip(settings.weapons.infiniteClip);
            weapons.SetAimbot(settings.weapons.aimbot);
            weapons.SetAimForHead(settings.weapons.aimForHead);
            weapons.SetTargetDrivers(settings.weapons.targetDrivers);
            weapons.SetReleaseDeadPed(settings.weapons.releaseDeadPed);
            weapons.SetExplosiveAmmo(settings.weapons.explosiveAmmo);
            weapons.SetExplosionType(settings.weapons.explosionType);
            weapons.SetExplosionDamage(settings.weapons.explosionDamage);
            weapons.SetExplosionCameraShake(settings.weapons.explosionCameraShake);

            auto& network = Game::NetworkFeatures::NetworkRuntime::Get();
            network.SetSilencePhoneCalls(settings.network.silencePhoneCalls);
            network.SetDisableDeathBarriers(settings.network.disableDeathBarriers);

            auto& recovery = Game::Recovery::RecoveryRuntime::Get();
            recovery.SetRpMultiplier(settings.recovery.rpMultiplier);
            recovery.SetRpMultiplierEnabled(settings.recovery.rpMultiplierEnabled);

            auto& world = Game::World::WorldRuntime::Get();
            world.SetPedDensity(settings.world.pedDensity);
            world.SetScenarioPedDensity(settings.world.scenarioPedDensity);
            world.SetVehicleDensity(settings.world.vehicleDensity);
            world.SetRandomVehicleDensity(settings.world.randomVehicleDensity);
            world.SetParkedVehicleDensity(settings.world.parkedVehicleDensity);
            static_cast<void>(world.QueuePersistentWorldState(settings.world.freezeClock, settings.world.blackout));

            Game::World::TeleportRuntime::Get().SetAutoWaypoint(settings.world.autoWaypoint);

            MiscPanelDetail::g_ShowCoordinates.store(settings.misc.showCoordinates, std::memory_order_release);
            MiscPanelDetail::g_ShowHeading.store(settings.misc.showHeading, std::memory_order_release);
            MiscPanelDetail::g_ShowFps.store(settings.misc.showFps, std::memory_order_release);
            MiscPanelDetail::g_ShowSessionInfo.store(settings.misc.showSessionInfo, std::memory_order_release);
            MiscPanelDetail::g_DisableCameraShake.store(settings.misc.disableCameraShake, std::memory_order_release);

            g_CaptureAfter = std::chrono::steady_clock::now() + std::chrono::milliseconds(1500);
            g_Staged = true;
        }

        inline void CaptureLiveSettings() noexcept
        {
            auto& settings = Core::Config::MenuSettingsService::Get().Current();

            const auto player = Game::PlayerFeatures::PlayerRuntime::Get().Snapshot();
            settings.player.invincible = player.invincible;
            settings.player.bulletproof = player.bulletproof;
            settings.player.aquaLungs = player.aquaLungs;
            settings.player.infiniteOxygen = player.infiniteOxygen;
            settings.player.invisible = player.invisible;
            settings.player.noRagdoll = player.noRagdoll;
            settings.player.superJump = player.superJump;
            settings.player.infiniteStamina = player.infiniteStamina;
            settings.player.keepPlayerClean = player.keepPlayerClean;
            settings.player.disableCriticalHits = player.disableCriticalHits;
            settings.player.standOnVehicles = player.standOnVehicles;
            settings.player.disableActionMode = player.disableActionMode;
            settings.player.infiniteParachutes = player.infiniteParachutes;
            settings.player.mobileRadio = player.mobileRadio;
            settings.player.neverWanted = player.neverWanted;
            settings.player.policeIgnore = player.policeIgnore;
            settings.player.everyoneIgnore = player.everyoneIgnore;
            settings.player.runMultiplier = player.runMultiplier;
            settings.player.swimMultiplier = player.swimMultiplier;

            settings.offRadar = Game::PlayerFeatures::OffRadarRuntime::Get().Snapshot().enabled;
            settings.vehicle.removeLscRestrictions = Game::Mods::LscBypassRuntime::Get().Enabled();

            const auto weapon = Game::WeaponFeatures::WeaponRuntime::Get().Snapshot().settings;
            settings.weapons.infiniteAmmo = weapon.infiniteAmmo;
            settings.weapons.infiniteClip = weapon.infiniteClip;
            settings.weapons.aimbot = weapon.aimbot;
            settings.weapons.aimForHead = weapon.aimForHead;
            settings.weapons.targetDrivers = weapon.targetDrivers;
            settings.weapons.releaseDeadPed = weapon.releaseDeadPed;
            settings.weapons.explosiveAmmo = weapon.explosiveAmmo;
            settings.weapons.explosionType = weapon.explosionType;
            settings.weapons.explosionDamage = weapon.explosionDamage;
            settings.weapons.explosionCameraShake = weapon.explosionCameraShake;

            const auto network = Game::NetworkFeatures::NetworkRuntime::Get().Snapshot();
            settings.network.silencePhoneCalls = network.silencePhoneCalls;
            settings.network.disableDeathBarriers = network.disableDeathBarriers;

            const auto recovery = Game::Recovery::RecoveryRuntime::Get().Snapshot();
            settings.recovery.rpMultiplierEnabled = recovery.rpMultiplierEnabled;
            settings.recovery.rpMultiplier = recovery.requestedRpMultiplier;

            const auto world = Game::World::WorldRuntime::Get().Snapshot();
            settings.world.pedDensity = world.pedDensity;
            settings.world.scenarioPedDensity = world.scenarioPedDensity;
            settings.world.vehicleDensity = world.vehicleDensity;
            settings.world.randomVehicleDensity = world.randomVehicleDensity;
            settings.world.parkedVehicleDensity = world.parkedVehicleDensity;
            settings.world.freezeClock = world.freezeClock;
            settings.world.blackout = world.blackout;
            settings.world.autoWaypoint = Game::World::TeleportRuntime::Get().Snapshot().autoWaypointEnabled;

            settings.misc.showCoordinates = MiscPanelDetail::g_ShowCoordinates.load(std::memory_order_acquire);
            settings.misc.showHeading = MiscPanelDetail::g_ShowHeading.load(std::memory_order_acquire);
            settings.misc.showFps = MiscPanelDetail::g_ShowFps.load(std::memory_order_acquire);
            settings.misc.showSessionInfo = MiscPanelDetail::g_ShowSessionInfo.load(std::memory_order_acquire);
            settings.misc.disableCameraShake = MiscPanelDetail::g_DisableCameraShake.load(std::memory_order_acquire);
        }
    }

    inline void SyncPersistentMenuState() noexcept
    {
        using namespace PersistentMenuStateDetail;
        if (!g_Staged)
        {
            StageLoadedSettings();
            return;
        }

        if (std::chrono::steady_clock::now() < g_CaptureAfter)
            return;

        CaptureLiveSettings();
    }
}
