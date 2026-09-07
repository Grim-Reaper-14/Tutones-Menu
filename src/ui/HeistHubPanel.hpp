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
        inline void RenderDecompileReference(
            const char* title,
            const char* planningScript,
            const char* missionController,
            const char* note) noexcept
        {
            ImGui::SeparatorText(title);
            ImGui::TextDisabled("Enhanced decompile-backed");
            ImGui::Spacing();

            ImGui::Text("Planning script:");
            ImGui::SameLine();
            ImGui::TextColored(V11Theme::Accent, "%s", planningScript);

            if (missionController && missionController[0] != '\0')
            {
                ImGui::Text("Mission controller:");
                ImGui::SameLine();
                ImGui::TextColored(V11Theme::Accent, "%s", missionController);
            }

            ImGui::Spacing();
            ImGui::TextWrapped("%s", note);
            ImGui::Spacing();
            ImGui::SeparatorText("Runtime status");
            ImGui::TextDisabled("Reference-only until the Enhanced 1.73 write state is verified.");
            ImGui::TextDisabled("No guessed globals, locals or finale-launch writes are exposed here.");
        }

        inline void RenderApartmentHeists() noexcept
        {
            RenderDecompileReference(
                "Apartment Heists",
                "fm_mission_controller.c",
                "fm_mission_controller.c",
                "The original Online heists share the classic mission-controller path. This tab is reserved for verified setup, finale and payout state once the current Enhanced offsets are mapped safely.");
        }

        inline void RenderDoomsdayHeist() noexcept
        {
            RenderDecompileReference(
                "Doomsday Heist",
                "gb_gang_ops_planning.c",
                "fm_mission_controller.c",
                "Facility planning is backed by gb_gang_ops_planning. Runtime controls stay disabled until its current Enhanced planning state and mission-controller handoff are verified together.");
        }

        inline void RenderCasinoHeist() noexcept
        {
            RenderDecompileReference(
                "Diamond Casino Heist",
                "gb_casino_heist_planning.c",
                "fm_mission_controller.c",
                "The Arcade planning board has its own decompiled planning script. This page is ready for scoped target, approach, crew and prep controls after their Enhanced state is confirmed.");
        }

        inline void RenderCayoPericoHeist() noexcept
        {
            RenderDecompileReference(
                "Cayo Perico Heist",
                "heist_island_planning.c",
                "fm_mission_controller_2020.c",
                "Cayo uses the island planning script and the 2020 mission controller. This keeps Cayo state isolated from the classic mission-controller flow before any writes are enabled.");
        }

        inline void RenderSalvageYard() noexcept
        {
            RenderDecompileReference(
                "Salvage Yard Robberies",
                "vehrob_planning.c",
                "",
                "Salvage Yard robbery planning is present in the Enhanced decompile set. Contract selection, planning and completion writes will remain disabled until the current script state is mapped and tested.");
        }

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
            ImGui::TextDisabled("Enhanced source: tuner_planning.c");
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
            ImGui::TextDisabled("Existing decompile-backed utility retained outside the heist families.");
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
        static_cast<void>(page);

        ImGui::SetCursorPos(ImVec2(226.0f, 52.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##heist_panel", ImVec2(490.0f, 394.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Enhanced Heist Hub");
            ImGui::SameLine();
            ImGui::TextDisabled("decompiled script routing");
            ImGui::Separator();

            if (ImGui::BeginTabBar("##enhanced_heist_tabs", ImGuiTabBarFlags_FittingPolicyScroll))
            {
                if (ImGui::BeginTabItem("Apartment"))
                {
                    HeistHubDetail::RenderApartmentHeists();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Doomsday"))
                {
                    HeistHubDetail::RenderDoomsdayHeist();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Casino"))
                {
                    HeistHubDetail::RenderCasinoHeist();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Cayo Perico"))
                {
                    HeistHubDetail::RenderCayoPericoHeist();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Auto Shop"))
                {
                    HeistHubDetail::RenderAutoShop();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Salvage Yard"))
                {
                    HeistHubDetail::RenderSalvageYard();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Exports"))
                {
                    HeistHubDetail::RenderExoticExports();
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        SetV11Description("");
    }
}
