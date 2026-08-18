#pragma once

#include "../types/ScriptProgram.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace Tutones::Game::Script
{
    class ScriptPointer final
    {
    public:
        ScriptPointer(std::string name, std::string signature, std::int32_t offset = 0, bool rip = false);

        [[nodiscard]] ScriptPointer Add(std::uint32_t offset) const;
        [[nodiscard]] ScriptPointer Sub(std::uint32_t offset) const;
        [[nodiscard]] ScriptPointer Rip() const;
        [[nodiscard]] std::uint32_t Scan(Types::ScriptProgram* program) const noexcept;
        [[nodiscard]] const std::string& Name() const noexcept { return m_Name; }

    private:
        std::string m_Name;
        std::string m_Signature;
        std::int32_t m_Offset{};
        bool m_Rip{};
    };

    [[nodiscard]] std::uint32_t FindCodePattern(Types::ScriptProgram* program, std::string_view signature) noexcept;
}
