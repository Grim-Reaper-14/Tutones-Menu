#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "CasinoHeistPanel.hpp"
#include "../features/heist/AutoShopContractRuntime.hpp"
#include "../features/heist/CayoLootRuntime.hpp"
#include "../features/heist/CayoPericoRuntime.hpp"
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
            RenderCasinoHeistPanel();
        }

        inline void RenderCayoPericoHeist() noexcept
        {
            using namespace Game::Heist::CayoPericoEnhanced173;
            using namespace Game::Heist::CayoLootEnhanced173;
            using Game::Heist::CayoLootConfig;
            using Game::Heist::CayoLootRuntime;
            using Game::Heist::CayoPericoRuntime;

            auto& runtime = CayoPericoRuntime::Get();
            auto& lootRuntime = CayoLootRuntime::Get();
            const auto state = runtime.Snapshot();
            const auto lootState = lootRuntime.Snapshot();

            static int selectedTarget = 5;
            static int selectedDifficulty = 0;
            static int selectedWeapon = 1;
            static CayoLootConfig lootConfig{};
            static CutArray cuts{{100, 0, 0, 0}};
            static bool lootInitialized = false;
            static bool cutsInitialized = false;

            selectedTarget = std::clamp(selectedTarget, 0, TargetCount - 1);
            selectedDifficulty = std::clamp(selectedDifficulty, 0, 1);
            selectedWeapon = std::clamp(selectedWeapon, 1, WeaponCount);

            if (!lootInitialized && lootState.lootReady)
            {
                lootConfig.cash = lootState.cash;
                lootConfig.coke = lootState.coke;
                lootConfig.gold = lootState.gold;
                lootConfig.weed = lootState.weed;
                lootConfig.paintings = lootState.paintings;
                lootConfig.cashValue = lootState.cashValue;
                lootConfig.cokeValue = lootState.cokeValue;
                lootConfig.goldValue = lootState.goldValue;
                lootConfig.weedValue = lootState.weedValue;
                lootConfig.paintingValue = lootState.paintingValue;
                lootInitialized = true;
            }

            if (!cutsInitialized && lootState.cutsReady)
            {
                cuts = lootState.cuts;
                cutsInitialized = true;
            }

            ImGui::SeparatorText("Cayo Perico Heist");
            ImGui::TextDisabled("Enhanced 1.73 source: heist_island_planning.c");
            ImGui::TextDisabled("Setup, loot and cuts verify writes and roll back on read-back failure.");

            ImGui::SeparatorText("Setup");
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##cayo_target", PrimaryTargetName(selectedTarget)))
            {
                for (int target = 0; target < TargetCount; ++target)
                {
                    const bool selected = target == selectedTarget;
                    if (ImGui::Selectable(PrimaryTargetName(target), selected))
                        selectedTarget = target;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            const char* difficultyPreview = selectedDifficulty == 0 ? "Normal" : "Hard";
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##cayo_difficulty", difficultyPreview))
            {
                if (ImGui::Selectable("Normal", selectedDifficulty == 0))
                    selectedDifficulty = 0;
                if (ImGui::Selectable("Hard", selectedDifficulty == 1))
                    selectedDifficulty = 1;
                ImGui::EndCombo();
            }

            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##cayo_weapon", WeaponLoadoutName(selectedWeapon)))
            {
                for (int weapon = 1; weapon <= WeaponCount; ++weapon)
                {
                    const bool selected = weapon == selectedWeapon;
                    if (ImGui::Selectable(WeaponLoadoutName(weapon), selected))
                        selectedWeapon = weapon;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            const int difficulty = selectedDifficulty == 0 ? NormalDifficulty : HardDifficulty;
            const bool busy = state.pending || lootState.pending;
            ImGui::BeginDisabled(busy);
            if (ImGui::Button("Complete Cayo Setup", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueSetup(selectedTarget, difficulty, selectedWeapon));
            if (ImGui::Button("Refresh All Cayo State", ImVec2(-1.0f, 0.0f)))
            {
                static_cast<void>(runtime.QueueRefresh());
                static_cast<void>(lootRuntime.QueueRefresh());
            }
            ImGui::EndDisabled();

            ImGui::SeparatorText("Secondary Loot");
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float halfWidth = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;
            if (ImGui::Button("All Loot", ImVec2(halfWidth, 0.0f)))
            {
                lootConfig.cash = true;
                lootConfig.coke = true;
                lootConfig.gold = true;
                lootConfig.weed = true;
                lootConfig.paintings = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear Loot", ImVec2(-1.0f, 0.0f)))
            {
                lootConfig.cash = false;
                lootConfig.coke = false;
                lootConfig.gold = false;
                lootConfig.weed = false;
                lootConfig.paintings = false;
            }

            ImGui::Checkbox("Cash", &lootConfig.cash);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputInt("##cayo_cash_value", &lootConfig.cashValue, 1000, 10000);

            ImGui::Checkbox("Coke", &lootConfig.coke);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputInt("##cayo_coke_value", &lootConfig.cokeValue, 1000, 10000);

            ImGui::Checkbox("Gold", &lootConfig.gold);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputInt("##cayo_gold_value", &lootConfig.goldValue, 1000, 10000);

            ImGui::Checkbox("Weed", &lootConfig.weed);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputInt("##cayo_weed_value", &lootConfig.weedValue, 1000, 10000);

            ImGui::Checkbox("Paintings", &lootConfig.paintings);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputInt("##cayo_paint_value", &lootConfig.paintingValue, 1000, 10000);

            lootConfig.cashValue = std::clamp(lootConfig.cashValue, 0, MaximumLootValue);
            lootConfig.cokeValue = std::clamp(lootConfig.cokeValue, 0, MaximumLootValue);
            lootConfig.goldValue = std::clamp(lootConfig.goldValue, 0, MaximumLootValue);
            lootConfig.weedValue = std::clamp(lootConfig.weedValue, 0, MaximumLootValue);
            lootConfig.paintingValue = std::clamp(lootConfig.paintingValue, 0, MaximumLootValue);

            ImGui::BeginDisabled(busy);
            if (ImGui::Button("Apply Secondary Loot", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(lootRuntime.QueueApplyLoot(lootConfig));
            ImGui::EndDisabled();
            ImGui::TextDisabled("Values are per-loot values. Selected loot is also marked scoped on the planning board.");

            if (lootState.lootReady)
            {
                ImGui::Text("Current Cash: %s / $%d", lootState.cash ? "ON" : "OFF", lootState.cashValue);
                ImGui::Text("Current Coke: %s / $%d", lootState.coke ? "ON" : "OFF", lootState.cokeValue);
                ImGui::Text("Current Gold: %s / $%d", lootState.gold ? "ON" : "OFF", lootState.goldValue);
                ImGui::Text("Current Weed: %s / $%d", lootState.weed ? "ON" : "OFF", lootState.weedValue);
                ImGui::Text("Current Paintings: %s / $%d", lootState.paintings ? "ON" : "OFF", lootState.paintingValue);
            }

            ImGui::SeparatorText("Player Pay Cuts");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputInt("Player 1 %##cayo_cut1", &cuts[0], 1, 5);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputInt("Player 2 %##cayo_cut2", &cuts[1], 1, 5);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputInt("Player 3 %##cayo_cut3", &cuts[2], 1, 5);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputInt("Player 4 %##cayo_cut4", &cuts[3], 1, 5);

            for (int& cut : cuts)
                cut = std::clamp(cut, 0, 100);

            const float thirdWidth = (ImGui::GetContentRegionAvail().x - (spacing * 2.0f)) / 3.0f;
            if (ImGui::Button("Solo 100", ImVec2(thirdWidth, 0.0f)))
                cuts = CutArray{{100, 0, 0, 0}};
            ImGui::SameLine();
            if (ImGui::Button("25 Each", ImVec2(thirdWidth, 0.0f)))
                cuts = CutArray{{25, 25, 25, 25}};
            ImGui::SameLine();
            if (ImGui::Button("100 Each", ImVec2(-1.0f, 0.0f)))
                cuts = CutArray{{100, 100, 100, 100}};

            const int cutTotal = cuts[0] + cuts[1] + cuts[2] + cuts[3];
            ImGui::TextDisabled("Configured total: %d%%", cutTotal);
            ImGui::BeginDisabled(busy);
            if (ImGui::Button("Apply Player Cuts", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(lootRuntime.QueueApplyCuts(cuts));
            ImGui::EndDisabled();

            if (lootState.cutsReady)
            {
                ImGui::Text(
                    "Current cuts: P1 %d%% | P2 %d%% | P3 %d%% | P4 %d%%",
                    lootState.cuts[0],
                    lootState.cuts[1],
                    lootState.cuts[2],
                    lootState.cuts[3]);
            }

            ImGui::SeparatorText("Live planning state");
            ImGui::Text("Session: %s", state.sessionStarted ? "Online" : "Offline");
            ImGui::Text("Script globals: %s", state.globalsReady ? "Ready" : "Unavailable");
            ImGui::Text("Planning board: %s", state.planningRunning ? "Running" : "Idle");

            if (state.haveResult && state.lastSucceeded)
            {
                ImGui::Text("Primary target: %s", PrimaryTargetName(state.primaryTarget));
                ImGui::Text("Difficulty: %s", DifficultyName(state.difficulty));
                ImGui::Text("Weapon loadout: %s", WeaponLoadoutName(state.weaponLoadout));
                ImGui::Text("Target variation: %d", state.targetVariation);
                ImGui::Text("Primary equipment: %s", RequiredPrimaryEquipment(state.targetVariation));
                ImGui::Text("Progress flags: 0x%08X", state.progressionFlags);
                ImGui::Text("Intel flags: 0x%08X", state.intelFlags);

                ImGui::SeparatorText("Scoped intel");
                ImGui::Text("Power Station: %s", state.powerStationScoped ? "Scoped" : "Not scoped");
                ImGui::Text("Control Tower: %s", state.controlTowerScoped ? "Scoped" : "Not scoped");
                ImGui::Text("Bolt Cutters: %s", state.boltCuttersScoped ? "Scoped" : "Not scoped");
                ImGui::Text("Grappling Equipment: %s", state.grapplingScoped ? "Scoped" : "Not scoped");
                ImGui::Text("Guard Clothing: %s", state.guardClothingScoped ? "Scoped" : "Not scoped");
                ImGui::Text("Supply Truck: %s", state.supplyTruckScoped ? "Scoped" : "Not scoped");
            }

            if (state.pending)
                ImGui::TextDisabled("Setup: %s", state.message.c_str());
            else if (state.haveResult)
                ImGui::TextDisabled("Setup: %s", state.message.c_str());

            if (lootState.pending)
                ImGui::TextDisabled("Loot/Cuts: %s", lootState.message.c_str());
            else if (lootState.haveResult)
                ImGui::TextDisabled("Loot/Cuts: %s", lootState.message.c_str());
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
