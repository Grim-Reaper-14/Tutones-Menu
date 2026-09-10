#include "HookManager.hpp"
#include "core/logging/Logger.hpp"

namespace TutonesV2::Backend::Hooking
{
    HookManager& HookManager::Get() noexcept
    {
        static HookManager instance;
        return instance;
    }

    bool HookManager::Initialize() noexcept
    {
        m_Ready = true;
        Core::Logging::Logger::Get().Info("hooks", "Hook manager shell ready; no GTA hooks installed in base milestone");
        return true;
    }

    void HookManager::Shutdown() noexcept
    {
        m_Ready = false;
    }

    bool HookManager::IsReady() const noexcept
    {
        return m_Ready;
    }
}
