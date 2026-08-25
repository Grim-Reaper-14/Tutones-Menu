#pragma once

#include <imgui.h>

namespace Tutones::UI::V11Theme
{
    // Tutones V12 ships with the red / charcoal dashboard palette from the
    // approved mockup. ThemeManager may still replace these values at runtime.
    inline ImVec4 Accent{229.0f / 255.0f, 42.0f / 255.0f, 48.0f / 255.0f, 1.0f};
    inline ImVec4 AccentHover{248.0f / 255.0f, 66.0f / 255.0f, 72.0f / 255.0f, 1.0f};
    inline ImVec4 AccentDark{74.0f / 255.0f, 10.0f / 255.0f, 15.0f / 255.0f, 1.0f};
    inline ImVec4 WindowBg{3.0f / 255.0f, 5.0f / 255.0f, 8.0f / 255.0f, 1.0f};
    inline ImVec4 BodyBg{5.0f / 255.0f, 8.0f / 255.0f, 12.0f / 255.0f, 1.0f};
    inline ImVec4 FooterBg{4.0f / 255.0f, 7.0f / 255.0f, 10.0f / 255.0f, 1.0f};
    inline ImVec4 RailBg{7.0f / 255.0f, 10.0f / 255.0f, 14.0f / 255.0f, 0.985f};
    inline ImVec4 PanelBg{9.0f / 255.0f, 12.0f / 255.0f, 17.0f / 255.0f, 0.985f};
    inline ImVec4 PanelBorder{78.0f / 255.0f, 83.0f / 255.0f, 91.0f / 255.0f, 0.38f};
    inline ImVec4 Separator{90.0f / 255.0f, 94.0f / 255.0f, 103.0f / 255.0f, 0.26f};
    inline ImVec4 ControlBg{12.0f / 255.0f, 16.0f / 255.0f, 22.0f / 255.0f, 1.0f};
    inline ImVec4 ControlHover{35.0f / 255.0f, 18.0f / 255.0f, 23.0f / 255.0f, 1.0f};
    inline ImVec4 HoverBg{35.0f / 255.0f, 18.0f / 255.0f, 23.0f / 255.0f, 1.0f};
    inline ImVec4 ActiveBg{74.0f / 255.0f, 10.0f / 255.0f, 15.0f / 255.0f, 1.0f};
    inline ImVec4 MutedText{151.0f / 255.0f, 157.0f / 255.0f, 168.0f / 255.0f, 1.0f};

    // V12 dashboard shell. HeaderHeight now represents the top dashboard chrome
    // (welcome/status row + page title + horizontal subtabs), so legacy feature
    // panels can keep their existing internal coordinates below it.
    inline constexpr float MenuWidth = 1460.0f;
    inline constexpr float MenuHeight = 820.0f;
    inline constexpr float HeaderHeight = 200.0f;
    inline constexpr float FooterHeight = 0.0f;
    inline constexpr float BodyHeight = MenuHeight - HeaderHeight;
    inline constexpr float FooterY = HeaderHeight + BodyHeight;

    inline constexpr float CategoryRailX = 16.0f;
    inline constexpr float CategoryRailWidth = 278.0f;

    // part03 sizes its transparent input child from SubtabRailX +
    // SubtabRailWidth. V12 uses the complete menu width because subtabs are now
    // horizontal across the center rather than a second vertical rail.
    inline constexpr float SubtabRailX = MenuWidth;
    inline constexpr float SubtabRailWidth = 0.0f;

    // Preserve the legacy feature-panel x=226 coordinate system. A 128px child
    // origin places those panels at x=354, directly to the right of the new rail.
    inline constexpr float ContentHostX = 128.0f;
    inline constexpr float ContentHostWidth = 1040.0f;

    inline constexpr float StatusX = 1180.0f;
    inline constexpr float StatusWidth = 264.0f;
}
