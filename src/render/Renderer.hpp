#pragma once

#include "RendererStatus.hpp"
#include "d3d12/FrameContext.hpp"

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include <cstdint>
#include <mutex>
#include <vector>

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
        void BeforeResize() noexcept;
        void AfterResize(std::uint32_t width, std::uint32_t height) noexcept;
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

        bool CreateFrameResources() noexcept;
        void ReleaseFrameResources() noexcept;
        bool InitializeImGui(HWND window) noexcept;
        void ShutdownImGui() noexcept;
        bool WaitForFrame(D3D12::FrameContext& frame) noexcept;
        void WaitForGpuIdle() noexcept;

        static void AllocateSrvDescriptor(
            struct ImGui_ImplDX12_InitInfo* info,
            D3D12_CPU_DESCRIPTOR_HANDLE* cpu,
            D3D12_GPU_DESCRIPTOR_HANDLE* gpu);
        static void FreeSrvDescriptor(
            struct ImGui_ImplDX12_InitInfo* info,
            D3D12_CPU_DESCRIPTOR_HANDLE cpu,
            D3D12_GPU_DESCRIPTOR_HANDLE gpu);

        RendererStatus m_Status{RendererStatus::NotInitialized};
        ID3D12Device* m_Device{};
        ID3D12CommandQueue* m_CommandQueue{};
        IDXGISwapChain3* m_SwapChain{};
        ID3D12DescriptorHeap* m_RtvHeap{};
        ID3D12DescriptorHeap* m_SrvHeap{};
        ID3D12GraphicsCommandList* m_CommandList{};
        ID3D12Fence* m_Fence{};
        HANDLE m_FenceEvent{};
        HWND m_Window{};

        std::vector<D3D12::FrameContext> m_Frames;
        std::vector<std::uint32_t> m_FreeSrvIndices;
        std::mutex m_SrvMutex;

        std::uint64_t m_NextFenceValue{1};
        std::uint32_t m_RtvIncrement{};
        std::uint32_t m_SrvIncrement{};
        std::uint32_t m_Width{};
        std::uint32_t m_Height{};
        DXGI_FORMAT m_RtvFormat{DXGI_FORMAT_R8G8B8A8_UNORM};
        bool m_ImGuiInitialized{};
        bool m_FrameBegun{};
    };
}
