#include "Renderer.hpp"

#include "../core/logging/Logger.hpp"

#include <d3d12.h>
#include <dxgi1_4.h>

namespace Tutones::Render
{
    Renderer& Renderer::Get() noexcept
    {
        static Renderer instance;
        return instance;
    }

    bool Renderer::Initialize() noexcept
    {
        if (m_Status == RendererStatus::Initialized || m_Status == RendererStatus::WaitingForDevice)
            return true;

        m_Status = RendererStatus::WaitingForDevice;
        TUTONES_LOG_INFO("render", "Renderer initialized and waiting for D3D12 device");
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

        if (m_Device) m_Device->Release();
        if (m_CommandQueue) m_CommandQueue->Release();
        if (m_SwapChain) m_SwapChain->Release();

        m_Device = device;
        commandQueue->AddRef();
        m_CommandQueue = commandQueue;
        swapChain->AddRef();
        m_SwapChain = swapChain;
        m_Width = desc.BufferDesc.Width;
        m_Height = desc.BufferDesc.Height;
        m_Status = RendererStatus::Initialized;

        TUTONES_LOG_INFO("render", "D3D12 renderer device, queue, and swap chain captured");
        return true;
    }

    void Renderer::BeginFrame() noexcept
    {
        if (m_Status != RendererStatus::Initialized)
            return;

        // ImGui/D3D12 frame resources are connected in the next renderer stage.
    }

    void Renderer::RenderFrame() noexcept
    {
        if (m_Status != RendererStatus::Initialized)
            return;

        // Hook callbacks reach this point only after a live DIRECT queue and
        // game swap chain have both been captured successfully.
    }

    void Renderer::OnResize(std::uint32_t width, std::uint32_t height) noexcept
    {
        m_Width = width;
        m_Height = height;
        if (m_Status == RendererStatus::Initialized)
            TUTONES_LOG_DEBUG("render", "D3D12 swap-chain resize received");
    }

    void Renderer::Shutdown() noexcept
    {
        if (m_Status == RendererStatus::Stopped || m_Status == RendererStatus::NotInitialized)
            return;

        m_Status = RendererStatus::ShuttingDown;
        TUTONES_LOG_INFO("render", "Renderer shutting down");

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

        m_Width = 0;
        m_Height = 0;
        m_Status = RendererStatus::Stopped;
    }

    RendererStatus Renderer::Status() const noexcept
    {
        return m_Status;
    }

    bool Renderer::IsInitialized() const noexcept
    {
        return m_Status == RendererStatus::Initialized;
    }

    ID3D12Device* Renderer::Device() const noexcept
    {
        return m_Device;
    }

    ID3D12CommandQueue* Renderer::CommandQueue() const noexcept
    {
        return m_CommandQueue;
    }

    IDXGISwapChain3* Renderer::SwapChain() const noexcept
    {
        return m_SwapChain;
    }
}
