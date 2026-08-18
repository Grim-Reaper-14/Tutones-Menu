#pragma once

#include <cstdint>

namespace Tutones::Game::PlayerFeatures
{
    struct OffRadarState final
    {
        bool enabled{};
        bool applied{};
        bool scriptGlobalsReady{};
        bool sessionStarted{};
        bool freemodeReady{};
        bool safeToModify{};
        std::uint32_t networkTime{};
        std::uint32_t lastRefreshTime{};
    };

    // V11 keeps Off Radar behind the Freemode broadcast globals used by GTA Online.
    // The runtime writes current network time exactly as the current Enhanced reference does;
    // this interval is only retained as a status/refresh horizon for UI state.
    inline constexpr std::uint32_t OffRadarRefreshMs = 60000;
}
