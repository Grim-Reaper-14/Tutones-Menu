#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace Tutones::Game::NetworkFeatures
{
    enum class CooldownSource : std::uint8_t
    {
        Tunable,
        ScriptStopwatch,
        NetworkTimer,
        GameTimer,
        DailyReset,
        WeeklyReset,
        Internal
    };

    struct CooldownDefinition final
    {
        std::string_view label;
        std::string_view group;
        std::string_view script;
        CooldownSource source{};
        std::size_t globalBase{};
        std::size_t globalOffset{};
        std::int64_t referenceDurationMs{};
        bool verified{};
    };

    enum class RewardKind : std::uint8_t
    {
        Earn,
        Refund
    };

    struct RewardDefinition final
    {
        std::string_view label;
        std::string_view serviceName;
        std::string_view script;
        RewardKind kind{};
        std::uint32_t hash{};
        std::size_t tunableBase{};
        std::size_t tunableOffset{};
        std::int64_t referenceAmount{};
        bool verified{};
    };

    struct CatalogObservation final
    {
        bool readable{};
        std::int64_t value{};
    };

    constexpr std::uint32_t Joaat(std::string_view text) noexcept
    {
        std::uint32_t hash{};
        for (char c : text)
        {
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

    inline constexpr std::size_t CooldownCatalogSize = 8;
    inline constexpr std::size_t RewardCatalogSize = 35;

    using CooldownObservations = std::array<CatalogObservation, CooldownCatalogSize>;
    using RewardObservations = std::array<CatalogObservation, RewardCatalogSize>;

    [[nodiscard]] std::span<const CooldownDefinition, CooldownCatalogSize> CooldownCatalog() noexcept;
    [[nodiscard]] std::span<const RewardDefinition, RewardCatalogSize> RewardCatalog() noexcept;
    [[nodiscard]] CooldownObservations SampleCooldownTunables(std::int64_t** globals) noexcept;
    [[nodiscard]] RewardObservations SampleRewardTunables(std::int64_t** globals) noexcept;
    [[nodiscard]] const char* CooldownSourceName(CooldownSource source) noexcept;
    [[nodiscard]] const char* RewardKindName(RewardKind kind) noexcept;
    [[nodiscard]] std::string FormatDuration(std::int64_t milliseconds);
}
