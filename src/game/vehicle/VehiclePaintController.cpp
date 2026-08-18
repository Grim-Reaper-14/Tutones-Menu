#include "VehiclePaintController.hpp"

namespace Tutones::Game::Paint
{
    bool VehiclePaintController::SetPrimary(VehicleHandle vehicle, PaintChoice choice) noexcept
    {
        if (vehicle == 0 || !IsIndexedColour(choice.colorIndex))
            return false;

        int primary{};
        int secondary{};
        if (!m_Backend.GetVehicleColours(vehicle, primary, secondary))
            return false;

        return m_Backend.SetVehicleColours(vehicle, choice.colorIndex, secondary);
    }

    bool VehiclePaintController::SetSecondary(VehicleHandle vehicle, PaintChoice choice) noexcept
    {
        if (vehicle == 0 || !IsIndexedColour(choice.colorIndex))
            return false;

        int primary{};
        int secondary{};
        if (!m_Backend.GetVehicleColours(vehicle, primary, secondary))
            return false;

        return m_Backend.SetVehicleColours(vehicle, primary, choice.colorIndex);
    }
}
