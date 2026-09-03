#pragma once

#include <cstddef>
#include <cstdint>

namespace Tutones::Game::Business::AcidLabProductionDetail
{
    [[nodiscard]] constexpr std::uint32_t Joaat(const char* text) noexcept
    {
        std::uint32_t hash{};
        while (text && *text)
        {
            char c = *text++;
            if (c >= 'A' && c <= 'Z')
                c = static_cast<char>(c - 'A' + 'a');
            hash += static_cast<std::uint8_t>(c);
            hash += hash << 10;
            hash ^= hash >> 6;
        }
        hash += hash << 3;
        hash ^= hash >> 11;
        hash += hash << 15;
        return hash;
    }

    inline constexpr std::uint32_t FreemodeHash = Joaat("freemode");
    inline constexpr const char* SetupStat = "MPX_FACTORYSETUP6";
    inline constexpr const char* StockStat = "MPX_PRODTOTALFORFACTORY6";
    inline constexpr int MaximumStockUnits = 160;

    // Enhanced 1.73 b1158.13 freemode.c:
    // Global_1845347[player /*884*/].f_260.f_205[6 /*13*/].f_1
    inline constexpr std::size_t PlayerFreemodeGlobal = 1845347;
    inline constexpr std::size_t PlayerFreemodeStride = 884;
    inline constexpr std::size_t PropertyDataOffset = 260;
    inline constexpr std::size_t FactoryArrayOffset = 205;
    inline constexpr std::size_t FactoryEntryStride = 13;
    inline constexpr std::size_t AcidFactoryIndex = 6;
    inline constexpr std::size_t ProductOffset = 1;
    inline constexpr int FactoryArrayCount = 7;
    inline constexpr int AcidFactoryType = 32;
    inline constexpr int MaximumPlayers = 32;

    [[nodiscard]] constexpr std::size_t FactoryArrayIndex(std::size_t player) noexcept
    {
        return PlayerFreemodeGlobal + 1 + (player * PlayerFreemodeStride)
            + PropertyDataOffset + FactoryArrayOffset;
    }

    [[nodiscard]] constexpr std::size_t AcidFactoryEntryIndex(std::size_t player) noexcept
    {
        // GTA script arrays keep their element count in the first slot.
        return FactoryArrayIndex(player) + 1 + (AcidFactoryIndex * FactoryEntryStride);
    }

    [[nodiscard]] constexpr std::size_t AcidProductIndex(std::size_t player) noexcept
    {
        return AcidFactoryEntryIndex(player) + ProductOffset;
    }

    [[nodiscard]] constexpr bool IsValidStock(int stockUnits) noexcept
    {
        return stockUnits >= 0 && stockUnits <= MaximumStockUnits;
    }

    [[nodiscard]] constexpr bool CanMirrorLiveStock(
        int factoryCount,
        int factoryType,
        int currentStock) noexcept
    {
        return factoryCount == FactoryArrayCount
            && factoryType == AcidFactoryType
            && IsValidStock(currentStock);
    }

    static_assert(AcidProductIndex(0) == 1845893);
    static_assert(AcidProductIndex(31) == 1873297);
}
