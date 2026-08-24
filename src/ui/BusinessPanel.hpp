#pragma once

#include "BunkerBusinessPanel.hpp"
#include "BunkerToolsPanel.hpp"
#include "NightclubPanel.hpp"
#include "SpecialCargoBusinessPanel.hpp"
#include "SpecialCargoToolsPanel.hpp"
#include "V11Description.hpp"

#include <imgui.h>

namespace Tutones::UI
{
    inline void RenderBusinessPanel() noexcept
    {
        // Misc -> Businesses owns the business navigation. Each business gets its
        // own page and only renders its own controls/backends.
        static int selectedBusinessPage = 0;

        if (selectedBusinessPage == 0)
        {
            RenderNightclubPanel();
            SetV11Description("Nightclub business tuning for Enhanced 1.73 / b1158.13: goods, cooldowns, production, equipment upgrade multiplier, and popularity income.");
        }
        else if (selectedBusinessPage == 1)
        {
            RenderSpecialCargoBusinessPanel();
            RenderSpecialCargoToolsControl();
            SetV11Description("Special Cargo only: warehouse crate stock plus Lupe sourcing, cooldowns, contraband mission locals, crate-price globals and unique special cargo.");
        }
        else
        {
            RenderBunkerBusinessPanel();
            RenderBunkerToolsControl();
            SetV11Description("Bunker only: supplies/product stock plus product value, sale multipliers, high-demand bonus, production times and gb_gunrunning Instant Sell.");
        }

        // Business tabs are drawn last so they remain clickable above each full-size
        // business child panel.
        ImGui::SetCursorPos(ImVec2(385.0f, 19.0f));
        if (ImGui::SmallButton("Nightclub##business_page"))
            selectedBusinessPage = 0;
        ImGui::SameLine();
        if (ImGui::SmallButton("Special Cargo##business_page"))
            selectedBusinessPage = 1;
        ImGui::SameLine();
        if (ImGui::SmallButton("Bunker##business_page"))
            selectedBusinessPage = 2;
    }
}
