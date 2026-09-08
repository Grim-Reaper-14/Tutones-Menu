#pragma once

#include "AcidLabBusinessPanel.hpp"
#include "AgencyBusinessPanel.hpp"
#include "BailOfficePanel.hpp"
#include "BunkerBusinessPanel.hpp"
#include "BusinessDashboardPanel.hpp"
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

                    tab("Dashboard", 0);
                    tab("Enhanced", 1);
                    tab("Nightclub", 2);
                    tab("Special Cargo", 3);
                    tab("Bunker", 4);
                    tab("Motorcycle Club", 5);
                    tab("Acid Lab", 6);
                    tab("Hangar", 7);
                    tab("Vehicle Cargo", 8);
                    tab("Bail Office", 9);
                    tab("Money Fronts", 10);
                    tab("Agency", 11);
                    tab("Garment Factory", 12);
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
            RenderBusinessDashboardPanel();
            SetV11Description("Business Runtime Dashboard: one-screen live status for the Enhanced business scripts Tutones currently resolves.");
        }
        else if (selectedBusinessPage == 1)
        {
            RenderEnhancedControlCenterPanel();
            SetV11Description("Enhanced Control Center: Business Manager, Street Dealer, Services, Mission Control, Daily Activity, Properties, latest DLC and semantic script resolver diagnostics.");
        }
        else if (selectedBusinessPage == 2)
        {
            RenderNightclubPanel();
            SetV11Description("");
        }
        else if (selectedBusinessPage == 3)
        {
            RenderSpecialCargoBusinessPanel();
            SetV11Description("Special Cargo only: warehouse crate stock, Lupe sourcing, cooldowns, contraband mission locals, crate-price globals and unique special cargo, all rendered directly in the menu.");
        }
        else if (selectedBusinessPage == 4)
        {
            RenderBunkerBusinessPanel();
            SetV11Description("Bunker only: supplies/product stock, Instant Resupply, product value, sale multipliers, high-demand bonus, production times and gb_gunrunning Instant Sell.");
        }
        else if (selectedBusinessPage == 5)
        {
            RenderMotorcycleClubPanel();
            SetV11Description("Motorcycle Club only: supplied Enhanced 1.73 stock values, Near/Far sale multipliers, max capacities and the five supplied Instant Resupply slots.");
        }
        else if (selectedBusinessPage == 6)
        {
            RenderAcidLabBusinessPanel();
            SetV11Description("Acid Lab business controls and Enhanced tuning values in the V2 Business Hub.");
        }
        else if (selectedBusinessPage == 7)
        {
            RenderHangarBusinessPanel();
            SetV11Description("Hangar / Air Freight live script telemetry and verified Enhanced source-transaction diagnostics.");
        }
        else if (selectedBusinessPage == 8)
        {
            RenderVehicleCargoBusinessPanel();
            SetV11Description("Vehicle Cargo cooldown and sell-value read/write controls with current-value refresh and read-back verification.");
        }
        else if (selectedBusinessPage == 9)
        {
            RenderBailOfficePanel();
            SetV11Description("Bail Office target-board telemetry from the Enhanced appBailOffice flow: standard target, mission, reward, completion and Most Wanted rotation state.");
        }
        else if (selectedBusinessPage == 10)
        {
            RenderMoneyFrontsPanel();
            SetV11Description("Money Fronts controls: edit heat for Hands On Car Wash, Smoke on the Water and Higgins Helitours, set all heat to 0 or 100, collect the Car Wash safe, and inspect live M25 flow state.");
        }
        else if (selectedBusinessPage == 11)
        {
            RenderAgencyBusinessPanel();
            SetV11Description("Agency controls: select and complete Dr. Dre contract progression, edit finale payout, kill Dre/Security/Payphone cooldowns, collect the Agency safe, rewrite Security Contract board slots, and inspect live Fixer flow state.");
        }
        else
        {
            RenderGarmentFactoryPanel();
            SetV11Description("Garment Factory controls: select the active FIB File, complete or reset its three preps, unbrick the computer, collect the factory safe, and inspect live Hacker24 flow state.");
        }
    }
}
