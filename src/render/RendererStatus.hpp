#pragma once

#include <string_view>

namespace Tutones::Render
{
    enum class RendererStatus
    {
        NotInitialized,
        WaitingForDevice,
        Initialized,
        Failed,
        ShuttingDown,
        Stopped,
    };

    [[nodiscard]] constexpr std::string_view ToString(RendererStatus status) noexcept
    {
        switch (status)
        {
        case RendererStatus::NotInitialized: return "NotInitialized";
        case RendererStatus::WaitingForDevice: return "WaitingForDevice";
        case RendererStatus::Initialized: return "Initialized";
        case RendererStatus::Failed: return "Failed";
        case RendererStatus::ShuttingDown: return "ShuttingDown";
        case RendererStatus::Stopped: return "Stopped";
        }
        return "Unknown";
    }
}
