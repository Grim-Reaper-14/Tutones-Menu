#pragma once

#include "AcidLabBusinessPanel.hpp"
#include "AgencyBusinessPanel.hpp"
#include "BailOfficePanel.hpp"
#include "BunkerBusinessPanel.hpp"
#include "EnhancedControlCenterPanel.hpp"
#include "GarmentFactoryPanel.hpp"
#include "HangarBusinessPanel.hpp"
#include "MoneyFrontsPanel.hpp"
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
            if (ImGui::BeginChild("##business_tab_strip", ImVec2(490.0f, 34.0f), false))
            {
                if (ImGui::BeginTabBar("##business_tabs", ImGuiTabBarFlags_FittingPolicyScroll))
                {
                    const auto tab = [&](const char* label, int page)
                    {
                        if (ImGui::BeginTabItem(label))
                        {
                            selectedBusinessPage = page;
                            ImGui::EndTabItem();
                        }
                    };

                    tab("Enhanced", 0);
                    tab("Nightclub", 1);
                    tab("Special Cargo", 2);
                    tab("Bunker", 3);
                    tab("Motorcycle Club", 4);
                    tab("Acid Lab", 5);
                    tab("Hangar", 6);
                    tab("Vehicle Cargo", 7);
                    tab("Bail Office", 8);
                    tab("Money Fronts", 9);
                    tab("Agency", 10);
                    tab("Garment Factory", 11);
                    ImGui::EndTabBar();
                }
            }
            ImGui::EndChild();
        }
    }

    inline void RenderBusinessPanel() noexcept
    {
        static int selectedBusinessPage = 0;

        BusinessPanelDetail::RenderBusinessTabs(selectedBusinessPage);

        if (selectedBusinessPage == 0)
        {
            RenderEnhancedControlCenterPanel();
            SetV11Description("Enhanced Control Center: Business Manager, Street Dealer, Services, Mission Control, Daily Activity, Properties, latest DLC and semantic script resolver diagnostics.");
        }
        else if (selectedBusinessPage == 1)
        {
            RenderNightclubPanel();
            SetV11Description("");
        }
        else if (selectedBusinessPage == 2)
        {
            RenderSpecialCargoBusinessPanel();
            SetV11Description("Special Cargo only: warehouse crate stock, Lupe sourcing, cooldowns, contraband mission locals, crate-price globals and unique special cargo, all rendered directly in the menu.");
        }
        else if (selectedBusinessPage == 3)
        {
            RenderBunkerBusinessPanel();
            SetV11Description("Bunker only: supplies/product stock, Instant Resupply, product value, sale multipliers, high-demand bonus, production times and gb_gunrunning Instant Sell.");
        }
        else if (selectedBusinessPage == 4)
        {
            RenderMotorcycleClubPanel();
            SetV11Description("Motorcycle Club only: supplied Enhanced 1.73 stock values, Near/Far sale multipliers, max capacities and the five supplied Instant Resupply slots.");
        }
        else if (selectedBusinessPage == 5)
        {
            RenderAcidLabBusinessPanel();
            SetV11Description("Acid Lab business controls and Enhanced tuning values in the V2 Business Hub.");
        }
        else if (selectedBusinessPage == 6)
        {
            RenderHangarBusinessPanel();
            SetV11Description("Hangar / Air Freight business controls and Enhanced tuning values in the V2 Business Hub.");
        }
        else if (selectedBusinessPage == 7)
        {
            RenderVehicleCargoBusinessPanel();
            SetV11Description("Vehicle Cargo cooldown and sell-value read/write controls with current-value refresh and read-back verification.");
        }
        else if (selectedBusinessPage == 8)
        {
            RenderBailOfficePanel();
            SetV11Description("Bail Office target-board telemetry from the Enhanced appBailOffice flow: standard target, mission, reward, completion and Most Wanted rotation state.");
        }
        else if (selectedBusinessPage == 9)
        {
            RenderMoneyFrontsPanel();
            SetV11Description("Money Fronts M25 player-flow telemetry. Raw mission and flag state stays read-only until exact current bit semantics are verified from the Enhanced decompile.");
        }
        else if (selectedBusinessPage == 10)
        {
            RenderAgencyBusinessPanel();
            SetV11Description("Agency Fixer-flow telemetry from appfixersecurity: live Security Contract board, rewards, contract count, earnings, story cooldown, Payphone bonus method and progression flags.");
        }
        else
        {
            RenderGarmentFactoryPanel();
            SetV11Description("Garment Factory / Agents of Sabotage telemetry from the current Hacker24 player-flow block. Active FIB File and raw progression flags stay read-only until each write path is verified.");
        }
    }
}
