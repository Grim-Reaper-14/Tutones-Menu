#include "Application.hpp"

#include "../core/CoreServices.hpp"
#include "../core/logging/Logger.hpp"
#include "../hooking/HookManager.hpp"
#include "../render/Renderer.hpp"

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
            return true;

        if (!Core::Services::Get().Initialize(moduleDirectory))
            return false;

        if (!Render::Renderer::Get().Initialize())
        {
            TUTONES_LOG_ERROR("app", "Renderer bootstrap failed");
            Core::Services::Get().Shutdown();
            return false;
        }

        if (!Hooking::HookManager::Get().Initialize() || !Hooking::HookManager::Get().Install())
        {
            TUTONES_LOG_ERROR("app", "D3D12 hook bootstrap failed");
            Hooking::HookManager::Get().Shutdown();
            Render::Renderer::Get().Shutdown();
            Core::Services::Get().Shutdown();
            return false;
        }

        TUTONES_LOG_INFO("app", "Tutones Menu application initialized with D3D12 hooks");
        m_Running = true;
        return true;
    }

    void Application::Shutdown() noexcept
    {
        if (!m_Running)
            return;

        TUTONES_LOG_INFO("app", "Tutones Menu application shutting down");

        // Stop callbacks before releasing renderer-owned D3D12 objects.
        Hooking::HookManager::Get().Shutdown();
        Render::Renderer::Get().Shutdown();
        Core::Services::Get().Shutdown();
        m_Running = false;
    }

    bool Application::IsRunning() const noexcept
    {
        return m_Running;
    }
}
