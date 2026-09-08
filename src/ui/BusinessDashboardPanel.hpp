#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/business/BusinessScriptMonitorRuntime.hpp"

#include <imgui.h>

namespace Tutones::UI
{
    namespace BusinessDashboardDetail
    {
        inline void RenderStateRow(const char* name, bool haveResult, bool running) noexcept
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(name);
            ImGui::TableSetColumnIndex(1);
            if (!haveResult)
                ImGui::TextDisabled("UNKNOWN");
            else if (running)
                ImGui::TextColored(V11Theme::Accent, "RUNNING");
            else
                ImGui::TextDisabled("IDLE");
        }
    }

    inline void RenderBusinessDashboardPanel() noexcept
    {
        using Game::Business::BusinessScriptMonitorRuntime;
        auto& runtime = BusinessScriptMonitorRuntime::Get();
        const auto state = runtime.Snapshot();

        ImGui::SetCursorPos(ImVec2(226.0f, 52.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##business_dashboard_panel", ImVec2(490.0f, 394.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Business Runtime Dashboard");
            ImGui::SameLine();
            ImGui::TextDisabled("Enhanced");
            ImGui::Separator();
            ImGui::TextWrapped(
                "One-screen live view of the Enhanced business scripts Tutones already knows how to resolve. Use it to see which business/mission backends are active before using their individual tools.");

            ImGui::BeginDisabled(state.pending);
            if (ImGui::Button(state.pending ? "Refreshing Businesses..." : "Refresh All Business Runtimes", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueRefresh());
            ImGui::EndDisabled();
            DescribeLastV11Item("Refresh all currently registered Enhanced business script threads in one game-thread pass.");

            if (ImGui::BeginTable(
                    "##business_runtime_table",
                    2,
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Business / Script Group", ImGuiTableColumnFlags_WidthStretch, 1.8f);
                ImGui::TableSetupColumn("Runtime", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableHeadersRow();

                BusinessDashboardDetail::RenderStateRow("Business Hub / Nightclub", state.haveResult, state.businessHubRunning);
                BusinessDashboardDetail::RenderStateRow("Bunker", state.haveResult, state.bunkerRunning);
                BusinessDashboardDetail::RenderStateRow("Acid Lab", state.haveResult, state.acidLabRunning);
                BusinessDashboardDetail::RenderStateRow("Auto Shop", state.haveResult, state.autoShopRunning);
                BusinessDashboardDetail::RenderStateRow("Bail Office", state.haveResult, state.bailOfficeRunning);
                BusinessDashboardDetail::RenderStateRow("Casino", state.haveResult, state.casinoRunning);
                BusinessDashboardDetail::RenderStateRow("Car Wash / Money Fronts", state.haveResult, state.carWashRunning);
                BusinessDashboardDetail::RenderStateRow("Luxury Showroom", state.haveResult, state.luxuryShowroomRunning);
                BusinessDashboardDetail::RenderStateRow("Hangar / Air Freight", state.haveResult, state.hangarRunning);
                BusinessDashboardDetail::RenderStateRow("Vehicle Cargo", state.haveResult, state.vehicleCargoRunning);
                ImGui::EndTable();
            }

            if (state.pending || state.haveResult)
                ImGui::TextDisabled("%s", state.message.c_str());
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        SetV11Description("Business Runtime Dashboard - consolidated live Enhanced business-script state.");
    }
}
