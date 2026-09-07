#pragma once

#include "V11Theme.hpp"
#include "../features/heist/SalvageYardRuntime.hpp"

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

        ImGui::SeparatorText("Runtime");
        ImGui::Text("Session: %s", state.sessionStarted ? "Online" : "Offline");
        ImGui::Text("Native backend: %s", state.nativeReady ? "Ready" : "Unavailable");
        ImGui::Text("Script globals: %s", state.globalsReady ? "Ready" : "Unavailable");

        if (state.pending || state.haveResult)
            ImGui::TextDisabled("%s", state.message.c_str());
    }
}
