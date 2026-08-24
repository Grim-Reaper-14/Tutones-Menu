#include "Application.hpp"
#include "BuildInfo.hpp"

#include "../core/CoreServices.hpp"
#include "../core/config/MenuSettings.hpp"
#include "../core/filesystem/FileSystem.hpp"
#include "../core/logging/Logger.hpp"
#include "../features/game/GameSessionRuntime.hpp"
#include "../features/network/NetworkRuntime.hpp"
#include "../features/player/OffRadarRuntime.hpp"
#include "../features/player/PlayerRuntime.hpp"
#include "../features/recovery/RecoveryRuntime.hpp"
#include "../features/vehicle/LscBypassRuntime.hpp"
#include "../features/vehicle/PersonalVehicleRuntime.hpp"
#include "../features/vehicle/VehicleLoopFeatures.hpp"
#include "../features/vehicle/VehicleModificationRuntime.hpp"
#include "../features/vehicle/VehiclePaintRuntime.hpp"
#include "../features/weapon/WeaponRuntime.hpp"
#include "../features/world/TeleportRuntime.hpp"
#include "../features/world/WorldRuntime.hpp"
#include "../game/MiscNatives.hpp"
#include "../hooking/HookManager.hpp"
#include "../render/Renderer.hpp"
#include "../runtime/GameRuntime.hpp"
#include "../runtime/RuntimeStartup.hpp"
#include "../ui/Input.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <thread>

namespace Tutones::App
{
    namespace
    {
        void StagePersistedMenuSettings() noexcept
        {
            const auto& settings = Core::Config::MenuSettingsService::Get().Current();

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

            TUTONES_LOG_INFO("config", "Staged persisted V11 state before GTA runtime startup; no one-shot commands were executed");
        }

        void SavePersistedMenuSettings() noexcept
        {
            auto& service = Core::Config::MenuSettingsService::Get();
            auto& settings = service.Current();

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

            const auto path = Core::FileSystem::Service::Get().UserRoot() / "menu_settings.json";
            if (service.Save(path))
                TUTONES_LOG_INFO("config", "Saved V11 stateful settings to menu_settings.json");
            else
                TUTONES_LOG_WARN("config", "Failed to save menu_settings.json");
        }

        void ReleaseWorldStateBeforeRuntimeShutdown() noexcept
        {
            auto& world = Game::World::WorldRuntime::Get();
            static_cast<void>(world.QueueReleasePersistentOverrides());

            auto& runtime = Runtime::GameRuntime::Get();
            if (!runtime.IsInitialized())
                return;

            const auto cleanup = [] {
                bool success = true;
                success = Game::MiscNatives::NetworkClearClockTimeOverride() && success;
                success = Game::MiscNatives::ClearOverrideWeather() && success;
                success = Game::MiscNatives::SetArtificialLightsState(false) && success;
                if (!success)
                    TUTONES_LOG_WARN("world.runtime", "One or more world overrides could not be cleared during shutdown");
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

        {
            std::string buildMessage("Tutones build provenance: revision=");
            buildMessage += BuildInfo::Revision;
            buildMessage += ", build=";
            buildMessage += BuildInfo::BuildNumber;
            TUTONES_LOG_INFO("app", buildMessage);
        }

        StagePersistedMenuSettings();

        // Renderer, hooks, GTA scheduler/native runtime, and menu input are the only
        // application-critical services. Feature runtimes below are deliberately
        // fail-soft so newly-added menu features cannot unload unrelated working code.
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

        TUTONES_LOG_INFO("app", "Game runtime ready; initializing menu input routing");
        if (!UI::Input::Get().Initialize())
        {
            TUTONES_LOG_ERROR("app", "Menu input initialization failed");
            UI::Input::Get().Shutdown();
            Runtime::GameRuntime::Get().Shutdown();
            Hooking::HookManager::Get().Shutdown();
            Render::Renderer::Get().Shutdown();
            Core::Services::Get().Shutdown();
            return false;
        }

        std::size_t featureRuntimeCount{};
        std::size_t featureRuntimeStarted{};
        const auto startFeature = [&featureRuntimeCount, &featureRuntimeStarted](
            const char* name,
            auto&& start,
            auto&& stop) {
            ++featureRuntimeCount;
            if (Runtime::StartOptionalRuntime(
                    name,
                    std::forward<decltype(start)>(start),
                    std::forward<decltype(stop)>(stop)))
            {
                ++featureRuntimeStarted;
            }
        };

        startFeature(
            "LSC restriction bypass",
            [] { return Game::Mods::LscBypassRuntime::Get().Start(); },
            [] { Game::Mods::LscBypassRuntime::Get().Stop(); });
        startFeature(
            "Vehicle paint",
            [] { return Game::Paint::VehiclePaintRuntime::Get().Start(); },
            [] { Game::Paint::VehiclePaintRuntime::Get().Stop(); });
        startFeature(
            "Vehicle modifications",
            [] { return Game::Mods::VehicleModificationRuntime::Get().Start(); },
            [] { Game::Mods::VehicleModificationRuntime::Get().Stop(); });
        startFeature(
            "Personal vehicle reader",
            [] { return Game::PersonalVehicles::PersonalVehicleRuntime::Get().Start(); },
            [] { Game::PersonalVehicles::PersonalVehicleRuntime::Get().Stop(); });
        startFeature(
            "Player runtime",
            [] { return Game::PlayerFeatures::PlayerRuntime::Get().Start(); },
            [] { Game::PlayerFeatures::PlayerRuntime::Get().Stop(); });
        startFeature(
            "Off Radar",
            [] { return Game::PlayerFeatures::OffRadarRuntime::Get().Start(); },
            [] { Game::PlayerFeatures::OffRadarRuntime::Get().Stop(); });
        startFeature(
            "Weapon runtime",
            [] { return Game::WeaponFeatures::WeaponRuntime::Get().Start(); },
            [] { Game::WeaponFeatures::WeaponRuntime::Get().Stop(); });
        startFeature(
            "Recovery runtime",
            [] { return Game::Recovery::RecoveryRuntime::Get().Start(); },
            [] { Game::Recovery::RecoveryRuntime::Get().Stop(); });
        startFeature(
            "Network/QoL runtime",
            [] { return Game::NetworkFeatures::NetworkRuntime::Get().Start(); },
            [] { Game::NetworkFeatures::NetworkRuntime::Get().Stop(); });

        TUTONES_LOG_INFO(
            "runtime.features",
            std::string("Optional feature runtimes online: ")
                + std::to_string(featureRuntimeStarted)
                + "/" + std::to_string(featureRuntimeCount));

        TUTONES_LOG_INFO(
            "app",
            "Tutones Menu core initialized; unavailable feature runtimes no longer tear down the menu");
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

        SavePersistedMenuSettings();

        TUTONES_LOG_DEBUG("app", "Restoring session utility state before runtime teardown");
        Game::SessionFeatures::GameSessionRuntime::Get().Shutdown();

        TUTONES_LOG_DEBUG("app", "Stopping vehicle loop features and restoring the last affected vehicle");
        Game::Mods::VehicleLoopFeatures::Get().Shutdown();

        TUTONES_LOG_DEBUG("app", "Stopping optional feature runtimes while GTA scheduling is active");
        Game::NetworkFeatures::NetworkRuntime::Get().Stop();
        Game::Recovery::RecoveryRuntime::Get().Stop();
        Game::WeaponFeatures::WeaponRuntime::Get().Stop();
        Game::PlayerFeatures::OffRadarRuntime::Get().Stop();
        Game::PlayerFeatures::PlayerRuntime::Get().Stop();
        Game::PersonalVehicles::PersonalVehicleRuntime::Get().Stop();
        Game::Mods::VehicleModificationRuntime::Get().Stop();
        Game::Paint::VehiclePaintRuntime::Get().Stop();
        Game::Mods::LscBypassRuntime::Get().Stop();

        TUTONES_LOG_DEBUG("app", "Stopping Win32 menu input routing");
        UI::Input::Get().Shutdown();

        ReleaseWorldStateBeforeRuntimeShutdown();

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
