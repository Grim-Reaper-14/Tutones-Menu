#include "Renderer.hpp"

#include "../core/logging/Logger.hpp"
#include "../hooking/Win32Hook.hpp"
#include "../ui/Input.hpp"
#include "../ui/TutonesMenu.hpp"

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

#include <algorithm>
#include <string>

namespace Tutones::Render
{
    namespace
    {
        constexpr std::uint32_t SrvHeapSize = 64;
    }

    Renderer& Renderer::Get() noexcept
    {
        static Renderer instance;
        return instance;
    }

    bool Renderer::Initialize() noexcept
    {
        if (m_Status == RendererStatus::Initialized)
        {
            TUTONES_LOG_TRACE("render", "Renderer initialize requested while already initialized");
            return true;
        }
        if (m_Status == RendererStatus::WaitingForDevice)
        {
            TUTONES_LOG_TRACE("render", "Renderer is already waiting for D3D12 device state");
            return true;
        }

        m_Status = RendererStatus::WaitingForDevice;
        TUTONES_LOG_INFO("render", "Renderer initialized and waiting for D3D12 device, queue, and primary swap chain");
        return true;
    }

    bool Renderer::InitializeD3D12(IDXGISwapChain3* swapChain, ID3D12CommandQueue* commandQueue) noexcept
    {
        if (!swapChain || !commandQueue)
        {
            m_Status = RendererStatus::Failed;
            TUTONES_LOG_ERROR("render", "D3D12 renderer received invalid device objects");
            return false;
        }

        if (m_Status == RendererStatus::Initialized && m_SwapChain == swapChain && m_CommandQueue == commandQueue)
            return true;

        TUTONES_LOG_INFO("render", "Starting D3D12 frame-resource initialization");

        ID3D12Device* device{};
        if (FAILED(swapChain->GetDevice(IID_PPV_ARGS(&device))) || !device)
        {
            m_Status = RendererStatus::Failed;
            TUTONES_LOG_ERROR("render", "Failed to acquire D3D12 device from swap chain");
            return false;
        }

        DXGI_SWAP_CHAIN_DESC desc{};
        if (FAILED(swapChain->GetDesc(&desc)))
        {
            device->Release();
            m_Status = RendererStatus::Failed;
            TUTONES_LOG_ERROR("render", "Failed to query D3D12 swap-chain description");
            return false;
        }

        if (desc.BufferCount == 0 || !desc.OutputWindow)
        {
            device->Release();
            m_Status = RendererStatus::Failed;
            TUTONES_LOG_ERROR("render", "Primary swap chain has invalid buffer count or output window");
            return false;
        }

        ShutdownImGui();
        ReleaseFrameResources();

        if (m_SwapChain) m_SwapChain->Release();
        if (m_CommandQueue) m_CommandQueue->Release();
        if (m_Device) m_Device->Release();

        m_Device = device;
        commandQueue->AddRef();
        m_CommandQueue = commandQueue;
        swapChain->AddRef();
        m_SwapChain = swapChain;
        m_Window = desc.OutputWindow;
        m_Width = desc.BufferDesc.Width;
        m_Height = desc.BufferDesc.Height;
        m_RtvFormat = desc.BufferDesc.Format == DXGI_FORMAT_UNKNOWN
            ? DXGI_FORMAT_R8G8B8A8_UNORM
            : desc.BufferDesc.Format;

        if (FAILED(m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence))))
        {
            TUTONES_LOG_ERROR("render", "Failed to create D3D12 renderer fence");
            m_Status = RendererStatus::Failed;
            return false;
        }

        m_FenceEvent = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!m_FenceEvent)
        {
            TUTONES_LOG_ERROR("render", "Failed to create D3D12 fence event");
            m_Status = RendererStatus::Failed;
            return false;
        }

        if (!CreateFrameResources())
        {
            TUTONES_LOG_ERROR("render", "Failed to create D3D12 frame resources");
            m_Status = RendererStatus::Failed;
            return false;
        }

        if (!InitializeImGui(m_Window))
        {
            TUTONES_LOG_ERROR("render", "Failed to initialize Dear ImGui D3D12/Win32 backends");
            m_Status = RendererStatus::Failed;
            return false;
        }

        m_Status = RendererStatus::Initialized;
        std::string message("D3D12 renderer ready; buffers=");
        message += std::to_string(m_Frames.size());
        message += ", size=";
        message += std::to_string(m_Width);
        message += 'x';
        message += std::to_string(m_Height);
        TUTONES_LOG_INFO("render", message);
        TUTONES_LOG_INFO("render", "First visible Tutones menu frame is ready for GTA testing");
        return true;
    }

    bool Renderer::CreateFrameResources() noexcept
    {
        if (!m_Device || !m_SwapChain)
            return false;

        DXGI_SWAP_CHAIN_DESC desc{};
        if (FAILED(m_SwapChain->GetDesc(&desc)) || desc.BufferCount == 0)
        {
            TUTONES_LOG_ERROR("render", "Could not query swap-chain buffers while creating frame resources");
            return false;
        }

        D3D12_DESCRIPTOR_HEAP_DESC rtvDesc{};
        rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvDesc.NumDescriptors = desc.BufferCount;
        rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        if (FAILED(m_Device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&m_RtvHeap))))
        {
            TUTONES_LOG_ERROR("render", "Failed to create RTV descriptor heap");
            return false;
        }

        D3D12_DESCRIPTOR_HEAP_DESC srvDesc{};
        srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvDesc.NumDescriptors = SrvHeapSize;
        srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (FAILED(m_Device->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&m_SrvHeap))))
        {
            TUTONES_LOG_ERROR("render", "Failed to create shader-visible SRV descriptor heap");
            return false;
        }

        m_RtvIncrement = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        m_SrvIncrement = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        m_FreeSrvIndices.clear();
        m_FreeSrvIndices.reserve(SrvHeapSize);
        for (std::uint32_t i = SrvHeapSize; i > 0; --i)
            m_FreeSrvIndices.push_back(i - 1);

        m_Frames.clear();
        m_Frames.resize(desc.BufferCount);

        auto rtv = m_RtvHeap->GetCPUDescriptorHandleForHeapStart();
        for (std::uint32_t i = 0; i < desc.BufferCount; ++i)
        {
            auto& frame = m_Frames[i];
            if (FAILED(m_Device->CreateCommandAllocator(
                    D3D12_COMMAND_LIST_TYPE_DIRECT,
                    IID_PPV_ARGS(&frame.commandAllocator))))
            {
                TUTONES_LOG_ERROR("render", "Failed to create D3D12 frame command allocator");
                return false;
            }

            if (FAILED(m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&frame.backBuffer))))
            {
                TUTONES_LOG_ERROR("render", "Failed to acquire D3D12 swap-chain back buffer");
                return false;
            }

            frame.rtv = rtv;
            m_Device->CreateRenderTargetView(frame.backBuffer, nullptr, frame.rtv);
            rtv.ptr += m_RtvIncrement;
        }

        if (FAILED(m_Device->CreateCommandList(
                0,
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                m_Frames.front().commandAllocator,
                nullptr,
                IID_PPV_ARGS(&m_CommandList))))
        {
            TUTONES_LOG_ERROR("render", "Failed to create D3D12 graphics command list");
            return false;
        }

        if (FAILED(m_CommandList->Close()))
        {
            TUTONES_LOG_ERROR("render", "Failed to close newly created D3D12 graphics command list");
            return false;
        }

        TUTONES_LOG_INFO("render", "D3D12 RTV/SRV heaps, frame allocators, and back buffers created");
        return true;
    }

    void Renderer::ReleaseFrameResources() noexcept
    {
        if (m_CommandList)
        {
            m_CommandList->Release();
            m_CommandList = nullptr;
        }

        for (auto& frame : m_Frames)
        {
            if (frame.backBuffer)
            {
                frame.backBuffer->Release();
                frame.backBuffer = nullptr;
            }
            if (frame.commandAllocator)
            {
                frame.commandAllocator->Release();
                frame.commandAllocator = nullptr;
            }
            frame.fenceValue = 0;
        }
        m_Frames.clear();

        if (m_SrvHeap)
        {
            m_SrvHeap->Release();
            m_SrvHeap = nullptr;
        }
        if (m_RtvHeap)
        {
            m_RtvHeap->Release();
            m_RtvHeap = nullptr;
        }

        m_FreeSrvIndices.clear();
        m_RtvIncrement = 0;
        m_SrvIncrement = 0;
        m_FrameBegun = false;
    }

    bool Renderer::InitializeImGui(HWND window) noexcept
    {
        if (!window || !m_Device || !m_CommandQueue || !m_SrvHeap || m_Frames.empty())
            return false;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();
        auto& style = ImGui::GetStyle();
        style.WindowRounding = 5.0f;
        style.ChildRounding = 4.0f;
        style.FrameRounding = 3.0f;
        style.Colors[ImGuiCol_CheckMark] = ImVec4(0.58f, 0.75f, 0.26f, 1.0f);
        style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.58f, 0.75f, 0.26f, 1.0f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.25f, 0.32f, 0.14f, 1.0f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.34f, 0.43f, 0.18f, 1.0f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.42f, 0.54f, 0.21f, 1.0f);

        if (!ImGui_ImplWin32_Init(window))
        {
            TUTONES_LOG_ERROR("render.imgui", "ImGui Win32 backend initialization failed");
            ImGui::DestroyContext();
            return false;
        }

        ImGui_ImplDX12_InitInfo info{};
        info.Device = m_Device;
        info.CommandQueue = m_CommandQueue;
        info.NumFramesInFlight = static_cast<int>(m_Frames.size());
        info.RTVFormat = m_RtvFormat;
        info.DSVFormat = DXGI_FORMAT_UNKNOWN;
        info.UserData = this;
        info.SrvDescriptorHeap = m_SrvHeap;
        info.SrvDescriptorAllocFn = &Renderer::AllocateSrvDescriptor;
        info.SrvDescriptorFreeFn = &Renderer::FreeSrvDescriptor;

        if (!ImGui_ImplDX12_Init(&info))
        {
            TUTONES_LOG_ERROR("render.imgui", "ImGui DX12 backend initialization failed");
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            return false;
        }

        m_ImGuiInitialized = true;
        UI::TutonesMenu::Get().Reset();
        TUTONES_LOG_INFO("render.imgui", "Dear ImGui Win32 and DX12 backends initialized");
        return true;
    }

    void Renderer::ShutdownImGui() noexcept
    {
        if (!m_ImGuiInitialized)
            return;

        TUTONES_LOG_INFO("render.imgui", "Shutting down Dear ImGui backends");
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        m_ImGuiInitialized = false;
        m_FrameBegun = false;
        TUTONES_LOG_INFO("render.imgui", "Dear ImGui shutdown complete");
    }

    bool Renderer::WaitForFrame(D3D12::FrameContext& frame) noexcept
    {
        if (!m_Fence || !m_FenceEvent || frame.fenceValue == 0)
            return true;

        if (m_Fence->GetCompletedValue() >= frame.fenceValue)
            return true;

        if (FAILED(m_Fence->SetEventOnCompletion(frame.fenceValue, m_FenceEvent)))
        {
            TUTONES_LOG_ERROR("render", "Failed to arm D3D12 frame fence event");
            return false;
        }

        ::WaitForSingleObject(m_FenceEvent, INFINITE);
        return true;
    }

    void Renderer::WaitForGpuIdle() noexcept
    {
        if (!m_CommandQueue || !m_Fence || !m_FenceEvent)
            return;

        const auto fenceValue = m_NextFenceValue++;
        if (FAILED(m_CommandQueue->Signal(m_Fence, fenceValue)))
        {
            TUTONES_LOG_ERROR("render", "Failed to signal D3D12 fence while waiting for GPU idle");
            return;
        }

        if (m_Fence->GetCompletedValue() < fenceValue)
        {
            if (SUCCEEDED(m_Fence->SetEventOnCompletion(fenceValue, m_FenceEvent)))
                ::WaitForSingleObject(m_FenceEvent, INFINITE);
            else
                TUTONES_LOG_ERROR("render", "Failed to wait for D3D12 GPU idle fence");
        }
    }

    void Renderer::BeginFrame() noexcept
    {
        if (m_Status != RendererStatus::Initialized || !m_ImGuiInitialized || m_FrameBegun)
            return;

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        UI::TutonesMenu::Get().Render();
        ImGui::Render();
        m_FrameBegun = true;
    }

    void Renderer::RenderFrame() noexcept
    {
        if (m_Status != RendererStatus::Initialized || !m_ImGuiInitialized || !m_FrameBegun || m_Frames.empty())
            return;

        m_FrameBegun = false;

        const auto index = m_SwapChain->GetCurrentBackBufferIndex();
        if (index >= m_Frames.size())
        {
            TUTONES_LOG_ERROR("render", "Swap-chain back-buffer index exceeded renderer frame count");
            return;
        }

        auto& frame = m_Frames[index];
        if (!WaitForFrame(frame))
            return;

        if (FAILED(frame.commandAllocator->Reset()))
        {
            TUTONES_LOG_ERROR("render", "Failed to reset D3D12 frame command allocator");
            return;
        }
        if (FAILED(m_CommandList->Reset(frame.commandAllocator, nullptr)))
        {
            TUTONES_LOG_ERROR("render", "Failed to reset D3D12 graphics command list");
            return;
        }

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = frame.backBuffer;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        m_CommandList->ResourceBarrier(1, &barrier);

        m_CommandList->OMSetRenderTargets(1, &frame.rtv, FALSE, nullptr);
        ID3D12DescriptorHeap* heaps[]{m_SrvHeap};
        m_CommandList->SetDescriptorHeaps(1, heaps);
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_CommandList);

        std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
        m_CommandList->ResourceBarrier(1, &barrier);

        if (FAILED(m_CommandList->Close()))
        {
            TUTONES_LOG_ERROR("render", "Failed to close D3D12 overlay command list");
            return;
        }

        ID3D12CommandList* lists[]{m_CommandList};
        m_CommandQueue->ExecuteCommandLists(1, lists);

        const auto fenceValue = m_NextFenceValue++;
        if (FAILED(m_CommandQueue->Signal(m_Fence, fenceValue)))
        {
            TUTONES_LOG_ERROR("render", "Failed to signal D3D12 overlay frame fence");
            return;
        }
        frame.fenceValue = fenceValue;
    }

    void Renderer::BeforeResize() noexcept
    {
        if (m_Status != RendererStatus::Initialized)
            return;

        TUTONES_LOG_INFO("render.resize", "Preparing renderer resources for ResizeBuffers");
        WaitForGpuIdle();
        if (m_ImGuiInitialized)
            ImGui_ImplDX12_InvalidateDeviceObjects();
        ReleaseFrameResources();
        TUTONES_LOG_DEBUG("render.resize", "Released back buffers and frame resources before ResizeBuffers");
    }

    void Renderer::AfterResize(std::uint32_t width, std::uint32_t height) noexcept
    {
        if (m_Status != RendererStatus::Initialized)
            return;

        m_Width = width;
        m_Height = height;
        TUTONES_LOG_INFO("render.resize", "Rebuilding renderer resources after ResizeBuffers");

        if (!CreateFrameResources())
        {
            m_Status = RendererStatus::Failed;
            TUTONES_LOG_ERROR("render.resize", "Failed to rebuild D3D12 frame resources after resize");
            return;
        }

        if (m_ImGuiInitialized && !ImGui_ImplDX12_CreateDeviceObjects())
        {
            m_Status = RendererStatus::Failed;
            TUTONES_LOG_ERROR("render.resize", "Failed to recreate ImGui DX12 device objects after resize");
            return;
        }

        std::string message("Renderer resize rebuild complete; size=");
        message += std::to_string(width);
        message += 'x';
        message += std::to_string(height);
        TUTONES_LOG_INFO("render.resize", message);
    }

    void Renderer::Shutdown() noexcept
    {
        if (m_Status == RendererStatus::Stopped || m_Status == RendererStatus::NotInitialized)
        {
            TUTONES_LOG_TRACE("render", "Renderer shutdown requested with no active renderer");
            return;
        }

        m_Status = RendererStatus::ShuttingDown;
        TUTONES_LOG_INFO("render", "Renderer shutting down");

        WaitForGpuIdle();
        ShutdownImGui();
        ReleaseFrameResources();

        if (m_FenceEvent)
        {
            ::CloseHandle(m_FenceEvent);
            m_FenceEvent = nullptr;
        }
        if (m_Fence)
        {
            m_Fence->Release();
            m_Fence = nullptr;
        }
        if (m_SwapChain)
        {
            m_SwapChain->Release();
            m_SwapChain = nullptr;
        }
        if (m_CommandQueue)
        {
            m_CommandQueue->Release();
            m_CommandQueue = nullptr;
        }
        if (m_Device)
        {
            m_Device->Release();
            m_Device = nullptr;
        }

        m_Window = nullptr;
        m_Width = 0;
        m_Height = 0;
        m_NextFenceValue = 1;
        m_Status = RendererStatus::Stopped;
        TUTONES_LOG_INFO("render", "Renderer stopped and D3D12/ImGui state cleared");
    }

    void Renderer::AllocateSrvDescriptor(
        ImGui_ImplDX12_InitInfo* info,
        D3D12_CPU_DESCRIPTOR_HANDLE* cpu,
        D3D12_GPU_DESCRIPTOR_HANDLE* gpu)
    {
        if (!info || !info->UserData || !cpu || !gpu)
            return;

        auto* renderer = static_cast<Renderer*>(info->UserData);
        std::scoped_lock lock(renderer->m_SrvMutex);
        if (renderer->m_FreeSrvIndices.empty() || !renderer->m_SrvHeap)
        {
            cpu->ptr = 0;
            gpu->ptr = 0;
            TUTONES_LOG_ERROR("render.imgui", "ImGui SRV descriptor heap exhausted");
            return;
        }

        const auto index = renderer->m_FreeSrvIndices.back();
        renderer->m_FreeSrvIndices.pop_back();

        *cpu = renderer->m_SrvHeap->GetCPUDescriptorHandleForHeapStart();
        *gpu = renderer->m_SrvHeap->GetGPUDescriptorHandleForHeapStart();
        cpu->ptr += static_cast<SIZE_T>(index) * renderer->m_SrvIncrement;
        gpu->ptr += static_cast<UINT64>(index) * renderer->m_SrvIncrement;
    }

    void Renderer::FreeSrvDescriptor(
        ImGui_ImplDX12_InitInfo* info,
        D3D12_CPU_DESCRIPTOR_HANDLE cpu,
        D3D12_GPU_DESCRIPTOR_HANDLE)
    {
        if (!info || !info->UserData || cpu.ptr == 0)
            return;

        auto* renderer = static_cast<Renderer*>(info->UserData);
        std::scoped_lock lock(renderer->m_SrvMutex);
        if (!renderer->m_SrvHeap || renderer->m_SrvIncrement == 0)
            return;

        const auto start = renderer->m_SrvHeap->GetCPUDescriptorHandleForHeapStart();
        if (cpu.ptr < start.ptr)
            return;

        const auto index = static_cast<std::uint32_t>((cpu.ptr - start.ptr) / renderer->m_SrvIncrement);
        if (index < SrvHeapSize)
            renderer->m_FreeSrvIndices.push_back(index);
    }

    RendererStatus Renderer::Status() const noexcept { return m_Status; }
    bool Renderer::IsInitialized() const noexcept { return m_Status == RendererStatus::Initialized; }
    ID3D12Device* Renderer::Device() const noexcept { return m_Device; }
    ID3D12CommandQueue* Renderer::CommandQueue() const noexcept { return m_CommandQueue; }
    IDXGISwapChain3* Renderer::SwapChain() const noexcept { return m_SwapChain; }
}
