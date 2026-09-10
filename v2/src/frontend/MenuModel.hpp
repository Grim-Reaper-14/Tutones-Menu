#pragma once

#include "MenuCategory.hpp"

#include <array>
#include <cstddef>

namespace TutonesV2::Frontend
{
    class MenuModel final
    {
    public:
        static MenuModel& Get() noexcept;

        void Initialize() noexcept;
        void Select(MenuCategory category) noexcept;

        [[nodiscard]] MenuCategory Selected() const noexcept;
        [[nodiscard]] const std::array<MenuCategory, 10>& MainCategories() const noexcept;
        [[nodiscard]] bool HasMainCategory(MenuCategory category) const noexcept;

    private:
        MenuModel() = default;

        std::array<MenuCategory, 10> m_MainCategories{
            MenuCategory::Player,
            MenuCategory::Vehicle,
            MenuCategory::Weapon,
            MenuCategory::Teleport,
            MenuCategory::World,
            MenuCategory::Network,
            MenuCategory::Recovery,
            MenuCategory::Heists,
            MenuCategory::Lua,
            MenuCategory::Settings
        };
        MenuCategory m_Selected{MenuCategory::Player};
    };
}
