#pragma once

#include "BunkerBusinessPanel.hpp"
#include "MotorcycleClubPanel.hpp"
#include "NightclubPanel.hpp"
#include "SpecialCargoBusinessPanel.hpp"
#include "V11Description.hpp"
#include "V11Theme.hpp"

#include <imgui.h>

namespace Tutones::UI
{
    namespace BusinessPanelDetail
    {
        inline void RenderBusinessTabs(int& selectedBusinessPage) noexcept
        {
            // The business selector belongs to the menu itself. Keep it in the
            // normal parent layout above the selected business child panel so no
            // top-level ImGui window or overlay is needed.
            ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));

            const auto tabButton = [&](const char* label, int page, float width)
            {
                if (selectedBusinessPage == page)
                    ImGui::PushStyleColor(ImGuiCol_Button, V11Theme::AccentDark);

                const bool pressed = ImGui::Button(label, ImVec2(width, 28.0f));

                if (selectedBusinessPage == page)
                    ImGui::PopStyleColor();

                if (pressed)
                    selectedBusinessPage = page;
            };

            tabButton("Nightclub", 0, 82.0f);
            ImGui::SameLine();
            tabButton("Special Cargo", 1, 112.0f);
            ImGui::SameLine();
            tabButton("Bunker", 2, 72.0f);
            ImGui::SameLine();
            tabButton("Motorcycle Club", 3, 136.0f);

            ImGui::PopStyleVar();
        }
    }

    inline void RenderBusinessPanel() noexcept
    {
        static int selectedBusinessPage = 0;

        BusinessPanelDetail::RenderBusinessTabs(selectedBusinessPage);

        if (selectedBusinessPage == 0)
        {
            RenderNightclubPanel();
            SetV11Description("Nightclub business tuning for Enhanced 1.73 / b1158.13: goods, cooldowns, production, equipment upgrade multiplier, and popularity income.");
        }
        else if (selectedBusinessPage == 1)
        {
            RenderSpecialCargoBusinessPanel();
            SetV11Description("Special Cargo only: warehouse crate stock, Lupe sourcing, cooldowns, contraband mission locals, crate-price globals and unique special cargo, all rendered directly in the menu.");
        }
        else if (selectedBusinessPage == 2)
        {
            RenderBunkerBusinessPanel();
            SetV11Description("Bunker only: supplies/product stock, product value, sale multipliers, high-demand bonus, production times and gb_gunrunning Instant Sell, all rendered directly in the menu.");
        }
        else
        {
            RenderMotorcycleClubPanel();
            SetV11Description("Motorcycle Club only: supplied Enhanced 1.73 stock values, Near/Far sale multipliers, and max capacities for Documents, Cash, Cocaine, Meth, Weed and Acid.");
        }
    }
}
