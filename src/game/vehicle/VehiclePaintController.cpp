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

        // Keep the mod-color metadata for status/debugging, but the editor uses the
        // indexed GET_VEHICLE_COLOURS pair as the authoritative base paint state.
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
        if (vehicle == 0 || !PaintChoiceAllowed(PaintTarget::Primary, choice)
            || !UsesIndexedVehicleColourPath(choice.palette))
        {
            return false;
        }

        int primary{};
        int secondary{};
        if (!m_Backend.GetVehicleColours(vehicle, primary, secondary))
            return false;
        if (!m_Backend.SetVehicleColours(vehicle, choice.colorIndex, secondary))
            return false;

        int observedPrimary{};
        int observedSecondary{};
        if (!m_Backend.GetVehicleColours(vehicle, observedPrimary, observedSecondary)
            || observedPrimary != choice.colorIndex
            || observedSecondary != secondary)
        {
            return false;
        }

        // Indexed paint is hidden by a custom RGB override. Clear and verify it so
        // a successful button press always means the selected base paint is visible.
        if (!m_Backend.ClearCustomPrimaryColour(vehicle))
            return false;
        bool custom{};
        if (!m_Backend.IsPrimaryColourCustom(vehicle, custom) || custom)
            return false;

        return m_Backend.GetVehicleColours(vehicle, observedPrimary, observedSecondary)
            && observedPrimary == choice.colorIndex
            && observedSecondary == secondary;
    }

    bool VehiclePaintController::SetSecondary(VehicleHandle vehicle, PaintChoice choice) noexcept
    {
        if (vehicle == 0 || !PaintChoiceAllowed(PaintTarget::Secondary, choice)
            || !UsesIndexedVehicleColourPath(choice.palette))
        {
            return false;
        }

        int primary{};
        int secondary{};
        if (!m_Backend.GetVehicleColours(vehicle, primary, secondary))
            return false;
        if (!m_Backend.SetVehicleColours(vehicle, primary, choice.colorIndex))
            return false;

        int observedPrimary{};
        int observedSecondary{};
        if (!m_Backend.GetVehicleColours(vehicle, observedPrimary, observedSecondary)
            || observedPrimary != primary
            || observedSecondary != choice.colorIndex)
        {
            return false;
        }

        if (!m_Backend.ClearCustomSecondaryColour(vehicle))
            return false;
        bool custom{};
        if (!m_Backend.IsSecondaryColourCustom(vehicle, custom) || custom)
            return false;

        return m_Backend.GetVehicleColours(vehicle, observedPrimary, observedSecondary)
            && observedPrimary == primary
            && observedSecondary == choice.colorIndex;
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
        if (!m_Backend.SetVehicleExtraColours(vehicle, colorIndex, wheel))
            return false;

        int observedPearlescent{};
        int observedWheel{};
        return m_Backend.GetVehicleExtraColours(vehicle, observedPearlescent, observedWheel)
            && observedPearlescent == colorIndex
            && observedWheel == wheel;
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
        if (!m_Backend.SetVehicleExtraColours(vehicle, pearlescent, colorIndex))
            return false;

        int observedPearlescent{};
        int observedWheel{};
        return m_Backend.GetVehicleExtraColours(vehicle, observedPearlescent, observedWheel)
            && observedPearlescent == pearlescent
            && observedWheel == colorIndex;
    }

    bool VehiclePaintController::SetCustomPrimary(VehicleHandle vehicle, RgbColor color) noexcept
    {
        if (vehicle == 0 || !m_Backend.SetCustomPrimaryColour(vehicle, color))
            return false;

        bool custom{};
        RgbColor observed{};
        return m_Backend.IsPrimaryColourCustom(vehicle, custom)
            && custom
            && m_Backend.GetCustomPrimaryColour(vehicle, observed)
            && observed == color;
    }

    bool VehiclePaintController::SetCustomSecondary(VehicleHandle vehicle, RgbColor color) noexcept
    {
        if (vehicle == 0 || !m_Backend.SetCustomSecondaryColour(vehicle, color))
            return false;

        bool custom{};
        RgbColor observed{};
        return m_Backend.IsSecondaryColourCustom(vehicle, custom)
            && custom
            && m_Backend.GetCustomSecondaryColour(vehicle, observed)
            && observed == color;
    }

    bool VehiclePaintController::ClearCustomPrimary(VehicleHandle vehicle) noexcept
    {
        if (vehicle == 0 || !m_Backend.ClearCustomPrimaryColour(vehicle))
            return false;

        bool custom{};
        return m_Backend.IsPrimaryColourCustom(vehicle, custom) && !custom;
    }

    bool VehiclePaintController::ClearCustomSecondary(VehicleHandle vehicle) noexcept
    {
        if (vehicle == 0 || !m_Backend.ClearCustomSecondaryColour(vehicle))
            return false;

        bool custom{};
        return m_Backend.IsSecondaryColourCustom(vehicle, custom) && !custom;
    }
}
