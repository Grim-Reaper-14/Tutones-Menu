#pragma once

#include <cstddef>
#include <cstdint>

namespace Tutones::Game::PlayerFeatures
{
    struct OffRadarState final
    {
        bool enabled{};
        bool scriptGlobalsReady{};
        bool sessionStarted{};
        std::uint32_t networkTime{};
        std::uint32_t expiryTime{};
    };

    // V11 keeps the Off Radar implementation behind the shared Freemode/script-global
    // runtime so the UI never substitutes local HUD hiding for real online state.
    inline constexpr std::uint32_t OffRadarRefreshMs = 60000;
}
