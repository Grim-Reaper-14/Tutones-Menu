#pragma once

#include "native/NativeInvoker.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace Tutones::Game::Stats
{
    namespace Detail
    {
        [[nodiscard]] inline char LowerAscii(char value) noexcept
        {
            if (value >= 'A' && value <= 'Z')
                return static_cast<char>(value - 'A' + 'a');
            return value;
        }

        [[nodiscard]] inline std::uint32_t Joaat(std::string_view text) noexcept
        {
            std::uint32_t hash{};
            for (char value : text)
            {
                const auto c = static_cast<std::uint8_t>(LowerAscii(value));
                hash += c;
                hash += hash << 10;
                hash ^= hash >> 6;
            }
            hash += hash << 3;
            hash ^= hash >> 11;
            hash += hash << 15;
            return hash;
        }

        inline void Normalize(std::string& statName) noexcept
        {
            std::transform(statName.begin(), statName.end(), statName.begin(), [](char value) {
                return LowerAscii(value);
            });
        }

        [[nodiscard]] inline std::optional<int> ReadInt(std::uint32_t statHash) noexcept
        {
            int value{};
            const auto result = Native::NativeInvoker::Invoke<std::int32_t>(
                Native::NativeId::StatGetInt,
                statHash,
                &value,
                -1);
            if (!result || *result == 0)
                return std::nullopt;
            return value;
        }
    }

    [[nodiscard]] inline std::optional<int> GetCharIndex() noexcept
    {
        return Detail::ReadInt(Detail::Joaat("MPPLY_LAST_MP_CHAR"));
    }

    [[nodiscard]] inline std::optional<int> GetInt(std::string statName, int characterIndex)
    {
        Detail::Normalize(statName);
        if (statName.size() >= 3 && statName[0] == 'm' && statName[1] == 'p' && statName[2] == 'x')
        {
            if (characterIndex < 0 || characterIndex > 9)
                return std::nullopt;
            statName[2] = static_cast<char>('0' + characterIndex);
        }
        return Detail::ReadInt(Detail::Joaat(statName));
    }

    [[nodiscard]] inline std::optional<int> GetInt(std::string statName)
    {
        Detail::Normalize(statName);
        if (statName.size() >= 3 && statName[0] == 'm' && statName[1] == 'p' && statName[2] == 'x')
        {
            const auto characterIndex = GetCharIndex();
            if (!characterIndex || *characterIndex < 0 || *characterIndex > 9)
                return std::nullopt;
            statName[2] = static_cast<char>('0' + *characterIndex);
        }
        return Detail::ReadInt(Detail::Joaat(statName));
    }
}
