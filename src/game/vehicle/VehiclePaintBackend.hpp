#pragma once

#include "VehiclePaintTypes.hpp"

namespace Tutones::Game::Paint
{
    class IVehiclePaintBackend
    {
    public:
        virtual ~IVehiclePaintBackend() = default;

        virtual bool GetVehicleColours(VehicleHandle vehicle, int& primary, int& secondary) noexcept = 0;
        virtual bool SetVehicleColours(VehicleHandle vehicle, int primary, int secondary) noexcept = 0;
    };
}
