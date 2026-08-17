#pragma once

#include "HookStatus.hpp"

#include <cstdint>

struct IDXGISwapChain;
struct ID3D12CommandQueue;

namespace Tutones::Hooking
{
    using PresentFn = long(__stdcall*)(IDXGISwapChain*, unsigned int, unsigned int);
    using ResizeBuffersFn = long(__stdcall*)(IDXGISwapChain*, unsigned int, unsigned int, unsigned int, int, unsigned int);

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

        void SetCommandQueue(ID3D12CommandQueue* queue) noexcept;
        [[nodiscard]] ID3D12CommandQueue* CommandQueue() const noexcept;

    private:
        HookManager() = default;
        ~HookManager() = default;
        HookManager(const HookManager&) = delete;
        HookManager& operator=(const HookManager&) = delete;

        HookStatus m_Status{HookStatus::NotInitialized};
        PresentFn m_OriginalPresent{};
        ResizeBuffersFn m_OriginalResizeBuffers{};
        ID3D12CommandQueue* m_CommandQueue{};
    };
}
