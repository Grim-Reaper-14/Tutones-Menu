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
    }
}
