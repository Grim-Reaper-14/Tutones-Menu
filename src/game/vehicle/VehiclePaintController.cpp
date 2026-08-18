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
        if (!m_Backend.GetVehicleColours(vehicle, primary, secondary))
            return false;
        if (!m_Backend.GetVehicleExtraColours(vehicle, pearlescent, wheel))
            return false;

        out = {};
        out.vehicle = vehicle;
        out.primaryColor = primary;
        out.secondaryColor = secondary;
        out.pearlescentColor = pearlescent;
        out.wheelColor = wheel;
        out.primaryPaintType = NativePaintType::Normal;
        out.secondaryPaintType = NativePaintType::Normal;
        out.primaryModColor = primary;
        out.secondaryModColor = secondary;
        out.primaryModPearlescent = pearlescent;

        int primaryPaintType{};
        int primaryModColor{};
        int primaryModPearlescent{};
        if (m_Backend.GetVehicleModColor1(vehicle, primaryPaintType, primaryModColor, primaryModPearlescent)
            && IsNativePaintType(primaryPaintType))
        {
            out.primaryPaintType = static_cast<NativePaintType>(primaryPaintType);
            out.primaryModColor = primaryModColor;
            out.primaryModPearlescent = primaryModPearlescent;
        }

        int secondaryPaintType{};
        int secondaryModColor{};
        if (m_Backend.GetVehicleModColor2(vehicle, secondaryPaintType, secondaryModColor)
            && IsNativePaintType(secondaryPaintType))
        {
            out.secondaryPaintType = static_cast<NativePaintType>(secondaryPaintType);
            out.secondaryModColor = secondaryModColor;
        }

        bool primaryCustom{};
        if (m_Backend.IsPrimaryColourCustom(vehicle, primaryCustom))
        {
            out.primaryCustom = primaryCustom;
            if (primaryCustom)
                static_cast<void>(m_Backend.GetCustomPrimaryColour(vehicle, out.customPrimary));
        }

        bool secondaryCustom{};
        if (m_Backend.IsSecondaryColourCustom(vehicle, secondaryCustom))
        {
            out.secondaryCustom = secondaryCustom;
            if (secondaryCustom)
                static_cast<void>(m_Backend.GetCustomSecondaryColour(vehicle, out.customSecondary));
        }

        out.valid = true;
        return true;
    }

    bool VehiclePaintController::SetPrimary(VehicleHandle vehicle, PaintChoice choice) noexcept
    {
        if (vehicle == 0 || !PaintChoiceAllowed(PaintTarget::Primary, choice))
            return false;

        bool written{};
        if (UsesIndexedVehicleColourPath(choice.palette))
        {
            int primary{};
            int secondary{};
            if (!m_Backend.GetVehicleColours(vehicle, primary, secondary))
                return false;
            written = m_Backend.SetVehicleColours(vehicle, choice.colorIndex, secondary);
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
            {
                int wheel{};
                if (!m_Backend.GetVehicleExtraColours(vehicle, currentPearlescent, wheel))
                    currentPearlescent = 0;
            }
            written = m_Backend.SetVehicleModColor1(vehicle, paintType, choice.colorIndex, currentPearlescent);
        }

        if (!written)
            return false;

        // A custom RGB override hides indexed/LSC paint. Clear it after a successful
        // base write, but do not turn a successful paint operation into a failure just
        // because the optional custom-clear metadata path is unavailable on this build.
        static_cast<void>(m_Backend.ClearCustomPrimaryColour(vehicle));
        return true;
    }

    bool VehiclePaintController::SetSecondary(VehicleHandle vehicle, PaintChoice choice) noexcept
    {
        if (vehicle == 0 || !PaintChoiceAllowed(PaintTarget::Secondary, choice))
            return false;

        bool written{};
        if (UsesIndexedVehicleColourPath(choice.palette))
        {
            int primary{};
            int secondary{};
            if (!m_Backend.GetVehicleColours(vehicle, primary, secondary))
                return false;
            written = m_Backend.SetVehicleColours(vehicle, primary, choice.colorIndex);
        }
        else
        {
            const int paintType = NativePaintTypeValue(choice.palette);
            if (!IsNativePaintType(paintType))
                return false;
            written = m_Backend.SetVehicleModColor2(vehicle, paintType, choice.colorIndex);
        }

        if (!written)
            return false;
        static_cast<void>(m_Backend.ClearCustomSecondaryColour(vehicle));
        return true;
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
