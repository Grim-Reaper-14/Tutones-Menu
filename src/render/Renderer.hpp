#pragma once

#include "RendererStatus.hpp"

#include <cstdint>

struct ID3D12Device;
struct ID3D12DescriptorHeap;
struct ID3D12CommandQueue;
struct IDXGISwapChain3;

namespace Tutones::Render
{
    class Renderer final
    {
    public:
        static Renderer& Get() noexcept;

        bool Initialize() noexcept;
        bool InitializeD3D12(IDXGISwapChain3* swapChain, ID3D12CommandQueue* commandQueue) noexcept;
        void BeginFrame() noexcept;
        void RenderFrame() noexcept;
        void OnResize(std::uint32_t width, std::uint32_t height) noexcept;
        void Shutdown() noexcept;

        [[nodiscard]] RendererStatus Status() const noexcept;
        [[nodiscard]] bool IsInitialized() const noexcept;
        [[nodiscard]] ID3D12Device* Device() const noexcept;
        [[nodiscard]] ID3D12CommandQueue* CommandQueue() const noexcept;
        [[nodiscard]] IDXGISwapChain3* SwapChain() const noexcept;

    private:
        Renderer() = default;
        ~Renderer() = default;
        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        RendererStatus m_Status{RendererStatus::NotInitialized};
        ID3D12Device* m_Device{};
        ID3D12CommandQueue* m_CommandQueue{};
        IDXGISwapChain3* m_SwapChain{};
        std::uint32_t m_Width{};
        std::uint32_t m_Height{};
    };
}
