#include "frontend/MenuModel.hpp"

#include <algorithm>
#include <cassert>

int main()
{
    using TutonesV2::Frontend::MenuCategory;
    using TutonesV2::Frontend::MenuModel;

    auto& menu = MenuModel::Get();
    menu.Initialize();

    const auto& categories = menu.MainCategories();
    assert(categories.size() == 10);
    assert(menu.HasMainCategory(MenuCategory::Teleport));

    const auto teleport = std::find(categories.begin(), categories.end(), MenuCategory::Teleport);
    assert(teleport != categories.end());
    assert(static_cast<std::size_t>(std::distance(categories.begin(), teleport)) == 3);

    menu.Select(MenuCategory::Teleport);
    assert(menu.Selected() == MenuCategory::Teleport);

    return 0;
}
