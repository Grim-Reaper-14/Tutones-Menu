#pragma once

#include "VehiclePaintBackend.hpp"

namespace Tutones::Game::Paint
{
    class VehiclePaintController final
    {
    public:
        explicit VehiclePaintController(IVehiclePaintBackend& backend) noexcept
            : m_Backend(backend)
        {
        }

        [[nodiscard]] bool SetPrimary(VehicleHandle vehicle, PaintChoice choice) noexcept;
        [[nodiscard]] bool SetSecondary(VehicleHandle vehicle, PaintChoice choice) noexcept;

    private:
        IVehiclePaintBackend& m_Backend;
    };
}
