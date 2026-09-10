#include "MenuModel.hpp"

#include <algorithm>

namespace TutonesV2::Frontend
{
    MenuModel& MenuModel::Get() noexcept
    {
        static MenuModel instance;
        return instance;
    }

    void MenuModel::Initialize() noexcept
    {
        m_Selected = MenuCategory::Player;
    }

    void MenuModel::Select(MenuCategory category) noexcept
    {
        if (HasMainCategory(category))
            m_Selected = category;
    }

    MenuCategory MenuModel::Selected() const noexcept
    {
        return m_Selected;
    }

    const std::array<MenuCategory, 10>& MenuModel::MainCategories() const noexcept
    {
        return m_MainCategories;
    }

    bool MenuModel::HasMainCategory(MenuCategory category) const noexcept
    {
        return std::find(m_MainCategories.begin(), m_MainCategories.end(), category) != m_MainCategories.end();
    }
}
