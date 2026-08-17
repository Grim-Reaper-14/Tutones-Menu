#include "HookManager.hpp"

#include "Win32Hook.hpp"
#include "../core/logging/Logger.hpp"
#include "../render/Renderer.hpp"

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <MinHook.h>

#include <chrono>
#include <string>
#include <thread>

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

        struct CallbackGuard final
        {
            explicit CallbackGuard(HookManager& manager) noexcept
                : hooks(manager), entered(manager.TryEnterCallback())
            {
            }

            ~CallbackGuard()
            {
                if (entered)
                    hooks.LeaveCallback();
            }

            HookManager& hooks;
            bool entered{};
        };

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
                TUTONES_LOG_TRACE("hook.d3d12", "Temporary D3D12 probe objects released");
            }
        };

        bool CreateProbeObjects(ProbeObjects& probe) noexcept
        {
            TUTONES_LOG_DEBUG("hook.d3d12", "Creating temporary D3D12 objects for vtable discovery");
            probe.instance = ::GetModuleHandleW(nullptr);
            if (!probe.instance)
            {
                TUTONES_LOG_ERROR("hook.d3d12", "GetModuleHandleW failed while creating hook probe");
                return false;
            }

            WNDCLASSEXW wc{};
            wc.cbSize = sizeof(wc);
            wc.lpfnWndProc = ProbeWndProc;
            wc.hInstance = probe.instance;
            wc.lpszClassName = L"TutonesD3D12HookProbe";
            probe.windowClass = ::RegisterClassExW(&wc);
            if (!probe.windowClass && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            {
                TUTONES_LOG_ERROR("hook.d3d12", "Failed to register temporary D3D12 probe window class");
                return false;
            }
            TUTONES_LOG_TRACE("hook.d3d12", "Probe window class ready");

            probe.window = ::CreateWindowExW(0, wc.lpszClassName, L"Tutones D3D12 Hook Probe", WS_OVERLAPPEDWINDOW,
                0, 0, 100, 100, nullptr, nullptr, probe.instance, nullptr);
            if (!probe.window)
            {
                TUTONES_LOG_ERROR("hook.d3d12", "Failed to create temporary D3D12 probe window");
                return false;
            }
            TUTONES_LOG_TRACE("hook.d3d12", "Probe window created");

            if (FAILED(::D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&probe.device))))
            {
                TUTONES_LOG_ERROR("hook.d3d12", "D3D12CreateDevice failed for hook probe");
                return false;
            }
            TUTONES_LOG_TRACE("hook.d3d12", "Probe D3D12 device created");

            D3D12_COMMAND_QUEUE_DESC queueDesc{};
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
            queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

            if (FAILED(probe.device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&probe.queue))))
            {
                TUTONES_LOG_ERROR("hook.d3d12", "Failed to create probe DIRECT command queue");
                return false;
            }
            TUTONES_LOG_TRACE("hook.d3d12", "Probe DIRECT command queue created");

            if (FAILED(::CreateDXGIFactory1(IID_PPV_ARGS(&probe.factory))))
            {
                TUTONES_LOG_ERROR("hook.dxgi", "CreateDXGIFactory1 failed for hook probe");
                return false;
            }
            TUTONES_LOG_TRACE("hook.dxgi", "Probe DXGI factory created");

            DXGI_SWAP_CHAIN_DESC1 swapDesc{};
            swapDesc.Width = 100;
            swapDesc.Height = 100;
            swapDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            swapDesc.SampleDesc.Count = 1;
            swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            swapDesc.BufferCount = 2;
            swapDesc.Scaling = DXGI_SCALING_STRETCH;
            swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            swapDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

            if (FAILED(probe.factory->CreateSwapChainForHwnd(probe.queue, probe.window, &swapDesc, nullptr, nullptr, &probe.swapChain)))
            {
                TUTONES_LOG_ERROR("hook.dxgi", "Failed to create temporary probe swap chain");
                return false;
            }

            TUTONES_LOG_DEBUG("hook.d3d12", "Temporary D3D12/DXGI probe objects created successfully");
            return true;
        }

        long __stdcall PresentDetour(IDXGISwapChain* swapChain, unsigned int syncInterval, unsigned int flags)
        {
            auto& hooks = HookManager::Get();
            const auto original = hooks.OriginalPresent();
            if (!original)
            {
                TUTONES_LOG_CRITICAL("hook.present", "Present detour has no original function pointer");
                return E_FAIL;
            }

            CallbackGuard callback(hooks);
            if (!callback.entered)
                return original(swapChain, syncInterval, flags);

            if (swapChain)
            {
                DXGI_SWAP_CHAIN_DESC desc{};
                if (SUCCEEDED(swapChain->GetDesc(&desc)) && desc.OutputWindow)
                {
                    auto& win32 = Win32Hook::Get();
                    if (win32.IsPrimaryWindow(desc.OutputWindow) || win32.Attach(desc.OutputWindow))
                        static_cast<void>(hooks.TrySetPrimarySwapChain(swapChain));
                }

                if (hooks.IsPrimarySwapChain(swapChain))
                {
                    IDXGISwapChain3* swapChain3{};
                    if (SUCCEEDED(swapChain->QueryInterface(IID_PPV_ARGS(&swapChain3))))
                    {
                        auto& renderer = Render::Renderer::Get();
                        auto* queue = hooks.CommandQueue();
                        if (queue && !renderer.IsInitialized())
                        {
                            TUTONES_LOG_DEBUG("hook.present", "Primary swap chain and live queue ready; initializing renderer");
                            if (!renderer.InitializeD3D12(swapChain3, queue))
                                TUTONES_LOG_ERROR("hook.present", "Renderer initialization from Present failed");
                        }

                        if (renderer.IsInitialized())
                        {
                            renderer.BeginFrame();
                            renderer.RenderFrame();
                        }

                        swapChain3->Release();
                    }
                    else
                    {
                        static std::atomic<bool> loggedSwapChain3Failure{false};
                        if (!loggedSwapChain3Failure.exchange(true, std::memory_order_acq_rel))
                            TUTONES_LOG_ERROR("hook.present", "Primary swap chain does not expose IDXGISwapChain3");
                    }
                }
            }

            return original(swapChain, syncInterval, flags);
        }

        long __stdcall ResizeBuffersDetour(IDXGISwapChain* swapChain, unsigned int bufferCount, unsigned int width,
            unsigned int height, DXGI_FORMAT format, unsigned int swapChainFlags)
        {
            auto& hooks = HookManager::Get();
            const auto original = hooks.OriginalResizeBuffers();
            if (!original)
            {
                TUTONES_LOG_CRITICAL("hook.resize", "ResizeBuffers detour has no original function pointer");
                return E_FAIL;
            }

            CallbackGuard callback(hooks);
            const bool primary = callback.entered && hooks.IsPrimarySwapChain(swapChain);
            auto& renderer = Render::Renderer::Get();

            if (primary && renderer.IsInitialized())
            {
                std::string message("Primary swap chain resizing to ");
                message += std::to_string(width);
                message += 'x';
                message += std::to_string(height);
                message += ", buffers=";
                message += std::to_string(bufferCount);
                TUTONES_LOG_INFO("hook.resize", message);
                renderer.BeforeResize();
            }

            const auto result = original(swapChain, bufferCount, width, height, format, swapChainFlags);

            if (primary && renderer.IsInitialized())
            {
                if (SUCCEEDED(result))
                    renderer.AfterResize(width, height);
                else
                    TUTONES_LOG_ERROR("hook.resize", "Original ResizeBuffers failed after renderer resources were released");
            }

            return result;
        }

        void __stdcall ExecuteCommandListsDetour(ID3D12CommandQueue* queue, unsigned int count,
            ID3D12CommandList* const* commandLists)
        {
            auto& hooks = HookManager::Get();
            const auto original = hooks.OriginalExecuteCommandLists();
            if (!original)
            {
                static std::atomic<bool> loggedMissingOriginal{false};
                if (!loggedMissingOriginal.exchange(true, std::memory_order_acq_rel))
                    TUTONES_LOG_CRITICAL("hook.queue", "ExecuteCommandLists detour has no original function pointer");
                return;
            }

            CallbackGuard callback(hooks);
            if (callback.entered && hooks.PrimarySwapChain() && queue && !hooks.CommandQueue())
            {
                const auto desc = queue->GetDesc();
                if (desc.Type == D3D12_COMMAND_LIST_TYPE_DIRECT)
                {
                    TUTONES_LOG_DEBUG("hook.queue", "Found DIRECT command queue candidate after primary swap chain selection");
                    hooks.SetCommandQueue(queue);
                    if (hooks.CommandQueue() == queue)
                        TUTONES_LOG_INFO("hook.queue", "Captured live D3D12 DIRECT command queue for primary render target");
                }
            }

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
        const auto current = m_Status.load(std::memory_order_acquire);
        if (current == HookStatus::Ready || current == HookStatus::Installed)
        {
            TUTONES_LOG_TRACE("hook", "Hook manager initialize requested while already ready");
            return true;
        }
        if (current == HookStatus::Initializing)
        {
            TUTONES_LOG_WARN("hook", "Hook manager initialize requested while initialization is already in progress");
            return false;
        }

        m_ShuttingDown.store(false, std::memory_order_release);
        m_Status.store(HookStatus::Initializing, std::memory_order_release);
        TUTONES_LOG_INFO("hook", "Initializing MinHook backend");

        const auto status = ::MH_Initialize();
        if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED)
        {
            m_Status.store(HookStatus::Failed, std::memory_order_release);
            TUTONES_LOG_ERROR("hook", MhFailure("MH_Initialize", status));
            return false;
        }

        if (status == MH_ERROR_ALREADY_INITIALIZED)
            TUTONES_LOG_WARN("hook", "MinHook was already initialized by another component");
        else
            TUTONES_LOG_DEBUG("hook", "MH_Initialize completed successfully");

        m_MinHookInitialized = true;
        m_Status.store(HookStatus::Ready, std::memory_order_release);
        TUTONES_LOG_INFO("hook", "MinHook backend ready");
        return true;
    }

    bool HookManager::Install() noexcept
    {
        if (m_Status.load(std::memory_order_acquire) == HookStatus::Installed)
        {
            TUTONES_LOG_TRACE("hook", "Hook installation requested while hooks are already installed");
            return true;
        }
        if (m_Status.load(std::memory_order_acquire) != HookStatus::Ready && !Initialize())
        {
            TUTONES_LOG_ERROR("hook", "Hook installation aborted because MinHook initialization failed");
            return false;
        }

        m_Status.store(HookStatus::Initializing, std::memory_order_release);
        TUTONES_LOG_INFO("hook", "Discovering D3D12 and DXGI hook targets");

        ProbeObjects probe;
        if (!CreateProbeObjects(probe))
        {
            m_Status.store(HookStatus::Failed, std::memory_order_release);
            TUTONES_LOG_ERROR("hook", "Failed to create temporary D3D12 probe objects");
            return false;
        }

        auto** swapVTable = *reinterpret_cast<void***>(probe.swapChain);
        auto** queueVTable = *reinterpret_cast<void***>(probe.queue);
        if (!swapVTable || !queueVTable)
        {
            m_Status.store(HookStatus::Failed, std::memory_order_release);
            TUTONES_LOG_ERROR("hook", "Failed to read D3D12/DXGI virtual tables");
            return false;
        }

        m_PresentTarget = swapVTable[PresentVTableIndex];
        m_ResizeBuffersTarget = swapVTable[ResizeBuffersVTableIndex];
        m_ExecuteCommandListsTarget = queueVTable[ExecuteCommandListsVTableIndex];
        if (!m_PresentTarget || !m_ResizeBuffersTarget || !m_ExecuteCommandListsTarget)
        {
            m_Status.store(HookStatus::Failed, std::memory_order_release);
            TUTONES_LOG_ERROR("hook", "One or more D3D12/DXGI vtable targets resolved to null");
            ResetTargets();
            return false;
        }
        TUTONES_LOG_DEBUG("hook", "Resolved Present, ResizeBuffers, and ExecuteCommandLists vtable targets");

        const auto createPresent = ::MH_CreateHook(m_PresentTarget, reinterpret_cast<void*>(&PresentDetour), reinterpret_cast<void**>(&m_OriginalPresent));
        if (createPresent != MH_OK)
        {
            TUTONES_LOG_ERROR("hook", MhFailure("Create Present hook", createPresent));
            ResetTargets();
            m_Status.store(HookStatus::Failed, std::memory_order_release);
            return false;
        }
        TUTONES_LOG_DEBUG("hook.present", "Present trampoline created");

        const auto createResize = ::MH_CreateHook(m_ResizeBuffersTarget, reinterpret_cast<void*>(&ResizeBuffersDetour), reinterpret_cast<void**>(&m_OriginalResizeBuffers));
        if (createResize != MH_OK)
        {
            TUTONES_LOG_ERROR("hook", MhFailure("Create ResizeBuffers hook", createResize));
            ::MH_RemoveHook(m_PresentTarget);
            ResetTargets();
            m_Status.store(HookStatus::Failed, std::memory_order_release);
            return false;
        }
        TUTONES_LOG_DEBUG("hook.resize", "ResizeBuffers trampoline created");

        const auto createExecute = ::MH_CreateHook(m_ExecuteCommandListsTarget, reinterpret_cast<void*>(&ExecuteCommandListsDetour), reinterpret_cast<void**>(&m_OriginalExecuteCommandLists));
        if (createExecute != MH_OK)
        {
            TUTONES_LOG_ERROR("hook", MhFailure("Create ExecuteCommandLists hook", createExecute));
            ::MH_RemoveHook(m_ResizeBuffersTarget);
            ::MH_RemoveHook(m_PresentTarget);
            ResetTargets();
            m_Status.store(HookStatus::Failed, std::memory_order_release);
            return false;
        }
        TUTONES_LOG_DEBUG("hook.queue", "ExecuteCommandLists trampoline created");

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
            m_Status.store(HookStatus::Failed, std::memory_order_release);
            return false;
        }
        TUTONES_LOG_DEBUG("hook", "All D3D12/DXGI hooks queued for enabling");

        const auto applyStatus = ::MH_ApplyQueued();
        if (applyStatus != MH_OK)
        {
            TUTONES_LOG_ERROR("hook", MhFailure("Enable D3D12 hooks", applyStatus));
            ::MH_RemoveHook(m_ExecuteCommandListsTarget);
            ::MH_RemoveHook(m_ResizeBuffersTarget);
            ::MH_RemoveHook(m_PresentTarget);
            ResetTargets();
            m_Status.store(HookStatus::Failed, std::memory_order_release);
            return false;
        }

        m_Status.store(HookStatus::Installed, std::memory_order_release);
        TUTONES_LOG_INFO("hook", "D3D12 Present, ResizeBuffers, and ExecuteCommandLists hooks installed");
        TUTONES_LOG_DEBUG("hook", "Waiting for a validated primary render window and swap chain");
        return true;
    }

    void HookManager::Shutdown() noexcept
    {
        const auto current = m_Status.load(std::memory_order_acquire);
        if (current == HookStatus::Stopped || current == HookStatus::NotInitialized)
        {
            TUTONES_LOG_TRACE("hook", "Hook shutdown requested with no active hooks");
            return;
        }

        m_ShuttingDown.store(true, std::memory_order_release);
        m_Status.store(HookStatus::ShuttingDown, std::memory_order_release);
        TUTONES_LOG_INFO("hook", "D3D12 hook manager shutting down");

        TUTONES_LOG_DEBUG("hook", "Queueing hook disable operations");
        if (m_PresentTarget) ::MH_QueueDisableHook(m_PresentTarget);
        if (m_ResizeBuffersTarget) ::MH_QueueDisableHook(m_ResizeBuffersTarget);
        if (m_ExecuteCommandListsTarget) ::MH_QueueDisableHook(m_ExecuteCommandListsTarget);
        const auto disableStatus = ::MH_ApplyQueued();
        if (disableStatus != MH_OK)
            TUTONES_LOG_WARN("hook", MhFailure("Disable D3D12 hooks", disableStatus));
        else
            TUTONES_LOG_DEBUG("hook", "D3D12/DXGI detours disabled");

        Win32Hook::Get().SetMessageHandler(nullptr);
        Win32Hook::Get().Detach();

        if (ActiveCallbacks() != 0)
        {
            std::string message("Waiting for active hook callbacks to drain; count=");
            message += std::to_string(ActiveCallbacks());
            TUTONES_LOG_DEBUG("hook", message);
        }
        static_cast<void>(WaitForCallbacksToDrain());
        TUTONES_LOG_DEBUG("hook", "All active hook callbacks drained");

        if (m_PresentTarget) ::MH_RemoveHook(m_PresentTarget);
        if (m_ResizeBuffersTarget) ::MH_RemoveHook(m_ResizeBuffersTarget);
        if (m_ExecuteCommandListsTarget) ::MH_RemoveHook(m_ExecuteCommandListsTarget);
        TUTONES_LOG_DEBUG("hook", "MinHook trampolines removed");

        if (auto* queue = m_CommandQueue.exchange(nullptr, std::memory_order_acq_rel))
        {
            queue->Release();
            TUTONES_LOG_DEBUG("hook.queue", "Released captured live command queue");
        }
        if (auto* swapChain = m_PrimarySwapChain.exchange(nullptr, std::memory_order_acq_rel))
        {
            swapChain->Release();
            TUTONES_LOG_DEBUG("hook.dxgi", "Released pinned primary swap chain");
        }

        ResetTargets();

        if (m_MinHookInitialized)
        {
            const auto status = ::MH_Uninitialize();
            if (status != MH_OK && status != MH_ERROR_NOT_INITIALIZED)
                TUTONES_LOG_WARN("hook", MhFailure("MH_Uninitialize", status));
            else
                TUTONES_LOG_DEBUG("hook", "MinHook backend uninitialized");
            m_MinHookInitialized = false;
        }

        m_Status.store(HookStatus::Stopped, std::memory_order_release);
        TUTONES_LOG_INFO("hook", "D3D12 and Win32 hooks stopped");
    }

    HookStatus HookManager::Status() const noexcept { return m_Status.load(std::memory_order_acquire); }
    bool HookManager::IsInstalled() const noexcept { return Status() == HookStatus::Installed; }
    bool HookManager::IsShuttingDown() const noexcept { return m_ShuttingDown.load(std::memory_order_acquire); }
    PresentFn HookManager::OriginalPresent() const noexcept { return m_OriginalPresent; }
    ResizeBuffersFn HookManager::OriginalResizeBuffers() const noexcept { return m_OriginalResizeBuffers; }
    ExecuteCommandListsFn HookManager::OriginalExecuteCommandLists() const noexcept { return m_OriginalExecuteCommandLists; }

    bool HookManager::TrySetPrimarySwapChain(IDXGISwapChain* swapChain) noexcept
    {
        if (!swapChain || IsShuttingDown())
            return false;
        if (IsPrimarySwapChain(swapChain))
            return true;

        swapChain->AddRef();
        IDXGISwapChain* expected = nullptr;
        if (m_PrimarySwapChain.compare_exchange_strong(expected, swapChain, std::memory_order_release, std::memory_order_relaxed))
        {
            TUTONES_LOG_INFO("hook.dxgi", "Pinned primary DXGI render swap chain");
            return true;
        }

        swapChain->Release();
        if (expected != swapChain)
        {
            static std::atomic<bool> loggedSecondarySwapChain{false};
            if (!loggedSecondarySwapChain.exchange(true, std::memory_order_acq_rel))
                TUTONES_LOG_DEBUG("hook.dxgi", "Ignored secondary DXGI swap chain because primary target is already pinned");
        }
        return expected == swapChain;
    }

    bool HookManager::IsPrimarySwapChain(IDXGISwapChain* swapChain) const noexcept
    {
        return swapChain && m_PrimarySwapChain.load(std::memory_order_acquire) == swapChain;
    }

    IDXGISwapChain* HookManager::PrimarySwapChain() const noexcept
    {
        return m_PrimarySwapChain.load(std::memory_order_acquire);
    }

    void HookManager::SetCommandQueue(ID3D12CommandQueue* queue) noexcept
    {
        if (!queue || !PrimarySwapChain() || IsShuttingDown())
            return;

        queue->AddRef();
        ID3D12CommandQueue* expected = nullptr;
        if (!m_CommandQueue.compare_exchange_strong(expected, queue, std::memory_order_release, std::memory_order_relaxed))
        {
            queue->Release();
            if (expected != queue)
                TUTONES_LOG_TRACE("hook.queue", "Ignored additional DIRECT command queue after live queue was captured");
        }
    }

    ID3D12CommandQueue* HookManager::CommandQueue() const noexcept
    {
        return m_CommandQueue.load(std::memory_order_acquire);
    }

    bool HookManager::TryEnterCallback() noexcept
    {
        if (IsShuttingDown())
            return false;

        m_ActiveCallbacks.fetch_add(1, std::memory_order_acq_rel);
        if (IsShuttingDown())
        {
            m_ActiveCallbacks.fetch_sub(1, std::memory_order_acq_rel);
            return false;
        }
        return true;
    }

    void HookManager::LeaveCallback() noexcept
    {
        m_ActiveCallbacks.fetch_sub(1, std::memory_order_acq_rel);
    }

    std::uint32_t HookManager::ActiveCallbacks() const noexcept
    {
        return m_ActiveCallbacks.load(std::memory_order_acquire);
    }

    bool HookManager::WaitForCallbacksToDrain() noexcept
    {
        while (ActiveCallbacks() != 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return true;
    }

    void HookManager::ResetTargets() noexcept
    {
        m_PresentTarget = nullptr;
        m_ResizeBuffersTarget = nullptr;
        m_ExecuteCommandListsTarget = nullptr;
        m_OriginalPresent = nullptr;
        m_OriginalResizeBuffers = nullptr;
        m_OriginalExecuteCommandLists = nullptr;
        TUTONES_LOG_TRACE("hook", "Hook target and original-function state reset");
    }
}
