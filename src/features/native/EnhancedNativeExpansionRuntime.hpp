#pragma once

#include "../../game/Natives.hpp"
#include "../../game/native/NativeInvoker.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <string>

namespace Tutones::Game::NativeTools
{
    class EnhancedNativeExpansionRuntime final
    {
    public:
        static EnhancedNativeExpansionRuntime& Get() noexcept
        {
            static EnhancedNativeExpansionRuntime instance;
            return instance;
        }

        [[nodiscard]] bool QueueSetVehicleSpeed(float speed)
        {
            speed = std::clamp(speed, -250.0f, 500.0f);
            return Runtime::GameRuntime::Get().Enqueue([speed] {
                const auto ped = Natives::PlayerPedId();
                if (!ped || *ped == 0)
                    return;
                const auto vehicle = Natives::GetVehiclePedIsIn(*ped, false);
                if (!vehicle || *vehicle == 0)
                    return;
                static_cast<void>(Native::NativeInvoker::InvokeVoid(Native::NativeId::SetVehicleForwardSpeed, *vehicle, speed));
            });
        }

        [[nodiscard]] bool QueueSetVehiclePlate(std::string text)
        {
            if (text.size() > 8)
                text.resize(8);
            return Runtime::GameRuntime::Get().Enqueue([text = std::move(text)] {
                const auto ped = Natives::PlayerPedId();
                if (!ped || *ped == 0)
                    return;
                const auto vehicle = Natives::GetVehiclePedIsIn(*ped, false);
                if (!vehicle || *vehicle == 0)
                    return;
                static_cast<void>(Native::NativeInvoker::InvokeVoid(Native::NativeId::SetVehicleNumberPlateText, *vehicle, text.c_str()));
            });
        }

        [[nodiscard]] bool QueueSetVehicleWindowTint(int tint)
        {
            tint = std::clamp(tint, 0, 6);
            return Runtime::GameRuntime::Get().Enqueue([tint] {
                const auto ped = Natives::PlayerPedId();
                if (!ped || *ped == 0)
                    return;
                const auto vehicle = Natives::GetVehiclePedIsIn(*ped, false);
                if (!vehicle || *vehicle == 0)
                    return;
                static_cast<void>(Native::NativeInvoker::InvokeVoid(Native::NativeId::SetVehicleWindowTint, *vehicle, tint));
            });
        }

        [[nodiscard]] bool QueueRepairVehicle()
        {
            return Runtime::GameRuntime::Get().Enqueue([] {
                const auto ped = Natives::PlayerPedId();
                if (!ped || *ped == 0)
                    return;
                const auto vehicle = Natives::GetVehiclePedIsIn(*ped, false);
                if (!vehicle || *vehicle == 0)
                    return;
                static_cast<void>(Native::NativeInvoker::InvokeVoid(Native::NativeId::SetVehicleFixed, *vehicle));
                static_cast<void>(Native::NativeInvoker::InvokeVoid(Native::NativeId::SetVehicleDeformationFixed, *vehicle));
                static_cast<void>(Native::NativeInvoker::InvokeVoid(Native::NativeId::SetVehicleBodyHealth, *vehicle, 1000.0f));
            });
        }

        [[nodiscard]] bool QueueSetVehicleGravity(bool enabled)
        {
            return Runtime::GameRuntime::Get().Enqueue([enabled] {
                const auto ped = Natives::PlayerPedId();
                if (!ped || *ped == 0)
                    return;
                const auto vehicle = Natives::GetVehiclePedIsIn(*ped, false);
                if (!vehicle || *vehicle == 0)
                    return;
                static_cast<void>(Native::NativeInvoker::InvokeVoid(Native::NativeId::SetVehicleGravity, *vehicle, static_cast<std::int32_t>(enabled)));
            });
        }

        [[nodiscard]] bool QueueSetEntityGhost(bool enabled)
        {
            return Runtime::GameRuntime::Get().Enqueue([enabled] {
                const auto ped = Natives::PlayerPedId();
                if (!ped || *ped == 0)
                    return;
                static_cast<void>(Native::NativeInvoker::InvokeVoid(Native::NativeId::SetEntityCollision, *ped, static_cast<std::int32_t>(!enabled), std::int32_t{0}));
                static_cast<void>(Native::NativeInvoker::InvokeVoid(Native::NativeId::SetEntityAlpha, *ped, enabled ? 120 : 255, std::int32_t{0}));
            });
        }

    private:
        EnhancedNativeExpansionRuntime() = default;
    };
}
