#include "HookManager.hpp"

#include "../core/logging/Logger.hpp"

namespace Tutones::Hooking
{
    HookManager& HookManager::Get() noexcept
    {
        static HookManager instance;
        return instance;
    }

    bool HookManager::Initialize() noexcept
    {
        if (m_Status == HookStatus::Installed || m_Status == HookStatus::Initializing)
            return true;

        m_Status = HookStatus::Initializing;
        TUTONES_LOG_INFO("hook", "Hook manager initialized");
        m_Status = HookStatus::NotInitialized;
        return true;
    }

    bool HookManager::Install() noexcept
    {
        if (m_Status == HookStatus::Installed)
            return true;
        if (m_Status == HookStatus::Initializing)
            return false;

        m_Status = HookStatus::Initializing;
        TUTONES_LOG_INFO("hook", "D3D12 hook installation requested");

        // Actual MinHook/vtable installation belongs in the platform-specific
        // implementation. We keep this facade side-effect free until that
        // backend is connected to avoid pretending a hook is installed.
        m_Status = HookStatus::Failed;
        TUTONES_LOG_ERROR("hook", "D3D12 hook backend is not connected yet");
        return false;
    }

    void HookManager::Shutdown() noexcept
    {
        if (m_Status == HookStatus::Stopped || m_Status == HookStatus::NotInitialized)
            return;

        m_Status = HookStatus::ShuttingDown;
        TUTONES_LOG_INFO("hook", "Hook manager shutting down");
        m_CommandQueue = nullptr;
        m_OriginalPresent = nullptr;
        m_OriginalResizeBuffers = nullptr;
        m_Status = HookStatus::Stopped;
    }

    HookStatus HookManager::Status() const noexcept
    {
        return m_Status;
    }

    bool HookManager::IsInstalled() const noexcept
    {
        return m_Status == HookStatus::Installed;
    }

    PresentFn HookManager::OriginalPresent() const noexcept
    {
        return m_OriginalPresent;
    }

    ResizeBuffersFn HookManager::OriginalResizeBuffers() const noexcept
    {
        return m_OriginalResizeBuffers;
    }

    void HookManager::SetCommandQueue(ID3D12CommandQueue* queue) noexcept
    {
        m_CommandQueue = queue;
    }

    ID3D12CommandQueue* HookManager::CommandQueue() const noexcept
    {
        return m_CommandQueue;
    }
}
