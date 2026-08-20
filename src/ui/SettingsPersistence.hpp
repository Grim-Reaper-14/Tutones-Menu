#pragma once

#include "ThemeManager.hpp"
#include "../core/config/MenuSettings.hpp"
#include "../core/filesystem/FileSystem.hpp"
#include "../features/player/OffRadarRuntime.hpp"
#include "../features/player/PlayerRuntime.hpp"
#include "../features/vehicle/LscBypassRuntime.hpp"
#include "../features/weapon/WeaponRuntime.hpp"

namespace Tutones::UI::SettingsPersistence
{
    inline std::filesystem::path Path()
    {
        return Core::FileSystem::Service::Get().RootPath(Core::FileSystem::Root::Config) / "menu_settings.json";
    }

    inline void Capture() noexcept
    {
        auto& s=Core::Config::MenuSettingsService::Get().Current();
        const auto p=Game::PlayerFeatures::PlayerRuntime::Get().Snapshot();
        s.player.invincible=p.invincible; s.player.invisible=p.invisible; s.player.noRagdoll=p.noRagdoll;
        s.player.superJump=p.superJump; s.player.infiniteStamina=p.infiniteStamina; s.player.neverWanted=p.neverWanted;
        s.player.policeIgnore=p.policeIgnore; s.player.everyoneIgnore=p.everyoneIgnore;
        s.player.runMultiplier=p.runMultiplier; s.player.swimMultiplier=p.swimMultiplier;
        s.offRadar=Game::PlayerFeatures::OffRadarRuntime::Get().Snapshot().enabled;
        s.vehicle.removeLscRestrictions=Game::Mods::LscBypassRuntime::Get().Enabled();
        const auto w=Game::WeaponFeatures::WeaponRuntime::Get().Snapshot().settings;
        s.weapons.infiniteAmmo=w.infiniteAmmo; s.weapons.infiniteClip=w.infiniteClip; s.weapons.aimbot=w.aimbot;
        s.weapons.aimForHead=w.aimForHead; s.weapons.targetDrivers=w.targetDrivers; s.weapons.releaseDeadPed=w.releaseDeadPed;
        s.weapons.explosiveAmmo=w.explosiveAmmo; s.weapons.explosionType=w.explosionType;
        s.weapons.explosionDamage=w.explosionDamage; s.weapons.explosionCameraShake=w.explosionCameraShake;
        if(!ThemeManager::Get().CurrentThemeFile().empty())s.ui.activeTheme=ThemeManager::Get().CurrentThemeFile();
    }

    inline void Apply() noexcept
    {
        const auto s=Core::Config::MenuSettingsService::Get().Current();
        auto& p=Game::PlayerFeatures::PlayerRuntime::Get();
        p.SetInvincible(s.player.invincible);p.SetInvisible(s.player.invisible);p.SetNoRagdoll(s.player.noRagdoll);
        p.SetSuperJump(s.player.superJump);p.SetInfiniteStamina(s.player.infiniteStamina);p.SetNeverWanted(s.player.neverWanted);
        p.SetPoliceIgnore(s.player.policeIgnore);p.SetEveryoneIgnore(s.player.everyoneIgnore);
        p.SetRunMultiplier(s.player.runMultiplier);p.SetSwimMultiplier(s.player.swimMultiplier);
        Game::PlayerFeatures::OffRadarRuntime::Get().SetEnabled(s.offRadar);
        Game::Mods::LscBypassRuntime::Get().SetEnabled(s.vehicle.removeLscRestrictions);
        auto& w=Game::WeaponFeatures::WeaponRuntime::Get();
        w.SetInfiniteAmmo(s.weapons.infiniteAmmo);w.SetInfiniteClip(s.weapons.infiniteClip);w.SetAimbot(s.weapons.aimbot);
        w.SetAimForHead(s.weapons.aimForHead);w.SetTargetDrivers(s.weapons.targetDrivers);w.SetReleaseDeadPed(s.weapons.releaseDeadPed);
        w.SetExplosiveAmmo(s.weapons.explosiveAmmo);w.SetExplosionType(s.weapons.explosionType);
        w.SetExplosionDamage(s.weapons.explosionDamage);w.SetExplosionCameraShake(s.weapons.explosionCameraShake);
        if(!s.ui.activeTheme.empty())static_cast<void>(ThemeManager::Get().LoadTheme(s.ui.activeTheme));
    }

    inline bool Save() noexcept { Capture(); return Core::Config::MenuSettingsService::Get().Save(Path()); }
    inline bool Load(bool refresh) noexcept
    {
        if(refresh)ThemeManager::Get().Refresh();
        if(!Core::Config::MenuSettingsService::Get().Load(Path()))return false;
        Apply();return true;
    }
}
