#pragma once

#include "VehiclePaintBackend.hpp"

namespace Tutones::Game::Paint
{
    class TutonesVehiclePaintBackend final : public IVehiclePaintBackend
    {
    public:
        bool GetVehicleColours(VehicleHandle vehicle, int& primary, int& secondary) noexcept override;
        bool SetVehicleColours(VehicleHandle vehicle, int primary, int secondary) noexcept override;
        bool GetVehicleExtraColours(VehicleHandle vehicle, int& pearlescent, int& wheel) noexcept override;
        bool SetVehicleExtraColours(VehicleHandle vehicle, int pearlescent, int wheel) noexcept override;

        bool GetVehicleModColor1(VehicleHandle vehicle, int& paintType, int& color, int& pearlescent) noexcept override;
        bool SetVehicleModColor1(VehicleHandle vehicle, int paintType, int color, int pearlescent) noexcept override;
        bool GetVehicleModColor2(VehicleHandle vehicle, int& paintType, int& color) noexcept override;
        bool SetVehicleModColor2(VehicleHandle vehicle, int paintType, int color) noexcept override;

        bool IsPrimaryColourCustom(VehicleHandle vehicle, bool& custom) noexcept override;
        bool IsSecondaryColourCustom(VehicleHandle vehicle, bool& custom) noexcept override;
        bool GetCustomPrimaryColour(VehicleHandle vehicle, RgbColor& color) noexcept override;
        bool GetCustomSecondaryColour(VehicleHandle vehicle, RgbColor& color) noexcept override;
        bool SetCustomPrimaryColour(VehicleHandle vehicle, RgbColor color) noexcept override;
        bool SetCustomSecondaryColour(VehicleHandle vehicle, RgbColor color) noexcept override;
        bool ClearCustomPrimaryColour(VehicleHandle vehicle) noexcept override;
        bool ClearCustomSecondaryColour(VehicleHandle vehicle) noexcept override;
    };
}
