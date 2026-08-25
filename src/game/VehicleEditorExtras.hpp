#pragma once

#include "GamePointers.hpp"
#include "Natives.hpp"
#include "native/NativeCallContext.hpp"
#include "native/NativeRegistry.hpp"

#include <Windows.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace Tutones::Game::VehicleEditorExtras
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
            GetWindowTint,
            SetWindowTint,
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
            if (handlers[GetWindowTint] && handlers[SetWindowTint])
                return true;
            if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread())
                return false;

            const auto init = GamePointers::Get().InitNativeTables();
            if (!init)
                return false;

            // Current GTA V Enhanced mappings from YimMenuV2's enhanced crossmap:
            // GET_VEHICLE_WINDOW_TINT  0x0EE21293DAD47C95 -> 0xDA63CE76F9AAB439
            // SET_VEHICLE_WINDOW_TINT  0x57C51E6BAD752696 -> 0xFE620ED8E0A3C209
            std::array<std::uint64_t, HandlerCount> slots{
                0xDA63CE76F9AAB439ull,
                0xFE620ED8E0A3C209ull,
            };

            NativeProgram program{};
            program.nativeCount = static_cast<std::uint32_t>(slots.size());
            program.nativeEntrypoints = reinterpret_cast<Native::NativeHandler*>(slots.data());
            init(&program);

            for (std::size_t index = 0; index < slots.size(); ++index)
            {
                const auto address = static_cast<std::uintptr_t>(slots[index]);
                if (address == 0)
                {
                    handlers.fill(nullptr);
                    return false;
                }

                MEMORY_BASIC_INFORMATION memory{};
                if (::VirtualQuery(reinterpret_cast<const void*>(address), &memory, sizeof(memory)) != sizeof(memory)
                    || memory.State != MEM_COMMIT
                    || (memory.Protect & PAGE_GUARD) != 0
                    || memory.Protect == PAGE_NOACCESS)
                {
                    handlers.fill(nullptr);
                    return false;
                }

                const DWORD protection = memory.Protect & 0xFF;
                if (protection != PAGE_EXECUTE
                    && protection != PAGE_EXECUTE_READ
                    && protection != PAGE_EXECUTE_READWRITE
                    && protection != PAGE_EXECUTE_WRITECOPY)
                {
                    handlers.fill(nullptr);
                    return false;
                }

                handlers[index] = reinterpret_cast<Native::NativeHandler>(address);
            }

            return true;
        }
    }

    [[nodiscard]] inline int GetWindowTint(Vehicle vehicle) noexcept
    {
        if (vehicle == 0 || !Detail::ResolveHandlers())
            return 0;

        Native::CallContext context;
        if (!context.PushArg(vehicle))
            return 0;
        Detail::Handlers()[Detail::GetWindowTint](&context);
        return context.GetReturnValue<int>();
    }

    inline bool SetWindowTint(Vehicle vehicle, int tint) noexcept
    {
        if (vehicle == 0 || tint < 0 || tint > 6 || !Detail::ResolveHandlers())
            return false;

        Native::CallContext context;
        if (!context.PushArg(vehicle) || !context.PushArg(tint))
            return false;
        Detail::Handlers()[Detail::SetWindowTint](&context);
        return true;
    }
}
