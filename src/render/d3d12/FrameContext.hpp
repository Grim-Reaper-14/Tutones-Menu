#pragma once

#include <d3d12.h>

#include <cstdint>

namespace Tutones::Render::D3D12
{
    struct FrameContext final
    {
        ID3D12CommandAllocator* commandAllocator{};
        ID3D12Resource* backBuffer{};
        D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
        std::uint64_t fenceValue{};
    };
}
