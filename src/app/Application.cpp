#include "Application.hpp"

#include "../core/CoreServices.hpp"
#include "../core/logging/Logger.hpp"
#include "../features/player/PlayerRuntime.hpp"
#include "../features/vehicle/VehicleModificationRuntime.hpp"
#include "../features/vehicle/VehiclePaintRuntime.hpp"
#include "../features/weapon/WeaponRuntime.hpp"
#include "../hooking/HookManager.hpp"
#include "../render/Renderer.hpp"
#include "../runtime/GameRuntime.hpp"
#include "../ui/Input.hpp"

namespace Tutones::App
{
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

        TUTONES_LOG_INFO("app", "Game runtime ready; starting vehicle feature runtimes");
        if (!Game::Paint::VehiclePaintRuntime::Get().Start())
        {
            TUTONES_LOG_ERROR("app", "Vehicle paint runtime failed to queue its first GTA script-thread tick");
            Game::Paint::VehiclePaintRuntime::Get().Stop();
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
            Game::Mods::VehicleModificationRuntime::Get().Stop();
            Game::Paint::VehiclePaintRuntime::Get().Stop();
            Runtime::GameRuntime::Get().Shutdown();
            Hooking::HookManager::Get().Shutdown();
            Render::Renderer::Get().Shutdown();
            Core::Services::Get().Shutdown();
            return false;
        }

        TUTONES_LOG_INFO("app", "Player runtime ready; starting weapon runtime");
        if (!Game::WeaponFeatures::WeaponRuntime::Get().Start())
        {
            TUTONES_LOG_ERROR("app", "Weapon runtime failed to queue its first GTA script-thread tick");
            Game::WeaponFeatures::WeaponRuntime::Get().Stop();
            Game::PlayerFeatures::PlayerRuntime::Get().Stop();
            Game::Mods::VehicleModificationRuntime::Get().Stop();
            Game::Paint::VehiclePaintRuntime::Get().Stop();
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
            Game::PlayerFeatures::PlayerRuntime::Get().Stop();
            Game::Mods::VehicleModificationRuntime::Get().Stop();
            Game::Paint::VehiclePaintRuntime::Get().Stop();
            Runtime::GameRuntime::Get().Shutdown();
            Hooking::HookManager::Get().Shutdown();
            Render::Renderer::Get().Shutdown();
            Core::Services::Get().Shutdown();
            return false;
        }

        TUTONES_LOG_INFO("app", "Tutones Menu application initialized with render, input, GTA native runtime, vehicle, player, and weapon feature layers");
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

        TUTONES_LOG_DEBUG("app", "Stopping Win32 menu input routing");
        UI::Input::Get().Shutdown();

        TUTONES_LOG_DEBUG("app", "Stopping weapon, player, and vehicle feature scheduling before GTA runtime teardown");
        Game::WeaponFeatures::WeaponRuntime::Get().Stop();
        Game::PlayerFeatures::PlayerRuntime::Get().Stop();
        Game::Mods::VehicleModificationRuntime::Get().Stop();
        Game::Paint::VehiclePaintRuntime::Get().Stop();

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
