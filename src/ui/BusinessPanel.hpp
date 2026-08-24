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
            ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 6.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 7.0f));

            const auto tabButton = [&](const char* label, int page, float width)
            {
                const bool selected = selectedBusinessPage == page;
                if (selected)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(
                        V11Theme::Accent.x,
                        V11Theme::Accent.y,
                        V11Theme::Accent.z,
                        0.28f));
                    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(
                        V11Theme::Accent.x,
                        V11Theme::Accent.y,
                        V11Theme::Accent.z,
                        0.82f));
                }

                const bool pressed = ImGui::Button(label, ImVec2(width, 34.0f));

                if (selected)
                    ImGui::PopStyleColor(2);

                if (pressed)
                    selectedBusinessPage = page;
            };

            tabButton("Nightclub", 0, 88.0f);
            ImGui::SameLine();
            tabButton("Special Cargo", 1, 116.0f);
            ImGui::SameLine();
            tabButton("Bunker", 2, 76.0f);
            ImGui::SameLine();
            tabButton("Motorcycle Club", 3, 126.0f);
            ImGui::SameLine();
            tabButton("Acid Lab", 4, 78.0f);
            ImGui::SameLine();
            tabButton("Hangar", 5, 78.0f);
            ImGui::SameLine();
            tabButton("Vehicle Cargo", 6, 110.0f);

            ImGui::PopStyleVar(3);
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
            SetV11Description("Acid Lab business controls and Enhanced tuning values in the V12 Business Hub.");
        }
        else if (selectedBusinessPage == 5)
        {
            RenderHangarBusinessPanel();
            SetV11Description("Hangar / Air Freight business controls and Enhanced tuning values in the V12 Business Hub.");
        }
        else
        {
            RenderVehicleCargoBusinessPanel();
            SetV11Description("Vehicle Cargo cooldown and sell-value read/write controls with current-value refresh and read-back verification.");
        }
    }
}
