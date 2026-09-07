#pragma once

#include "V11Theme.hpp"
#include "../features/heist/CasinoHeistRuntime.hpp"

#include <imgui.h>

#include <algorithm>

namespace Tutones::UI
{
    inline void RenderCasinoHeistPanel() noexcept
    {
        using namespace Game::Heist::CasinoHeistEnhanced173;
        using Game::Heist::CasinoHeistRuntime;

        auto& runtime = CasinoHeistRuntime::Get();
        const auto state = runtime.Snapshot();

        static int selectedTarget = 3;
        static int selectedApproach = 1;
        static bool selectedHardMode = false;
        static int selectedGunman = 4;
        static int selectedDriver = 2;
        static int selectedHacker = 5;
        static int selectedWeapon = 0;
        static int selectedVehicle = 0;
        static CutArray cuts{{100, 0, 0, 0}};
        static bool setupInitialized = false;
        static bool cutsInitialized = false;

        if (!setupInitialized && state.setupReady)
        {
            if (state.primaryTarget >= 0 && state.primaryTarget < TargetCount)
                selectedTarget = state.primaryTarget;
            if (state.approach >= 1 && state.approach <= ApproachCount)
                selectedApproach = state.approach;
            if (state.gunman >= 1 && state.gunman <= CrewCount)
                selectedGunman = state.gunman;
            if (state.driver >= 1 && state.driver <= CrewCount)
                selectedDriver = state.driver;
            if (state.hacker >= 1 && state.hacker <= CrewCount)
                selectedHacker = state.hacker;
            if (state.weaponLoadout >= 0 && state.weaponLoadout < WeaponLoadoutCount)
                selectedWeapon = state.weaponLoadout;
            if (state.getawayVehicle >= 0 && state.getawayVehicle < VehicleCount)
                selectedVehicle = state.getawayVehicle;
            selectedHardMode = state.approach >= 1 && state.hardApproach == state.approach;
            setupInitialized = true;
        }

        if (!cutsInitialized && state.cutsReady)
        {
            cuts = state.cuts;
            cutsInitialized = true;
        }

        selectedTarget = std::clamp(selectedTarget, 0, TargetCount - 1);
        selectedApproach = std::clamp(selectedApproach, 1, ApproachCount);
        selectedGunman = std::clamp(selectedGunman, 1, CrewCount);
        selectedDriver = std::clamp(selectedDriver, 1, CrewCount);
        selectedHacker = std::clamp(selectedHacker, 1, CrewCount);
        selectedWeapon = std::clamp(selectedWeapon, 0, WeaponLoadoutCount - 1);
        selectedVehicle = std::clamp(selectedVehicle, 0, VehicleCount - 1);

        ImGui::SeparatorText("Diamond Casino Heist");
        ImGui::SeparatorText("Planning Setup");
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("##casino_target", TargetName(selectedTarget)))
        {
            for (int target = 0; target < TargetCount; ++target)
            {
                const bool selected = target == selectedTarget;
                if (ImGui::Selectable(TargetName(target), selected))
                    selectedTarget = target;
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("##casino_approach", ApproachName(selectedApproach)))
        {
            for (int approach = 1; approach <= ApproachCount; ++approach)
            {
                const bool selected = approach == selectedApproach;
                if (ImGui::Selectable(ApproachName(approach), selected))
                    selectedApproach = approach;
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Checkbox("Hard Mode", &selectedHardMode);

        ImGui::SeparatorText("Crew");
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("##casino_gunman", GunmanName(selectedGunman)))
        {
            for (int gunman = 1; gunman <= CrewCount; ++gunman)
            {
                const bool selected = gunman == selectedGunman;
                if (ImGui::Selectable(GunmanName(gunman), selected))
                {
                    selectedGunman = gunman;
                    selectedWeapon = 0;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("##casino_driver", DriverName(selectedDriver)))
        {
            for (int driver = 1; driver <= CrewCount; ++driver)
            {
                const bool selected = driver == selectedDriver;
                if (ImGui::Selectable(DriverName(driver), selected))
                {
                    selectedDriver = driver;
                    selectedVehicle = 0;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("##casino_hacker", HackerName(selectedHacker)))
        {
            for (int hacker = 1; hacker <= CrewCount; ++hacker)
            {
                const bool selected = hacker == selectedHacker;
                if (ImGui::Selectable(HackerName(hacker), selected))
                    selectedHacker = hacker;
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::SeparatorText("Loadout & Getaway Vehicle");
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("##casino_weapon", WeaponLoadoutName(selectedGunman, selectedApproach, selectedWeapon)))
        {
            for (int weapon = 0; weapon < WeaponLoadoutCount; ++weapon)
            {
                const bool selected = weapon == selectedWeapon;
                if (ImGui::Selectable(WeaponLoadoutName(selectedGunman, selectedApproach, weapon), selected))
                    selectedWeapon = weapon;
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("##casino_vehicle", GetawayVehicleName(selectedDriver, selectedVehicle)))
        {
            for (int vehicle = 0; vehicle < VehicleCount; ++vehicle)
            {
                const bool selected = vehicle == selectedVehicle;
                if (ImGui::Selectable(GetawayVehicleName(selectedDriver, vehicle), selected))
                    selectedVehicle = vehicle;
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        const bool busy = state.pending;
        ImGui::BeginDisabled(busy);
        if (ImGui::Button("Apply + Complete Casino Setup", ImVec2(-1.0f, 0.0f)))
        {
            static_cast<void>(runtime.QueueSetup(
                selectedTarget,
                selectedApproach,
                selectedHardMode,
                selectedGunman,
                selectedDriver,
                selectedHacker,
                selectedWeapon,
                selectedVehicle));
        }

        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float halfWidth = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;
        if (ImGui::Button("Refresh Casino State", ImVec2(halfWidth, 0.0f)))
        {
            setupInitialized = false;
            cutsInitialized = false;
            static_cast<void>(runtime.QueueRefresh());
        }
        ImGui::SameLine();
        if (ImGui::Button("Refresh Planning Board", ImVec2(-1.0f, 0.0f)))
            static_cast<void>(runtime.QueuePlanningBoardRefresh());
        ImGui::EndDisabled();

        ImGui::SeparatorText("Player Pay Cuts");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputInt("Player 1 %##casino_cut1", &cuts[0], 1, 5);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputInt("Player 2 %##casino_cut2", &cuts[1], 1, 5);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputInt("Player 3 %##casino_cut3", &cuts[2], 1, 5);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputInt("Player 4 %##casino_cut4", &cuts[3], 1, 5);

        for (int& cut : cuts)
            cut = std::clamp(cut, 0, MaxCut);

        const float thirdWidth = (ImGui::GetContentRegionAvail().x - (spacing * 2.0f)) / 3.0f;
        if (ImGui::Button("Solo 100##casino", ImVec2(thirdWidth, 0.0f)))
            cuts = CutArray{{100, 0, 0, 0}};
        ImGui::SameLine();
        if (ImGui::Button("25 Each##casino", ImVec2(thirdWidth, 0.0f)))
            cuts = CutArray{{25, 25, 25, 25}};
        ImGui::SameLine();
        if (ImGui::Button("100 Each##casino", ImVec2(-1.0f, 0.0f)))
            cuts = CutArray{{100, 100, 100, 100}};

        ImGui::Text("Configured total: %d%%", cuts[0] + cuts[1] + cuts[2] + cuts[3]);
        ImGui::BeginDisabled(busy);
        if (ImGui::Button("Apply Casino Cuts", ImVec2(-1.0f, 0.0f)))
            static_cast<void>(runtime.QueueCuts(cuts));
        ImGui::EndDisabled();

        ImGui::SeparatorText("Live Planning State");
        ImGui::Text("Session: %s", state.sessionStarted ? "Online" : "Offline");
        ImGui::Text("Planning board: %s", state.planningRunning ? "Running" : "Idle");
        ImGui::Text("Script globals: %s", state.globalsReady ? "Ready" : "Unavailable");

        if (state.setupReady)
        {
            ImGui::Text("Target: %s", TargetName(state.primaryTarget));
            ImGui::Text("Approach: %s", ApproachName(state.approach));
            ImGui::Text("Difficulty: %s", state.hardApproach == state.approach && state.approach > 0 ? "Hard" : "Normal");
            ImGui::Text("Gunman: %s", GunmanName(state.gunman));
            ImGui::Text("Driver: %s", DriverName(state.driver));
            ImGui::Text("Hacker: %s", HackerName(state.hacker));
            ImGui::Text("Weapon: %s", WeaponLoadoutName(state.gunman, state.approach, state.weaponLoadout));
            ImGui::Text("Vehicle: %s", GetawayVehicleName(state.driver, state.getawayVehicle));
            ImGui::Text("POIs: %s (0x%X)", state.poiMask == AllPoiMask ? "All scoped" : "Partial", state.poiMask);
            ImGui::Text("Access points: %s (0x%X)", state.accessPointsMask == AllAccessPointsMask ? "All unlocked" : "Partial", state.accessPointsMask);
            ImGui::Text("Security: %s", state.disruptShipment == 3 ? "Weak" : "Custom");
            ImGui::Text("Security pass: %s", state.keyLevel == 2 ? "Level 2" : "Custom");
        }

        if (state.cutsReady)
        {
            ImGui::Text(
                "Current cuts: P1 %d%% | P2 %d%% | P3 %d%% | P4 %d%%",
                state.cuts[0],
                state.cuts[1],
                state.cuts[2],
                state.cuts[3]);
        }

        if (state.pending || state.haveResult)
            ImGui::TextDisabled("%s", state.message.c_str());
    }
}
