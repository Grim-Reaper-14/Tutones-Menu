#include "VehiclePaintController.hpp"

namespace Tutones::Game::Paint
{
    bool VehiclePaintController::ReadPaintState(VehicleHandle vehicle, VehiclePaintState& out) noexcept
    {
        if (vehicle == 0)
            return false;

        int primary{};
        int secondary{};
        int pearlescent{};
        int wheel{};
        int primaryPaintType{};
        int primaryModColor{};
        int primaryModPearlescent{};
        int secondaryPaintType{};
        int secondaryModColor{};
        bool primaryCustom{};
        bool secondaryCustom{};
        RgbColor customPrimary{};
        RgbColor customSecondary{};

        if (!m_Backend.GetVehicleColours(vehicle, primary, secondary))
            return false;
        if (!m_Backend.GetVehicleExtraColours(vehicle, pearlescent, wheel))
            return false;
        if (!m_Backend.GetVehicleModColor1(vehicle, primaryPaintType, primaryModColor, primaryModPearlescent))
            return false;
        if (!m_Backend.GetVehicleModColor2(vehicle, secondaryPaintType, secondaryModColor))
            return false;
        if (!IsNativePaintType(primaryPaintType) || !IsNativePaintType(secondaryPaintType))
            return false;
        if (!m_Backend.IsPrimaryColourCustom(vehicle, primaryCustom))
            return false;
        if (!m_Backend.IsSecondaryColourCustom(vehicle, secondaryCustom))
            return false;
        if (primaryCustom && !m_Backend.GetCustomPrimaryColour(vehicle, customPrimary))
            return false;
        if (secondaryCustom && !m_Backend.GetCustomSecondaryColour(vehicle, customSecondary))
            return false;

        out.vehicle = vehicle;
        out.primaryColor = primary;
        out.secondaryColor = secondary;
        out.pearlescentColor = pearlescent;
        out.wheelColor = wheel;
        out.primaryPaintType = static_cast<NativePaintType>(primaryPaintType);
        out.secondaryPaintType = static_cast<NativePaintType>(secondaryPaintType);
        out.primaryModColor = primaryModColor;
        out.secondaryModColor = secondaryModColor;
        out.primaryModPearlescent = primaryModPearlescent;
        out.primaryCustom = primaryCustom;
        out.secondaryCustom = secondaryCustom;
        out.customPrimary = customPrimary;
        out.customSecondary = customSecondary;
        out.valid = true;
        return true;
    }

    bool VehiclePaintController::SetPrimary(VehicleHandle vehicle, PaintChoice choice) noexcept
    {
        if (vehicle == 0 || !PaintChoiceAllowed(PaintTarget::Primary, choice))
            return false;

        bool custom{};
        if (!m_Backend.IsPrimaryColourCustom(vehicle, custom))
            return false;

        if (UsesIndexedVehicleColourPath(choice.palette))
        {
            int primary{};
            int secondary{};
            if (!m_Backend.GetVehicleColours(vehicle, primary, secondary))
                return false;
            if (!m_Backend.SetVehicleColours(vehicle, choice.colorIndex, secondary))
                return false;
        }
        else
        {
            const int paintType = NativePaintTypeValue(choice.palette);
            if (!IsNativePaintType(paintType))
                return false;

            int currentPaintType{};
            int currentColor{};
            int currentPearlescent{};
            if (!m_Backend.GetVehicleModColor1(vehicle, currentPaintType, currentColor, currentPearlescent))
                return false;
            if (!m_Backend.SetVehicleModColor1(vehicle, paintType, choice.colorIndex, currentPearlescent))
                return false;
        }

        // Always write the requested indexed/mod paint first. A failed custom-clear
        // leaves the visible custom RGB override intact and reports failure conservatively.
        return !custom || m_Backend.ClearCustomPrimaryColour(vehicle);
    }

    bool VehiclePaintController::SetSecondary(VehicleHandle vehicle, PaintChoice choice) noexcept
    {
        if (vehicle == 0 || !PaintChoiceAllowed(PaintTarget::Secondary, choice))
            return false;

        bool custom{};
        if (!m_Backend.IsSecondaryColourCustom(vehicle, custom))
            return false;

        if (UsesIndexedVehicleColourPath(choice.palette))
        {
            int primary{};
            int secondary{};
            if (!m_Backend.GetVehicleColours(vehicle, primary, secondary))
                return false;
            if (!m_Backend.SetVehicleColours(vehicle, primary, choice.colorIndex))
                return false;
        }
        else
        {
            const int paintType = NativePaintTypeValue(choice.palette);
            if (!IsNativePaintType(paintType))
                return false;
            if (!m_Backend.SetVehicleModColor2(vehicle, paintType, choice.colorIndex))
                return false;
        }

        return !custom || m_Backend.ClearCustomSecondaryColour(vehicle);
    }

    bool VehiclePaintController::SetPearlescent(VehicleHandle vehicle, int colorIndex) noexcept
    {
        const PaintChoice choice{PaintPalette::Classic, colorIndex};
        if (vehicle == 0 || !PaintChoiceAllowed(PaintTarget::Pearlescent, choice))
            return false;

        int pearlescent{};
        int wheel{};
        if (!m_Backend.GetVehicleExtraColours(vehicle, pearlescent, wheel))
            return false;

        return m_Backend.SetVehicleExtraColours(vehicle, colorIndex, wheel);
    }

    bool VehiclePaintController::SetWheel(VehicleHandle vehicle, int colorIndex) noexcept
    {
        PaintChoice choice{PaintPalette::Classic, colorIndex};
        if (colorIndex >= 161)
            choice.palette = PaintPalette::Chameleon;

        if (vehicle == 0 || !PaintChoiceAllowed(PaintTarget::Wheel, choice))
            return false;

        int pearlescent{};
        int wheel{};
        if (!m_Backend.GetVehicleExtraColours(vehicle, pearlescent, wheel))
            return false;

        return m_Backend.SetVehicleExtraColours(vehicle, pearlescent, colorIndex);
    }

    bool VehiclePaintController::SetCustomPrimary(VehicleHandle vehicle, RgbColor color) noexcept
    {
        return vehicle != 0 && m_Backend.SetCustomPrimaryColour(vehicle, color);
    }

    bool VehiclePaintController::SetCustomSecondary(VehicleHandle vehicle, RgbColor color) noexcept
    {
        return vehicle != 0 && m_Backend.SetCustomSecondaryColour(vehicle, color);
    }

    bool VehiclePaintController::ClearCustomPrimary(VehicleHandle vehicle) noexcept
    {
        return vehicle != 0 && m_Backend.ClearCustomPrimaryColour(vehicle);
    }

    bool VehiclePaintController::ClearCustomSecondary(VehicleHandle vehicle) noexcept
    {
        return vehicle != 0 && m_Backend.ClearCustomSecondaryColour(vehicle);
    }
}
