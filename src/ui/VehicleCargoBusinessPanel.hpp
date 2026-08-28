#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/business/BusinessScriptMonitorRuntime.hpp"
#include "../features/business/VehicleCargoAutoSourceRuntime.hpp"
#include "../features/business/VehicleCargoDeliveryRuntime.hpp"
#include "../features/business/VehicleCargoInstantGarageRuntime.hpp"
#include "../features/business/VehicleCargoInstantSourceRuntime.hpp"
#include "../features/business/VehicleCargoTuningRuntime.hpp"

#include <imgui.h>

#include <algorithm>

namespace Tutones::UI
{
    inline void RenderVehicleCargoBusinessPanel() noexcept
    {
        using Game::Business::BusinessScriptMonitorRuntime;
        using Game::Business::VehicleCargoAutoSourceRuntime;
        using Game::Business::VehicleCargoDeliveryRuntime;
        using Game::Business::VehicleCargoInstantGarageRuntime;
        using Game::Business::VehicleCargoInstantSourceRuntime;
        using Game::Business::VehicleCargoTuningProfile;
        using Game::Business::VehicleCargoTuningRuntime;

        auto& monitor = BusinessScriptMonitorRuntime::Get();
        const auto monitorState = monitor.Snapshot();
        auto& autoSource = VehicleCargoAutoSourceRuntime::Get();
        const auto autoSourceState = autoSource.Snapshot();
        auto& source = VehicleCargoInstantSourceRuntime::Get();
        const auto sourceState = source.Snapshot();
        auto& delivery = VehicleCargoDeliveryRuntime::Get();
        const auto deliveryState = delivery.Snapshot();
        auto& pipeline = VehicleCargoInstantGarageRuntime::Get();
        const auto pipelineState = pipeline.Snapshot();
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
                "Vehicle Cargo is split into independent runtimes. Source owns activity 178, target resolution and acquisition. Delivery only accepts an already-acquired Rockstar source car and owns warehouse validation/transition. The full pipeline only coordinates those two stages.");
            ImGui::Spacing();

            if (ImGui::CollapsingHeader("Vehicle Cargo Automation", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::SeparatorText("Full Pipeline Coordinator");
                bool fullAuto = pipelineState.enabled;
                if (ImGui::Checkbox("Full Auto Source + Delivery", &fullAuto))
                {
                    if (fullAuto)
                    {
                        autoSource.SetEnabled(false);
                        pipeline.SetEnabled(true);
                    }
                    else
                    {
                        pipeline.SetEnabled(false);
                    }
                }
                DescribeLastV11Item(
                    "Coordinates the dedicated Instant Source runtime and dedicated Instant Delivery runtime. The coordinator contains no source natives and no warehouse movement code.");

                ImGui::SameLine();
                ImGui::BeginDisabled(pipelineState.pending);
                if (ImGui::Button("Run one full cycle"))
                    static_cast<void>(pipeline.QueueStoreNow());
                ImGui::EndDisabled();
                DescribeLastV11Item("Run exactly one source -> acquire -> delivery cycle through the two independent runtimes.");

                ImGui::Text("Pipeline: %s", pipelineState.enabled ? "AUTO" : "MANUAL/OFF");
                ImGui::TextWrapped("%s", pipelineState.message.c_str());

                ImGui::SeparatorText("Independent Instant Source Runtime");
                ImGui::BeginDisabled(sourceState.active || sourceState.pending || pipelineState.enabled);
                if (ImGui::Button("Source + acquire vehicle", ImVec2(250.0f, 0.0f)))
                {
                    autoSource.SetEnabled(false);
                    static_cast<void>(source.QueueSourceNow());
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::BeginDisabled(!sourceState.active);
                if (ImGui::Button("Cancel source", ImVec2(-1.0f, 0.0f)))
                    source.Cancel();
                ImGui::EndDisabled();
                DescribeLastV11Item(
                    "Source-only action. Launches genuine activity 178, resolves Rockstar's exact source entity, obtains network control and acquires the driver seat. It never reads or touches a warehouse entrance.");

                ImGui::Text("Source runtime: %s", sourceState.active ? "ACTIVE" : "IDLE");
                ImGui::Text("Source mission: %s", sourceState.missionRunning ? "RUNNING" : "WAITING");
                ImGui::Text("Target entity: %s", sourceState.targetResolved ? "RESOLVED" : "WAITING");
                ImGui::Text("Source vehicle: %s", sourceState.vehicleReady ? "ACQUIRED" : "NOT READY");
                if (sourceState.variation > 0)
                    ImGui::Text("Variation: %d / 96 | entity: %d", sourceState.variation, sourceState.vehicle);
                ImGui::TextWrapped("%s", sourceState.message.c_str());

                ImGui::SeparatorText("Independent Instant Delivery Runtime");
                const bool haveSourceForDelivery = sourceState.vehicleReady
                    && sourceState.vehicle != 0
                    && sourceState.variation > 0;
                ImGui::BeginDisabled(!haveSourceForDelivery || deliveryState.active || pipelineState.enabled);
                if (ImGui::Button("Deliver acquired source vehicle", ImVec2(250.0f, 0.0f)))
                {
                    if (delivery.QueueDelivery(sourceState.vehicle, sourceState.variation))
                        source.ClearResult();
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::BeginDisabled(!deliveryState.active);
                if (ImGui::Button("Cancel delivery", ImVec2(-1.0f, 0.0f)))
                    delivery.Cancel();
                ImGui::EndDisabled();
                DescribeLastV11Item(
                    "Delivery-only action. Requires an already-acquired source car. It validates Rockstar's ContrabandDeliveryType and the exact model/plate before moving anything toward the real warehouse transition.");

                ImGui::Text("Delivery runtime: %s", deliveryState.active ? "ACTIVE" : "IDLE");
                ImGui::Text("Rockstar warehouse gate: %s", deliveryState.rockstarGateReady ? "READY" : "WAITING");
                ImGui::Text("Validated source vehicle: %s", deliveryState.sourceVehicleValid ? "YES" : "WAITING");
                ImGui::Text("Warehouse movement: %s", deliveryState.deliveryIssued ? "ISSUED" : "NOT ISSUED");
                if (deliveryState.warehouseProperty != 0)
                {
                    ImGui::Text("Warehouse property: %d | stock: %d / 40",
                        deliveryState.warehouseProperty,
                        deliveryState.warehouseStock);
                }
                if (deliveryState.attempts > 0)
                    ImGui::Text("Controlled approach attempts: %d / 2", deliveryState.attempts);
                ImGui::TextWrapped("%s", deliveryState.message.c_str());

                ImGui::SeparatorText("Rockstar Mission Launcher Only");
                bool normalAuto = autoSourceState.enabled;
                if (ImGui::Checkbox("Auto Source missions only", &normalAuto))
                {
                    if (normalAuto)
                    {
                        pipeline.SetEnabled(false);
                        source.Cancel();
                        delivery.Cancel();
                    }
                    autoSource.SetEnabled(normalAuto);
                }
                DescribeLastV11Item(
                    "Only launches Rockstar Vehicle Cargo source missions. It does not acquire the source vehicle and does not perform delivery.");

                ImGui::SameLine();
                ImGui::BeginDisabled(autoSourceState.pending || pipelineState.enabled);
                if (ImGui::Button("Launch source mission now"))
                    static_cast<void>(autoSource.QueueSourceNow());
                ImGui::EndDisabled();

                ImGui::Text("Launcher: %s | gb_vehicle_export: %s",
                    autoSourceState.launcherReady ? "READY" : "WAITING",
                    autoSourceState.vehicleCargoRunning ? "RUNNING" : "IDLE");
                ImGui::TextWrapped("%s", autoSourceState.message.c_str());

                ImGui::Spacing();
                ImGui::TextDisabled(
                    "Separation contract: Source never touches warehouse coordinates. Delivery never starts missions or searches source blips. Full Auto only hands the acquired vehicle from Source to Delivery.");
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
                    "Launcher runtime: activity 178/TU event only. Instant Source runtime: target entity/network control/acquisition only. Instant Delivery runtime: Rockstar cargo gate/warehouse transition/save observation only. Activity 188 is never modified by either instant runtime.");
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        SetV11Description(
            "Vehicle Cargo / Import Export now uses separate source and delivery runtimes with an optional full-auto coordinator, plus Enhanced tuning and mission diagnostics.");
    }
}
