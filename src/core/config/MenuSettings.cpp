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
            const auto recovery = document.value("recovery", nlohmann::json::object());
            const auto world = document.value("world", nlohmann::json::object());
            const auto misc = document.value("misc", nlohmann::json::object());
            const auto ui = document.value("ui", nlohmann::json::object());

            // Older files remain valid. The next save upgrades them to schema v3.
            m_Settings.version = 3;
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
            m_Settings.player.runMultiplier = player.value("run_multiplier", m_Settings.player.runMultiplier);
            m_Settings.player.swimMultiplier = player.value("swim_multiplier", m_Settings.player.swimMultiplier);

            m_Settings.vehicle.removeLscRestrictions = vehicle.value("remove_lsc_restrictions", m_Settings.vehicle.removeLscRestrictions);

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

            m_Settings.recovery.rpMultiplierEnabled = recovery.value("rp_multiplier_enabled", m_Settings.recovery.rpMultiplierEnabled);
            m_Settings.recovery.rpMultiplier = recovery.value("rp_multiplier", m_Settings.recovery.rpMultiplier);

            m_Settings.world.pedDensity = world.value("ped_density", m_Settings.world.pedDensity);
            m_Settings.world.scenarioPedDensity = world.value("scenario_ped_density", m_Settings.world.scenarioPedDensity);
            m_Settings.world.vehicleDensity = world.value("vehicle_density", m_Settings.world.vehicleDensity);
            m_Settings.world.randomVehicleDensity = world.value("random_vehicle_density", m_Settings.world.randomVehicleDensity);
            m_Settings.world.parkedVehicleDensity = world.value("parked_vehicle_density", m_Settings.world.parkedVehicleDensity);
            m_Settings.world.freezeClock = world.value("freeze_clock", m_Settings.world.freezeClock);
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
                    {"run_multiplier", m_Settings.player.runMultiplier},
                    {"swim_multiplier", m_Settings.player.swimMultiplier},
                }},
                {"vehicle", {
                    {"remove_lsc_restrictions", m_Settings.vehicle.removeLscRestrictions},
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
                }},
                {"recovery", {
                    {"rp_multiplier_enabled", m_Settings.recovery.rpMultiplierEnabled},
                    {"rp_multiplier", m_Settings.recovery.rpMultiplier},
                }},
                {"world", {
                    {"ped_density", m_Settings.world.pedDensity},
                    {"scenario_ped_density", m_Settings.world.scenarioPedDensity},
                    {"vehicle_density", m_Settings.world.vehicleDensity},
                    {"random_vehicle_density", m_Settings.world.randomVehicleDensity},
                    {"parked_vehicle_density", m_Settings.world.parkedVehicleDensity},
                    {"freeze_clock", m_Settings.world.freezeClock},
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
