#include "Renderer.hpp"

#include "../core/logging/Logger.hpp"

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

        m_SwapChain = swapChain;
        m_CommandQueue = commandQueue;
        m_Status = RendererStatus::Initialized;
        TUTONES_LOG_INFO("render", "D3D12 renderer device state captured");
        return true;
    }

    void Renderer::BeginFrame() noexcept
    {
        if (m_Status != RendererStatus::Initialized)
            return;
    }

    void Renderer::RenderFrame() noexcept
    {
        if (m_Status != RendererStatus::Initialized)
            return;
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
        m_Device = nullptr;
        m_CommandQueue = nullptr;
        m_SwapChain = nullptr;
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
