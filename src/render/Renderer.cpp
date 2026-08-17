#include "Renderer.hpp"

#include "../core/logging/Logger.hpp"

#include <d3d12.h>
#include <dxgi1_4.h>

#include <string>

namespace Tutones::Render
{
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
        {
            TUTONES_LOG_TRACE("render", "D3D12 renderer already owns the current swap chain and command queue");
            return true;
        }

        TUTONES_LOG_DEBUG("render", "Acquiring D3D12 device from primary swap chain");
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

        TUTONES_LOG_DEBUG("render", "Releasing any previous renderer-owned D3D12 objects");
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

        std::string message("D3D12 renderer captured device, queue, and primary swap chain; size=");
        message += std::to_string(m_Width);
        message += 'x';
        message += std::to_string(m_Height);
        TUTONES_LOG_INFO("render", message);
        TUTONES_LOG_DEBUG("render", "Renderer is ready for frame-resource and ImGui backend initialization");
        return true;
    }

    void Renderer::BeginFrame() noexcept
    {
        if (m_Status != RendererStatus::Initialized)
            return;

        // Intentionally no per-frame log here. This path executes every Present.
        // ImGui/D3D12 frame resources are connected in the next renderer stage.
    }

    void Renderer::RenderFrame() noexcept
    {
        if (m_Status != RendererStatus::Initialized)
            return;

        // Intentionally no per-frame log here. Hook callbacks reach this point
        // only after the live DIRECT queue and primary game swap chain are ready.
    }

    void Renderer::OnResize(std::uint32_t width, std::uint32_t height) noexcept
    {
        const auto oldWidth = m_Width;
        const auto oldHeight = m_Height;
        m_Width = width;
        m_Height = height;

        if (m_Status != RendererStatus::Initialized)
        {
            TUTONES_LOG_TRACE("render", "Resize received before renderer initialization completed");
            return;
        }

        std::string message("D3D12 swap-chain resize recorded: ");
        message += std::to_string(oldWidth);
        message += 'x';
        message += std::to_string(oldHeight);
        message += " -> ";
        message += std::to_string(width);
        message += 'x';
        message += std::to_string(height);
        TUTONES_LOG_DEBUG("render", message);
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

        if (m_SwapChain)
        {
            m_SwapChain->Release();
            m_SwapChain = nullptr;
            TUTONES_LOG_DEBUG("render", "Released renderer-owned swap chain reference");
        }
        if (m_CommandQueue)
        {
            m_CommandQueue->Release();
            m_CommandQueue = nullptr;
            TUTONES_LOG_DEBUG("render", "Released renderer-owned command queue reference");
        }
        if (m_Device)
        {
            m_Device->Release();
            m_Device = nullptr;
            TUTONES_LOG_DEBUG("render", "Released renderer-owned D3D12 device reference");
        }

        m_Width = 0;
        m_Height = 0;
        m_Status = RendererStatus::Stopped;
        TUTONES_LOG_INFO("render", "Renderer stopped and D3D12 object state cleared");
    }

    RendererStatus Renderer::Status() const noexcept { return m_Status; }
    bool Renderer::IsInitialized() const noexcept { return m_Status == RendererStatus::Initialized; }
    ID3D12Device* Renderer::Device() const noexcept { return m_Device; }
    ID3D12CommandQueue* Renderer::CommandQueue() const noexcept { return m_CommandQueue; }
    IDXGISwapChain3* Renderer::SwapChain() const noexcept { return m_SwapChain; }
}
