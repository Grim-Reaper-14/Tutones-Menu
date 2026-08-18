#pragma once

#include "Natives.hpp"
#include "PlayerNatives.hpp"

#include <cstdint>
#include <optional>

namespace Tutones::Game
{
    using Vector3 = Native::NativeVector3;

    namespace VehicleNatives
    {
        [[nodiscard]] inline std::optional<float> GetEntityHeading(Entity entity) noexcept
        {
            return Native::NativeInvoker::Invoke<float>(Native::NativeId::GetEntityHeading, entity);
        }

        [[nodiscard]] inline std::optional<Vector3> GetEntityCoords(Entity entity, bool alive = false) noexcept
        {
            return Native::NativeInvoker::Invoke<Vector3>(
                Native::NativeId::GetEntityCoords,
                entity,
                static_cast<std::int32_t>(alive));
        }

        [[nodiscard]] inline std::optional<Vehicle> GetVehiclePedIsUsing(Ped ped) noexcept
        {
            return Native::NativeInvoker::Invoke<Vehicle>(Native::NativeId::GetVehiclePedIsUsing, ped);
        }

        [[nodiscard]] inline std::optional<bool> IsModelAVehicle(Hash model) noexcept
        {
            const auto result = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::IsModelAVehicle, model);
            return result ? std::optional<bool>(*result != 0) : std::nullopt;
        }

        [[nodiscard]] inline std::optional<Vehicle> CreateVehicle(
            Hash model,
            float x,
            float y,
            float z,
            float heading,
            bool isNetwork = true,
            bool netMissionEntity = false,
            bool p7 = false) noexcept
        {
            return Native::NativeInvoker::Invoke<Vehicle>(
                Native::NativeId::CreateVehicle,
                model,
                x,
                y,
                z,
                heading,
                static_cast<std::int32_t>(isNetwork),
                static_cast<std::int32_t>(netMissionEntity),
                static_cast<std::int32_t>(p7));
        }

        inline bool SetPedIntoVehicle(Ped ped, Vehicle vehicle, int seatIndex = -1) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetPedIntoVehicle,
                ped,
                vehicle,
                seatIndex);
        }

        [[nodiscard]] inline std::optional<int> GetVehicleClassFromName(Hash model) noexcept
        {
            return Native::NativeInvoker::Invoke<int>(Native::NativeId::GetVehicleClassFromName, model);
        }

        [[nodiscard]] inline std::optional<const char*> GetDisplayNameFromVehicleModel(Hash model) noexcept
        {
            return Native::NativeInvoker::Invoke<const char*>(Native::NativeId::GetDisplayNameFromVehicleModel, model);
        }

        [[nodiscard]] inline std::optional<const char*> GetMakeNameFromVehicleModel(Hash model) noexcept
        {
            return Native::NativeInvoker::Invoke<const char*>(Native::NativeId::GetMakeNameFromVehicleModel, model);
        }

        [[nodiscard]] inline std::optional<Vehicle> GetClosestVehicle(
            float x, float y, float z, float radius, Hash model = 0, int flags = 70) noexcept
        {
            return Native::NativeInvoker::Invoke<Vehicle>(
                Native::NativeId::GetClosestVehicle, x, y, z, radius, model, flags);
        }

        [[nodiscard]] inline std::optional<const char*> GetModTextLabel(Vehicle vehicle, int modType, int modIndex) noexcept
        {
            return Native::NativeInvoker::Invoke<const char*>(Native::NativeId::GetModTextLabel, vehicle, modType, modIndex);
        }

        [[nodiscard]] inline std::optional<const char*> GetLabelText(const char* label) noexcept
        {
            return Native::NativeInvoker::Invoke<const char*>(Native::NativeId::GetLabelText, label);
        }

        inline bool GetVehicleTyreSmokeColor(Vehicle vehicle, int& red, int& green, int& blue) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::GetVehicleTyreSmokeColor, vehicle, &red, &green, &blue);
        }

        inline bool SetVehicleTyreSmokeColor(Vehicle vehicle, int red, int green, int blue) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::SetVehicleTyreSmokeColor, vehicle, red, green, blue);
        }

        [[nodiscard]] inline std::optional<int> GetVehicleXenonLightColor(Vehicle vehicle) noexcept
        {
            return Native::NativeInvoker::Invoke<int>(Native::NativeId::GetVehicleXenonLightColor, vehicle);
        }

        inline bool SetVehicleXenonLightColor(Vehicle vehicle, int colorIndex) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::SetVehicleXenonLightColor, vehicle, colorIndex);
        }

        [[nodiscard]] inline std::optional<bool> GetVehicleNeonEnabled(Vehicle vehicle, int index) noexcept
        {
            const auto result = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::GetVehicleNeonEnabled, vehicle, index);
            return result ? std::optional<bool>(*result != 0) : std::nullopt;
        }

        inline bool SetVehicleNeonEnabled(Vehicle vehicle, int index, bool enabled) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetVehicleNeonEnabled, vehicle, index, static_cast<std::int32_t>(enabled));
        }

        inline bool GetVehicleNeonColour(Vehicle vehicle, int& red, int& green, int& blue) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::GetVehicleNeonColour, vehicle, &red, &green, &blue);
        }

        inline bool SetVehicleNeonColour(Vehicle vehicle, int red, int green, int blue) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::SetVehicleNeonColour, vehicle, red, green, blue);
        }

        [[nodiscard]] inline std::optional<bool> GetVehicleTyresCanBurst(Vehicle vehicle) noexcept
        {
            const auto result = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::GetVehicleTyresCanBurst, vehicle);
            return result ? std::optional<bool>(*result != 0) : std::nullopt;
        }

        inline bool SetVehicleTyresCanBurst(Vehicle vehicle, bool canBurst) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetVehicleTyresCanBurst, vehicle, static_cast<std::int32_t>(canBurst));
        }

        [[nodiscard]] inline std::optional<bool> GetDriftTyresSet(Vehicle vehicle) noexcept
        {
            const auto result = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::GetDriftTyresSet, vehicle);
            return result ? std::optional<bool>(*result != 0) : std::nullopt;
        }

        inline bool SetDriftTyres(Vehicle vehicle, bool enabled) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetDriftTyres, vehicle, static_cast<std::int32_t>(enabled));
        }
    }
}
