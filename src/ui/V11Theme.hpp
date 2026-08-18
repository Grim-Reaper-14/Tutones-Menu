#pragma once

#include <imgui.h>

namespace Tutones::UI::V11Theme
{
    inline constexpr ImVec4 Accent{2.0f / 255.0f, 108.0f / 255.0f, 249.0f / 255.0f, 1.0f};
    inline constexpr ImVec4 AccentHover{15.0f / 255.0f, 143.0f / 255.0f, 239.0f / 255.0f, 1.0f};
    inline constexpr ImVec4 AccentDark{1.0f / 255.0f, 26.0f / 255.0f, 75.0f / 255.0f, 1.0f};
    inline constexpr ImVec4 WindowBg{7.0f / 255.0f, 9.0f / 255.0f, 14.0f / 255.0f, 1.0f};
    inline constexpr ImVec4 HeaderTop{3.0f / 255.0f, 36.0f / 255.0f, 91.0f / 255.0f, 1.0f};
    inline constexpr ImVec4 HeaderBottom{2.0f / 255.0f, 18.0f / 255.0f, 48.0f / 255.0f, 1.0f};
    inline constexpr ImVec4 FooterBg{6.0f / 255.0f, 11.0f / 255.0f, 21.0f / 255.0f, 1.0f};
    inline constexpr ImVec4 SidebarBg{9.0f / 255.0f, 13.0f / 255.0f, 21.0f / 255.0f, 1.0f};
    inline constexpr ImVec4 IconRailBg{5.0f / 255.0f, 8.0f / 255.0f, 14.0f / 255.0f, 1.0f};
    inline constexpr ImVec4 PanelBg{13.0f / 255.0f, 17.0f / 255.0f, 25.0f / 255.0f, 1.0f};
    inline constexpr ImVec4 PanelBorder{2.0f / 255.0f, 108.0f / 255.0f, 249.0f / 255.0f, 0.15f};
    inline constexpr ImVec4 MutedText{139.0f / 255.0f, 153.0f / 255.0f, 177.0f / 255.0f, 1.0f};

    inline constexpr float MenuWidth = 730.0f;
    inline constexpr float MenuHeight = 540.0f;
    inline constexpr float HeaderHeight = 58.0f;
    inline constexpr float FooterHeight = 36.0f;
    inline constexpr float BodyHeight = MenuHeight - HeaderHeight - FooterHeight;
    inline constexpr float FooterY = MenuHeight - FooterHeight;
    inline constexpr float LeftPanelWidth = 210.0f;
    inline constexpr float IconRailWidth = 47.0f;
    inline constexpr float ContentX = 226.0f;
    inline constexpr float ContentY = HeaderHeight + 16.0f;
    inline constexpr float ContentWidth = 490.0f;
    inline constexpr float ContentHeight = 430.0f;
}
