#pragma once

#include "V11Theme.hpp"
#include "../features/heist/SalvageYardRuntime.hpp"
#include "../features/heist/TowTruckOperationsRuntime.hpp"

#include <imgui.h>

namespace Tutones::UI
{
    inline void RenderSalvageYardPanel() noexcept
    {
        using namespace Game::Heist::SalvageYardEnhanced173;
        using Game::Heist::SalvageYardRuntime;

        auto& runtime = SalvageYardRuntime::Get();
        const auto state = runtime.Snapshot();

        ImGui::SeparatorText("Salvage Yard Robberies");

        ImGui::BeginDisabled(state.pending);
        if (ImGui::Button("Complete Current Robbery Preps", ImVec2(-1.0f, 0.0f)))
            static_cast<void>(runtime.QueueCompleteCurrentPreps());
        if (ImGui::Button("Refresh Salvage State", ImVec2(-1.0f, 0.0f)))
            static_cast<void>(runtime.QueueRefresh());
        ImGui::EndDisabled();

        ImGui::SeparatorText("Current Robbery");
        ImGui::Text("Robbery: %s", RobberyName(state.currentRobbery));
        ImGui::Text("Planning board: %s", state.planningRunning ? "Running" : "Idle");
        ImGui::Text("Scope: %s", state.scopeComplete ? "Complete" : "Incomplete");
        ImGui::Text("Prep flags: %s", state.prepFlagsComplete ? "Complete" : "Incomplete");

        if (state.currentRobbery >= 0 && state.currentRobbery < RobberyCount)
        {
            ImGui::Text("Target model hash: 0x%08X", static_cast<unsigned int>(state.vehicleModel));
            ImGui::Text("Vehicle mods: %d", state.vehicleMods);
            ImGui::Text("Can keep: %s", state.canKeep != 0 ? "Yes" : "No");
            ImGui::Text("Sale field: %d", state.saleValue);
            ImGui::Text("Vehicle slot: %d", state.vehicleSlot);
            ImGui::Text("Vehicle week: %d", state.vehicleWeek);
            ImGui::Text("Disruption state: %d", state.disruptionState);
        }

        ImGui::SeparatorText("Weekly Robbery Status");
        ImGui::Text("Slot 1: %d", state.weeklyStatuses[0]);
        ImGui::Text("Slot 2: %d", state.weeklyStatuses[1]);
        ImGui::Text("Slot 3: %d", state.weeklyStatuses[2]);

        ImGui::SeparatorText("Live Flow Flags");
        ImGui::Text("General: 0x%08X", state.generalFlags);
        ImGui::Text("Freemode progress: 0x%08X", state.freemodeProgress);
        ImGui::Text("Instance progress: 0x%08X", state.instanceProgress);
        ImGui::Text("Scope flags: 0x%08X", state.scopeFlags);
        ImGui::Text("Salvage flags: 0x%08X", state.salvageFlags);
        ImGui::Text("Packed vehicle state: %d", state.packedVehicleState);

        ImGui::SeparatorText("Tow Truck Operations");
        auto& towRuntime = Game::Heist::TowTruckOperationsRuntime::Get();
        const auto tow = towRuntime.Snapshot();
        ImGui::BeginDisabled(tow.pending);
        if (ImGui::Button(tow.pending ? "Refreshing Tow Truck Runtime..." : "Refresh Tow Truck Runtime", ImVec2(-1.0f, 0.0f)))
            static_cast<void>(towRuntime.QueueRefresh());
        ImGui::EndDisabled();
        ImGui::Text("controller_towing: %s", tow.haveResult ? (tow.towingControllerRunning ? "RUNNING" : "IDLE") : "UNKNOWN");
        ImGui::Text("Tow work reward: 0x%08X", Game::Heist::TowTruckEnhanced173::TowTruckWorkRewardHash);
        ImGui::Text("Salvage vehicle: 0x%08X", Game::Heist::TowTruckEnhanced173::SalvageVehicleRewardHash);
        ImGui::Text("Yard vehicle sale: 0x%08X", Game::Heist::TowTruckEnhanced173::SalvageYardSellRewardHash);
        ImGui::TextDisabled("Reward contracts are decompile-proven; job-state and processing writes remain locked until their exact Enhanced flow is verified.");
        if (tow.pending || tow.haveResult)
            ImGui::TextDisabled("%s", tow.message.c_str());

        ImGui::SeparatorText("Runtime");
        ImGui::Text("Session: %s", state.sessionStarted ? "Online" : "Offline");
        ImGui::Text("Native backend: %s", state.nativeReady ? "Ready" : "Unavailable");
        ImGui::Text("Script globals: %s", state.globalsReady ? "Ready" : "Unavailable");

        if (state.pending || state.haveResult)
            ImGui::TextDisabled("%s", state.message.c_str());
    }
}
