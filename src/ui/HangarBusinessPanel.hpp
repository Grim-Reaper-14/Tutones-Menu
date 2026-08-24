#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/business/BusinessScriptMonitorRuntime.hpp"

#include <imgui.h>

namespace Tutones::UI
{
    inline void RenderHangarBusinessPanel() noexcept
    {
        using Game::Business::BusinessScriptMonitorRuntime;

        auto& runtime = BusinessScriptMonitorRuntime::Get();
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

            ImGui::TextWrapped("Dedicated Hangar page tied to the Enhanced gb_smuggler mission runtime. Script-local writes stay disabled until their current decompile offsets and state values are validated.");
            ImGui::Spacing();

            ImGui::SeparatorText("Enhanced Mission Runtime");
            ImGui::Text("gb_smuggler: %s", state.haveResult ? (state.hangarRunning ? "RUNNING" : "IDLE") : "UNKNOWN");
            ImGui::BeginDisabled(state.pending);
            if (ImGui::Button("Refresh Hangar script state", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueRefresh());
            ImGui::EndDisabled();
            DescribeLastV11Item("Resolve the Enhanced gb_smuggler thread through Tutones' shared script runtime without writing an unverified local.");

            ImGui::SeparatorText("Decompile Guard");
            ImGui::TextWrapped("Tutones found the Enhanced gb_smuggler decompile, but this page will not guess mission-complete, cargo or payout locals. Those actions should be added only after their 1.73 layout is verified.");

            if (state.pending)
                ImGui::TextDisabled("%s", state.message.c_str());
            else if (state.haveResult)
                ImGui::TextDisabled("%s: %s", state.lastSucceeded ? "Success" : "Failed", state.message.c_str());
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        SetV11Description("Hangar / Air Freight Cargo now has its own page and validated gb_smuggler runtime detection, ready for verified Enhanced mission locals and tunables.");
    }
}
