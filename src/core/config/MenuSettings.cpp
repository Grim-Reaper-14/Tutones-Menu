#include "MenuSettings.hpp"

#include <nlohmann/json.hpp>

#include <fstream>

namespace Tutones::Core::Config
{
    MenuSettingsService& MenuSettingsService::Get() noexcept
    {
        static MenuSettingsService instance;
        return instance;
    }

    bool MenuSettingsService::Load(const std::filesystem::path& path) noexcept
    {
        Reset();

        try
        {
            std::ifstream stream(path);
            if (!stream)
                return false;

            nlohmann::json document;
            stream >> document;
            if (!document.is_object())
                return false;

            const auto player = document.value("player", nlohmann::json::object());
            const auto vehicle = document.value("vehicle", nlohmann::json::object());
            const auto weapons = document.value("weapons", nlohmann::json::object());
            const auto network = document.value("network", nlohmann::json::object());
            const auto protections = document.value("protections", nlohmann::json::object());
            const auto session = document.value("session", nlohmann::json::object());
            const auto business = document.value("business", nlohmann::json::object());
            const auto recovery = document.value("recovery", nlohmann::json::object());
            const auto world = document.value("world", nlohmann::json::object());
            const auto misc = document.value("misc", nlohmann::json::object());
            const auto ui = document.value("ui", nlohmann::json::object());

            // Older files remain valid. The next save upgrades them to schema v6.
            m_Settings.version = 6;
            m_Settings.offRadar = document.value("off_radar", m_Settings.offRadar);

            m_Settings.player.invincible = player.value("invincible", m_Settings.player.invincible);
            m_Settings.player.bulletproof = player.value("bulletproof", m_Settings.player.bulletproof);
            m_Settings.player.aquaLungs = player.value("aqua_lungs", m_Settings.player.aquaLungs);
            m_Settings.player.infiniteOxygen = player.value("infinite_oxygen", m_Settings.player.infiniteOxygen);
            m_Settings.player.invisible = player.value("invisible", m_Settings.player.invisible);
            m_Settings.player.noRagdoll = player.value("no_ragdoll", m_Settings.player.noRagdoll);
            m_Settings.player.superJump = player.value("super_jump", m_Settings.player.superJump);
            m_Settings.player.infiniteStamina = player.value("infinite_stamina", m_Settings.player.infiniteStamina);
            m_Settings.player.keepPlayerClean = player.value("keep_player_clean", m_Settings.player.keepPlayerClean);
            m_Settings.player.disableCriticalHits = player.value("disable_critical_hits", m_Settings.player.disableCriticalHits);
            m_Settings.player.standOnVehicles = player.value("stand_on_vehicles", m_Settings.player.standOnVehicles);
            m_Settings.player.disableActionMode = player.value("disable_action_mode", m_Settings.player.disableActionMode);
            m_Settings.player.infiniteParachutes = player.value("infinite_parachutes", m_Settings.player.infiniteParachutes);
            m_Settings.player.mobileRadio = player.value("mobile_radio", m_Settings.player.mobileRadio);
            m_Settings.player.neverWanted = player.value("never_wanted", m_Settings.player.neverWanted);
            m_Settings.player.policeIgnore = player.value("police_ignore", m_Settings.player.policeIgnore);
            m_Settings.player.everyoneIgnore = player.value("everyone_ignore", m_Settings.player.everyoneIgnore);
            m_Settings.player.ghostOrganization = player.value("ghost_organization", m_Settings.player.ghostOrganization);
            m_Settings.player.runMultiplier = player.value("run_multiplier", m_Settings.player.runMultiplier);
            m_Settings.player.swimMultiplier = player.value("swim_multiplier", m_Settings.player.swimMultiplier);

            m_Settings.vehicle.removeLscRestrictions = vehicle.value("remove_lsc_restrictions", m_Settings.vehicle.removeLscRestrictions);
            m_Settings.vehicle.enableDlcVehicles = vehicle.value("enable_dlc_vehicles", m_Settings.vehicle.enableDlcVehicles);
            m_Settings.vehicle.vehicleGodMode = vehicle.value("god_mode", m_Settings.vehicle.vehicleGodMode);
            m_Settings.vehicle.keepVehicleClean = vehicle.value("keep_clean", m_Settings.vehicle.keepVehicleClean);
            m_Settings.vehicle.loweredStance = vehicle.value("lowered_stance", m_Settings.vehicle.loweredStance);
            m_Settings.vehicle.hornBoost = vehicle.value("horn_boost", m_Settings.vehicle.hornBoost);
            m_Settings.vehicle.infiniteVehicleAmmo = vehicle.value("infinite_vehicle_ammo", m_Settings.vehicle.infiniteVehicleAmmo);
            m_Settings.vehicle.nitrousEnabled = vehicle.value("nitrous_enabled", m_Settings.vehicle.nitrousEnabled);
            m_Settings.vehicle.nitrousUnlimited = vehicle.value("nitrous_unlimited", m_Settings.vehicle.nitrousUnlimited);
            m_Settings.vehicle.nitrousLevel = vehicle.value("nitrous_level", m_Settings.vehicle.nitrousLevel);
            m_Settings.vehicle.nitrousPower = vehicle.value("nitrous_power", m_Settings.vehicle.nitrousPower);
            m_Settings.vehicle.suspensionLoweringEnabled = vehicle.value("suspension_lowering_enabled", m_Settings.vehicle.suspensionLoweringEnabled);
            m_Settings.vehicle.suspensionLoweringAmount = vehicle.value("suspension_lowering_amount", m_Settings.vehicle.suspensionLoweringAmount);

            m_Settings.weapons.infiniteAmmo = weapons.value("infinite_ammo", m_Settings.weapons.infiniteAmmo);
            m_Settings.weapons.infiniteClip = weapons.value("infinite_clip", m_Settings.weapons.infiniteClip);
            m_Settings.weapons.aimbot = weapons.value("aimbot", m_Settings.weapons.aimbot);
            m_Settings.weapons.aimForHead = weapons.value("aim_for_head", m_Settings.weapons.aimForHead);
            m_Settings.weapons.targetDrivers = weapons.value("target_drivers", m_Settings.weapons.targetDrivers);
            m_Settings.weapons.releaseDeadPed = weapons.value("release_dead_target", m_Settings.weapons.releaseDeadPed);
            m_Settings.weapons.explosiveAmmo = weapons.value("explosive_ammo", m_Settings.weapons.explosiveAmmo);
            m_Settings.weapons.explosionType = weapons.value("explosion_type", m_Settings.weapons.explosionType);
            m_Settings.weapons.explosionDamage = weapons.value("explosion_damage", m_Settings.weapons.explosionDamage);
            m_Settings.weapons.explosionCameraShake = weapons.value("explosion_camera_shake", m_Settings.weapons.explosionCameraShake);

            m_Settings.network.silencePhoneCalls = network.value("silence_phone_calls", m_Settings.network.silencePhoneCalls);
            m_Settings.network.disableDeathBarriers = network.value("disable_death_barriers", m_Settings.network.disableDeathBarriers);
            m_Settings.network.proximityWarningsEnabled = network.value("proximity_warnings", m_Settings.network.proximityWarningsEnabled);
            m_Settings.network.restrictWatchlistedActions = network.value("restrict_watchlisted_actions", m_Settings.network.restrictWatchlistedActions);
            m_Settings.network.autoWatchHighRisk = network.value("auto_watch_high_risk", m_Settings.network.autoWatchHighRisk);
            m_Settings.network.proximityRadius = network.value("proximity_radius", m_Settings.network.proximityRadius);

            m_Settings.protections.blockMalformed = protections.value("block_malformed", m_Settings.protections.blockMalformed);
            m_Settings.protections.blockForcedLeave = protections.value("block_forced_leave", m_Settings.protections.blockForcedLeave);
            m_Settings.protections.blockKnownCrashes = protections.value("block_known_crashes", m_Settings.protections.blockKnownCrashes);
            m_Settings.protections.blockSounds = protections.value("block_sounds", m_Settings.protections.blockSounds);
            m_Settings.protections.blockExplosions = protections.value("block_explosions", m_Settings.protections.blockExplosions);
            m_Settings.protections.blockFire = protections.value("block_fire", m_Settings.protections.blockFire);
            m_Settings.protections.blockWeaponDamage = protections.value("block_weapon_damage", m_Settings.protections.blockWeaponDamage);
            m_Settings.protections.blockRagdoll = protections.value("block_ragdoll", m_Settings.protections.blockRagdoll);
            m_Settings.protections.blockClearTasks = protections.value("block_clear_tasks", m_Settings.protections.blockClearTasks);
            m_Settings.protections.blockPtfx = protections.value("block_ptfx", m_Settings.protections.blockPtfx);
            m_Settings.protections.blockScriptEvents = protections.value("block_script_events", m_Settings.protections.blockScriptEvents);
            m_Settings.protections.blockMalformedScriptEvents = protections.value("block_malformed_script_events", m_Settings.protections.blockMalformedScriptEvents);

            m_Settings.session.noIdle = session.value("no_idle", m_Settings.session.noIdle);

            m_Settings.business.vehicleCargoAutoSource = business.value("vehicle_cargo_auto_source", m_Settings.business.vehicleCargoAutoSource);
            m_Settings.business.vehicleCargoInstantGarage = business.value("vehicle_cargo_instant_garage", m_Settings.business.vehicleCargoInstantGarage);
            m_Settings.business.vehicleCargoInstantSell = business.value("vehicle_cargo_instant_sell", m_Settings.business.vehicleCargoInstantSell);

            m_Settings.recovery.rpMultiplierEnabled = recovery.value("rp_multiplier_enabled", m_Settings.recovery.rpMultiplierEnabled);
            m_Settings.recovery.rpMultiplier = recovery.value("rp_multiplier", m_Settings.recovery.rpMultiplier);
            m_Settings.recovery.casinoSlotRig = recovery.value("casino_slot_rig", m_Settings.recovery.casinoSlotRig);

            m_Settings.world.pedDensity = world.value("ped_density", m_Settings.world.pedDensity);
            m_Settings.world.scenarioPedDensity = world.value("scenario_ped_density", m_Settings.world.scenarioPedDensity);
            m_Settings.world.vehicleDensity = world.value("vehicle_density", m_Settings.world.vehicleDensity);
            m_Settings.world.randomVehicleDensity = world.value("random_vehicle_density", m_Settings.world.randomVehicleDensity);
            m_Settings.world.parkedVehicleDensity = world.value("parked_vehicle_density", m_Settings.world.parkedVehicleDensity);
            m_Settings.world.freezeClock = world.value("freeze_clock", m_Settings.world.freezeClock);
            m_Settings.world.forceWeather = world.value("force_weather", m_Settings.world.forceWeather);
            m_Settings.world.blackout = world.value("blackout", m_Settings.world.blackout);
            m_Settings.world.autoWaypoint = world.value("auto_waypoint", m_Settings.world.autoWaypoint);
            m_Settings.world.entityInspectorLive = world.value("entity_inspector_live", m_Settings.world.entityInspectorLive);
            m_Settings.world.setHour = world.value("set_hour", m_Settings.world.setHour);
            m_Settings.world.setMinute = world.value("set_minute", m_Settings.world.setMinute);
            m_Settings.world.weatherIndex = world.value("weather_index", m_Settings.world.weatherIndex);
            m_Settings.world.clearRadius = world.value("clear_radius", m_Settings.world.clearRadius);

            m_Settings.misc.showCoordinates = misc.value("show_coordinates", m_Settings.misc.showCoordinates);
            m_Settings.misc.showHeading = misc.value("show_heading", m_Settings.misc.showHeading);
            m_Settings.misc.showFps = misc.value("show_fps", m_Settings.misc.showFps);
            m_Settings.misc.showSessionInfo = misc.value("show_session_info", m_Settings.misc.showSessionInfo);
            m_Settings.misc.disableCameraShake = misc.value("disable_camera_shake", m_Settings.misc.disableCameraShake);

            m_Settings.ui.activeTheme = ui.value("active_theme", m_Settings.ui.activeTheme);
            m_Settings.ui.resizable = ui.value("resizable", m_Settings.ui.resizable);
            m_Settings.ui.anchorTopLeft = ui.value("anchor_top_left", m_Settings.ui.anchorTopLeft);
            m_Settings.ui.menuWidth = ui.value("menu_width", m_Settings.ui.menuWidth);
            m_Settings.ui.menuHeight = ui.value("menu_height", m_Settings.ui.menuHeight);

            if (m_Settings.ui.menuWidth < 1460.0f)
                m_Settings.ui.menuWidth = 1460.0f;
            if (m_Settings.ui.menuHeight < 820.0f)
                m_Settings.ui.menuHeight = 820.0f;

            m_Loaded = true;
            return true;
        }
        catch (...)
        {
            Reset();
            return false;
        }
    }

    bool MenuSettingsService::Save(const std::filesystem::path& path) const noexcept
    {
        try
        {
            std::error_code ec;
            if (!path.parent_path().empty())
                std::filesystem::create_directories(path.parent_path(), ec);
            if (ec)
                return false;

            nlohmann::json document{
                {"version", m_Settings.version},
                {"off_radar", m_Settings.offRadar},
                {"player", {
                    {"invincible", m_Settings.player.invincible},
                    {"bulletproof", m_Settings.player.bulletproof},
                    {"aqua_lungs", m_Settings.player.aquaLungs},
                    {"infinite_oxygen", m_Settings.player.infiniteOxygen},
                    {"invisible", m_Settings.player.invisible},
                    {"no_ragdoll", m_Settings.player.noRagdoll},
                    {"super_jump", m_Settings.player.superJump},
                    {"infinite_stamina", m_Settings.player.infiniteStamina},
                    {"keep_player_clean", m_Settings.player.keepPlayerClean},
                    {"disable_critical_hits", m_Settings.player.disableCriticalHits},
                    {"stand_on_vehicles", m_Settings.player.standOnVehicles},
                    {"disable_action_mode", m_Settings.player.disableActionMode},
                    {"infinite_parachutes", m_Settings.player.infiniteParachutes},
                    {"mobile_radio", m_Settings.player.mobileRadio},
                    {"never_wanted", m_Settings.player.neverWanted},
                    {"police_ignore", m_Settings.player.policeIgnore},
                    {"everyone_ignore", m_Settings.player.everyoneIgnore},
                    {"ghost_organization", m_Settings.player.ghostOrganization},
                    {"run_multiplier", m_Settings.player.runMultiplier},
                    {"swim_multiplier", m_Settings.player.swimMultiplier},
                }},
                {"vehicle", {
                    {"remove_lsc_restrictions", m_Settings.vehicle.removeLscRestrictions},
                    {"enable_dlc_vehicles", m_Settings.vehicle.enableDlcVehicles},
                    {"god_mode", m_Settings.vehicle.vehicleGodMode},
                    {"keep_clean", m_Settings.vehicle.keepVehicleClean},
                    {"lowered_stance", m_Settings.vehicle.loweredStance},
                    {"horn_boost", m_Settings.vehicle.hornBoost},
                    {"infinite_vehicle_ammo", m_Settings.vehicle.infiniteVehicleAmmo},
                    {"nitrous_enabled", m_Settings.vehicle.nitrousEnabled},
                    {"nitrous_unlimited", m_Settings.vehicle.nitrousUnlimited},
                    {"nitrous_level", m_Settings.vehicle.nitrousLevel},
                    {"nitrous_power", m_Settings.vehicle.nitrousPower},
                    {"suspension_lowering_enabled", m_Settings.vehicle.suspensionLoweringEnabled},
                    {"suspension_lowering_amount", m_Settings.vehicle.suspensionLoweringAmount},
                }},
                {"weapons", {
                    {"infinite_ammo", m_Settings.weapons.infiniteAmmo},
                    {"infinite_clip", m_Settings.weapons.infiniteClip},
                    {"aimbot", m_Settings.weapons.aimbot},
                    {"aim_for_head", m_Settings.weapons.aimForHead},
                    {"target_drivers", m_Settings.weapons.targetDrivers},
                    {"release_dead_target", m_Settings.weapons.releaseDeadPed},
                    {"explosive_ammo", m_Settings.weapons.explosiveAmmo},
                    {"explosion_type", m_Settings.weapons.explosionType},
                    {"explosion_damage", m_Settings.weapons.explosionDamage},
                    {"explosion_camera_shake", m_Settings.weapons.explosionCameraShake},
                }},
                {"network", {
                    {"silence_phone_calls", m_Settings.network.silencePhoneCalls},
                    {"disable_death_barriers", m_Settings.network.disableDeathBarriers},
                    {"proximity_warnings", m_Settings.network.proximityWarningsEnabled},
                    {"restrict_watchlisted_actions", m_Settings.network.restrictWatchlistedActions},
                    {"auto_watch_high_risk", m_Settings.network.autoWatchHighRisk},
                    {"proximity_radius", m_Settings.network.proximityRadius},
                }},
                {"protections", {
                    {"block_malformed", m_Settings.protections.blockMalformed},
                    {"block_forced_leave", m_Settings.protections.blockForcedLeave},
                    {"block_known_crashes", m_Settings.protections.blockKnownCrashes},
                    {"block_sounds", m_Settings.protections.blockSounds},
                    {"block_explosions", m_Settings.protections.blockExplosions},
                    {"block_fire", m_Settings.protections.blockFire},
                    {"block_weapon_damage", m_Settings.protections.blockWeaponDamage},
                    {"block_ragdoll", m_Settings.protections.blockRagdoll},
                    {"block_clear_tasks", m_Settings.protections.blockClearTasks},
                    {"block_ptfx", m_Settings.protections.blockPtfx},
                    {"block_script_events", m_Settings.protections.blockScriptEvents},
                    {"block_malformed_script_events", m_Settings.protections.blockMalformedScriptEvents},
                }},
                {"session", {
                    {"no_idle", m_Settings.session.noIdle},
                }},
                {"business", {
                    {"vehicle_cargo_auto_source", m_Settings.business.vehicleCargoAutoSource},
                    {"vehicle_cargo_instant_garage", m_Settings.business.vehicleCargoInstantGarage},
                    {"vehicle_cargo_instant_sell", m_Settings.business.vehicleCargoInstantSell},
                }},
                {"recovery", {
                    {"rp_multiplier_enabled", m_Settings.recovery.rpMultiplierEnabled},
                    {"rp_multiplier", m_Settings.recovery.rpMultiplier},
                    {"casino_slot_rig", m_Settings.recovery.casinoSlotRig},
                }},
                {"world", {
                    {"ped_density", m_Settings.world.pedDensity},
                    {"scenario_ped_density", m_Settings.world.scenarioPedDensity},
                    {"vehicle_density", m_Settings.world.vehicleDensity},
                    {"random_vehicle_density", m_Settings.world.randomVehicleDensity},
                    {"parked_vehicle_density", m_Settings.world.parkedVehicleDensity},
                    {"freeze_clock", m_Settings.world.freezeClock},
                    {"force_weather", m_Settings.world.forceWeather},
                    {"blackout", m_Settings.world.blackout},
                    {"auto_waypoint", m_Settings.world.autoWaypoint},
                    {"entity_inspector_live", m_Settings.world.entityInspectorLive},
                    {"set_hour", m_Settings.world.setHour},
                    {"set_minute", m_Settings.world.setMinute},
                    {"weather_index", m_Settings.world.weatherIndex},
                    {"clear_radius", m_Settings.world.clearRadius},
                }},
                {"misc", {
                    {"show_coordinates", m_Settings.misc.showCoordinates},
                    {"show_heading", m_Settings.misc.showHeading},
                    {"show_fps", m_Settings.misc.showFps},
                    {"show_session_info", m_Settings.misc.showSessionInfo},
                    {"disable_camera_shake", m_Settings.misc.disableCameraShake},
                }},
                {"ui", {
                    {"active_theme", m_Settings.ui.activeTheme},
                    {"resizable", m_Settings.ui.resizable},
                    {"anchor_top_left", m_Settings.ui.anchorTopLeft},
                    {"menu_width", m_Settings.ui.menuWidth},
                    {"menu_height", m_Settings.ui.menuHeight},
                }},
            };

            std::ofstream stream(path, std::ios::trunc);
            if (!stream)
                return false;

            stream << document.dump(2) << '\n';
            return stream.good();
        }
        catch (...)
        {
            return false;
        }
    }

    void MenuSettingsService::Reset() noexcept
    {
        m_Settings = MenuSettingsData{};
        m_Loaded = false;
    }

    const MenuSettingsData& MenuSettingsService::Current() const noexcept { return m_Settings; }
    MenuSettingsData& MenuSettingsService::Current() noexcept { return m_Settings; }
    bool MenuSettingsService::IsLoaded() const noexcept { return m_Loaded; }
}
