#include "Application.hpp"

#include "logging/Logger.hpp"
#include "backend/Backend.hpp"
#include "frontend/MenuModel.hpp"
#include "frontend/render/Renderer.hpp"

namespace TutonesV2::Core
{
    Application& Application::Get() noexcept
    {
        static Application instance;
        return instance;
    }

    bool Application::Initialize(HMODULE module) noexcept
    {
        bool expected = false;
        if (!m_Running.compare_exchange_strong(expected, true))
            return true;

        m_Module = module;
        Logging::Logger::Get().Initialize();
        Logging::Logger::Get().Info("core", "Tutones Menu V2 bootstrap starting");

        if (!Backend::Backend::Get().Initialize())
        {
            Logging::Logger::Get().Error("core", "Backend initialization failed");
            m_Running = false;
            return false;
        }

        Frontend::MenuModel::Get().Initialize();
        Frontend::Render::Renderer::Get().Initialize();

        Logging::Logger::Get().Info("core", "Tutones Menu V2 base initialized");
        return true;
    }

    void Application::Shutdown() noexcept
    {
        if (!m_Running.exchange(false))
            return;

        Frontend::Render::Renderer::Get().Shutdown();
        Backend::Backend::Get().Shutdown();
        Logging::Logger::Get().Info("core", "Tutones Menu V2 shutdown complete");
        Logging::Logger::Get().Shutdown();
        m_Module = nullptr;
    }

    bool Application::IsRunning() const noexcept
    {
        return m_Running.load();
    }

    HMODULE Application::Module() const noexcept
    {
        return m_Module;
    }
}
