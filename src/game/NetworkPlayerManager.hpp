#pragma once

#include "GamePointers.hpp"
#include "memory/PatternScanner.hpp"
#include "types/NetworkPlayerTypes.hpp"

#include <cstdint>

namespace Tutones::Game::NetworkPlayerManager
{
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

        auto* manager = cachedManagerSlot ? *cachedManagerSlot : nullptr;
        if (!manager)
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
