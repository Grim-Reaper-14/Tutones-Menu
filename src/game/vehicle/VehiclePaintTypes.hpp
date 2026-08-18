#pragma once

#include <cstdint>

namespace Tutones::Game::Paint
{
    using VehicleHandle = std::int32_t;

    struct PaintChoice final
    {
        int colorIndex{};
    };

    [[nodiscard]] constexpr bool IsIndexedColour(int colorIndex) noexcept
    {
        return colorIndex >= 0 && colorIndex <= 223;
    }
}
