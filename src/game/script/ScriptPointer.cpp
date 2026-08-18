#include "ScriptPointer.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace Tutones::Game::Script
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

        std::uint32_t ReadThreeByte(const std::uint8_t* data) noexcept
        {
            if (!data)
                return 0;

            return static_cast<std::uint32_t>(data[0])
                | (static_cast<std::uint32_t>(data[1]) << 8)
                | (static_cast<std::uint32_t>(data[2]) << 16);
        }
    }

    std::uint32_t FindCodePattern(Types::ScriptProgram* program, std::string_view signature) noexcept
    {
        if (!program || program->codeSize == 0 || signature.empty())
            return 0;

        const auto pattern = ParsePattern(signature);
        if (pattern.empty() || program->codeSize < pattern.size())
            return 0;

        const auto last = program->codeSize - static_cast<std::uint32_t>(pattern.size());
        for (std::uint32_t offset = 0; offset <= last; ++offset)
        {
            bool matched = true;
            for (std::size_t index = 0; index < pattern.size(); ++index)
            {
                if (pattern[index] < 0)
                    continue;

                const auto* byte = program->GetCodeAddress(offset + static_cast<std::uint32_t>(index));
                if (!byte || *byte != static_cast<std::uint8_t>(pattern[index]))
                {
                    matched = false;
                    break;
                }
            }

            if (matched)
                return offset;
        }

        return 0;
    }

    ScriptPointer::ScriptPointer(std::string name, std::string signature, std::int32_t offset, bool rip)
        : m_Name(std::move(name)), m_Signature(std::move(signature)), m_Offset(offset), m_Rip(rip)
    {
    }

    ScriptPointer ScriptPointer::Add(std::uint32_t offset) const
    {
        return ScriptPointer(m_Name, m_Signature, m_Offset + static_cast<std::int32_t>(offset), m_Rip);
    }

    ScriptPointer ScriptPointer::Sub(std::uint32_t offset) const
    {
        return ScriptPointer(m_Name, m_Signature, m_Offset - static_cast<std::int32_t>(offset), m_Rip);
    }

    ScriptPointer ScriptPointer::Rip() const
    {
        return ScriptPointer(m_Name, m_Signature, m_Offset, true);
    }

    std::uint32_t ScriptPointer::Scan(Types::ScriptProgram* program) const noexcept
    {
        const std::uint32_t location = FindCodePattern(program, m_Signature);
        if (location == 0)
            return 0;

        const std::int64_t adjusted = static_cast<std::int64_t>(location) + m_Offset;
        if (adjusted < 0 || static_cast<std::uint64_t>(adjusted) >= program->codeSize)
            return 0;

        const auto address = static_cast<std::uint32_t>(adjusted);
        return m_Rip ? ReadThreeByte(program->GetCodeAddress(address)) : address;
    }
}
