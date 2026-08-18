#pragma once

#include <cstdint>

namespace Tutones::Game::Paint
{
    using VehicleHandle = std::int32_t;

    enum class PaintPalette : std::uint8_t
    {
        Alloy,
        Chrome,
        Classic,
        Matte,
        Metals,
        Utility,
        Worn,
        Chameleon,
    };

    enum class PaintTarget : std::uint8_t
    {
        Primary,
        Secondary,
        Pearlescent,
        Wheel,
    };

    enum class PaintOperation : std::uint8_t
    {
        None,
        Primary,
        Secondary,
        Pearlescent,
        Wheel,
    };

    struct PaintChoice final
    {
        PaintPalette palette{};
        int colorIndex{};
    };

    struct VehiclePaintState final
    {
        VehicleHandle vehicle{};
        int primaryColor{};
        int secondaryColor{};
        int pearlescentColor{};
        int wheelColor{};
        bool valid{};
    };

    struct PaintServiceSnapshot final
    {
        VehiclePaintState paint{};
        PaintOperation lastOperation{PaintOperation::None};
        bool lastOperationSucceeded{};
        bool lastOperationRejectedAsStale{};
    };

    [[nodiscard]] constexpr bool PaletteAllowed(PaintTarget target, PaintPalette palette) noexcept
    {
        switch (target)
        {
        case PaintTarget::Primary:
        case PaintTarget::Secondary:
            return palette != PaintPalette::Alloy;
        case PaintTarget::Pearlescent:
            return palette == PaintPalette::Classic;
        case PaintTarget::Wheel:
            return palette == PaintPalette::Alloy
                || palette == PaintPalette::Classic
                || palette == PaintPalette::Chameleon;
        }
        return false;
    }

    [[nodiscard]] constexpr bool IndexAllowed(PaintPalette palette, int colorIndex) noexcept
    {
        if (palette == PaintPalette::Chameleon)
            return colorIndex >= 161 && colorIndex <= 223;

        return colorIndex >= 0 && colorIndex <= 160;
    }

    [[nodiscard]] constexpr bool PaintChoiceAllowed(PaintTarget target, PaintChoice choice) noexcept
    {
        return PaletteAllowed(target, choice.palette) && IndexAllowed(choice.palette, choice.colorIndex);
    }
}
