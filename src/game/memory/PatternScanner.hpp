#pragma once

#include "ModuleView.hpp"

#include <cstddef>
#include <string_view>

namespace Tutones::Game::Memory
{
    class PatternScanner final
    {
    public:
        [[nodiscard]] static std::byte* FindFirst(const ModuleView& module, std::string_view signature);
        [[nodiscard]] static std::byte* ResolveRip(std::byte* displacement) noexcept;
    };
}
