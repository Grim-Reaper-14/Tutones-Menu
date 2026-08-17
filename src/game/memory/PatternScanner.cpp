#include "PatternScanner.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

namespace Tutones::Game::Memory
{
    namespace
    {
        int HexNibble(char value) noexcept
        {
            if (value >= '0' && value <= '9') return value - '0';
            if (value >= 'a' && value <= 'f') return 10 + (value - 'a');
            if (value >= 'A' && value <= 'F') return 10 + (value - 'A');
            return -1;
        }

        std::vector<int> ParsePattern(std::string_view signature)
        {
            std::vector<int> bytes;
            bytes.reserve(signature.size() / 2);

            for (std::size_t i = 0; i < signature.size();)
            {
                if (signature[i] == ' ' || signature[i] == '\t')
                {
                    ++i;
                    continue;
                }

                if (signature[i] == '?')
                {
                    bytes.push_back(-1);
                    ++i;
                    if (i < signature.size() && signature[i] == '?')
                        ++i;
                    continue;
                }

                if (i + 1 >= signature.size())
                    return {};

                const int high = HexNibble(signature[i]);
                const int low = HexNibble(signature[i + 1]);
                if (high < 0 || low < 0)
                    return {};

                bytes.push_back((high << 4) | low);
                i += 2;
            }

            return bytes;
        }
    }

    std::byte* PatternScanner::FindFirst(const ModuleView& module, std::string_view signature)
    {
        if (!module.IsValid() || signature.empty())
            return nullptr;

        const auto pattern = ParsePattern(signature);
        if (pattern.empty())
            return nullptr;

        for (const auto& range : module.CodeRanges())
        {
            if (!range.data || range.size < pattern.size())
                continue;

            const auto* bytes = reinterpret_cast<const std::uint8_t*>(range.data);
            const auto last = range.size - pattern.size();

            for (std::size_t offset = 0; offset <= last; ++offset)
            {
                bool matched = true;
                for (std::size_t index = 0; index < pattern.size(); ++index)
                {
                    if (pattern[index] >= 0 && bytes[offset + index] != static_cast<std::uint8_t>(pattern[index]))
                    {
                        matched = false;
                        break;
                    }
                }

                if (matched)
                    return const_cast<std::byte*>(range.data + offset);
            }
        }

        return nullptr;
    }

    std::byte* PatternScanner::ResolveRip(std::byte* displacement) noexcept
    {
        if (!displacement)
            return nullptr;

        std::int32_t relative{};
        std::memcpy(&relative, displacement, sizeof(relative));
        return displacement + sizeof(relative) + relative;
    }
}
