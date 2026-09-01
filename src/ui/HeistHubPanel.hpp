#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/heist/ExoticExportRuntime.hpp"

#include <imgui.h>

namespace Tutones::UI
{
    inline void RenderHeistHubPanel() noexcept
    {
        using Game::Heist::ExoticExportRuntime;
        using Game::Heist::ExoticExportStateName;

        auto& runtime = ExoticExportRuntime::Get();
        const auto state = runtime.Snapshot();

        ImGui::SetCursorPos(ImVec2(226.0f, 52.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##heist_hub_panel", ImVec2(490.0f, 394.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Heist Hub");
            ImGui::SameLine();
            ImGui::TextDisabled("Enhanced 1.73 / b1158.13");
            ImGui::Separator();

            ImGui::TextWrapped("Heist and Auto Shop workflows live here so the existing Business menu stays untouched.");
            ImGui::Spacing();

            ImGui::SeparatorText("Auto Shop / Exotic Exports");
            ImGui::TextWrapped("Locate the Exotic Export vehicle Rockstar has actually spawned for the current session. After delivering one vehicle, refresh again to locate the next active export.");

            ImGui::BeginDisabled(state.pending);
            if (ImGui::Button("Refresh Active Exotic Export", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueRefresh());
            ImGui::EndDisabled();
            DescribeLastV11Item("Read GSBD_RandomEvents event 3 from Enhanced global 1882345 and refresh the current Exotic Export trigger position.");

            const bool canTeleport = !state.pending && state.coordinatesValid;
            ImGui::BeginDisabled(!canTeleport);
            if (ImGui::Button("Teleport to Active Exotic Export", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueTeleportToActive());
            ImGui::EndDisabled();
            DescribeLastV11Item("Teleport beside the currently spawned Exotic Export vehicle using the live random-event trigger coordinates.");

            ImGui::Spacing();
            ImGui::TextDisabled("Event state: %s", ExoticExportStateName(state.eventState));
            ImGui::TextDisabled("Event variation: %d  |  subvariation: %d", state.eventVariation, state.eventSubvariation);
            ImGui::TextDisabled("Vehicle-list index: %d  |  variation: %d", state.vehicleListIndex, state.vehicleListVariation);

            if (state.coordinatesValid)
            {
                ImGui::TextDisabled("Live coordinates: %.3f, %.3f, %.3f", state.x, state.y, state.z);
                ImGui::TextDisabled("Trigger range: %.1f", state.triggerRange);
            }
            else
            {
                ImGui::TextDisabled("Live coordinates: no Exotic Export is currently available/active");
            }

            ImGui::SeparatorText("Status");
            if (state.pending)
                ImGui::TextDisabled("%s", state.message.c_str());
            else if (state.haveResult)
                ImGui::TextDisabled("%s: %s", state.lastSucceeded ? "Success" : "Failed", state.message.c_str());
            else
                ImGui::TextDisabled("Ready - refresh when you want to locate the current export vehicle");
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        SetV11Description("HIEST -> Heist Hub now includes a live Auto Shop Exotic Export locator backed by Enhanced random-event state.");
    }
}
