#pragma once

#include <string_view>

namespace Tutones::Hooking
{
    enum class HookStatus
    {
        NotInitialized,
        Initializing,
        Installed,
        Failed,
        ShuttingDown,
        Stopped,
    };

    [[nodiscard]] constexpr std::string_view ToString(HookStatus status) noexcept
    {
        switch (status)
        {
        case HookStatus::NotInitialized: return "NotInitialized";
        case HookStatus::Initializing: return "Initializing";
        case HookStatus::Installed: return "Installed";
        case HookStatus::Failed: return "Failed";
        case HookStatus::ShuttingDown: return "ShuttingDown";
        case HookStatus::Stopped: return "Stopped";
        }
        return "Unknown";
    }
}
