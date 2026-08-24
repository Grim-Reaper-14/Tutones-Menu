#pragma once

#include "AcidLabBusinessPanel.hpp"
#include "BunkerBusinessPanel.hpp"
#include "HangarBusinessPanel.hpp"
#include "MotorcycleClubPanel.hpp"
#include "NightclubPanel.hpp"
#include "SpecialCargoBusinessPanel.hpp"
#include "VehicleCargoBusinessPanel.hpp"
#include "V11Description.hpp"
#include "V11Theme.hpp"

#include <imgui.h>

namespace Tutones::UI
{
    namespace BusinessPanelDetail
    {
        inline void RenderBusinessTabs(int& selectedBusinessPage) noexcept
        {
            // Keep every business isolated on its own page while fitting the
            // complete selector inside the existing 730px content host.
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

            tabButton("Nightclub", 0, 78.0f);
            ImGui::SameLine();
            tabButton("Special Cargo", 1, 106.0f);
            ImGui::SameLine();
            tabButton("Bunker", 2, 68.0f);
            ImGui::SameLine();
            tabButton("MC", 3, 44.0f);
            ImGui::SameLine();
            tabButton("Acid Lab", 4, 72.0f);
            ImGui::SameLine();
            tabButton("Hangar", 5, 68.0f);
            ImGui::SameLine();
            tabButton("Vehicle Cargo", 6, 108.0f);

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
            SetV11Description("Bunker only: supplies/product stock, Instant Resupply, product value, sale multipliers, high-demand bonus, production times and gb_gunrunning Instant Sell.");
        }
        else if (selectedBusinessPage == 3)
        {
            RenderMotorcycleClubPanel();
            SetV11Description("Motorcycle Club only: supplied Enhanced 1.73 stock values, Near/Far sale multipliers, max capacities and the five supplied Instant Resupply slots.");
        }
        else if (selectedBusinessPage == 4)
        {
            RenderAcidLabBusinessPanel();
        }
        else if (selectedBusinessPage == 5)
        {
            RenderHangarBusinessPanel();
        }
        else
        {
            RenderVehicleCargoBusinessPanel();
        }
    }
}
