#include "Renderer.hpp"
#include "core/logging/Logger.hpp"

namespace TutonesV2::Frontend::Render
{
    Renderer& Renderer::Get() noexcept
    {
        static Renderer instance;
        return instance;
    }

    bool Renderer::Initialize() noexcept
    {
        m_Ready = true;
        Core::Logging::Logger::Get().Info("frontend", "Renderer shell ready; D3D12/ImGui integration follows base validation");
        return true;
    }

    void Renderer::Shutdown() noexcept
    {
        m_Ready = false;
    }

    bool Renderer::IsReady() const noexcept
    {
        return m_Ready;
    }
}
