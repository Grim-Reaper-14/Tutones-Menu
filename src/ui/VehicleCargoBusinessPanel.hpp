#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/business/BusinessScriptMonitorRuntime.hpp"
#include "../features/business/VehicleCargoInstantGarageRuntime.hpp"
#include "../features/business/VehicleCargoInstantSellRuntime.hpp"
#include "../features/business/VehicleCargoTuningRuntime.hpp"

#include <imgui.h>

#include <algorithm>

namespace Tutones::UI
{
    inline void RenderVehicleCargoBusinessPanel() noexcept
    {
        using Game::Business::BusinessScriptMonitorRuntime;
        using Game::Business::VehicleCargoInstantGarageRuntime;
        using Game::Business::VehicleCargoInstantSellRuntime;
        using Game::Business::VehicleCargoTuningProfile;
        using Game::Business::VehicleCargoTuningRuntime;

        auto& monitor = BusinessScriptMonitorRuntime::Get();
        const auto monitorState = monitor.Snapshot();
        auto& garageSource = VehicleCargoInstantGarageRuntime::Get();
        const auto garageSourceState = garageSource.Snapshot();
        auto& instantSell = VehicleCargoInstantSellRuntime::Get();
        const auto instantSellState = instantSell.Snapshot();
        auto& tuning = VehicleCargoTuningRuntime::Get();
        const auto tuningState = tuning.Snapshot();

        static VehicleCargoTuningProfile profile{};

        ImGui::SetCursorPos(ImVec2(226.0f, 52.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##vehicle_cargo_business_panel", ImVec2(640.0f, 394.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Vehicle Cargo / Import Export");
            ImGui::SameLine();
            ImGui::TextDisabled("Enhanced 1.73 / b1158.13");
            ImGui::Separator();

            ImGui::TextWrapped(
                "Auto Source finishes through the Vehicle Warehouse delivery path, and Instant Sell now follows Rockstar's live export objectives during activity 188 so the mission itself owns completion, payout and save state.");
            ImGui::Spacing();

            if (ImGui::CollapsingHeader("Auto Source Into Garage", ImGuiTreeNodeFlags_DefaultOpen))
            {
                bool enabled = garageSourceState.enabled;
                if (ImGui::Checkbox("Auto Source vehicles into garage", &enabled))
                    garageSource.SetEnabled(enabled);
                DescribeLastV11Item(
                    "Runs the dedicated source -> acquire -> warehouse delivery pipeline repeatedly. Rockstar performs the warehouse transition/save so each completed source becomes real Vehicle Warehouse stock.");

                ImGui::SameLine();
                ImGui::BeginDisabled(garageSourceState.pending);
                if (ImGui::Button("Source one into garage", ImVec2(-1.0f, 0.0f)))
                    static_cast<void>(garageSource.QueueStoreNow());
                ImGui::EndDisabled();
                DescribeLastV11Item(
                    "Runs one complete Vehicle Cargo source and immediately delivers the acquired source car into the owned Vehicle Warehouse.");

                ImGui::SeparatorText("Garage Source Status");
                ImGui::Text("Runtime: %s",
                    garageSourceState.pending ? "WORKING" : (garageSourceState.enabled ? "AUTO" : "IDLE"));
                ImGui::Text("Session: %s", garageSourceState.sessionReady ? "READY" : "WAITING");
                ImGui::Text("Vehicle Warehouse: %s", garageSourceState.warehouseReady ? "READY" : "WAITING");
                ImGui::Text("Source mission: %s", garageSourceState.missionRunning ? "RUNNING" : "WAITING");
                ImGui::Text("Source vehicle: %s", garageSourceState.sourceVehicleReady ? "ACQUIRED" : "WAITING");
                ImGui::Text("Garage delivery: %s", garageSourceState.deliveryIssued ? "ISSUED" : "WAITING");

                if (garageSourceState.warehouseProperty != 0)
                    ImGui::Text("Warehouse property: %d", garageSourceState.warehouseProperty);
                if (garageSourceState.sourceVariation > 0)
                    ImGui::Text("Source variation: %d / 96", garageSourceState.sourceVariation);
                ImGui::Text("Warehouse stock: %d / 40", garageSourceState.warehouseStock);

                ImGui::TextWrapped("%s", garageSourceState.message.c_str());
                if (garageSourceState.pending || garageSourceState.missionRunning)
                {
                    ImGui::TextDisabled("Source: %s", garageSourceState.sourceMessage.c_str());
                    ImGui::TextDisabled("Delivery: %s", garageSourceState.deliveryMessage.c_str());
                }

                ImGui::Spacing();
                ImGui::TextDisabled(
                    "Garage-source contract: source owns activity 178 and acquisition; delivery owns the real warehouse entrance transition and save observation. Auto Source only coordinates the handoff, so a completed cycle lands the sourced car in the Vehicle Warehouse instead of relying on a raw inventory-slot write.");
            }

            if (ImGui::CollapsingHeader("Instant Vehicle Cargo Sell", ImGuiTreeNodeFlags_DefaultOpen))
            {
                bool enabled = instantSellState.enabled;
                if (ImGui::Checkbox("Instantly deliver Vehicle Cargo sales", &enabled))
                    instantSell.SetEnabled(enabled);
                DescribeLastV11Item(
                    "Arms only for Vehicle Cargo sell activity 188. Tutones discovers Rockstar's newly-created coordinate objective, preloads collision, obtains network control of the actual export vehicle and moves it once through that objective. Intermediate export objectives can advance in sequence; gb_vehicle_export remains responsible for the payout and final save.");

                ImGui::SeparatorText("Instant Sell Status");
                ImGui::Text("Runtime: %s",
                    instantSellState.pending ? "WORKING" : (instantSellState.enabled ? "ARMED" : "IDLE"));
                ImGui::Text("Sell activity 188: %s", instantSellState.sellActivity ? "RUNNING" : "WAITING");
                ImGui::Text("Export vehicle: %s", instantSellState.vehicleReady ? "READY" : "WAITING");
                ImGui::Text("Route objective: %s", instantSellState.objectiveReady ? "LOCKED" : "WAITING");
                ImGui::Text("Objective stages completed: %d", instantSellState.stagesCompleted);
                if (instantSellState.controlAttempts > 0)
                    ImGui::Text("Network-control attempts: %d", instantSellState.controlAttempts);
                if (instantSellState.objectiveReady)
                {
                    ImGui::Text("Objective: %.2f, %.2f, %.2f",
                        instantSellState.targetX,
                        instantSellState.targetY,
                        instantSellState.targetZ);
                }
                ImGui::TextWrapped("%s", instantSellState.message.c_str());

                if (instantSellState.haveResult)
                    ImGui::TextDisabled("Last result: %s", instantSellState.lastSucceeded ? "SUCCESS" : "STOPPED SAFELY");

                ImGui::Spacing();
                ImGui::TextDisabled(
                    "Sell contract: no warehouse-stock writes, no forced payout globals and no blind destination list. The runtime only acts while activity 188 is live and refuses movement when more than one unrelated new coordinate objective is present.");
            }

            if (ImGui::CollapsingHeader("Vehicle Cargo Tunables", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::SeparatorText("Steal Mission Cooldown");
                ImGui::SetNextItemWidth(250.0f);
                ImGui::InputInt("Steal cooldown (ms)", &profile.stealCooldownMs, 1000, 10000);
                profile.stealCooldownMs = std::max(0, profile.stealCooldownMs);
                ImGui::TextDisabled("Supplied default: 180000 ms / 3 minutes");

                ImGui::SeparatorText("Sell Cooldowns");
                ImGui::SetNextItemWidth(250.0f);
                ImGui::InputInt("1 vehicle (ms)", &profile.sellCooldown1Ms, 1000, 10000);
                ImGui::SetNextItemWidth(250.0f);
                ImGui::InputInt("2 vehicles (ms)", &profile.sellCooldown2Ms, 1000, 10000);
                ImGui::SetNextItemWidth(250.0f);
                ImGui::InputInt("3 vehicles (ms)", &profile.sellCooldown3Ms, 1000, 10000);
                ImGui::SetNextItemWidth(250.0f);
                ImGui::InputInt("4 vehicles (ms)", &profile.sellCooldown4Ms, 1000, 10000);
                profile.sellCooldown1Ms = std::max(0, profile.sellCooldown1Ms);
                profile.sellCooldown2Ms = std::max(0, profile.sellCooldown2Ms);
                profile.sellCooldown3Ms = std::max(0, profile.sellCooldown3Ms);
                profile.sellCooldown4Ms = std::max(0, profile.sellCooldown4Ms);
                ImGui::TextDisabled("Supplied: 1200000 / 1680000 / 2340000 / 2880000 ms");

                ImGui::SeparatorText("Sell Prices");
                ImGui::SetNextItemWidth(250.0f);
                ImGui::InputInt("Top Range", &profile.topRangeSellPrice, 1000, 5000);
                ImGui::SetNextItemWidth(250.0f);
                ImGui::InputInt("Mid Range", &profile.midRangeSellPrice, 1000, 5000);
                ImGui::SetNextItemWidth(250.0f);
                ImGui::InputInt("Standard Range", &profile.standardRangeSellPrice, 1000, 5000);
                profile.topRangeSellPrice = std::max(0, profile.topRangeSellPrice);
                profile.midRangeSellPrice = std::max(0, profile.midRangeSellPrice);
                profile.standardRangeSellPrice = std::max(0, profile.standardRangeSellPrice);
                ImGui::TextDisabled("Supplied: Top $40000 / Mid $25000 / Standard $15000");

                ImGui::SeparatorText("Actions");
                ImGui::BeginDisabled(tuningState.pending);
                if (ImGui::Button("Apply Vehicle Cargo globals", ImVec2(250.0f, 0.0f)))
                    static_cast<void>(tuning.QueueApplyProfile(profile));
                ImGui::SameLine();
                if (ImGui::Button("Refresh current values", ImVec2(-1.0f, 0.0f)))
                    static_cast<void>(tuning.QueueRefresh());
                ImGui::EndDisabled();

                if (tuningState.readable)
                {
                    if (ImGui::Button("Load current values into editor", ImVec2(250.0f, 0.0f)))
                        profile = tuningState.values;
                    ImGui::SameLine();
                    if (ImGui::Button("Reset supplied defaults", ImVec2(-1.0f, 0.0f)))
                        profile = VehicleCargoTuningProfile{};

                    ImGui::SeparatorText("Current Read-Back");
                    ImGui::Text("Steal cooldown: %d ms", tuningState.values.stealCooldownMs);
                    ImGui::Text("Sell cooldowns: %d / %d / %d / %d ms",
                        tuningState.values.sellCooldown1Ms,
                        tuningState.values.sellCooldown2Ms,
                        tuningState.values.sellCooldown3Ms,
                        tuningState.values.sellCooldown4Ms);
                    ImGui::Text("Sell prices: Top $%d / Mid $%d / Standard $%d",
                        tuningState.values.topRangeSellPrice,
                        tuningState.values.midRangeSellPrice,
                        tuningState.values.standardRangeSellPrice);
                }
                else if (!tuningState.pending)
                {
                    if (ImGui::Button("Reset supplied defaults", ImVec2(-1.0f, 0.0f)))
                        profile = VehicleCargoTuningProfile{};
                }

                if (tuningState.pending)
                    ImGui::TextDisabled("%s", tuningState.message.c_str());
                else if (tuningState.haveResult)
                    ImGui::TextDisabled("%s: %s", tuningState.lastSucceeded ? "Success" : "Failed", tuningState.message.c_str());
                else
                    ImGui::TextDisabled("Ready. Join GTA Online before reading or applying Vehicle Cargo globals.");
            }

            if (ImGui::CollapsingHeader("Enhanced Mission Runtime"))
            {
                ImGui::Text("gb_vehicle_export: %s",
                    monitorState.haveResult ? (monitorState.vehicleCargoRunning ? "RUNNING" : "IDLE") : "UNKNOWN");

                ImGui::BeginDisabled(monitorState.pending);
                if (ImGui::Button("Refresh Vehicle Cargo script state", ImVec2(-1.0f, 0.0f)))
                    static_cast<void>(monitor.QueueRefresh());
                ImGui::EndDisabled();
                DescribeLastV11Item("Resolve the Enhanced gb_vehicle_export thread through Tutones' shared script runtime.");

                if (monitorState.pending)
                    ImGui::TextDisabled("%s", monitorState.message.c_str());
                else if (monitorState.haveResult)
                    ImGui::TextDisabled("%s: %s", monitorState.lastSucceeded ? "Success" : "Failed", monitorState.message.c_str());

                ImGui::SeparatorText("Runtime Ownership");
                ImGui::TextWrapped(
                    "Auto Source owns activity 178 and warehouse acquisition. Instant Sell owns only guarded movement during activity 188. Rockstar's gb_vehicle_export script remains authoritative for sell-stage progression, commission payout and mission completion.");
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        SetV11Description(
            "Vehicle Cargo includes real source-to-garage delivery plus guarded activity-188 instant export delivery, with Enhanced tuning and mission diagnostics kept intact.");
    }
}
