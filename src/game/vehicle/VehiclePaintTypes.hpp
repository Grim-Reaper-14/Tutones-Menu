#pragma once

#include <cstdint>

namespace Tutones::Game::Paint
{
    using VehicleHandle = std::int32_t;

    enum class NativePaintType : std::uint8_t
    {
        Normal = 0,
        Metallic = 1,
        Pearl = 2,
        Matte = 3,
        Metal = 4,
        Chrome = 5,
    };

    enum class PaintPalette : std::uint8_t
    {
        Alloy,
        Normal,
        Metallic,
        Pearl,
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
        CustomPrimary,
        CustomSecondary,
        ClearCustomPrimary,
        ClearCustomSecondary,
    };

    struct RgbColor final
    {
        std::uint8_t red{};
        std::uint8_t green{};
        std::uint8_t blue{};

        [[nodiscard]] friend constexpr bool operator==(const RgbColor&, const RgbColor&) noexcept = default;
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
        NativePaintType primaryPaintType{NativePaintType::Normal};
        NativePaintType secondaryPaintType{NativePaintType::Normal};
        int primaryModColor{};
        int secondaryModColor{};
        int primaryModPearlescent{};
        bool primaryCustom{};
        bool secondaryCustom{};
        RgbColor customPrimary{};
        RgbColor customSecondary{};
        bool valid{};
    };

    struct PaintServiceSnapshot final
    {
        VehiclePaintState paint{};
        PaintOperation lastOperation{PaintOperation::None};
        bool lastOperationSucceeded{};
        bool lastOperationRejectedAsStale{};
    };

    [[nodiscard]] constexpr bool UsesIndexedVehicleColourPath(PaintPalette palette) noexcept
    {
        return palette == PaintPalette::Worn || palette == PaintPalette::Chameleon;
    }

    [[nodiscard]] constexpr int NativePaintTypeValue(PaintPalette palette) noexcept
    {
        switch (palette)
        {
        case PaintPalette::Normal:
        case PaintPalette::Classic:
        case PaintPalette::Utility:
            return static_cast<int>(NativePaintType::Normal);
        case PaintPalette::Metallic:
            return static_cast<int>(NativePaintType::Metallic);
        case PaintPalette::Pearl:
            return static_cast<int>(NativePaintType::Pearl);
        case PaintPalette::Matte:
            return static_cast<int>(NativePaintType::Matte);
        case PaintPalette::Metals:
            return static_cast<int>(NativePaintType::Metal);
        case PaintPalette::Chrome:
            return static_cast<int>(NativePaintType::Chrome);
        case PaintPalette::Alloy:
        case PaintPalette::Worn:
        case PaintPalette::Chameleon:
            return -1;
        }
        return -1;
    }

    [[nodiscard]] constexpr bool IsNativePaintType(int value) noexcept
    {
        return value >= static_cast<int>(NativePaintType::Normal)
            && value <= static_cast<int>(NativePaintType::Chrome);
    }

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
