#pragma once

#include "GamePointers.hpp"
#include "native/NativeCallContext.hpp"
#include "native/NativeRegistry.hpp"

#include <Windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace Tutones::Game::MiscNatives
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
            SetClockTime,
            PauseClock,
            GetClockHours,
            GetClockMinutes,
            NetworkOverrideClockTime,
            NetworkClearClockTimeOverride,
            SetWeatherTypeNowPersist,
            SetWeatherTypePersist,
            SetOverrideWeather,
            ClearOverrideWeather,
            SetArtificialLightsState,
            GetGameplayCamFov,
            StopGameplayCamShaking,
            HandlerCount,
        };

        inline std::array<Native::NativeHandler, HandlerCount>& Handlers() noexcept
        {
            static std::array<Native::NativeHandler, HandlerCount> handlers{};
            return handlers;
        }

        [[nodiscard]] inline bool IsExecutableAddress(std::uintptr_t address) noexcept
        {
            if (address == 0)
                return false;

            MEMORY_BASIC_INFORMATION memory{};
            if (::VirtualQuery(reinterpret_cast<const void*>(address), &memory, sizeof(memory)) != sizeof(memory))
                return false;
            if (memory.State != MEM_COMMIT || (memory.Protect & PAGE_GUARD) != 0 || memory.Protect == PAGE_NOACCESS)
                return false;

            switch (memory.Protect & 0xFF)
            {
            case PAGE_EXECUTE:
            case PAGE_EXECUTE_READ:
            case PAGE_EXECUTE_READWRITE:
            case PAGE_EXECUTE_WRITECOPY:
                return true;
            default:
                return false;
            }
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

            // Current GTA V Enhanced targets verified against YimMenuV2's Enhanced
            // crossmap. Time uses the NETWORK clock natives because local CLOCK natives
            // are routinely overwritten in GTA Online. Weather has both one-shot persist
            // and per-frame override/clear handlers so the runtime can own its lifecycle.
            std::array<std::uint64_t, HandlerCount> slots{
                0xCBE10A13619B9FAAull, // SET_CLOCK_TIME
                0xB9C1EC5EDDAAA115ull, // PAUSE_CLOCK
                0x5295501D0862870Dull, // GET_CLOCK_HOURS
                0x18E502A71E28968Cull, // GET_CLOCK_MINUTES
                0xAFD3BC0F6EBB5474ull, // NETWORK_OVERRIDE_CLOCK_TIME
                0x2B7C09622E980A72ull, // NETWORK_CLEAR_CLOCK_TIME_OVERRIDE
                0xE38A58649E049502ull, // SET_WEATHER_TYPE_NOW_PERSIST
                0xFD5A87843250F1B3ull, // SET_WEATHER_TYPE_PERSIST
                0x88791F880F624022ull, // SET_OVERRIDE_WEATHER
                0x58A3B74F26D2B532ull, // CLEAR_OVERRIDE_WEATHER
                0x771FE86D2A331DD7ull, // SET_ARTIFICIAL_LIGHTS_STATE
                0x9FA6E15C7A998E4Full, // GET_GAMEPLAY_CAM_FOV
                0x9AFEC71EEA2F7754ull, // STOP_GAMEPLAY_CAM_SHAKING
            };

            NativeProgram program{};
            program.nativeCount = static_cast<std::uint32_t>(slots.size());
            program.nativeEntrypoints = reinterpret_cast<Native::NativeHandler*>(slots.data());
            init(&program);

            for (std::size_t index = 0; index < slots.size(); ++index)
            {
                const auto address = static_cast<std::uintptr_t>(slots[index]);
                if (!IsExecutableAddress(address))
                {
                    handlers.fill(nullptr);
                    return false;
                }
                handlers[index] = reinterpret_cast<Native::NativeHandler>(address);
            }
            return true;
        }
    }

    inline bool SetClockTime(int hour, int minute, int second = 0) noexcept
    {
        if (!Detail::ResolveHandlers())
            return false;
        Native::CallContext context;
        if (!context.PushArg(hour) || !context.PushArg(minute) || !context.PushArg(second))
            return false;
        Detail::Handlers()[Detail::SetClockTime](&context);
        return true;
    }

    inline bool PauseClock(bool paused) noexcept
    {
        if (!Detail::ResolveHandlers())
            return false;
        Native::CallContext context;
        if (!context.PushArg(static_cast<std::int32_t>(paused)))
            return false;
        Detail::Handlers()[Detail::PauseClock](&context);
        return true;
    }

    [[nodiscard]] inline std::optional<int> GetClockHours() noexcept
    {
        if (!Detail::ResolveHandlers())
            return std::nullopt;
        Native::CallContext context;
        Detail::Handlers()[Detail::GetClockHours](&context);
        return context.GetReturnValue<int>();
    }

    [[nodiscard]] inline std::optional<int> GetClockMinutes() noexcept
    {
        if (!Detail::ResolveHandlers())
            return std::nullopt;
        Native::CallContext context;
        Detail::Handlers()[Detail::GetClockMinutes](&context);
        return context.GetReturnValue<int>();
    }

    inline bool NetworkOverrideClockTime(int hour, int minute, int second = 0) noexcept
    {
        if (!Detail::ResolveHandlers())
            return false;
        Native::CallContext context;
        if (!context.PushArg(hour) || !context.PushArg(minute) || !context.PushArg(second))
            return false;
        Detail::Handlers()[Detail::NetworkOverrideClockTime](&context);
        return true;
    }

    inline bool NetworkClearClockTimeOverride() noexcept
    {
        if (!Detail::ResolveHandlers())
            return false;
        Native::CallContext context;
        Detail::Handlers()[Detail::NetworkClearClockTimeOverride](&context);
        return true;
    }

    inline bool SetWeatherTypeNowPersist(const char* weather) noexcept
    {
        if (!weather || !*weather || !Detail::ResolveHandlers())
            return false;
        Native::CallContext context;
        if (!context.PushArg(weather))
            return false;
        Detail::Handlers()[Detail::SetWeatherTypeNowPersist](&context);
        return true;
    }

    inline bool SetWeatherTypePersist(const char* weather) noexcept
    {
        if (!weather || !*weather || !Detail::ResolveHandlers())
            return false;
        Native::CallContext context;
        if (!context.PushArg(weather))
            return false;
        Detail::Handlers()[Detail::SetWeatherTypePersist](&context);
        return true;
    }

    inline bool SetOverrideWeather(const char* weather) noexcept
    {
        if (!weather || !*weather || !Detail::ResolveHandlers())
            return false;
        Native::CallContext context;
        if (!context.PushArg(weather))
            return false;
        Detail::Handlers()[Detail::SetOverrideWeather](&context);
        return true;
    }

    inline bool ClearOverrideWeather() noexcept
    {
        if (!Detail::ResolveHandlers())
            return false;
        Native::CallContext context;
        Detail::Handlers()[Detail::ClearOverrideWeather](&context);
        return true;
    }

    inline bool SetArtificialLightsState(bool enabled) noexcept
    {
        if (!Detail::ResolveHandlers())
            return false;
        Native::CallContext context;
        if (!context.PushArg(static_cast<std::int32_t>(enabled)))
            return false;
        Detail::Handlers()[Detail::SetArtificialLightsState](&context);
        return true;
    }

    [[nodiscard]] inline std::optional<float> GetGameplayCamFov() noexcept
    {
        if (!Detail::ResolveHandlers())
            return std::nullopt;
        Native::CallContext context;
        Detail::Handlers()[Detail::GetGameplayCamFov](&context);
        return context.GetReturnValue<float>();
    }

    inline bool StopGameplayCamShaking(bool immediately = true) noexcept
    {
        if (!Detail::ResolveHandlers())
            return false;
        Native::CallContext context;
        if (!context.PushArg(static_cast<std::int32_t>(immediately)))
            return false;
        Detail::Handlers()[Detail::StopGameplayCamShaking](&context);
        return true;
    }
}
