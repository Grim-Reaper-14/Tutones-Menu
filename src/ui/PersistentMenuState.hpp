#pragma once

#include "MiscPanel.hpp"
#include "../core/config/MenuSettings.hpp"
#include "../features/business/VehicleCargoAutoSourceRuntime.hpp"
#include "../features/business/VehicleCargoInstantGarageRuntime.hpp"
#include "../features/business/VehicleCargoInstantSellRuntime.hpp"
#include "../features/game/NoIdleRuntime.hpp"
#include "../features/network/NetworkPlayerDefenseRuntime.hpp"
#include "../features/network/NetworkRuntime.hpp"
#include "../features/network/ProtectionRuntime.hpp"
#include "../features/player/GhostOrganizationRuntime.hpp"
#include "../features/player/OffRadarRuntime.hpp"
#include "../features/player/PlayerRuntime.hpp"
#include "../features/recovery/CasinoSlotMachineRuntime.hpp"
#include "../features/recovery/RecoveryRuntime.hpp"
#include "../features/vehicle/DlcVehicleRuntime.hpp"
#include "../features/vehicle/HornBoostRuntime.hpp"
#include "../features/vehicle/LscBypassRuntime.hpp"
#include "../features/vehicle/NitrousRuntime.hpp"
#include "../features/vehicle/VehicleAmmoRuntime.hpp"
#include "../features/vehicle/VehicleLoopFeatures.hpp"
#include "../features/vehicle/VehicleSuspensionRuntime.hpp"
#include "../features/weapon/WeaponRuntime.hpp"
#include "../features/world/TeleportRuntime.hpp"
#include "../features/world/WorldRuntime.hpp"
#include "../runtime/GameRuntime.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>

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
            Game::PlayerFeatures::GhostOrganizationRuntime::Get().SetEnabled(settings.player.ghostOrganization);

            Game::Mods::LscBypassRuntime::Get().SetEnabled(settings.vehicle.removeLscRestrictions);
            Game::VehicleFeatures::DlcVehicleRuntime::Get().SetEnabled(settings.vehicle.enableDlcVehicles);

            auto& vehicleLoop = Game::Mods::VehicleLoopFeatures::Get();
            vehicleLoop.SetVehicleGodMode(settings.vehicle.vehicleGodMode);
            vehicleLoop.SetKeepVehicleClean(settings.vehicle.keepVehicleClean);
            vehicleLoop.SetLoweredStance(settings.vehicle.loweredStance);

            Game::Mods::HornBoostRuntime::Get().SetEnabled(settings.vehicle.hornBoost);
            Game::Mods::VehicleAmmoRuntime::Get().SetEnabled(settings.vehicle.infiniteVehicleAmmo);

            auto& nitrous = Game::Mods::NitrousRuntime::Get();
            nitrous.SetUnlimited(settings.vehicle.nitrousUnlimited);
            nitrous.SetLevel(settings.vehicle.nitrousLevel);
            nitrous.SetPower(settings.vehicle.nitrousPower);
            nitrous.SetEnabled(settings.vehicle.nitrousEnabled);

            auto& suspension = Game::Mods::VehicleSuspensionRuntime::Get();
            suspension.SetLoweringAmount(settings.vehicle.suspensionLoweringAmount);
            suspension.SetEnabled(settings.vehicle.suspensionLoweringEnabled);

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

            auto& defense = Game::NetworkFeatures::NetworkPlayerDefenseRuntime::Get();
            defense.SetProximityWarningsEnabled(settings.network.proximityWarningsEnabled);
            defense.SetRestrictWatchlistedActions(settings.network.restrictWatchlistedActions);
            defense.SetAutoWatchHighRisk(settings.network.autoWatchHighRisk);
            defense.SetProximityRadius(settings.network.proximityRadius);

            auto& protections = Game::Protections::ProtectionRuntime::Get();
            protections.SetBlockMalformed(settings.protections.blockMalformed);
            protections.SetBlockForcedLeave(settings.protections.blockForcedLeave);
            protections.SetBlockKnownCrashes(settings.protections.blockKnownCrashes);
            protections.SetBlockSounds(settings.protections.blockSounds);
            protections.SetBlockExplosions(settings.protections.blockExplosions);
            protections.SetBlockFire(settings.protections.blockFire);
            protections.SetBlockWeaponDamage(settings.protections.blockWeaponDamage);
            protections.SetBlockRagdoll(settings.protections.blockRagdoll);
            protections.SetBlockClearTasks(settings.protections.blockClearTasks);
            protections.SetBlockPtfx(settings.protections.blockPtfx);
            protections.SetBlockScriptEvents(settings.protections.blockScriptEvents);
            protections.SetBlockMalformedScriptEvents(settings.protections.blockMalformedScriptEvents);

            Game::SessionFeatures::NoIdleRuntime::Get().SetEnabled(settings.session.noIdle);

            auto& recovery = Game::Recovery::RecoveryRuntime::Get();
            recovery.SetRpMultiplier(settings.recovery.rpMultiplier);
            recovery.SetRpMultiplierEnabled(settings.recovery.rpMultiplierEnabled);
            Game::Recovery::CasinoSlotMachineRuntime::Get().SetEnabled(settings.recovery.casinoSlotRig);

            // Auto Source and the full pipeline are mutually exclusive. Apply Auto Source
            // first so the full pipeline remains authoritative if a hand-edited file enables both.
            Game::Business::VehicleCargoAutoSourceRuntime::Get().SetEnabled(settings.business.vehicleCargoAutoSource);
            Game::Business::VehicleCargoInstantGarageRuntime::Get().SetEnabled(settings.business.vehicleCargoInstantGarage);
            Game::Business::VehicleCargoInstantSellRuntime::Get().SetEnabled(settings.business.vehicleCargoInstantSell);

            auto& world = Game::World::WorldRuntime::Get();
            world.SetPedDensity(settings.world.pedDensity);
            world.SetScenarioPedDensity(settings.world.scenarioPedDensity);
            world.SetVehicleDensity(settings.world.vehicleDensity);
            world.SetRandomVehicleDensity(settings.world.randomVehicleDensity);
            world.SetParkedVehicleDensity(settings.world.parkedVehicleDensity);
            static_cast<void>(world.QueuePersistentWorldState(
                settings.world.freezeClock,
                settings.world.blackout,
                settings.world.setHour,
                settings.world.setMinute,
                settings.world.weatherIndex,
                settings.world.forceWeather));

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
            settings.player.ghostOrganization = Game::PlayerFeatures::GhostOrganizationRuntime::Get().Snapshot().enabled;

            settings.offRadar = Game::PlayerFeatures::OffRadarRuntime::Get().Snapshot().enabled;

            settings.vehicle.removeLscRestrictions = Game::Mods::LscBypassRuntime::Get().Enabled();
            settings.vehicle.enableDlcVehicles = Game::VehicleFeatures::DlcVehicleRuntime::Get().Enabled();
            auto& vehicleLoop = Game::Mods::VehicleLoopFeatures::Get();
            settings.vehicle.vehicleGodMode = vehicleLoop.VehicleGodMode();
            settings.vehicle.keepVehicleClean = vehicleLoop.KeepVehicleClean();
            settings.vehicle.loweredStance = vehicleLoop.LoweredStance();
            settings.vehicle.hornBoost = Game::Mods::HornBoostRuntime::Get().Enabled();
            settings.vehicle.infiniteVehicleAmmo = Game::Mods::VehicleAmmoRuntime::Get().Enabled();
            auto& nitrous = Game::Mods::NitrousRuntime::Get();
            settings.vehicle.nitrousEnabled = nitrous.Enabled();
            settings.vehicle.nitrousUnlimited = nitrous.Unlimited();
            settings.vehicle.nitrousLevel = nitrous.Level();
            settings.vehicle.nitrousPower = nitrous.Power();
            auto& suspension = Game::Mods::VehicleSuspensionRuntime::Get();
            settings.vehicle.suspensionLoweringEnabled = suspension.Enabled();
            settings.vehicle.suspensionLoweringAmount = suspension.LoweringAmount();

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

            const auto defense = Game::NetworkFeatures::NetworkPlayerDefenseRuntime::Get().Snapshot();
            settings.network.proximityWarningsEnabled = defense.proximityWarningsEnabled;
            settings.network.restrictWatchlistedActions = defense.restrictWatchlistedActions;
            settings.network.autoWatchHighRisk = defense.autoWatchHighRisk;
            settings.network.proximityRadius = defense.proximityRadius;

            const auto protections = Game::Protections::ProtectionRuntime::Get().Snapshot();
            settings.protections.blockMalformed = protections.blockMalformed;
            settings.protections.blockForcedLeave = protections.blockForcedLeave;
            settings.protections.blockKnownCrashes = protections.blockKnownCrashes;
            settings.protections.blockSounds = protections.blockSounds;
            settings.protections.blockExplosions = protections.blockExplosions;
            settings.protections.blockFire = protections.blockFire;
            settings.protections.blockWeaponDamage = protections.blockWeaponDamage;
            settings.protections.blockRagdoll = protections.blockRagdoll;
            settings.protections.blockClearTasks = protections.blockClearTasks;
            settings.protections.blockPtfx = protections.blockPtfx;
            settings.protections.blockScriptEvents = protections.blockScriptEvents;
            settings.protections.blockMalformedScriptEvents = protections.blockMalformedScriptEvents;

            settings.session.noIdle = Game::SessionFeatures::NoIdleRuntime::Get().Snapshot().enabled;

            settings.business.vehicleCargoAutoSource = Game::Business::VehicleCargoAutoSourceRuntime::Get().Enabled();
            settings.business.vehicleCargoInstantGarage = Game::Business::VehicleCargoInstantGarageRuntime::Get().Enabled();
            settings.business.vehicleCargoInstantSell = Game::Business::VehicleCargoInstantSellRuntime::Get().Enabled();

            const auto recovery = Game::Recovery::RecoveryRuntime::Get().Snapshot();
            settings.recovery.rpMultiplierEnabled = recovery.rpMultiplierEnabled;
            settings.recovery.rpMultiplier = recovery.requestedRpMultiplier;
            settings.recovery.casinoSlotRig = Game::Recovery::CasinoSlotMachineRuntime::Get().Enabled();

            const auto world = Game::World::WorldRuntime::Get().Snapshot();
            settings.world.pedDensity = world.pedDensity;
            settings.world.scenarioPedDensity = world.scenarioPedDensity;
            settings.world.vehicleDensity = world.vehicleDensity;
            settings.world.randomVehicleDensity = world.randomVehicleDensity;
            settings.world.parkedVehicleDensity = world.parkedVehicleDensity;
            settings.world.freezeClock = world.freezeClock;
            settings.world.forceWeather = world.weatherOverrideActive;
            settings.world.blackout = world.blackout;
            settings.world.setHour = world.selectedHour;
            settings.world.setMinute = world.selectedMinute;
            for (std::size_t index = 0; index < Game::World::WeatherCodes.size(); ++index)
            {
                if (world.weatherCode == Game::World::WeatherCodes[index])
                {
                    settings.world.weatherIndex = static_cast<int>(index);
                    break;
                }
            }
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

        // Persistent settings belong to the menu core, not to any one feature.
        // One-shot commands and live diagnostic results are intentionally excluded.
        if (!Runtime::GameRuntime::Get().IsInitialized())
        {
            g_Staged = false;
            g_CaptureAfter = {};
            return;
        }

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
