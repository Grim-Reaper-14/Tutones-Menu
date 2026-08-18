#include "TutonesVehiclePaintBackend.hpp"

#include "../Natives.hpp"

namespace Tutones::Game::Paint
{
    namespace
    {
        bool ToRgb(int red, int green, int blue, RgbColor& out) noexcept
        {
            if (red < 0 || red > 255 || green < 0 || green > 255 || blue < 0 || blue > 255)
                return false;

            out = {
                static_cast<std::uint8_t>(red),
                static_cast<std::uint8_t>(green),
                static_cast<std::uint8_t>(blue),
            };
            return true;
        }
    }

    bool TutonesVehiclePaintBackend::GetVehicleColours(VehicleHandle vehicle, int& primary, int& secondary) noexcept
    {
        return Natives::GetVehicleColours(vehicle, primary, secondary);
    }

    bool TutonesVehiclePaintBackend::SetVehicleColours(VehicleHandle vehicle, int primary, int secondary) noexcept
    {
        return Natives::SetVehicleColours(vehicle, primary, secondary);
    }

    bool TutonesVehiclePaintBackend::GetVehicleExtraColours(VehicleHandle vehicle, int& pearlescent, int& wheel) noexcept
    {
        return Natives::GetVehicleExtraColours(vehicle, pearlescent, wheel);
    }

    bool TutonesVehiclePaintBackend::SetVehicleExtraColours(VehicleHandle vehicle, int pearlescent, int wheel) noexcept
    {
        return Natives::SetVehicleExtraColours(vehicle, pearlescent, wheel);
    }

    bool TutonesVehiclePaintBackend::GetVehicleModColor1(
        VehicleHandle vehicle,
        int& paintType,
        int& color,
        int& pearlescent) noexcept
    {
        return Natives::GetVehicleModColor1(vehicle, paintType, color, pearlescent);
    }

    bool TutonesVehiclePaintBackend::SetVehicleModColor1(
        VehicleHandle vehicle,
        int paintType,
        int color,
        int pearlescent) noexcept
    {
        return Natives::SetVehicleModColor1(vehicle, paintType, color, pearlescent);
    }

    bool TutonesVehiclePaintBackend::GetVehicleModColor2(VehicleHandle vehicle, int& paintType, int& color) noexcept
    {
        return Natives::GetVehicleModColor2(vehicle, paintType, color);
    }

    bool TutonesVehiclePaintBackend::SetVehicleModColor2(VehicleHandle vehicle, int paintType, int color) noexcept
    {
        return Natives::SetVehicleModColor2(vehicle, paintType, color);
    }

    bool TutonesVehiclePaintBackend::IsPrimaryColourCustom(VehicleHandle vehicle, bool& custom) noexcept
    {
        const auto result = Natives::GetIsVehiclePrimaryColourCustom(vehicle);
        if (!result)
            return false;
        custom = *result;
        return true;
    }

    bool TutonesVehiclePaintBackend::IsSecondaryColourCustom(VehicleHandle vehicle, bool& custom) noexcept
    {
        const auto result = Natives::GetIsVehicleSecondaryColourCustom(vehicle);
        if (!result)
            return false;
        custom = *result;
        return true;
    }

    bool TutonesVehiclePaintBackend::GetCustomPrimaryColour(VehicleHandle vehicle, RgbColor& color) noexcept
    {
        int red{};
        int green{};
        int blue{};
        return Natives::GetVehicleCustomPrimaryColour(vehicle, red, green, blue)
            && ToRgb(red, green, blue, color);
    }

    bool TutonesVehiclePaintBackend::GetCustomSecondaryColour(VehicleHandle vehicle, RgbColor& color) noexcept
    {
        int red{};
        int green{};
        int blue{};
        return Natives::GetVehicleCustomSecondaryColour(vehicle, red, green, blue)
            && ToRgb(red, green, blue, color);
    }

    bool TutonesVehiclePaintBackend::SetCustomPrimaryColour(VehicleHandle vehicle, RgbColor color) noexcept
    {
        return Natives::SetVehicleCustomPrimaryColour(vehicle, color.red, color.green, color.blue);
    }

    bool TutonesVehiclePaintBackend::SetCustomSecondaryColour(VehicleHandle vehicle, RgbColor color) noexcept
    {
        return Natives::SetVehicleCustomSecondaryColour(vehicle, color.red, color.green, color.blue);
    }

    bool TutonesVehiclePaintBackend::ClearCustomPrimaryColour(VehicleHandle vehicle) noexcept
    {
        return Natives::ClearVehicleCustomPrimaryColour(vehicle);
    }

    bool TutonesVehiclePaintBackend::ClearCustomSecondaryColour(VehicleHandle vehicle) noexcept
    {
        return Natives::ClearVehicleCustomSecondaryColour(vehicle);
    }
}
