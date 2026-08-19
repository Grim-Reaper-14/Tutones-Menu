#include "Renderer.hpp"

#include "../core/logging/Logger.hpp"
#include "../hooking/Win32Hook.hpp"
#include "../ui/Input.hpp"
#include "../ui/TutonesMenu.hpp"
#include "../ui/V11ResourceIds.h"

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace Tutones::Render
{
    namespace
    {
        constexpr std::uint32_t SrvHeapSize = 64;
        constexpr std::uint32_t InvalidSrvIndex = 0xFFFFFFFFu;
        int g_RendererModuleAnchor{};

        HMODULE CurrentModule() noexcept
        {
            HMODULE module{};
            if (!::GetModuleHandleExW(
                    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCWSTR>(&g_RendererModuleAnchor),
                    &module))
                return nullptr;
            return module;
        }

        bool DecodeBannerResource(
            std::vector<std::uint8_t>& pixels,
            std::uint32_t& width,
            std::uint32_t& height) noexcept
        {
            width = 0;
            height = 0;
            pixels.clear();

            const HMODULE module = CurrentModule();
            if (!module)
            {
                TUTONES_LOG_WARN("render.banner", "Could not resolve Tutones module for V11 banner resource");
                return false;
            }

            const HRSRC resource = ::FindResourceW(
                module,
                MAKEINTRESOURCEW(IDR_V11_BANNER_COMPOSITE),
                MAKEINTRESOURCEW(10));
            if (!resource)
            {
                TUTONES_LOG_WARN("render.banner", "V11 banner RCDATA resource was not found");
                return false;
            }

            const HGLOBAL loaded = ::LoadResource(module, resource);
            const DWORD resourceSize = ::SizeofResource(module, resource);
            const void* resourceData = loaded ? ::LockResource(loaded) : nullptr;
            if (!resourceData || resourceSize == 0)
            {
                TUTONES_LOG_WARN("render.banner", "V11 banner resource could not be loaded");
                return false;
            }

            const HRESULT comResult = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            if (FAILED(comResult) && comResult != RPC_E_CHANGED_MODE)
            {
                TUTONES_LOG_WARN("render.banner", "COM initialization failed while decoding V11 banner");
                return false;
            }
            const bool uninitializeCom = SUCCEEDED(comResult);

            using Microsoft::WRL::ComPtr;
            ComPtr<IWICImagingFactory> factory;
            ComPtr<IWICStream> stream;
            ComPtr<IWICBitmapDecoder> decoder;
            ComPtr<IWICBitmapFrameDecode> frame;
            ComPtr<IWICFormatConverter> converter;

            HRESULT hr = ::CoCreateInstance(
                CLSID_WICImagingFactory,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(factory.GetAddressOf()));
            if (SUCCEEDED(hr))
                hr = factory->CreateStream(stream.GetAddressOf());
            if (SUCCEEDED(hr))
                hr = stream->InitializeFromMemory(
                    const_cast<BYTE*>(static_cast<const BYTE*>(resourceData)),
                    resourceSize);
            if (SUCCEEDED(hr))
                hr = factory->CreateDecoderFromStream(
                    stream.Get(),
                    nullptr,
                    WICDecodeMetadataCacheOnLoad,
                    decoder.GetAddressOf());
            if (SUCCEEDED(hr))
                hr = decoder->GetFrame(0, frame.GetAddressOf());
            if (SUCCEEDED(hr))
                hr = factory->CreateFormatConverter(converter.GetAddressOf());
            if (SUCCEEDED(hr))
                hr = converter->Initialize(
                    frame.Get(),
                    GUID_WICPixelFormat32bppRGBA,
                    WICBitmapDitherTypeNone,
                    nullptr,
                    0.0,
                    WICBitmapPaletteTypeCustom);

            UINT decodedWidth{};
            UINT decodedHeight{};
            if (SUCCEEDED(hr))
                hr = converter->GetSize(&decodedWidth, &decodedHeight);

            if (SUCCEEDED(hr) && decodedWidth > 0 && decodedHeight > 0)
            {
                const std::size_t stride = static_cast<std::size_t>(decodedWidth) * 4u;
                const std::size_t total = stride * static_cast<std::size_t>(decodedHeight);
                if (total <= static_cast<std::size_t>(UINT32_MAX))
                {
                    pixels.resize(total);
                    hr = converter->CopyPixels(
                        nullptr,
                        static_cast<UINT>(stride),
                        static_cast<UINT>(total),
                        pixels.data());
                }
                else
                {
                    hr = E_OUTOFMEMORY;
                }
            }

            if (uninitializeCom)
                ::CoUninitialize();

            if (FAILED(hr) || pixels.empty())
            {
                pixels.clear();
                TUTONES_LOG_WARN("render.banner", "WIC failed to decode the embedded V11 banner artwork");
                return false;
            }

            width = decodedWidth;
            height = decodedHeight;
            return true;
        }
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
        ReleaseV11BannerTexture();

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
        if (!CreateV11BannerTexture())
            TUTONES_LOG_WARN("render.banner", "V11 banner texture unavailable; vector fallback will be used");
        UI::TutonesMenu::Get().Reset();
        TUTONES_LOG_INFO("render.imgui", "Dear ImGui Win32 and DX12 backends initialized");
        return true;
    }

    void Renderer::ShutdownImGui() noexcept
    {
        if (!m_ImGuiInitialized)
            return;

        TUTONES_LOG_INFO("render.imgui", "Shutting down Dear ImGui backends");
        ReleaseV11BannerTexture();
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        m_ImGuiInitialized = false;
        m_FrameBegun = false;
        TUTONES_LOG_INFO("render.imgui", "Dear ImGui shutdown complete");
    }

    bool Renderer::CreateV11BannerTexture() noexcept
    {
        ReleaseV11BannerTexture();
        if (!m_Device || !m_CommandQueue || !m_SrvHeap || !m_Fence || !m_FenceEvent || m_SrvIncrement == 0)
            return false;

        std::vector<std::uint8_t> pixels;
        std::uint32_t width{};
        std::uint32_t height{};
        if (!DecodeBannerResource(pixels, width, height))
            return false;

        using Microsoft::WRL::ComPtr;
        ComPtr<ID3D12Resource> texture;
        ComPtr<ID3D12Resource> upload;
        ComPtr<ID3D12CommandAllocator> allocator;
        ComPtr<ID3D12GraphicsCommandList> commandList;

        D3D12_HEAP_PROPERTIES defaultHeap{};
        defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC textureDesc{};
        textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        textureDesc.Width = width;
        textureDesc.Height = height;
        textureDesc.DepthOrArraySize = 1;
        textureDesc.MipLevels = 1;
        textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

        HRESULT hr = m_Device->CreateCommittedResource(
            &defaultHeap,
            D3D12_HEAP_FLAG_NONE,
            &textureDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(texture.GetAddressOf()));
        if (FAILED(hr))
            return false;

        const UINT uploadPitch = (width * 4u + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u)
            & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);
        const UINT64 uploadSize = static_cast<UINT64>(uploadPitch) * height;

        D3D12_HEAP_PROPERTIES uploadHeap{};
        uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC uploadDesc{};
        uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        uploadDesc.Width = uploadSize;
        uploadDesc.Height = 1;
        uploadDesc.DepthOrArraySize = 1;
        uploadDesc.MipLevels = 1;
        uploadDesc.SampleDesc.Count = 1;
        uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        hr = m_Device->CreateCommittedResource(
            &uploadHeap,
            D3D12_HEAP_FLAG_NONE,
            &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(upload.GetAddressOf()));
        if (FAILED(hr))
            return false;

        void* mapped{};
        D3D12_RANGE readRange{0, 0};
        hr = upload->Map(0, &readRange, &mapped);
        if (FAILED(hr) || !mapped)
            return false;

        const std::size_t sourcePitch = static_cast<std::size_t>(width) * 4u;
        for (std::uint32_t y = 0; y < height; ++y)
        {
            std::memcpy(
                static_cast<std::uint8_t*>(mapped) + static_cast<std::size_t>(y) * uploadPitch,
                pixels.data() + static_cast<std::size_t>(y) * sourcePitch,
                sourcePitch);
        }
        upload->Unmap(0, nullptr);

        hr = m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(allocator.GetAddressOf()));
        if (FAILED(hr))
            return false;
        hr = m_Device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            allocator.Get(),
            nullptr,
            IID_PPV_ARGS(commandList.GetAddressOf()));
        if (FAILED(hr))
            return false;

        D3D12_TEXTURE_COPY_LOCATION source{};
        source.pResource = upload.Get();
        source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        source.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        source.PlacedFootprint.Footprint.Width = width;
        source.PlacedFootprint.Footprint.Height = height;
        source.PlacedFootprint.Footprint.Depth = 1;
        source.PlacedFootprint.Footprint.RowPitch = uploadPitch;

        D3D12_TEXTURE_COPY_LOCATION destination{};
        destination.pResource = texture.Get();
        destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        destination.SubresourceIndex = 0;

        commandList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = texture.Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        commandList->ResourceBarrier(1, &barrier);

        hr = commandList->Close();
        if (FAILED(hr))
            return false;

        ID3D12CommandList* lists[]{commandList.Get()};
        m_CommandQueue->ExecuteCommandLists(1, lists);
        const auto fenceValue = m_NextFenceValue++;
        hr = m_CommandQueue->Signal(m_Fence, fenceValue);
        if (FAILED(hr))
            return false;
        if (m_Fence->GetCompletedValue() < fenceValue)
        {
            hr = m_Fence->SetEventOnCompletion(fenceValue, m_FenceEvent);
            if (FAILED(hr))
                return false;
            ::WaitForSingleObject(m_FenceEvent, INFINITE);
        }

        std::uint32_t descriptorIndex = InvalidSrvIndex;
        {
            std::scoped_lock lock(m_SrvMutex);
            if (m_FreeSrvIndices.empty())
            {
                TUTONES_LOG_WARN("render.banner", "No SRV descriptor was available for V11 banner texture");
                return false;
            }
            descriptorIndex = m_FreeSrvIndices.back();
            m_FreeSrvIndices.pop_back();
        }

        D3D12_CPU_DESCRIPTOR_HANDLE cpu = m_SrvHeap->GetCPUDescriptorHandleForHeapStart();
        D3D12_GPU_DESCRIPTOR_HANDLE gpu = m_SrvHeap->GetGPUDescriptorHandleForHeapStart();
        cpu.ptr += static_cast<SIZE_T>(descriptorIndex) * m_SrvIncrement;
        gpu.ptr += static_cast<UINT64>(descriptorIndex) * m_SrvIncrement;

        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv.Texture2D.MostDetailedMip = 0;
        srv.Texture2D.MipLevels = 1;
        m_Device->CreateShaderResourceView(texture.Get(), &srv, cpu);

        m_V11BannerTexture = texture.Detach();
        m_V11BannerCpu = cpu;
        m_V11BannerGpu = gpu;
        m_V11BannerSrvIndex = descriptorIndex;
        TUTONES_LOG_INFO("render.banner", "Embedded V11 header/description artwork uploaded to D3D12");
        return true;
    }

    void Renderer::ReleaseV11BannerTexture() noexcept
    {
        if (m_V11BannerTexture)
        {
            m_V11BannerTexture->Release();
            m_V11BannerTexture = nullptr;
        }

        if (m_V11BannerSrvIndex != InvalidSrvIndex)
        {
            std::scoped_lock lock(m_SrvMutex);
            if (m_SrvHeap && m_SrvIncrement != 0)
                m_FreeSrvIndices.push_back(m_V11BannerSrvIndex);
            m_V11BannerSrvIndex = InvalidSrvIndex;
        }
        m_V11BannerCpu.ptr = 0;
        m_V11BannerGpu.ptr = 0;
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
        ReleaseV11BannerTexture();
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

        if (m_ImGuiInitialized && !CreateV11BannerTexture())
            TUTONES_LOG_WARN("render.banner", "V11 banner texture was not recreated after resize");

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
    std::uint64_t Renderer::V11BannerTextureId() const noexcept
    {
        return m_V11BannerTexture ? m_V11BannerGpu.ptr : 0;
    }
}
