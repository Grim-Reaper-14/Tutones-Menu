#include "HookManager.hpp"

#include "../core/logging/Logger.hpp"
#include "../render/Renderer.hpp"

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <MinHook.h>

#include <string>

namespace Tutones::Hooking
{
    namespace
    {
        constexpr std::size_t PresentVTableIndex = 8;
        constexpr std::size_t ResizeBuffersVTableIndex = 13;
        constexpr std::size_t ExecuteCommandListsVTableIndex = 10;

        LRESULT CALLBACK ProbeWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
        {
            return ::DefWindowProcW(window, message, wParam, lParam);
        }

        std::string MhFailure(std::string_view operation, MH_STATUS status)
        {
            std::string message(operation);
            message += " failed: ";
            message += MH_StatusToString(status);
            return message;
        }

        struct ProbeObjects final
        {
            HWND window{};
            HINSTANCE instance{};
            ATOM windowClass{};
            ID3D12Device* device{};
            ID3D12CommandQueue* queue{};
            IDXGIFactory4* factory{};
            IDXGISwapChain1* swapChain{};

            ~ProbeObjects()
            {
                if (swapChain) swapChain->Release();
                if (factory) factory->Release();
                if (queue) queue->Release();
                if (device) device->Release();
                if (window) ::DestroyWindow(window);
                if (windowClass && instance) ::UnregisterClassW(L"TutonesD3D12HookProbe", instance);
            }
        };

        bool CreateProbeObjects(ProbeObjects& probe) noexcept
        {
            probe.instance = ::GetModuleHandleW(nullptr);

            WNDCLASSEXW wc{};
            wc.cbSize = sizeof(wc);
            wc.lpfnWndProc = ProbeWndProc;
            wc.hInstance = probe.instance;
            wc.lpszClassName = L"TutonesD3D12HookProbe";
            probe.windowClass = ::RegisterClassExW(&wc);
            if (!probe.windowClass && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
                return false;

            probe.window = ::CreateWindowExW(
                0,
                wc.lpszClassName,
                L"Tutones D3D12 Hook Probe",
                WS_OVERLAPPEDWINDOW,
                0,
                0,
                100,
                100,
                nullptr,
                nullptr,
                probe.instance,
                nullptr);
            if (!probe.window)
                return false;

            if (FAILED(::D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&probe.device))))
                return false;

            D3D12_COMMAND_QUEUE_DESC queueDesc{};
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
            queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            queueDesc.NodeMask = 0;

            if (FAILED(probe.device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&probe.queue))))
                return false;

            if (FAILED(::CreateDXGIFactory1(IID_PPV_ARGS(&probe.factory))))
                return false;

            DXGI_SWAP_CHAIN_DESC1 swapDesc{};
            swapDesc.Width = 100;
            swapDesc.Height = 100;
            swapDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            swapDesc.Stereo = FALSE;
            swapDesc.SampleDesc.Count = 1;
            swapDesc.SampleDesc.Quality = 0;
            swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            swapDesc.BufferCount = 2;
            swapDesc.Scaling = DXGI_SCALING_STRETCH;
            swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            swapDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
            swapDesc.Flags = 0;

            if (FAILED(probe.factory->CreateSwapChainForHwnd(
                    probe.queue,
                    probe.window,
                    &swapDesc,
                    nullptr,
                    nullptr,
                    &probe.swapChain)))
            {
                return false;
            }

            return true;
        }

        long __stdcall PresentDetour(IDXGISwapChain* swapChain, unsigned int syncInterval, unsigned int flags)
        {
            auto& hooks = HookManager::Get();
            const auto original = hooks.OriginalPresent();
            if (!original)
                return E_FAIL;

            if (swapChain)
            {
                IDXGISwapChain3* swapChain3{};
                if (SUCCEEDED(swapChain->QueryInterface(IID_PPV_ARGS(&swapChain3))))
                {
                    auto& renderer = Render::Renderer::Get();
                    auto* queue = hooks.CommandQueue();
                    if (queue && !renderer.IsInitialized())
                        static_cast<void>(renderer.InitializeD3D12(swapChain3, queue));

                    if (renderer.IsInitialized())
                    {
                        renderer.BeginFrame();
                        renderer.RenderFrame();
                    }

                    swapChain3->Release();
                }
            }

            return original(swapChain, syncInterval, flags);
        }

        long __stdcall ResizeBuffersDetour(
            IDXGISwapChain* swapChain,
            unsigned int bufferCount,
            unsigned int width,
            unsigned int height,
            DXGI_FORMAT format,
            unsigned int swapChainFlags)
        {
            auto& hooks = HookManager::Get();
            const auto original = hooks.OriginalResizeBuffers();
            if (!original)
                return E_FAIL;

            Render::Renderer::Get().OnResize(width, height);
            return original(swapChain, bufferCount, width, height, format, swapChainFlags);
        }

        void __stdcall ExecuteCommandListsDetour(
            ID3D12CommandQueue* queue,
            unsigned int count,
            ID3D12CommandList* const* commandLists)
        {
            auto& hooks = HookManager::Get();
            if (queue && !hooks.CommandQueue())
            {
                const auto desc = queue->GetDesc();
                if (desc.Type == D3D12_COMMAND_LIST_TYPE_DIRECT)
                {
                    hooks.SetCommandQueue(queue);
                    if (hooks.CommandQueue() == queue)
                        TUTONES_LOG_INFO("hook", "Captured live D3D12 DIRECT command queue");
                }
            }

            if (const auto original = hooks.OriginalExecuteCommandLists())
                original(queue, count, commandLists);
        }
    }

    HookManager& HookManager::Get() noexcept
    {
        static HookManager instance;
        return instance;
    }

    bool HookManager::Initialize() noexcept
    {
        if (m_Status == HookStatus::Ready || m_Status == HookStatus::Installed)
            return true;
        if (m_Status == HookStatus::Initializing)
            return false;

        m_Status = HookStatus::Initializing;
        TUTONES_LOG_INFO("hook", "Initializing MinHook backend");

        const auto status = ::MH_Initialize();
        if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED)
        {
            m_Status = HookStatus::Failed;
            TUTONES_LOG_ERROR("hook", MhFailure("MH_Initialize", status));
            return false;
        }

        m_MinHookInitialized = true;
        m_Status = HookStatus::Ready;
        TUTONES_LOG_INFO("hook", "MinHook backend ready");
        return true;
    }

    bool HookManager::Install() noexcept
    {
        if (m_Status == HookStatus::Installed)
            return true;
        if (m_Status != HookStatus::Ready && !Initialize())
            return false;

        m_Status = HookStatus::Initializing;
        TUTONES_LOG_INFO("hook", "Discovering D3D12 and DXGI hook targets");

        ProbeObjects probe;
        if (!CreateProbeObjects(probe))
        {
            m_Status = HookStatus::Failed;
            TUTONES_LOG_ERROR("hook", "Failed to create temporary D3D12 probe objects");
            return false;
        }

        auto** swapVTable = *reinterpret_cast<void***>(probe.swapChain);
        auto** queueVTable = *reinterpret_cast<void***>(probe.queue);
        if (!swapVTable || !queueVTable)
        {
            m_Status = HookStatus::Failed;
            TUTONES_LOG_ERROR("hook", "Failed to read D3D12/DXGI virtual tables");
            return false;
        }

        m_PresentTarget = swapVTable[PresentVTableIndex];
        m_ResizeBuffersTarget = swapVTable[ResizeBuffersVTableIndex];
        m_ExecuteCommandListsTarget = queueVTable[ExecuteCommandListsVTableIndex];

        const auto createPresent = ::MH_CreateHook(
            m_PresentTarget,
            reinterpret_cast<void*>(&PresentDetour),
            reinterpret_cast<void**>(&m_OriginalPresent));
        if (createPresent != MH_OK)
        {
            TUTONES_LOG_ERROR("hook", MhFailure("Create Present hook", createPresent));
            ResetTargets();
            m_Status = HookStatus::Failed;
            return false;
        }

        const auto createResize = ::MH_CreateHook(
            m_ResizeBuffersTarget,
            reinterpret_cast<void*>(&ResizeBuffersDetour),
            reinterpret_cast<void**>(&m_OriginalResizeBuffers));
        if (createResize != MH_OK)
        {
            TUTONES_LOG_ERROR("hook", MhFailure("Create ResizeBuffers hook", createResize));
            ::MH_RemoveHook(m_PresentTarget);
            ResetTargets();
            m_Status = HookStatus::Failed;
            return false;
        }

        const auto createExecute = ::MH_CreateHook(
            m_ExecuteCommandListsTarget,
            reinterpret_cast<void*>(&ExecuteCommandListsDetour),
            reinterpret_cast<void**>(&m_OriginalExecuteCommandLists));
        if (createExecute != MH_OK)
        {
            TUTONES_LOG_ERROR("hook", MhFailure("Create ExecuteCommandLists hook", createExecute));
            ::MH_RemoveHook(m_ResizeBuffersTarget);
            ::MH_RemoveHook(m_PresentTarget);
            ResetTargets();
            m_Status = HookStatus::Failed;
            return false;
        }

        const auto queuePresent = ::MH_QueueEnableHook(m_PresentTarget);
        const auto queueResize = ::MH_QueueEnableHook(m_ResizeBuffersTarget);
        const auto queueExecute = ::MH_QueueEnableHook(m_ExecuteCommandListsTarget);
        if (queuePresent != MH_OK || queueResize != MH_OK || queueExecute != MH_OK)
        {
            TUTONES_LOG_ERROR("hook", "Failed to queue one or more D3D12 hooks for enabling");
            ::MH_RemoveHook(m_ExecuteCommandListsTarget);
            ::MH_RemoveHook(m_ResizeBuffersTarget);
            ::MH_RemoveHook(m_PresentTarget);
            ResetTargets();
            m_Status = HookStatus::Failed;
            return false;
        }

        const auto applyStatus = ::MH_ApplyQueued();
        if (applyStatus != MH_OK)
        {
            TUTONES_LOG_ERROR("hook", MhFailure("Enable D3D12 hooks", applyStatus));
            ::MH_RemoveHook(m_ExecuteCommandListsTarget);
            ::MH_RemoveHook(m_ResizeBuffersTarget);
            ::MH_RemoveHook(m_PresentTarget);
            ResetTargets();
            m_Status = HookStatus::Failed;
            return false;
        }

        m_Status = HookStatus::Installed;
        TUTONES_LOG_INFO("hook", "D3D12 Present, ResizeBuffers, and ExecuteCommandLists hooks installed");
        return true;
    }

    void HookManager::Shutdown() noexcept
    {
        if (m_Status == HookStatus::Stopped || m_Status == HookStatus::NotInitialized)
            return;

        m_Status = HookStatus::ShuttingDown;
        TUTONES_LOG_INFO("hook", "D3D12 hook manager shutting down");

        if (m_PresentTarget) ::MH_DisableHook(m_PresentTarget);
        if (m_ResizeBuffersTarget) ::MH_DisableHook(m_ResizeBuffersTarget);
        if (m_ExecuteCommandListsTarget) ::MH_DisableHook(m_ExecuteCommandListsTarget);

        if (m_PresentTarget) ::MH_RemoveHook(m_PresentTarget);
        if (m_ResizeBuffersTarget) ::MH_RemoveHook(m_ResizeBuffersTarget);
        if (m_ExecuteCommandListsTarget) ::MH_RemoveHook(m_ExecuteCommandListsTarget);

        if (auto* queue = m_CommandQueue.exchange(nullptr, std::memory_order_acq_rel))
            queue->Release();

        ResetTargets();

        if (m_MinHookInitialized)
        {
            const auto status = ::MH_Uninitialize();
            if (status != MH_OK && status != MH_ERROR_NOT_INITIALIZED)
                TUTONES_LOG_WARN("hook", MhFailure("MH_Uninitialize", status));
            m_MinHookInitialized = false;
        }

        m_Status = HookStatus::Stopped;
        TUTONES_LOG_INFO("hook", "D3D12 hooks stopped");
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

    ExecuteCommandListsFn HookManager::OriginalExecuteCommandLists() const noexcept
    {
        return m_OriginalExecuteCommandLists;
    }

    void HookManager::SetCommandQueue(ID3D12CommandQueue* queue) noexcept
    {
        if (!queue)
            return;

        queue->AddRef();
        ID3D12CommandQueue* expected = nullptr;
        if (!m_CommandQueue.compare_exchange_strong(
                expected,
                queue,
                std::memory_order_release,
                std::memory_order_relaxed))
        {
            queue->Release();
        }
    }

    ID3D12CommandQueue* HookManager::CommandQueue() const noexcept
    {
        return m_CommandQueue.load(std::memory_order_acquire);
    }

    void HookManager::ResetTargets() noexcept
    {
        m_PresentTarget = nullptr;
        m_ResizeBuffersTarget = nullptr;
        m_ExecuteCommandListsTarget = nullptr;
        m_OriginalPresent = nullptr;
        m_OriginalResizeBuffers = nullptr;
        m_OriginalExecuteCommandLists = nullptr;
    }
}
