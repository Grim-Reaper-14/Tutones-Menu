#pragma once

#include <string_view>

namespace TutonesV2::Frontend
{
    enum class MenuCategory
    {
        Player,
        Vehicle,
        Weapon,
        Teleport,
        World,
        Network,
        Recovery,
        Heists,
        Lua,
        Settings
    };

    [[nodiscard]] constexpr std::string_view ToString(MenuCategory category) noexcept
    {
        switch (category)
        {
        case MenuCategory::Player: return "Player";
        case MenuCategory::Vehicle: return "Vehicle";
        case MenuCategory::Weapon: return "Weapon";
        case MenuCategory::Teleport: return "Teleport";
        case MenuCategory::World: return "World";
        case MenuCategory::Network: return "Network";
        case MenuCategory::Recovery: return "Recovery";
        case MenuCategory::Heists: return "Heists";
        case MenuCategory::Lua: return "Lua";
        case MenuCategory::Settings: return "Settings";
        }
        return "Unknown";
    }
}
