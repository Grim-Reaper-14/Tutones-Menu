#pragma once

#include "../../game/Natives.hpp"
#include "../../game/native/NativeInvoker.hpp"

#include <cstdint>

namespace Tutones::Game::Business
{
    // Thin typed facade over Tutones' central NativeRegistry/NativeInvoker.
    // It owns no native table, no raw handler pointers and no TLS state.
    class VehicleCargoNativeBridge final
    {
    public:
        static VehicleCargoNativeBridge& Get() noexcept
        {
            static VehicleCargoNativeBridge instance;
            return instance;
        }

        [[nodiscard]] bool NetworkHasControl(Entity entity, bool& out) noexcept
        {
            const auto value = Native::NativeInvoker::Invoke<std::int32_t>(
                Native::NativeId::NetworkHasControlOfEntity,
                entity);
            if (!value)
                return false;
            out = *value != 0;
            return true;
        }

        [[nodiscard]] bool NetworkRequestControl(Entity entity) noexcept
        {
            return Native::NativeInvoker::Invoke<std::int32_t>(
                Native::NativeId::NetworkRequestControlOfEntity,
                entity).has_value();
        }

        [[nodiscard]] bool GetBlipIterator(std::int32_t& out) noexcept
        {
            const auto value = Native::NativeInvoker::Invoke<std::int32_t>(
                Native::NativeId::GetBlipInfoIdIterator);
            if (!value)
                return false;
            out = *value;
            return true;
        }

        [[nodiscard]] bool GetFirstBlip(std::int32_t iterator, std::int32_t& out) noexcept
        {
            const auto value = Native::NativeInvoker::Invoke<std::int32_t>(
                Native::NativeId::GetFirstBlipInfoId,
                iterator);
            if (!value)
                return false;
            out = *value;
            return true;
        }

        [[nodiscard]] bool GetNextBlip(std::int32_t iterator, std::int32_t& out) noexcept
        {
            const auto value = Native::NativeInvoker::Invoke<std::int32_t>(
                Native::NativeId::GetNextBlipInfoId,
                iterator);
            if (!value)
                return false;
            out = *value;
            return true;
        }

        [[nodiscard]] bool BlipExists(std::int32_t blip, bool& out) noexcept
        {
            const auto value = Native::NativeInvoker::Invoke<std::int32_t>(
                Native::NativeId::DoesBlipExist,
                blip);
            if (!value)
                return false;
            out = *value != 0;
            return true;
        }

        [[nodiscard]] bool GetBlipEntity(std::int32_t blip, Entity& out) noexcept
        {
            const auto value = Native::NativeInvoker::Invoke<std::int32_t>(
                Native::NativeId::GetBlipInfoIdEntityIndex,
                blip);
            if (!value)
                return false;
            out = static_cast<Entity>(*value);
            return true;
        }

        [[nodiscard]] bool GetBlipCoords(
            std::int32_t blip,
            Native::NativeVector3& out) noexcept
        {
            const auto value = Native::NativeInvoker::Invoke<Native::NativeVector3>(
                Native::NativeId::GetBlipCoords,
                blip);
            if (!value)
                return false;
            out = *value;
            return true;
        }

        [[nodiscard]] bool RequestCollisionAt(float x, float y, float z) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(
                Native::NativeId::RequestCollisionAtCoord,
                x,
                y,
                z);
        }

        [[nodiscard]] bool SetCoordsNoOffset(Entity entity, float x, float y, float z) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetEntityCoordsNoOffset,
                entity,
                x,
                y,
                z,
                std::int32_t{1},
                std::int32_t{1},
                std::int32_t{1});
        }

        [[nodiscard]] bool SetHeading(Entity entity, float heading) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetEntityHeading,
                entity,
                heading);
        }

        [[nodiscard]] bool SetVelocity(Entity entity, float x, float y, float z) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetEntityVelocity,
                entity,
                x,
                y,
                z);
        }

    private:
        VehicleCargoNativeBridge() = default;
    };
}
