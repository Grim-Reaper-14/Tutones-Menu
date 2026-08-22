#pragma once

#include "GamePointers.hpp"
#include "memory/PatternScanner.hpp"
#include "types/NetworkPlayerTypes.hpp"

#include <Windows.h>

#include <cstddef>
#include <cstdint>

namespace Tutones::Game::NetworkPlayerManager
{
    namespace Detail
    {
        [[nodiscard]] inline bool IsReadableProtection(DWORD protection) noexcept
        {
            if ((protection & PAGE_GUARD) != 0 || (protection & PAGE_NOACCESS) != 0)
                return false;

            switch (protection & 0xFFu)
            {
            case PAGE_READONLY:
            case PAGE_READWRITE:
            case PAGE_WRITECOPY:
            case PAGE_EXECUTE_READ:
            case PAGE_EXECUTE_READWRITE:
            case PAGE_EXECUTE_WRITECOPY:
                return true;
            default:
                return false;
            }
        }
    }

    [[nodiscard]] inline bool IsReadableRange(const void* address, std::size_t size) noexcept
    {
        if (!address || size == 0)
            return false;

        auto current = reinterpret_cast<std::uintptr_t>(address);
        std::size_t remaining = size;
        while (remaining > 0)
        {
            MEMORY_BASIC_INFORMATION memory{};
            if (::VirtualQuery(reinterpret_cast<const void*>(current), &memory, sizeof(memory)) == 0)
                return false;
            if (memory.State != MEM_COMMIT || !Detail::IsReadableProtection(memory.Protect))
                return false;

            const auto regionStart = reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
            const auto regionEnd = regionStart + memory.RegionSize;
            if (current < regionStart || current >= regionEnd)
                return false;

            const auto available = static_cast<std::size_t>(regionEnd - current);
            if (available >= remaining)
                return true;

            remaining -= available;
            current = regionEnd;
        }

        return true;
    }

    [[nodiscard]] inline bool IsPlayerReadable(const Types::NetworkPlayerView* player) noexcept
    {
        return IsReadableRange(player, sizeof(Types::NetworkPlayerView));
    }

    [[nodiscard]] inline Types::NetworkPlayerManagerView* Get() noexcept
    {
        static std::uintptr_t cachedModuleBase{};
        static Types::NetworkPlayerManagerView** cachedManagerSlot{};

        const auto& module = GamePointers::Get().Module();
        if (!module.IsValid())
            return nullptr;

        if (cachedModuleBase != module.Base())
        {
            cachedModuleBase = module.Base();
            cachedManagerSlot = nullptr;
        }

        if (!cachedManagerSlot)
        {
            constexpr auto pattern = "75 0E 48 8B 05 ? ? ? ? 48 8B 88 F0 00 00 00";
            auto* match = Memory::PatternScanner::FindFirst(module, pattern);
            if (!match)
                return nullptr;

            auto* slotAddress = Memory::PatternScanner::ResolveRip(match + 5);
            if (!slotAddress || !module.Contains(slotAddress))
                return nullptr;

            cachedManagerSlot = reinterpret_cast<Types::NetworkPlayerManagerView**>(slotAddress);
        }

        if (!cachedManagerSlot || !IsReadableRange(cachedManagerSlot, sizeof(*cachedManagerSlot)))
            return nullptr;

        auto* manager = *cachedManagerSlot;
        constexpr std::size_t RequiredManagerBytes = 0x2A8;
        if (!IsReadableRange(manager, RequiredManagerBytes))
            return nullptr;

        if (manager->maxPlayers == 0 || manager->maxPlayers > 32)
            return nullptr;
        if (manager->loadedPlayerCount < 0 || manager->loadedPlayerCount > 32)
            return nullptr;
        if (manager->physicalPlayerCount < 0 || manager->physicalPlayerCount > 32)
            return nullptr;
        if (manager->nonLocalPhysicalPlayerCount < 0 || manager->nonLocalPhysicalPlayerCount > 31)
            return nullptr;

        return manager;
    }
}
