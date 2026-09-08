#pragma once

#include "V11Theme.hpp"
#include "../features/heist/SalvageProcessingRuntime.hpp"
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

        ImGui::SeparatorText("Salvage Processing Lifts");
        auto& processing = Game::Heist::SalvageProcessingRuntime::Get();
        const auto process = processing.Snapshot();
        ImGui::BeginDisabled(process.pending);
        if (ImGui::Button(process.pending ? "Refreshing Processing..." : "Refresh Processing", ImVec2(-1.0f, 0.0f)))
            static_cast<void>(processing.QueueRefresh());
        if (ImGui::Button("Finish Lift 1 Processing", ImVec2(-1.0f, 0.0f)))
            static_cast<void>(processing.QueueFinishLift(1));
        if (ImGui::Button("Finish Lift 2 Processing", ImVec2(-1.0f, 0.0f)))
            static_cast<void>(processing.QueueFinishLift(2));
        if (ImGui::Button("Max Salvage Yard Income", ImVec2(-1.0f, 0.0f)))
            static_cast<void>(processing.QueueMaxIncome());
        ImGui::EndDisabled();
        ImGui::Text("Staff upgrade: %s", process.staffUpgrade ? "Yes" : "No");
        ImGui::Text("Wall safe upgrade: %s", process.wallSafeUpgrade ? "Yes" : "No");
        ImGui::Text("Income threshold: %d / 100", process.incomeThreshold);
        ImGui::Text("Lift 1 model: 0x%08X", static_cast<unsigned int>(process.liftModels[0]));
        ImGui::Text("Lift 1 value: $%d", process.liftValues[0]);
        ImGui::Text("Lift 1 POSIX: %d", process.liftPosix[0]);
        ImGui::Text("Lift 2 model: 0x%08X", static_cast<unsigned int>(process.liftModels[1]));
        ImGui::Text("Lift 2 value: $%d", process.liftValues[1]);
        ImGui::Text("Lift 2 POSIX: %d", process.liftPosix[1]);
        ImGui::TextDisabled("Lift completion advances Rockstar's processing timestamp by 5760s, or 2880s with the staff upgrade, and verifies the stat write.");
        if (process.pending || process.haveResult)
            ImGui::TextDisabled("%s", process.message.c_str());

        ImGui::SeparatorText("Tow Truck Operations");
        auto& towRuntime = Game::Heist::TowTruckOperationsRuntime::Get();
        const auto tow = towRuntime.Snapshot();
        ImGui::BeginDisabled(tow.pending);
        if (ImGui::Button(tow.pending ? "Refreshing Tow Truck Runtime..." : "Refresh Tow Truck Runtime", ImVec2(-1.0f, 0.0f)))
            static_cast<void>(towRuntime.QueueRefresh());
        if (ImGui::Button("Finish Current Tow Truck Job", ImVec2(-1.0f, 0.0f)))
            static_cast<void>(towRuntime.QueueFinishCurrentTowJob());
        ImGui::EndDisabled();
        ImGui::Text("fm_content_tow_truck_work: %s", tow.haveResult ? (tow.towWorkRunning ? "RUNNING" : "IDLE") : "UNKNOWN");
        ImGui::Text("Finish locals: %s", tow.finishLocalsReadable ? "VALID" : "UNAVAILABLE");
        ImGui::Text("Generic bitset: 0x%08X", tow.genericBitset);
        ImGui::Text("End reason: %d", tow.endReason);
        ImGui::Text("Tow work reward: 0x%08X", Game::Heist::TowTruckEnhanced173::TowTruckWorkRewardHash);
        ImGui::Text("Salvage vehicle: 0x%08X", Game::Heist::TowTruckEnhanced173::SalvageVehicleRewardHash);
        ImGui::Text("Yard vehicle sale: 0x%08X", Game::Heist::TowTruckEnhanced173::SalvageYardSellRewardHash);
        ImGui::TextDisabled("Instant finish preserves existing mission flags, sets only completion bit 11, then writes end reason 3. No Story Mode controller_towing dependency remains.");
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
