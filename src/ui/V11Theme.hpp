#pragma once

#include <imgui.h>

namespace Tutones::UI::V11Theme
{
    inline ImVec4 Accent{2.0f / 255.0f, 108.0f / 255.0f, 249.0f / 255.0f, 1.0f};
    inline ImVec4 AccentHover{15.0f / 255.0f, 143.0f / 255.0f, 239.0f / 255.0f, 1.0f};
    inline ImVec4 AccentDark{1.0f / 255.0f, 26.0f / 255.0f, 75.0f / 255.0f, 1.0f};
    inline ImVec4 WindowBg{1.0f / 255.0f, 3.0f / 255.0f, 7.0f / 255.0f, 1.0f};
    inline ImVec4 BodyBg{4.0f / 255.0f, 7.0f / 255.0f, 12.0f / 255.0f, 1.0f};
    inline ImVec4 FooterBg{3.0f / 255.0f, 7.0f / 255.0f, 13.0f / 255.0f, 1.0f};
    inline ImVec4 RailBg{6.0f / 255.0f, 10.0f / 255.0f, 17.0f / 255.0f, 0.96f};
    inline ImVec4 PanelBg{7.0f / 255.0f, 11.0f / 255.0f, 18.0f / 255.0f, 0.98f};
    inline ImVec4 PanelBorder{2.0f / 255.0f, 108.0f / 255.0f, 249.0f / 255.0f, 0.28f};
    inline ImVec4 Separator{2.0f / 255.0f, 108.0f / 255.0f, 249.0f / 255.0f, 0.22f};
    inline ImVec4 ControlBg{10.0f / 255.0f, 16.0f / 255.0f, 25.0f / 255.0f, 1.0f};
    inline ImVec4 ControlHover{9.0f / 255.0f, 34.0f / 255.0f, 66.0f / 255.0f, 1.0f};
    inline ImVec4 MutedText{139.0f / 255.0f, 153.0f / 255.0f, 177.0f / 255.0f, 1.0f};

    inline constexpr float MenuWidth = 1120.0f;
    // Eleven category buttons use a 42px pitch from HeaderHeight + 12. The old
    // 446px body ended before the last category, which let Native Tools overlap
    // the description/footer. Give the navigation rail enough vertical body space
    // while keeping the existing header and footer proportions unchanged.
    inline constexpr float MenuHeight = 854.0f;
    inline constexpr float HeaderHeight = 220.0f;
    inline constexpr float FooterHeight = 150.0f;
    inline constexpr float BodyHeight = 484.0f;
    inline constexpr float FooterY = HeaderHeight + BodyHeight;

    inline constexpr float CategoryRailX = 12.0f;
    inline constexpr float CategoryRailWidth = 170.0f;
    inline constexpr float SubtabRailX = 188.0f;
    inline constexpr float SubtabRailWidth = 170.0f;

    inline constexpr float ContentHostX = 139.0f;
    inline constexpr float ContentHostWidth = 716.0f;

    inline constexpr float StatusX = 870.0f;
    inline constexpr float StatusWidth = 236.0f;
}
