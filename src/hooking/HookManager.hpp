#pragma once

#include "HookStatus.hpp"

#include <dxgiformat.h>

#include <atomic>
#include <cstdint>

struct IDXGISwapChain;
struct ID3D12CommandList;
struct ID3D12CommandQueue;

namespace Tutones::Hooking
{
    using PresentFn = long(__stdcall*)(IDXGISwapChain*, unsigned int, unsigned int);
    using ResizeBuffersFn = long(__stdcall*)(IDXGISwapChain*, unsigned int, unsigned int, unsigned int, DXGI_FORMAT, unsigned int);
    using ExecuteCommandListsFn = void(__stdcall*)(ID3D12CommandQueue*, unsigned int, ID3D12CommandList* const*);

    class HookManager final
    {
    public:
        static HookManager& Get() noexcept;

        bool Initialize() noexcept;
        bool Install() noexcept;
        void Shutdown() noexcept;

        [[nodiscard]] HookStatus Status() const noexcept;
        [[nodiscard]] bool IsInstalled() const noexcept;

        [[nodiscard]] PresentFn OriginalPresent() const noexcept;
        [[nodiscard]] ResizeBuffersFn OriginalResizeBuffers() const noexcept;
        [[nodiscard]] ExecuteCommandListsFn OriginalExecuteCommandLists() const noexcept;

        void SetCommandQueue(ID3D12CommandQueue* queue) noexcept;
        [[nodiscard]] ID3D12CommandQueue* CommandQueue() const noexcept;

    private:
        HookManager() = default;
        ~HookManager() = default;
        HookManager(const HookManager&) = delete;
        HookManager& operator=(const HookManager&) = delete;

        void ResetTargets() noexcept;

        HookStatus m_Status{HookStatus::NotInitialized};
        PresentFn m_OriginalPresent{};
        ResizeBuffersFn m_OriginalResizeBuffers{};
        ExecuteCommandListsFn m_OriginalExecuteCommandLists{};
        std::atomic<ID3D12CommandQueue*> m_CommandQueue{nullptr};

        void* m_PresentTarget{};
        void* m_ResizeBuffersTarget{};
        void* m_ExecuteCommandListsTarget{};
        bool m_MinHookInitialized{};
    };
}
