#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/business/HangarRuntime.hpp"

#include <imgui.h>

namespace Tutones::UI
{
    inline void RenderHangarBusinessPanel() noexcept
    {
        using namespace Game::Business::HangarEnhanced173;
        using Game::Business::HangarRuntime;

        auto& runtime = HangarRuntime::Get();
        const auto state = runtime.Snapshot();

        ImGui::SetCursorPos(ImVec2(226.0f, 52.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##hangar_business_panel", ImVec2(490.0f, 394.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Hangar / Air Freight Cargo");
            ImGui::SameLine();
            ImGui::TextDisabled("Enhanced 1.73 / b1158.13");
            ImGui::Separator();

            ImGui::TextWrapped(
                "Live Enhanced Hangar runtime telemetry from the scripts that actually drive Air-Freight Cargo. Unknown cargo/payout locals are not guessed.");
            ImGui::Spacing();

            ImGui::BeginDisabled(state.pending);
            if (ImGui::Button(state.pending ? "Refreshing Hangar Runtime..." : "Refresh Hangar Runtime", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueRefresh());
            ImGui::EndDisabled();
            DescribeLastV11Item("Resolve the live Enhanced Hangar, Terrorbyte source-app and Business Hub script threads on the GTA script thread.");

            ImGui::SeparatorText("Air-Freight Runtime");
            ImGui::Text("GTA Online: %s", state.sessionStarted ? "Ready" : "Offline");
            ImGui::Text("Script runtime: %s", state.scriptRuntimeReady ? "Ready" : "Unavailable");
            ImGui::Text("gb_smuggler mission: %s", state.haveResult ? (state.smugglerRunning ? "RUNNING" : "IDLE") : "UNKNOWN");
            ImGui::Text("apphackertruck source app: %s", state.haveResult ? (state.hackerTruckRunning ? "RUNNING" : "IDLE") : "UNKNOWN");
            ImGui::Text("Business Hub: %s", state.haveResult ? (state.businessHubRunning ? "RUNNING" : "IDLE") : "UNKNOWN");

            ImGui::SeparatorText("Verified Decompile Contracts");
            ImGui::Text("Source mission tx: 0x%08X", SourceMissionTransactionHash);
            ImGui::Text("Mission stat hash: 0x%08X", SourceMissionStatHash);
            ImGui::TextDisabled("HANGAR_CONTRABAND_MISSION_0_t0_v0");
            ImGui::TextDisabled("MP_STAT_HANGAR_CONTRABAND_MISSION_v0");

            ImGui::SeparatorText("Write Guard");
            ImGui::TextWrapped(
                "Cargo stock, source-complete, sale-complete and payout writes remain locked until their exact Enhanced 1.73 state layout is verified. Runtime detection and transaction discovery are active now.");

            if (state.pending || state.haveResult)
                ImGui::TextDisabled("%s", state.message.c_str());
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        SetV11Description("Hangar / Air-Freight: live gb_smuggler, apphackertruck and Business Hub state with decompile-proven transaction diagnostics.");
    }
}
