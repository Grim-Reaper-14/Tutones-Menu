#pragma once

#include "GamePointers.hpp"
#include "native/NativeCallContext.hpp"
#include "native/NativeRegistry.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace Tutones::Game::WorldNatives
{
    namespace Detail
    {
        struct NativeProgram final
        {
            std::byte pad00[0x2C]{};
            std::uint32_t nativeCount{};
            std::byte pad30[0x10]{};
            Native::NativeHandler* nativeEntrypoints{};
            std::byte pad48[0x38]{};
        };

        static_assert(offsetof(NativeProgram, nativeCount) == 0x2C);
        static_assert(offsetof(NativeProgram, nativeEntrypoints) == 0x40);
        static_assert(sizeof(NativeProgram) == 0x80);

        enum HandlerIndex : std::size_t
        {
            SetPedDensity,
            SetScenarioPedDensity,
            SetVehicleDensity,
            SetRandomVehicleDensity,
            SetParkedVehicleDensity,
            ClearAreaOfVehicles,
            ClearAreaOfObjects,
            ClearAreaOfPeds,
            HandlerCount,
        };

        inline std::array<Native::NativeHandler, HandlerCount>& Handlers() noexcept
        {
            static std::array<Native::NativeHandler, HandlerCount> handlers{};
            return handlers;
        }

        inline bool ResolveHandlers() noexcept
        {
            auto& handlers = Handlers();
            bool ready = true;
            for (const auto handler : handlers)
                ready = ready && handler != nullptr;
            if (ready)
                return true;

            if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread())
                return false;

            const auto init = GamePointers::Get().InitNativeTables();
            if (!init)
                return false;

            std::array<std::uint64_t, HandlerCount> slots{
                0xF9A2335AB37CF17Eull, // SET_PED_DENSITY_MULTIPLIER_THIS_FRAME
                0x0397A00D015A11D4ull, // SET_SCENARIO_PED_DENSITY_MULTIPLIER_THIS_FRAME
                0xA0265306DFF63938ull, // SET_VEHICLE_DENSITY_MULTIPLIER_THIS_FRAME
                0x23D563236A543309ull, // SET_RANDOM_VEHICLE_DENSITY_MULTIPLIER_THIS_FRAME
                0x40C1C94D5A5157C5ull, // SET_PARKED_VEHICLE_DENSITY_MULTIPLIER_THIS_FRAME
                0x60040CDD28AA1BC3ull, // CLEAR_AREA_OF_VEHICLES
                0xBAAB54D57B40765Eull, // CLEAR_AREA_OF_OBJECTS
                0x55F7AC4B2B875901ull, // CLEAR_AREA_OF_PEDS
            };

            NativeProgram program{};
            program.nativeCount = static_cast<std::uint32_t>(slots.size());
            program.nativeEntrypoints = reinterpret_cast<Native::NativeHandler*>(slots.data());
            init(&program);

            for (std::size_t index = 0; index < slots.size(); ++index)
                handlers[index] = reinterpret_cast<Native::NativeHandler>(static_cast<std::uintptr_t>(slots[index]));

            for (const auto handler : handlers)
            {
                if (!handler)
                    return false;
            }
            return true;
        }

        template <typename... Args>
        bool InvokeVoid(std::size_t index, Args... args) noexcept
        {
            if (index >= HandlerCount || !ResolveHandlers())
                return false;

            Native::CallContext context;
            if (!(context.PushArg(args) && ...))
                return false;
            Handlers()[index](&context);
            context.FixVectors();
            return true;
        }
    }

    inline bool SetPedDensity(float multiplier) noexcept
    {
        return Detail::InvokeVoid(Detail::SetPedDensity, multiplier);
    }

    inline bool SetScenarioPedDensity(float interiorMultiplier, float exteriorMultiplier) noexcept
    {
        return Detail::InvokeVoid(
            Detail::SetScenarioPedDensity,
            interiorMultiplier,
            exteriorMultiplier);
    }

    inline bool SetVehicleDensity(float multiplier) noexcept
    {
        return Detail::InvokeVoid(Detail::SetVehicleDensity, multiplier);
    }

    inline bool SetRandomVehicleDensity(float multiplier) noexcept
    {
        return Detail::InvokeVoid(Detail::SetRandomVehicleDensity, multiplier);
    }

    inline bool SetParkedVehicleDensity(float multiplier) noexcept
    {
        return Detail::InvokeVoid(Detail::SetParkedVehicleDensity, multiplier);
    }

    inline bool ClearAreaOfPeds(float x, float y, float z, float radius, int flags = 0) noexcept
    {
        return Detail::InvokeVoid(Detail::ClearAreaOfPeds, x, y, z, radius, flags);
    }

    inline bool ClearAreaOfObjects(float x, float y, float z, float radius, int flags = 0) noexcept
    {
        return Detail::InvokeVoid(Detail::ClearAreaOfObjects, x, y, z, radius, flags);
    }

    inline bool ClearAreaOfVehicles(float x, float y, float z, float radius) noexcept
    {
        return Detail::InvokeVoid(
            Detail::ClearAreaOfVehicles,
            x,
            y,
            z,
            radius,
            std::int32_t{0},
            std::int32_t{0},
            std::int32_t{0},
            std::int32_t{0},
            std::int32_t{0},
            std::int32_t{0},
            std::int32_t{0});
    }
}
