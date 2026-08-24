#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/business/BusinessScriptMonitorRuntime.hpp"

#include <imgui.h>

namespace Tutones::UI
{
    inline void RenderVehicleCargoBusinessPanel() noexcept
    {
        using Game::Business::BusinessScriptMonitorRuntime;

        auto& runtime = BusinessScriptMonitorRuntime::Get();
        const auto state = runtime.Snapshot();

        ImGui::SetCursorPos(ImVec2(226.0f, 52.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##vehicle_cargo_business_panel", ImVec2(490.0f, 394.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Vehicle Cargo / Import Export");
            ImGui::SameLine();
            ImGui::TextDisabled("Enhanced 1.73 / b1158.13");
            ImGui::Separator();

            ImGui::TextWrapped("Dedicated Vehicle Cargo page tied to the Enhanced gb_vehicle_export mission runtime. Fragile mission locals are not written until their current decompile layout is validated.");
            ImGui::Spacing();

            ImGui::SeparatorText("Enhanced Mission Runtime");
            ImGui::Text("gb_vehicle_export: %s", state.haveResult ? (state.vehicleCargoRunning ? "RUNNING" : "IDLE") : "UNKNOWN");
            ImGui::BeginDisabled(state.pending);
            if (ImGui::Button("Refresh Vehicle Cargo script state", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueRefresh());
            ImGui::EndDisabled();
            DescribeLastV11Item("Resolve the Enhanced gb_vehicle_export thread through Tutones' shared script runtime without writing an unverified local.");

            ImGui::SeparatorText("Decompile Guard");
            ImGui::TextWrapped("Tutones found the Enhanced gb_vehicle_export decompile. Source/sell mission state changes will be added here only after the 1.73 locals and expected values are verified.");

            if (state.pending)
                ImGui::TextDisabled("%s", state.message.c_str());
            else if (state.haveResult)
                ImGui::TextDisabled("%s: %s", state.lastSucceeded ? "Success" : "Failed", state.message.c_str());
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        SetV11Description("Vehicle Cargo / Import Export now has its own page and validated gb_vehicle_export runtime detection, ready for verified Enhanced mission locals and tunables.");
    }
}
