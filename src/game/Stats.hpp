#pragma once

#include "native/NativeInvoker.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

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

        [[nodiscard]] inline bool ResolveCharacterStat(std::string& statName, int characterIndex) noexcept
        {
            Normalize(statName);
            if (statName.size() >= 3 && statName[0] == 'm' && statName[1] == 'p' && statName[2] == 'x')
            {
                if (characterIndex < 0 || characterIndex > 9)
                    return false;
                statName[2] = static_cast<char>('0' + characterIndex);
            }
            return true;
        }

        [[nodiscard]] inline bool ValidMask(int bitStart, int bitSize) noexcept
        {
            return bitStart >= 0 && bitSize > 0 && bitSize <= 32 && bitStart <= 63 && bitStart + bitSize <= 64;
        }

        [[nodiscard]] inline bool ValidCharacterIndex(int characterIndex) noexcept
        {
            return characterIndex >= -1 && characterIndex <= 9;
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

        [[nodiscard]] inline bool WriteInt(std::uint32_t statHash, int value) noexcept
        {
            const auto result = Native::NativeInvoker::Invoke<std::int32_t>(
                Native::NativeId::StatSetInt,
                statHash,
                value,
                std::int32_t{1});
            return result && *result != 0;
        }

        [[nodiscard]] inline std::optional<int> ReadMaskedInt(std::uint32_t statHash, int bitStart, int bitSize) noexcept
        {
            if (!ValidMask(bitStart, bitSize))
                return std::nullopt;

            int value{};
            const auto result = Native::NativeInvoker::Invoke<std::int32_t>(
                Native::NativeId::StatGetMaskedInt,
                statHash,
                &value,
                bitStart,
                bitSize,
                -1);
            if (!result || *result == 0)
                return std::nullopt;
            return value;
        }

        [[nodiscard]] inline bool WriteMaskedInt(std::uint32_t statHash, int value, int bitStart, int bitSize) noexcept
        {
            if (!ValidMask(bitStart, bitSize))
                return false;

            const auto result = Native::NativeInvoker::Invoke<std::int32_t>(
                Native::NativeId::StatSetMaskedInt,
                statHash,
                value,
                bitStart,
                bitSize,
                std::int32_t{1});
            return result && *result != 0;
        }
    }

    [[nodiscard]] inline std::optional<int> GetCharIndex() noexcept
    {
        return Detail::ReadInt(Detail::Joaat("MPPLY_LAST_MP_CHAR"));
    }

    [[nodiscard]] inline std::optional<int> GetInt(std::string statName, int characterIndex)
    {
        if (!Detail::ResolveCharacterStat(statName, characterIndex))
            return std::nullopt;
        return Detail::ReadInt(Detail::Joaat(statName));
    }

    [[nodiscard]] inline std::optional<int> GetInt(std::string statName)
    {
        Detail::Normalize(statName);
        if (statName.size() >= 3 && statName[0] == 'm' && statName[1] == 'p' && statName[2] == 'x')
        {
            const auto characterIndex = GetCharIndex();
            if (!characterIndex || !Detail::ResolveCharacterStat(statName, *characterIndex))
                return std::nullopt;
        }
        return Detail::ReadInt(Detail::Joaat(statName));
    }

    [[nodiscard]] inline bool SetInt(std::string statName, int value, int characterIndex)
    {
        if (!Detail::ResolveCharacterStat(statName, characterIndex))
            return false;
        return Detail::WriteInt(Detail::Joaat(statName), value);
    }

    [[nodiscard]] inline bool SetInt(std::string statName, int value)
    {
        Detail::Normalize(statName);
        if (statName.size() >= 3 && statName[0] == 'm' && statName[1] == 'p' && statName[2] == 'x')
        {
            const auto characterIndex = GetCharIndex();
            if (!characterIndex || !Detail::ResolveCharacterStat(statName, *characterIndex))
                return false;
        }
        return Detail::WriteInt(Detail::Joaat(statName), value);
    }

    [[nodiscard]] inline std::optional<int> GetMaskedInt(std::string statName, int bitStart, int bitSize, int characterIndex)
    {
        if (!Detail::ResolveCharacterStat(statName, characterIndex))
            return std::nullopt;
        return Detail::ReadMaskedInt(Detail::Joaat(statName), bitStart, bitSize);
    }

    [[nodiscard]] inline std::optional<int> GetMaskedInt(std::string statName, int bitStart, int bitSize)
    {
        Detail::Normalize(statName);
        if (statName.size() >= 3 && statName[0] == 'm' && statName[1] == 'p' && statName[2] == 'x')
        {
            const auto characterIndex = GetCharIndex();
            if (!characterIndex || !Detail::ResolveCharacterStat(statName, *characterIndex))
                return std::nullopt;
        }
        return Detail::ReadMaskedInt(Detail::Joaat(statName), bitStart, bitSize);
    }

    [[nodiscard]] inline bool SetMaskedInt(std::string statName, int bitStart, int bitSize, int value, int characterIndex)
    {
        if (!Detail::ResolveCharacterStat(statName, characterIndex))
            return false;
        return Detail::WriteMaskedInt(Detail::Joaat(statName), value, bitStart, bitSize);
    }

    [[nodiscard]] inline bool SetMaskedInt(std::string statName, int bitStart, int bitSize, int value)
    {
        Detail::Normalize(statName);
        if (statName.size() >= 3 && statName[0] == 'm' && statName[1] == 'p' && statName[2] == 'x')
        {
            const auto characterIndex = GetCharIndex();
            if (!characterIndex || !Detail::ResolveCharacterStat(statName, *characterIndex))
                return false;
        }
        return Detail::WriteMaskedInt(Detail::Joaat(statName), value, bitStart, bitSize);
    }

    [[nodiscard]] inline std::optional<bool> GetMaskedBool(std::string statName, int bitIndex, int characterIndex)
    {
        const auto value = GetMaskedInt(std::move(statName), bitIndex, 1, characterIndex);
        if (!value)
            return std::nullopt;
        return *value != 0;
    }

    [[nodiscard]] inline std::optional<bool> GetMaskedBool(std::string statName, int bitIndex)
    {
        const auto value = GetMaskedInt(std::move(statName), bitIndex, 1);
        if (!value)
            return std::nullopt;
        return *value != 0;
    }

    [[nodiscard]] inline bool SetMaskedBool(std::string statName, int bitIndex, bool value, int characterIndex)
    {
        return SetMaskedInt(std::move(statName), bitIndex, 1, value ? 1 : 0, characterIndex);
    }

    [[nodiscard]] inline bool SetMaskedBool(std::string statName, int bitIndex, bool value)
    {
        return SetMaskedInt(std::move(statName), bitIndex, 1, value ? 1 : 0);
    }

    [[nodiscard]] inline std::optional<bool> GetPackedBool(int index, int characterIndex = -1) noexcept
    {
        if (index < 0 || !Detail::ValidCharacterIndex(characterIndex))
            return std::nullopt;

        const auto result = Native::NativeInvoker::Invoke<std::int32_t>(
            Native::NativeId::GetPackedStatBoolCode,
            index,
            characterIndex);
        if (!result)
            return std::nullopt;
        return *result != 0;
    }

    [[nodiscard]] inline bool SetPackedBool(int index, bool value, int characterIndex = -1) noexcept
    {
        if (index < 0 || !Detail::ValidCharacterIndex(characterIndex))
            return false;

        return Native::NativeInvoker::InvokeVoid(
            Native::NativeId::SetPackedStatBoolCode,
            index,
            std::int32_t{value ? 1 : 0},
            characterIndex);
    }

    [[nodiscard]] inline std::optional<int> GetPackedInt(int index, int characterIndex = -1) noexcept
    {
        if (index < 0 || !Detail::ValidCharacterIndex(characterIndex))
            return std::nullopt;

        return Native::NativeInvoker::Invoke<std::int32_t>(
            Native::NativeId::GetPackedStatIntCode,
            index,
            characterIndex);
    }

    [[nodiscard]] inline bool SetPackedInt(int index, int value, int characterIndex = -1) noexcept
    {
        if (index < 0 || !Detail::ValidCharacterIndex(characterIndex))
            return false;

        return Native::NativeInvoker::InvokeVoid(
            Native::NativeId::SetPackedStatIntCode,
            index,
            value,
            characterIndex);
    }
}
