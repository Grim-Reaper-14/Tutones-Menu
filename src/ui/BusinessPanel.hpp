#pragma once

#include "BunkerBusinessPanel.hpp"
#include "BunkerToolsPanel.hpp"
#include "MotorcycleClubPanel.hpp"
#include "NightclubPanel.hpp"
#include "SpecialCargoBusinessPanel.hpp"
#include "SpecialCargoToolsPanel.hpp"
#include "V11Description.hpp"
#include "V11Theme.hpp"

#include <imgui.h>

namespace Tutones::UI
{
    namespace BusinessPanelDetail
    {
        inline void RenderBusinessTabs(int& selectedBusinessPage) noexcept
        {
            // Each business page is a full ImGui child. Parent-window buttons can be
            // covered by those children even if submitted later, so keep the selector
            // in a tiny top-level overlay that is always above the active business page.
            const ImVec2 hostPos = ImGui::GetWindowPos();
            ImGui::SetNextWindowPos(ImVec2(hostPos.x + 280.0f, hostPos.y + 18.0f), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(430.0f, 36.0f), ImGuiCond_Always);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.0f, 5.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 3.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, V11Theme::PanelBg);
            ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

            constexpr ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoScrollWithMouse |
                ImGuiWindowFlags_NoNavInputs |
                ImGuiWindowFlags_NoNavFocus;

            if (ImGui::Begin("##business_tabs_overlay", nullptr, flags))
            {
                const auto tabButton = [&](const char* label, int page, float width)
                {
                    if (selectedBusinessPage == page)
                        ImGui::PushStyleColor(ImGuiCol_Button, V11Theme::AccentDark);
                    const bool pressed = ImGui::Button(label, ImVec2(width, 0.0f));
                    if (selectedBusinessPage == page)
                        ImGui::PopStyleColor();
                    if (pressed)
                        selectedBusinessPage = page;
                };

                tabButton("Nightclub", 0, 78.0f);
                ImGui::SameLine();
                tabButton("Special Cargo", 1, 104.0f);
                ImGui::SameLine();
                tabButton("Bunker", 2, 64.0f);
                ImGui::SameLine();
                tabButton("Motorcycle Club", 3, 128.0f);
            }

            ImGui::End();
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(3);
        }
    }

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
        else if (selectedBusinessPage == 2)
        {
            RenderBunkerBusinessPanel();
            RenderBunkerToolsControl();
            SetV11Description("Bunker only: supplies/product stock plus product value, sale multipliers, high-demand bonus, production times and gb_gunrunning Instant Sell.");
        }
        else
        {
            RenderMotorcycleClubPanel();
            SetV11Description("Motorcycle Club only: supplied Enhanced 1.73 stock values, Near/Far sale multipliers, and max capacities for Documents, Cash, Cocaine, Meth, Weed and Acid.");
        }

        BusinessPanelDetail::RenderBusinessTabs(selectedBusinessPage);
    }
}
