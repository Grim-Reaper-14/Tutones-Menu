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
#include "../features/business/BusinessScriptMonitorRuntime.hpp"

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

            tabButton("Overview", 0, 82.0f);
            ImGui::SameLine();
            tabButton("Nightclub", 1, 88.0f);
            ImGui::SameLine();
            tabButton("Special Cargo", 2, 116.0f);
            ImGui::SameLine();
            tabButton("Bunker", 3, 76.0f);
            ImGui::SameLine();
            tabButton("Motorcycle Club", 4, 126.0f);
            ImGui::SameLine();
            tabButton("Acid Lab", 5, 78.0f);
            ImGui::SameLine();
            tabButton("Hangar", 6, 78.0f);
            ImGui::SameLine();
            tabButton("Vehicle Cargo", 7, 110.0f);

            ImGui::PopStyleVar(3);
        }

        inline void ScriptStateRow(const char* label, bool haveResult, bool running) noexcept
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(label);
            ImGui::TableSetColumnIndex(1);
            if (!haveResult)
                ImGui::TextDisabled("UNKNOWN");
            else if (running)
                ImGui::TextColored(V11Theme::Accent, "RUNNING");
            else
                ImGui::TextDisabled("IDLE");
        }

        inline void RenderBusinessOverview() noexcept
        {
            using Game::Business::BusinessScriptMonitorRuntime;

            auto& runtime = BusinessScriptMonitorRuntime::Get();
            const auto state = runtime.Snapshot();

            ImGui::SetCursorPos(ImVec2(226.0f, 52.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
            ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

            if (ImGui::BeginChild("##business_overview_panel", ImVec2(490.0f, 394.0f), true))
            {
                ImGui::TextColored(V11Theme::Accent, "Tutones Business Control Center");
                ImGui::SameLine();
                ImGui::TextDisabled("Enhanced");
                ImGui::Separator();

                ImGui::TextWrapped("Live script diagnostics for the Enhanced business controllers found in the decompiled scripts. Tutones checks the Rockstar script thread before any future script-local feature is enabled.");
                ImGui::Spacing();

                if (ImGui::BeginTable("##business_script_states", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
                {
                    ImGui::TableSetupColumn("Controller", ImGuiTableColumnFlags_WidthStretch, 2.0f);
                    ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                    ImGui::TableHeadersRow();

                    ScriptStateRow("Nightclub / Business Hub", state.haveResult, state.businessHubRunning);
                    ScriptStateRow("Bunker", state.haveResult, state.bunkerRunning);
                    ScriptStateRow("Acid Lab", state.haveResult, state.acidLabRunning);
                    ScriptStateRow("Auto Shop", state.haveResult, state.autoShopRunning);
                    ScriptStateRow("Bail Office", state.haveResult, state.bailOfficeRunning);
                    ScriptStateRow("Casino", state.haveResult, state.casinoRunning);
                    ScriptStateRow("Car Wash", state.haveResult, state.carWashRunning);
                    ScriptStateRow("Luxury Showroom", state.haveResult, state.luxuryShowroomRunning);
                    ScriptStateRow("Hangar Mission", state.haveResult, state.hangarRunning);
                    ScriptStateRow("Vehicle Cargo Mission", state.haveResult, state.vehicleCargoRunning);
                    ImGui::EndTable();
                }

                ImGui::Spacing();
                ImGui::BeginDisabled(state.pending);
                if (ImGui::Button(state.pending ? "Refreshing..." : "Refresh Enhanced Script State", ImVec2(-1.0f, 30.0f)))
                    static_cast<void>(runtime.QueueRefresh());
                ImGui::EndDisabled();
                DescribeLastV11Item("Resolve supported Enhanced business threads through Tutones' shared script runtime on the GTA game thread.");

                if (state.pending)
                    ImGui::TextDisabled("%s", state.message.c_str());
                else if (state.haveResult)
                    ImGui::TextDisabled("%s: %s", state.lastSucceeded ? "Success" : "Failed", state.message.c_str());
                else
                    ImGui::TextDisabled("Press Refresh after joining GTA Online.");

                ImGui::TextDisabled("IDLE only means that controller thread is not active right now; it does not mean the property is unavailable.");
            }

            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(2);
        }
    }

    inline void RenderBusinessPanel() noexcept
    {
        static int selectedBusinessPage = 0;

        BusinessPanelDetail::RenderBusinessTabs(selectedBusinessPage);

        if (selectedBusinessPage == 0)
        {
            BusinessPanelDetail::RenderBusinessOverview();
            SetV11Description("Tutones Business Control Center: live Enhanced script diagnostics for Rockstar's business controllers before script-local features are exposed.");
        }
        else if (selectedBusinessPage == 1)
        {
            RenderNightclubPanel();
            SetV11Description("Nightclub business tuning for Enhanced 1.73 / b1158.13: goods, cooldowns, production, equipment upgrade multiplier, and popularity income.");
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
        else
        {
            RenderVehicleCargoBusinessPanel();
            SetV11Description("Vehicle Cargo cooldown and sell-value read/write controls with current-value refresh and read-back verification.");
        }
    }
}
