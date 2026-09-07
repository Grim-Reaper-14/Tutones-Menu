#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/heist/AutoShopContractRuntime.hpp"
#include "../features/heist/ExoticExportRuntime.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstddef>

namespace Tutones::UI
{
    namespace HeistHubDetail
    {
        inline void RenderAutoShop() noexcept
        {
            using Game::Heist::AutoShopContractName;
            using Game::Heist::AutoShopContractRuntime;
            using Game::Heist::AutoShopEnhanced173::ContractCount;

            auto& runtime = AutoShopContractRuntime::Get();
            const auto state = runtime.Snapshot();
            static int selectedContract = 0;
            selectedContract = std::clamp(selectedContract, 0, ContractCount - 1);

            ImGui::SeparatorText("Auto Shop Contracts");
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##autoshop_contract", AutoShopContractName(selectedContract)))
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

            ImGui::BeginDisabled(state.pending);
            if (ImGui::Button("Complete Contract Preps", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueReadyFinale(selectedContract));

            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float halfWidth = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;
            if (ImGui::Button("Reload Planning Board", ImVec2(halfWidth, 0.0f)))
                static_cast<void>(runtime.QueueReloadPlanning());
            ImGui::SameLine();
            if (ImGui::Button("Refresh", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueRefresh());
            ImGui::EndDisabled();

            ImGui::SeparatorText("Status");
            ImGui::Text("Contract: %s", AutoShopContractName(state.currentContract));
            ImGui::Text("Prep Mask: %d", state.prepMask);
            ImGui::Text("Planning Board: %s", state.planningRunning ? "Running" : "Idle");

            if (state.pending)
                ImGui::TextDisabled("%s", state.message.c_str());
            else if (state.haveResult)
                ImGui::TextDisabled("%s", state.message.c_str());
        }

        inline void RenderExoticExports() noexcept
        {
            using Game::Heist::ExoticExportRuntime;
            using Game::Heist::ExoticExportStateName;

            auto& runtime = ExoticExportRuntime::Get();
            const auto state = runtime.Snapshot();

            ImGui::SeparatorText("Exotic Exports");
            ImGui::BeginDisabled(state.pending);
            if (ImGui::Button("Refresh Active Export", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueRefresh());
            if (ImGui::Button("Set Waypoint", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueWaypointToActive());
            if (ImGui::Button("Teleport to Export", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueTeleportToActive());
            ImGui::EndDisabled();

            ImGui::SeparatorText("Status");
            ImGui::Text("State: %s", ExoticExportStateName(state.eventState));
            ImGui::Text("Event: %d / %d", state.eventVariation, state.eventSubvariation);
            ImGui::Text("Vehicle List: %d / %d", state.vehicleListIndex, state.vehicleListVariation);
            if (state.coordinatesValid)
                ImGui::Text("Location: %.1f, %.1f, %.1f", state.x, state.y, state.z);
            else
                ImGui::Text("Location: --");

            if (state.pending)
                ImGui::TextDisabled("%s", state.message.c_str());
            else if (state.haveResult)
                ImGui::TextDisabled("%s", state.message.c_str());
        }
    }

    inline void RenderHeistHubPanel(std::size_t page) noexcept
    {
        ImGui::SetCursorPos(ImVec2(226.0f, 52.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##heist_panel", ImVec2(490.0f, 394.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, page == 1 ? "Exotic Exports" : "Auto Shop");
            ImGui::Separator();

            if (page == 1)
                HeistHubDetail::RenderExoticExports();
            else
                HeistHubDetail::RenderAutoShop();
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        SetV11Description("");
    }
}
