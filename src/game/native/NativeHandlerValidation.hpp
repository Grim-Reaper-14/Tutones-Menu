#pragma once

#include "NativeCallContext.hpp"

#include <Windows.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace Tutones::Game::Native
{
    [[nodiscard]] inline bool IsExecutableHandlerAddress(std::uintptr_t address) noexcept
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

    [[nodiscard]] inline bool AssignValidatedHandler(
        std::uint64_t resolvedSlot,
        NativeHandler& destination) noexcept
    {
        const auto address = static_cast<std::uintptr_t>(resolvedSlot);
        if (!IsExecutableHandlerAddress(address))
        {
            destination = nullptr;
            return false;
        }

        destination = reinterpret_cast<NativeHandler>(address);
        return true;
    }

    template <std::size_t N>
    [[nodiscard]] inline bool AssignValidatedHandlers(
        const std::array<std::uint64_t, N>& resolvedSlots,
        std::array<NativeHandler, N>& destinations) noexcept
    {
        for (std::size_t index = 0; index < N; ++index)
        {
            if (!AssignValidatedHandler(resolvedSlots[index], destinations[index]))
            {
                destinations.fill(nullptr);
                return false;
            }
        }
        return true;
    }
}
