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
            const auto ui = document.value("ui", nlohmann::json::object());

            m_Settings.version = 2; // migrate older files to the current schema on the next save
            m_Settings.offRadar = document.value("off_radar", m_Settings.offRadar);

            m_Settings.player.invincible = player.value("invincible", m_Settings.player.invincible);
            m_Settings.player.invisible = player.value("invisible", m_Settings.player.invisible);
            m_Settings.player.noRagdoll = player.value("no_ragdoll", m_Settings.player.noRagdoll);
            m_Settings.player.superJump = player.value("super_jump", m_Settings.player.superJump);
            m_Settings.player.infiniteStamina = player.value("infinite_stamina", m_Settings.player.infiniteStamina);
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
                    {"invisible", m_Settings.player.invisible},
                    {"no_ragdoll", m_Settings.player.noRagdoll},
                    {"super_jump", m_Settings.player.superJump},
                    {"infinite_stamina", m_Settings.player.infiniteStamina},
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
