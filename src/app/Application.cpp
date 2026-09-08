#include "Application.hpp"
#include "BuildInfo.hpp"

#include "../backend/BackendHub.hpp"
#include "../core/CoreServices.hpp"
#include "../core/config/MenuSettings.hpp"
#include "../core/filesystem/FileSystem.hpp"
#include "../core/logging/Logger.hpp"
#include "../features/game/GameSessionRuntime.hpp"
#include "../features/network/NetworkRuntime.hpp"
#include "../features/network/ProtectionRuntime.hpp"
#include "../features/player/OffRadarRuntime.hpp"
#include "../features/player/PlayerRuntime.hpp"
#include "../features/recovery/RecoveryRuntime.hpp"
#include "../features/vehicle/LscBypassRuntime.hpp"
#include "../features/vehicle/VehicleLoopFeatures.hpp"
#include "../features/weapon/WeaponRuntime.hpp"
#include "../features/world/TeleportRuntime.hpp"
#include "../features/world/WorldRuntime.hpp"
#include "../game/MiscNatives.hpp"
#include "../hooking/HookManager.hpp"
#include "../render/Renderer.hpp"
#include "../runtime/GameRuntime.hpp"
#include "../ui/Input.hpp"
#include "../ui/PersistentMenuState.hpp"

#include <atomic>
#include <chrono>
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

            TUTONES_LOG_INFO(
                "config",
                "Staged early persisted settings before GTA runtime startup; extended feature settings apply after runtime initialization");
        }

        void SavePersistedMenuSettings() noexcept
        {
            // Use the same complete live-state capture used by the Settings page so
            // shutdown persistence cannot silently omit newer feature runtimes.
            UI::PersistentMenuStateDetail::CaptureLiveSettings();

            auto& service = Core::Config::MenuSettingsService::Get();
            const auto path = Core::FileSystem::Service::Get().UserRoot()
                / "settings" / "menu_settings.json";
            if (service.Save(path))
                TUTONES_LOG_INFO("config", "Saved all stateful menu settings to settings\\menu_settings.json");
            else
                TUTONES_LOG_WARN("config", "Failed to save settings\\menu_settings.json");
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
                    TUTONES_LOG_WARN(
                        "world.runtime",
                        "One or more world overrides could not be cleared during shutdown");
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

        TUTONES_LOG_INFO("app", "Render hooks ready; initializing menu input routing");
        if (!UI::Input::Get().Initialize())
        {
            TUTONES_LOG_ERROR("app", "Menu input initialization failed");
            UI::Input::Get().Shutdown();
            Hooking::HookManager::Get().Shutdown();
            Render::Renderer::Get().Shutdown();
            Core::Services::Get().Shutdown();
            return false;
        }

        TUTONES_LOG_INFO("app", "Menu shell ready; initializing GTA Enhanced game runtime");
        const bool gameRuntimeReady = Runtime::GameRuntime::Get().Initialize();
        if (!gameRuntimeReady)
        {
            TUTONES_LOG_ERROR(
                "app",
                "GTA Enhanced game runtime initialization failed; Tutones V2 UI and F4 input will remain active for diagnostics");
        }

        if (gameRuntimeReady)
        {
            Game::Protections::ProtectionRuntime::Get().PrepareForStart();
            TUTONES_LOG_INFO("app", "Game runtime ready; starting centralized BackendHub");
            if (!Backend::BackendHub::Get().Initialize())
            {
                TUTONES_LOG_ERROR(
                    "app",
                    "BackendHub initialization failed; menu core will remain available for diagnostics");
            }
        }
        else
        {
            TUTONES_LOG_WARN(
                "app",
                "Skipping BackendHub startup because the GTA game runtime is unavailable; UI-only diagnostics remain active");
        }

        const auto backend = Backend::BackendHub::Get().Snapshot();
        std::string readyMessage("Tutones Menu initialized; game runtime=");
        readyMessage += gameRuntimeReady ? "READY" : "UNAVAILABLE";
        readyMessage += ", BackendHub features registered=";
        readyMessage += std::to_string(backend.features.size());
        TUTONES_LOG_INFO("app", readyMessage);
        TUTONES_LOG_DEBUG(
            "app",
            "Runtime is waiting for primary render state and the first GTA script-thread tick");
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

        TUTONES_LOG_DEBUG("app", "Stopping centralized BackendHub while GTA scheduling is active");
        Backend::BackendHub::Get().Shutdown();

        TUTONES_LOG_DEBUG("app", "Stopping Win32 menu input routing");
        UI::Input::Get().Shutdown();

        ReleaseWorldStateBeforeRuntimeShutdown();

        TUTONES_LOG_DEBUG("app", "Stopping network protection hook before global MinHook teardown");
        Game::Protections::ProtectionRuntime::Get().Stop();

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
