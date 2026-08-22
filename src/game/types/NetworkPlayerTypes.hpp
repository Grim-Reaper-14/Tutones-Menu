#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace Tutones::Game::Types
{
    // Read-only views for the current GTA V Enhanced player-manager layout.
    // Account, Rockstar, platform-account and message identifiers are deliberately
    // kept opaque here; the menu only consumes session-scoped structural state.
    struct NetworkPlayerView final
    {
        void* vtable{};                         // 0x00
        std::byte reservedSensitive[0x58]{};   // 0x08 .. 0x5F
        std::uint8_t activeIndex{};            // 0x60
        std::uint8_t playerIndex{};            // 0x61
        std::byte reserved62[0x6E]{};          // 0x62 .. 0xCF
        std::uint8_t flags{};                  // 0xD0
        std::byte reservedD1[0x0F]{};          // 0xD1 .. 0xDF

        [[nodiscard]] bool IsLocalFlagSet() const noexcept
        {
            return (flags & 1u) != 0;
        }
    };

    static_assert(offsetof(NetworkPlayerView, activeIndex) == 0x60);
    static_assert(offsetof(NetworkPlayerView, playerIndex) == 0x61);
    static_assert(offsetof(NetworkPlayerView, flags) == 0xD0);
    static_assert(sizeof(NetworkPlayerView) == 0xE0);

    struct NetworkPlayerManagerView final
    {
        void* vtable{};                              // 0x000
        void* netConnectionManager{};               // 0x008
        void* bandwidthManager{};                   // 0x010
        std::byte reserved18[0xD8]{};               // 0x018 .. 0x0EF
        NetworkPlayerView* localPlayer{};           // 0x0F0
        std::byte reservedF8[0x90]{};               // 0x0F8 .. 0x187
        std::array<NetworkPlayerView*, 32> players{}; // 0x188
        std::uint32_t maxPlayers{};                 // 0x288
        std::uint32_t reserved28C{};                // 0x28C
        std::int32_t unloadedPlayerCount{};         // 0x290
        std::int32_t loadedPlayerCount{};           // 0x294
        std::int32_t loadedNonLocalPlayerCount{};   // 0x298
        std::int32_t physicalPlayerCount{};         // 0x29C
        std::int32_t localPhysicalPlayerCount{};    // 0x2A0
        std::int32_t nonLocalPhysicalPlayerCount{}; // 0x2A4
        std::byte reserved2A8[0x648]{};             // 0x2A8 .. 0x8EF
    };

    static_assert(offsetof(NetworkPlayerManagerView, localPlayer) == 0xF0);
    static_assert(offsetof(NetworkPlayerManagerView, players) == 0x188);
    static_assert(offsetof(NetworkPlayerManagerView, maxPlayers) == 0x288);
    static_assert(offsetof(NetworkPlayerManagerView, loadedPlayerCount) == 0x294);
    static_assert(offsetof(NetworkPlayerManagerView, physicalPlayerCount) == 0x29C);
    static_assert(sizeof(NetworkPlayerManagerView) == 0x8F0);
}
