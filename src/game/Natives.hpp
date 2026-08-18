#pragma once

#include "native/NativeInvoker.hpp"

#include <cstdint>
#include <optional>

namespace Tutones::Game
{
    using Entity = std::int32_t;
    using Ped = std::int32_t;
    using Vehicle = std::int32_t;
    using Hash = std::uint32_t;

    namespace Natives
    {
        [[nodiscard]] inline std::optional<Ped> PlayerPedId() noexcept
        {
            return Native::NativeInvoker::Invoke<Ped>(Native::NativeId::PlayerPedId);
        }

        [[nodiscard]] inline std::optional<bool> DoesEntityExist(Entity entity) noexcept
        {
            const auto result = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::DoesEntityExist, entity);
            return result ? std::optional<bool>(*result != 0) : std::nullopt;
        }

        [[nodiscard]] inline std::optional<Hash> GetEntityModel(Entity entity) noexcept
        {
            return Native::NativeInvoker::Invoke<Hash>(Native::NativeId::GetEntityModel, entity);
        }

        [[nodiscard]] inline std::optional<bool> IsPedInAnyVehicle(Ped ped, bool includeEntering = false) noexcept
        {
            const auto result = Native::NativeInvoker::Invoke<std::int32_t>(
                Native::NativeId::IsPedInAnyVehicle,
                ped,
                static_cast<std::int32_t>(includeEntering));
            return result ? std::optional<bool>(*result != 0) : std::nullopt;
        }

        [[nodiscard]] inline std::optional<Vehicle> GetVehiclePedIsIn(Ped ped, bool includeEntering = false) noexcept
        {
            return Native::NativeInvoker::Invoke<Vehicle>(
                Native::NativeId::GetVehiclePedIsIn,
                ped,
                static_cast<std::int32_t>(includeEntering));
        }

        inline bool GetVehicleColours(Vehicle vehicle, int& primary, int& secondary) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::GetVehicleColours, vehicle, &primary, &secondary);
        }

        inline bool SetVehicleColours(Vehicle vehicle, int primary, int secondary) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::SetVehicleColours, vehicle, primary, secondary);
        }

        inline bool GetVehicleExtraColours(Vehicle vehicle, int& pearlescent, int& wheel) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::GetVehicleExtraColours, vehicle, &pearlescent, &wheel);
        }

        inline bool SetVehicleExtraColours(Vehicle vehicle, int pearlescent, int wheel) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::SetVehicleExtraColours, vehicle, pearlescent, wheel);
        }

        inline bool GetVehicleModColor1(Vehicle vehicle, int& paintType, int& color, int& pearlescent) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(
                Native::NativeId::GetVehicleModColor1,
                vehicle,
                &paintType,
                &color,
                &pearlescent);
        }

        inline bool SetVehicleModColor1(Vehicle vehicle, int paintType, int color, int pearlescent) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetVehicleModColor1,
                vehicle,
                paintType,
                color,
                pearlescent);
        }

        inline bool GetVehicleModColor2(Vehicle vehicle, int& paintType, int& color) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::GetVehicleModColor2, vehicle, &paintType, &color);
        }

        inline bool SetVehicleModColor2(Vehicle vehicle, int paintType, int color) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::SetVehicleModColor2, vehicle, paintType, color);
        }

        [[nodiscard]] inline std::optional<bool> GetIsVehiclePrimaryColourCustom(Vehicle vehicle) noexcept
        {
            const auto result = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::GetIsVehiclePrimaryColourCustom, vehicle);
            return result ? std::optional<bool>(*result != 0) : std::nullopt;
        }

        [[nodiscard]] inline std::optional<bool> GetIsVehicleSecondaryColourCustom(Vehicle vehicle) noexcept
        {
            const auto result = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::GetIsVehicleSecondaryColourCustom, vehicle);
            return result ? std::optional<bool>(*result != 0) : std::nullopt;
        }

        inline bool GetVehicleCustomPrimaryColour(Vehicle vehicle, int& red, int& green, int& blue) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::GetVehicleCustomPrimaryColour, vehicle, &red, &green, &blue);
        }

        inline bool GetVehicleCustomSecondaryColour(Vehicle vehicle, int& red, int& green, int& blue) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::GetVehicleCustomSecondaryColour, vehicle, &red, &green, &blue);
        }

        inline bool SetVehicleCustomPrimaryColour(Vehicle vehicle, int red, int green, int blue) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::SetVehicleCustomPrimaryColour, vehicle, red, green, blue);
        }

        inline bool SetVehicleCustomSecondaryColour(Vehicle vehicle, int red, int green, int blue) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::SetVehicleCustomSecondaryColour, vehicle, red, green, blue);
        }

        inline bool ClearVehicleCustomPrimaryColour(Vehicle vehicle) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::ClearVehicleCustomPrimaryColour, vehicle);
        }

        inline bool ClearVehicleCustomSecondaryColour(Vehicle vehicle) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::ClearVehicleCustomSecondaryColour, vehicle);
        }

        inline bool SetVehicleModKit(Vehicle vehicle, int modKit) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::SetVehicleModKit, vehicle, modKit);
        }

        [[nodiscard]] inline std::optional<int> GetVehicleWheelType(Vehicle vehicle) noexcept
        {
            return Native::NativeInvoker::Invoke<int>(Native::NativeId::GetVehicleWheelType, vehicle);
        }

        inline bool SetVehicleWheelType(Vehicle vehicle, int wheelType) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::SetVehicleWheelType, vehicle, wheelType);
        }

        inline bool SetVehicleMod(Vehicle vehicle, int modType, int modIndex, bool customTires) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetVehicleMod,
                vehicle,
                modType,
                modIndex,
                static_cast<std::int32_t>(customTires));
        }

        [[nodiscard]] inline std::optional<int> GetVehicleMod(Vehicle vehicle, int modType) noexcept
        {
            return Native::NativeInvoker::Invoke<int>(Native::NativeId::GetVehicleMod, vehicle, modType);
        }

        [[nodiscard]] inline std::optional<bool> GetVehicleModVariation(Vehicle vehicle, int modType) noexcept
        {
            const auto result = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::GetVehicleModVariation, vehicle, modType);
            return result ? std::optional<bool>(*result != 0) : std::nullopt;
        }

        [[nodiscard]] inline std::optional<int> GetNumVehicleMods(Vehicle vehicle, int modType) noexcept
        {
            return Native::NativeInvoker::Invoke<int>(Native::NativeId::GetNumVehicleMods, vehicle, modType);
        }

        inline bool RemoveVehicleMod(Vehicle vehicle, int modType) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::RemoveVehicleMod, vehicle, modType);
        }

        inline bool ToggleVehicleMod(Vehicle vehicle, int modType, bool enabled) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(
                Native::NativeId::ToggleVehicleMod,
                vehicle,
                modType,
                static_cast<std::int32_t>(enabled));
        }

        [[nodiscard]] inline std::optional<bool> IsToggleModOn(Vehicle vehicle, int modType) noexcept
        {
            const auto result = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::IsToggleModOn, vehicle, modType);
            return result ? std::optional<bool>(*result != 0) : std::nullopt;
        }

        inline bool SetVehicleFixed(Vehicle vehicle) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::SetVehicleFixed, vehicle);
        }

        inline bool SetVehicleDirtLevel(Vehicle vehicle, float dirtLevel) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::SetVehicleDirtLevel, vehicle, dirtLevel);
        }

        [[nodiscard]] inline std::optional<bool> SetVehicleOnGroundProperly(Vehicle vehicle, float probeHeight) noexcept
        {
            const auto result = Native::NativeInvoker::Invoke<std::int32_t>(
                Native::NativeId::SetVehicleOnGroundProperly,
                vehicle,
                probeHeight);
            return result ? std::optional<bool>(*result != 0) : std::nullopt;
        }
    }
}
