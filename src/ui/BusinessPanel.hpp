#pragma once

#include "NightclubPanel.hpp"
#include "RecoveryPanel.hpp"
#include "SpecialCargoToolsPanel.hpp"
#include "V11Description.hpp"

#include <imgui.h>

namespace Tutones::UI
{
    inline void RenderBusinessPanel() noexcept
    {
        // Keep every business feature in one Misc location while reusing the
        // already-tested Nightclub and Recovery business renderers/backends.
        static int selectedBusinessPage = 0;

        if (selectedBusinessPage == 0)
        {
            RenderNightclubPanel();
            SetV11Description("Nightclub business tuning for Enhanced 1.73 / b1158.13: goods, cooldowns, production, equipment upgrade multiplier, and popularity income.");
        }
        else
        {
            // Recovery subtab 2 is the existing Special Cargo + Bunker business
            // renderer. It remains reusable internally even though Businesses is
            // no longer exposed as a Recovery navigation item.
            RenderRecoveryPanel(2);

            // Draw the advanced Special Cargo launcher after the business panel so
            // it stays visible above the panel instead of being hidden behind it.
            RenderSpecialCargoToolsControl();
            SetV11Description("Special Cargo and Bunker business controls, including owned warehouse state, Lupe sourcing, cooldowns, mission locals, crate-price globals, special cargo, and Bunker stock controls.");
        }

        // Compact page selector drawn last so it stays clickable above either
        // reused full-size business panel.
        ImGui::SetCursorPos(ImVec2(492.0f, 19.0f));
        if (ImGui::SmallButton("Nightclub##business_page"))
            selectedBusinessPage = 0;
        ImGui::SameLine();
        if (ImGui::SmallButton("Cargo / Bunker##business_page"))
            selectedBusinessPage = 1;
    }
}
