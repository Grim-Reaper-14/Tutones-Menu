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

        out.vehicle = vehicle;
        out.primaryColor = primary;
        out.secondaryColor = secondary;
        out.pearlescentColor = pearlescent;
        out.wheelColor = wheel;
        out.valid = true;
        return true;
    }

    bool VehiclePaintController::SetPrimary(VehicleHandle vehicle, PaintChoice choice) noexcept
    {
        if (vehicle == 0 || !PaintChoiceAllowed(PaintTarget::Primary, choice))
            return false;

        int primary{};
        int secondary{};
        if (!m_Backend.GetVehicleColours(vehicle, primary, secondary))
            return false;

        return m_Backend.SetVehicleColours(vehicle, choice.colorIndex, secondary);
    }

    bool VehiclePaintController::SetSecondary(VehicleHandle vehicle, PaintChoice choice) noexcept
    {
        if (vehicle == 0 || !PaintChoiceAllowed(PaintTarget::Secondary, choice))
            return false;

        int primary{};
        int secondary{};
        if (!m_Backend.GetVehicleColours(vehicle, primary, secondary))
            return false;

        return m_Backend.SetVehicleColours(vehicle, primary, choice.colorIndex);
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
}
