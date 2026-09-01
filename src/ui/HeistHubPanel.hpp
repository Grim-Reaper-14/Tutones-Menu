#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/heist/AutoShopContractRuntime.hpp"
#include "../features/heist/ExoticExportRuntime.hpp"

#include <imgui.h>

namespace Tutones::UI
{
    inline void RenderHeistHubPanel() noexcept
    {
        using Game::Heist::AutoShopContractName;
        using Game::Heist::AutoShopContractRuntime;
        using Game::Heist::AutoShopEnhanced173::ContractCount;
        using Game::Heist::ExoticExportRuntime;
        using Game::Heist::ExoticExportStateName;

        auto& autoShop = AutoShopContractRuntime::Get();
        auto& exoticExports = ExoticExportRuntime::Get();
        const auto autoShopState = autoShop.Snapshot();
        const auto exportState = exoticExports.Snapshot();
        static int selectedContract = 0;

        ImGui::SetCursorPos(ImVec2(226.0f, 52.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild(
                "##heist_hub_panel",
                ImVec2(490.0f, 394.0f),
                true,
                ImGuiWindowFlags_None))
        {
            ImGui::TextColored(V11Theme::Accent, "Heist Hub");
            ImGui::SameLine();
            ImGui::TextDisabled("Enhanced 1.73 / b1158.13");
            ImGui::Separator();

            ImGui::SeparatorText("Auto Shop Contracts");
            ImGui::TextWrapped("Choose one of the eight Auto Shop robbery contracts, mark its verified prep state complete, refresh the planning board, then launch the finale when you are standing at the Auto Shop board.");

            if (ImGui::BeginCombo("Contract##autoshop_contract", AutoShopContractName(selectedContract)))
            {
                for (int contract = 0; contract < ContractCount; ++contract)
                {
                    const bool selected = contract == selectedContract;
                    if (ImGui::Selectable(AutoShopContractName(contract), selected))
                        selectedContract = contract;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            DescribeLastV11Item("Select the Auto Shop robbery contract that should be readied for its finale.");

            ImGui::BeginDisabled(autoShopState.pending);
            if (ImGui::Button("Ready Selected Contract for Finale", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(autoShop.QueueReadyFinale(selectedContract));
            ImGui::EndDisabled();
            DescribeLastV11Item("Write MPX_TUNER_CURRENT and the verified MPX_TUNER_GEN_BS prep mask, verify both, and reload tuner_planning when the board is open.");

            ImGui::BeginDisabled(autoShopState.pending);
            if (ImGui::Button("Reload Auto Shop Planning Board", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(autoShop.QueueReloadPlanning());
            ImGui::EndDisabled();
            DescribeLastV11Item("Set tuner_planning locals 406 and 408 to 2. The Auto Shop planning board must be open/running.");

            ImGui::BeginDisabled(autoShopState.pending);
            if (ImGui::Button("Launch Selected Finale", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(autoShop.QueueLaunchFinale());
            ImGui::EndDisabled();
            DescribeLastV11Item("Trigger tuner_planning local 3627 after the selected contract has been readied. Open the planning board first.");

            ImGui::BeginDisabled(autoShopState.pending);
            if (ImGui::SmallButton("Refresh Auto Shop State"))
                static_cast<void>(autoShop.QueueRefresh());
            ImGui::EndDisabled();

            ImGui::TextDisabled("Current contract: %s (%d)", AutoShopContractName(autoShopState.currentContract), autoShopState.currentContract);
            ImGui::TextDisabled("Prep mask: %d", autoShopState.prepMask);
            ImGui::TextDisabled("Planning board: %s", autoShopState.planningRunning ? "running" : "not running");
            if (autoShopState.pending)
                ImGui::TextDisabled("Auto Shop: %s", autoShopState.message.c_str());
            else if (autoShopState.haveResult)
                ImGui::TextDisabled("Auto Shop %s: %s", autoShopState.lastSucceeded ? "success" : "failed", autoShopState.message.c_str());

            ImGui::Spacing();
            ImGui::SeparatorText("Auto Shop / Exotic Exports");
            ImGui::TextWrapped("This reads Rockstar's live VEHICLE_LIST random event. GTA publishes one spawned Exotic Export at a time; refresh after each delivery to locate the next vehicle when it spawns.");

            ImGui::BeginDisabled(exportState.pending);
            if (ImGui::Button("Refresh Live Exotic Export", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(exoticExports.QueueRefresh());
            ImGui::EndDisabled();
            DescribeLastV11Item("Read the Enhanced GSBD_RandomEvents VEHICLE_LIST block and its live trigger position.");

            ImGui::BeginDisabled(exportState.pending);
            if (ImGui::Button("Set Waypoint to Active Export", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(exoticExports.QueueWaypointToActive());
            ImGui::EndDisabled();
            DescribeLastV11Item("Re-read the live event and set a GTA waypoint directly to the spawned Exotic Export coordinates.");

            ImGui::BeginDisabled(exportState.pending);
            if (ImGui::Button("Teleport to Active Export", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(exoticExports.QueueTeleportToActive());
            ImGui::EndDisabled();
            DescribeLastV11Item("Re-read the live event and teleport beside the currently spawned Exotic Export vehicle.");

            ImGui::TextDisabled("Export state: %s", ExoticExportStateName(exportState.eventState));
            ImGui::TextDisabled("Event variation: %d | subvariation: %d", exportState.eventVariation, exportState.eventSubvariation);
            ImGui::TextDisabled("Vehicle-list index: %d | variation: %d", exportState.vehicleListIndex, exportState.vehicleListVariation);
            if (exportState.coordinatesValid)
            {
                ImGui::TextDisabled("Live coordinates: %.3f, %.3f, %.3f", exportState.x, exportState.y, exportState.z);
                ImGui::TextDisabled("Trigger range: %.1f", exportState.triggerRange);
            }
            else
            {
                ImGui::TextDisabled("Live coordinates: no spawned export is currently published");
            }

            if (exportState.pending)
                ImGui::TextDisabled("Exotic Export: %s", exportState.message.c_str());
            else if (exportState.haveResult)
                ImGui::TextDisabled("Exotic Export %s: %s", exportState.lastSucceeded ? "success" : "failed", exportState.message.c_str());
            else
                ImGui::TextDisabled("Exotic Export: ready");
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        SetV11Description("HIEST -> Heist Hub: Auto Shop contract/finale preparation plus live Exotic Export waypoint and teleport tools.");
    }
}
