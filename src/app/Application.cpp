#include "Application.hpp"

#include "../core/CoreServices.hpp"
#include "../core/config/MenuSettings.hpp"
#include "../core/filesystem/FileSystem.hpp"
#include "../core/logging/Logger.hpp"
#include "../features/player/OffRadarRuntime.hpp"
#include "../features/player/PlayerRuntime.hpp"
#include "../features/vehicle/LscBypassRuntime.hpp"
#include "../features/vehicle/PersonalVehicleRuntime.hpp"
#include "../features/vehicle/VehicleModificationRuntime.hpp"
#include "../features/vehicle/VehiclePaintRuntime.hpp"
#include "../features/weapon/WeaponRuntime.hpp"
#include "../hooking/HookManager.hpp"
#include "../render/Renderer.hpp"
#include "../runtime/GameRuntime.hpp"
#include "../ui/Input.hpp"

namespace Tutones::App
{
    namespace
    {
        void StagePersistedMenuSettings() noexcept
        {
            const auto& settings = Core::Config::MenuSettingsService::Get().Current();

            auto& player = Game::PlayerFeatures::PlayerRuntime::Get();
            player.SetInvincible(settings.player.invincible);
            player.SetInvisible(settings.player.invisible);
            player.SetNoRagdoll(settings.player.noRagdoll);
            player.SetSuperJump(settings.player.superJump);
            player.SetInfiniteStamina(settings.player.infiniteStamina);
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

            TUTONES_LOG_INFO("config", "Staged persisted V11 state before GTA runtime startup; no one-shot commands were executed");
        }

        void SavePersistedMenuSettings() noexcept
        {
            auto& service = Core::Config::MenuSettingsService::Get();
            auto& settings = service.Current();

            const auto player = Game::PlayerFeatures::PlayerRuntime::Get().Snapshot();
            settings.player.invincible = player.invincible;
            settings.player.invisible = player.invisible;
            settings.player.noRagdoll = player.noRagdoll;
            settings.player.superJump = player.superJump;
            settings.player.infiniteStamina = player.infiniteStamina;
            settings.player.neverWanted = player.neverWanted;
            settings.player.policeIgnore = player.policeIgnore;
            settings.player.everyoneIgnore = player.everyoneIgnore;
            settings.player.runMultiplier = player.runMultiplier;
            settings.player.swimMultiplier = player.swimMultiplier;

            settings.offRadar = Game::PlayerFeatures::OffRadarRuntime::Get().Snapshot().enabled;
            settings.vehicle.removeLscRestrictions = Game::Mods::LscBypassRuntime::Get().Enabled();

            const auto weapons = Game::WeaponFeatures::WeaponRuntime::Get().Snapshot().settings;
            settings.weapons.infiniteAmmo = weapons.infiniteAmmo;
            settings.weapons.infiniteClip = weapons.infiniteClip;
            settings.weapons.aimbot = weapons.aimbot;
            settings.weapons.aimForHead = weapons.aimForHead;
            settings.weapons.targetDrivers = weapons.targetDrivers;
            settings.weapons.releaseDeadPed = weapons.releaseDeadPed;
            settings.weapons.explosiveAmmo = weapons.explosiveAmmo;
            settings.weapons.explosionType = weapons.explosionType;
            settings.weapons.explosionDamage = weapons.explosionDamage;
            settings.weapons.explosionCameraShake = weapons.explosionCameraShake;

            const auto path = Core::FileSystem::Service::Get().UserRoot() / "menu_settings.json";
            if (service.Save(path))
                TUTONES_LOG_INFO("config", "Saved V11 stateful settings to menu_settings.json");
            else
                TUTONES_LOG_WARN("config", "Failed to save menu_settings.json");
        }
    }

    Application& Application::Get() noexcept
    {
        static Application instance;
        return instance;
    }

    bool Application::Initialize(const std::filesystem::path& moduleDirectory)
    {
        if (m_Running)
        {
            TUTONES_LOG_TRACE("app", "Application initialize requested while already running");
            return true;
        }

        if (moduleDirectory.empty())
            return false;

        if (!Core::Services::Get().Initialize(moduleDirectory))
            return false;

        // Load persistent feature state while GameRuntime is still inactive. Player setters may
        // attempt an immediate apply, but GameRuntime::Enqueue rejects those operations here,
        // guaranteeing config load cannot execute one-shot GTA actions.
        StagePersistedMenuSettings();

        TUTONES_LOG_INFO("app", "Core services ready; starting renderer bootstrap");
        if (!Render::Renderer::Get().Initialize())
        {
            TUTONES_LOG_ERROR("app", "Renderer bootstrap failed");
            Core::Services::Get().Shutdown();
            return false;
        }

        TUTONES_LOG_INFO("app", "Renderer bootstrap ready; initializing hook backend");
        if (!Hooking::HookManager::Get().Initialize())
        {
            TUTONES_LOG_ERROR("app", "Hook backend initialization failed");
            Hooking::HookManager::Get().Shutdown();
            Render::Renderer::Get().Shutdown();
            Core::Services::Get().Shutdown();
            return false;
        }

        TUTONES_LOG_INFO("app", "Hook backend ready; installing D3D12/DXGI hooks");
        if (!Hooking::HookManager::Get().Install())
        {
            TUTONES_LOG_ERROR("app", "D3D12/DXGI hook installation failed");
            Hooking::HookManager::Get().Shutdown();
            Render::Renderer::Get().Shutdown();
            Core::Services::Get().Shutdown();
            return false;
        }

        TUTONES_LOG_INFO("app", "Render hooks ready; initializing GTA Enhanced game runtime");
        if (!Runtime::GameRuntime::Get().Initialize())
        {
            TUTONES_LOG_ERROR("app", "GTA Enhanced game runtime initialization failed");
            Runtime::GameRuntime::Get().Shutdown();
            Hooking::HookManager::Get().Shutdown();
            Render::Renderer::Get().Shutdown();
            Core::Services::Get().Shutdown();
            return false;
        }

        TUTONES_LOG_INFO("app", "Game runtime ready; starting LSC script-patch runtime");
        if (!Game::Mods::LscBypassRuntime::Get().Start())
        {
            TUTONES_LOG_ERROR("app", "LSC restriction bypass runtime failed to start");
            Game::Mods::LscBypassRuntime::Get().Stop();
            Runtime::GameRuntime::Get().Shutdown();
            Hooking::HookManager::Get().Shutdown();
            Render::Renderer::Get().Shutdown();
            Core::Services::Get().Shutdown();
            return false;
        }

        TUTONES_LOG_INFO("app", "Script-patch runtime ready; starting vehicle feature runtimes");
        if (!Game::Paint::VehiclePaintRuntime::Get().Start())
        {
            TUTONES_LOG_ERROR("app", "Vehicle paint runtime failed to queue its first GTA script-thread tick");
            Game::Paint::VehiclePaintRuntime::Get().Stop();
            Game::Mods::LscBypassRuntime::Get().Stop();
            Runtime::GameRuntime::Get().Shutdown();
            Hooking::HookManager::Get().Shutdown();
            Render::Renderer::Get().Shutdown();
            Core::Services::Get().Shutdown();
            return false;
        }

        if (!Game::Mods::VehicleModificationRuntime::Get().Start())
        {
            TUTONES_LOG_ERROR("app", "Vehicle modification runtime failed to queue its first GTA script-thread tick");
            Game::Mods::VehicleModificationRuntime::Get().Stop();
            Game::Paint::VehiclePaintRuntime::Get().Stop();
            Game::Mods::LscBypassRuntime::Get().Stop();
            Runtime::GameRuntime::Get().Shutdown();
            Hooking::HookManager::Get().Shutdown();
            Render::Renderer::Get().Shutdown();
            Core::Services::Get().Shutdown();
            return false;
        }

        if (!Game::PersonalVehicles::PersonalVehicleRuntime::Get().Start())
        {
            TUTONES_LOG_ERROR("app", "Personal vehicle reader failed to queue its first GTA script-thread tick");
            Game::PersonalVehicles::PersonalVehicleRuntime::Get().Stop();
            Game::Mods::VehicleModificationRuntime::Get().Stop();
            Game::Paint::VehiclePaintRuntime::Get().Stop();
            Game::Mods::LscBypassRuntime::Get().Stop();
            Runtime::GameRuntime::Get().Shutdown();
            Hooking::HookManager::Get().Shutdown();
            Render::Renderer::Get().Shutdown();
            Core::Services::Get().Shutdown();
            return false;
        }

        TUTONES_LOG_INFO("app", "Vehicle runtimes ready; starting player runtime");
        if (!Game::PlayerFeatures::PlayerRuntime::Get().Start())
        {
            TUTONES_LOG_ERROR("app", "Player runtime failed to queue its first GTA script-thread tick");
            Game::PlayerFeatures::PlayerRuntime::Get().Stop();
            Game::PersonalVehicles::PersonalVehicleRuntime::Get().Stop();
            Game::Mods::VehicleModificationRuntime::Get().Stop();
            Game::Paint::VehiclePaintRuntime::Get().Stop();
            Game::Mods::LscBypassRuntime::Get().Stop();
            Runtime::GameRuntime::Get().Shutdown();
            Hooking::HookManager::Get().Shutdown();
            Render::Renderer::Get().Shutdown();
            Core::Services::Get().Shutdown();
            return false;
        }

        TUTONES_LOG_INFO("app", "Player runtime ready; starting Off Radar runtime");
        if (!Game::PlayerFeatures::OffRadarRuntime::Get().Start())
        {
            TUTONES_LOG_ERROR("app", "Off Radar runtime failed to queue its first GTA script-thread tick");
            Game::PlayerFeatures::OffRadarRuntime::Get().Stop();
            Game::PlayerFeatures::PlayerRuntime::Get().Stop();
            Game::PersonalVehicles::PersonalVehicleRuntime::Get().Stop();
            Game::Mods::VehicleModificationRuntime::Get().Stop();
            Game::Paint::VehiclePaintRuntime::Get().Stop();
            Game::Mods::LscBypassRuntime::Get().Stop();
            Runtime::GameRuntime::Get().Shutdown();
            Hooking::HookManager::Get().Shutdown();
            Render::Renderer::Get().Shutdown();
            Core::Services::Get().Shutdown();
            return false;
        }

        TUTONES_LOG_INFO("app", "Player online runtime ready; starting weapon runtime");
        if (!Game::WeaponFeatures::WeaponRuntime::Get().Start())
        {
            TUTONES_LOG_ERROR("app", "Weapon runtime failed to queue its first GTA script-thread tick");
            Game::WeaponFeatures::WeaponRuntime::Get().Stop();
            Game::PlayerFeatures::OffRadarRuntime::Get().Stop();
            Game::PlayerFeatures::PlayerRuntime::Get().Stop();
            Game::PersonalVehicles::PersonalVehicleRuntime::Get().Stop();
            Game::Mods::VehicleModificationRuntime::Get().Stop();
            Game::Paint::VehiclePaintRuntime::Get().Stop();
            Game::Mods::LscBypassRuntime::Get().Stop();
            Runtime::GameRuntime::Get().Shutdown();
            Hooking::HookManager::Get().Shutdown();
            Render::Renderer::Get().Shutdown();
            Core::Services::Get().Shutdown();
            return false;
        }

        TUTONES_LOG_INFO("app", "Weapon runtime ready; initializing menu input routing");
        if (!UI::Input::Get().Initialize())
        {
            TUTONES_LOG_ERROR("app", "Menu input initialization failed");
            UI::Input::Get().Shutdown();
            Game::WeaponFeatures::WeaponRuntime::Get().Stop();
            Game::PlayerFeatures::OffRadarRuntime::Get().Stop();
            Game::PlayerFeatures::PlayerRuntime::Get().Stop();
            Game::PersonalVehicles::PersonalVehicleRuntime::Get().Stop();
            Game::Mods::VehicleModificationRuntime::Get().Stop();
            Game::Paint::VehiclePaintRuntime::Get().Stop();
            Game::Mods::LscBypassRuntime::Get().Stop();
            Runtime::GameRuntime::Get().Shutdown();
            Hooking::HookManager::Get().Shutdown();
            Render::Renderer::Get().Shutdown();
            Core::Services::Get().Shutdown();
            return false;
        }

        TUTONES_LOG_INFO("app", "Tutones Menu application initialized with render, input, GTA native runtime, real LSC script-patch bypass, vehicle, personal vehicle, player, online, and weapon feature layers");
        TUTONES_LOG_DEBUG("app", "Runtime is waiting for primary render state and the first GTA script-thread tick");
        m_Running = true;
        return true;
    }

    void Application::Shutdown() noexcept
    {
        if (!m_Running)
        {
            TUTONES_LOG_TRACE("app", "Application shutdown requested while not running");
            return;
        }

        TUTONES_LOG_INFO("app", "Tutones Menu application shutting down");

        // Capture only stateful settings while every feature snapshot is still live.
        // One-shot actions are not represented by MenuSettingsData and therefore cannot persist.
        SavePersistedMenuSettings();

        TUTONES_LOG_DEBUG("app", "Stopping Win32 menu input routing");
        UI::Input::Get().Shutdown();

        TUTONES_LOG_DEBUG("app", "Stopping weapon, online, player, personal vehicle, LSC patch, and vehicle feature scheduling before GTA runtime teardown");
        Game::WeaponFeatures::WeaponRuntime::Get().Stop();
        Game::PlayerFeatures::OffRadarRuntime::Get().Stop();
        Game::PlayerFeatures::PlayerRuntime::Get().Stop();
        Game::PersonalVehicles::PersonalVehicleRuntime::Get().Stop();
        Game::Mods::VehicleModificationRuntime::Get().Stop();
        Game::Paint::VehiclePaintRuntime::Get().Stop();
        Game::Mods::LscBypassRuntime::Get().Stop();

        TUTONES_LOG_DEBUG("app", "Stopping GTA script/native runtime before MinHook teardown");
        Runtime::GameRuntime::Get().Shutdown();

        TUTONES_LOG_DEBUG("app", "Stopping render hook callbacks before renderer teardown");
        Hooking::HookManager::Get().Shutdown();

        TUTONES_LOG_DEBUG("app", "Hook layer stopped; shutting down renderer");
        Render::Renderer::Get().Shutdown();

        TUTONES_LOG_DEBUG("app", "Renderer stopped; shutting down core services");
        m_Running = false;
        TUTONES_LOG_INFO("app", "Application runtime stopped; shutting down core services");
        Core::Services::Get().Shutdown();
    }

    bool Application::IsRunning() const noexcept
    {
        return m_Running;
    }
}
