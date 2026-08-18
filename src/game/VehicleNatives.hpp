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
    }
}
