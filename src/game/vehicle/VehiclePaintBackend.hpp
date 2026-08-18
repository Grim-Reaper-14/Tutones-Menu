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
        virtual bool GetVehicleExtraColours(VehicleHandle vehicle, int& pearlescent, int& wheel) noexcept = 0;
        virtual bool SetVehicleExtraColours(VehicleHandle vehicle, int pearlescent, int wheel) noexcept = 0;

        virtual bool GetVehicleModColor1(VehicleHandle vehicle, int& paintType, int& color, int& pearlescent) noexcept = 0;
        virtual bool SetVehicleModColor1(VehicleHandle vehicle, int paintType, int color, int pearlescent) noexcept = 0;
        virtual bool GetVehicleModColor2(VehicleHandle vehicle, int& paintType, int& color) noexcept = 0;
        virtual bool SetVehicleModColor2(VehicleHandle vehicle, int paintType, int color) noexcept = 0;

        virtual bool IsPrimaryColourCustom(VehicleHandle vehicle, bool& custom) noexcept = 0;
        virtual bool IsSecondaryColourCustom(VehicleHandle vehicle, bool& custom) noexcept = 0;
        virtual bool GetCustomPrimaryColour(VehicleHandle vehicle, RgbColor& color) noexcept = 0;
        virtual bool GetCustomSecondaryColour(VehicleHandle vehicle, RgbColor& color) noexcept = 0;
        virtual bool SetCustomPrimaryColour(VehicleHandle vehicle, RgbColor color) noexcept = 0;
        virtual bool SetCustomSecondaryColour(VehicleHandle vehicle, RgbColor color) noexcept = 0;
        virtual bool ClearCustomPrimaryColour(VehicleHandle vehicle) noexcept = 0;
        virtual bool ClearCustomSecondaryColour(VehicleHandle vehicle) noexcept = 0;
    };
}
