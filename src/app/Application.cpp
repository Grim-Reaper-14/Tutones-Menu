#include "Application.hpp"

#include "../core/CoreServices.hpp"
#include "../core/logging/Logger.hpp"

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

        TUTONES_LOG_INFO("app", "Tutones Menu application initialized");
        m_Running = true;
        return true;
    }

    void Application::Shutdown() noexcept
    {
        if (!m_Running)
            return;

        TUTONES_LOG_INFO("app", "Tutones Menu application shutting down");
        Core::Services::Get().Shutdown();
        m_Running = false;
    }

    bool Application::IsRunning() const noexcept
    {
        return m_Running;
    }
}
