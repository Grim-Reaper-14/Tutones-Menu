#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/business/BusinessScriptMonitorRuntime.hpp"
#include "../features/business/VehicleCargoAutoSourceRuntime.hpp"
#include "../features/business/VehicleCargoInstantGarageRuntime.hpp"
#include "../features/business/VehicleCargoTuningRuntime.hpp"

#include <imgui.h>

#include <algorithm>

namespace Tutones::UI
{
    inline void RenderVehicleCargoBusinessPanel() noexcept
    {
        using Game::Business::BusinessScriptMonitorRuntime;
        using Game::Business::VehicleCargoAutoSourceRuntime;
        using Game::Business::VehicleCargoInstantGarageRuntime;
        using Game::Business::VehicleCargoTuningProfile;
        using Game::Business::VehicleCargoTuningRuntime;

        auto& monitor = BusinessScriptMonitorRuntime::Get();
        const auto monitorState = monitor.Snapshot();
        auto& autoSource = VehicleCargoAutoSourceRuntime::Get();
        const auto autoSourceState = autoSource.Snapshot();
        auto& instantGarage = VehicleCargoInstantGarageRuntime::Get();
        const auto instantGarageState = instantGarage.Snapshot();
        auto& tuning = VehicleCargoTuningRuntime::Get();
        const auto tuningState = tuning.Snapshot();

        static VehicleCargoTuningProfile profile{};
        static bool instantGarageMode = true;

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

            ImGui::TextWrapped("Vehicle Cargo uses the genuine Enhanced freemode source route. Instant Source + Delivery automatically finds the exact requested Import/Export vehicle, acquires it, and sends it through Rockstar's real warehouse-delivery path without fabricating warehouse slots.");
            ImGui::Spacing();

            if (ImGui::CollapsingHeader("Auto Source Vehicle Cargo", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGui::Checkbox("Instant Source + Delivery mode", &instantGarageMode))
                {
                    if (instantGarageMode)
                        autoSource.SetEnabled(false);
                    else
                        instantGarage.SetEnabled(false);
                }
                DescribeLastV11Item("Starts a genuine Vehicle Cargo source mission, scans for the exact requested model and unique Import/Export plate, takes network control, warps you into that mission car, then sends it to your owned Vehicle Warehouse entrance for Rockstar to store normally.");

                bool enabled = instantGarageMode ? instantGarageState.enabled : autoSourceState.enabled;
                if (ImGui::Checkbox("Enable Auto Source", &enabled))
                {
                    if (instantGarageMode)
                    {
                        autoSource.SetEnabled(false);
                        instantGarage.SetEnabled(enabled);
                    }
                    else
                    {
                        instantGarage.SetEnabled(false);
                        autoSource.SetEnabled(enabled);
                    }
                }
                DescribeLastV11Item(instantGarageMode
                    ? "Repeats fully automatic source cycles one at a time: launch mission, locate exact source car, acquire it, instant-deliver it, then wait for Rockstar's save before starting the next one."
                    : "Automatically requests the next Enhanced Vehicle Cargo source mission whenever gb_vehicle_export is idle.");

                ImGui::SameLine();
                const bool sourcePending = instantGarageMode ? instantGarageState.pending : autoSourceState.pending;
                ImGui::BeginDisabled(sourcePending);
                if (ImGui::Button(instantGarageMode ? "Instant source + store" : "Source next vehicle now"))
                {
                    if (instantGarageMode)
                        static_cast<void>(instantGarage.QueueStoreNow());
                    else
                        static_cast<void>(autoSource.QueueSourceNow());
                }
                ImGui::EndDisabled();
                DescribeLastV11Item(instantGarageMode
                    ? "Run one fully automatic source cycle now. You do not need to travel to or manually enter the sourced vehicle."
                    : "Send one Enhanced Vehicle Cargo source request immediately, even when Auto Source is disabled.");

                ImGui::SeparatorText("Auto Source Status");

                if (instantGarageMode)
                {
                    ImGui::Text("Mode: INSTANT SOURCE + ROCKSTAR SAVE");
                    ImGui::Text("Auto Source: %s", instantGarageState.enabled ? "ON" : "OFF");
                    ImGui::Text("GTA Online session: %s", instantGarageState.sessionReady ? "READY" : "WAITING");
                    ImGui::Text("Vehicle Warehouse: %s", instantGarageState.warehouseReady ? "READY" : "WAITING");
                    ImGui::Text("Source mission: %s", instantGarageState.missionRunning ? "RUNNING" : "IDLE");
                    ImGui::Text("Instant source vehicle: %s", instantGarageState.sourceVehicleReady ? "ACQUIRED" : "SEARCHING");
                    ImGui::Text("Instant delivery: %s", instantGarageState.deliveryIssued ? "SENT" : "WAITING");

                    if (instantGarageState.warehouseProperty != 0)
                    {
                        ImGui::Text("Warehouse property: %d | stock: %d / 40",
                            instantGarageState.warehouseProperty,
                            instantGarageState.warehouseStock);
                    }
                    if (instantGarageState.sourceVariation > 0)
                        ImGui::Text("Source variation: %d / 96", instantGarageState.sourceVariation);

                    if (instantGarageState.pending)
                        ImGui::TextDisabled("Checking Vehicle Cargo instant-source state...");
                    else
                        ImGui::TextWrapped("%s", instantGarageState.message.c_str());

                    ImGui::Spacing();
                    ImGui::TextDisabled("Automatic route: source 178 -> GB_VEHICLE_EXPORT -> exact model + plate scan -> network control -> driver-seat acquire -> owned warehouse entrance -> Rockstar delivery/save.");
                    ImGui::TextWrapped("Instant Source does not write Vehicle Warehouse inventory globals or persistent vehicle slots. Sell/export activity 188 is ignored completely.");
                }
                else
                {
                    ImGui::Text("Mode: ROCKSTAR SOURCE MISSION");
                    ImGui::Text("Auto Source: %s", autoSourceState.enabled ? "ON" : "OFF");
                    ImGui::Text("GTA Online session: %s", autoSourceState.sessionReady ? "READY" : "WAITING");
                    ImGui::Text("Freemode launch route: %s", autoSourceState.launcherReady ? "READY" : "WAITING");
                    ImGui::Text("gb_vehicle_export: %s", autoSourceState.vehicleCargoRunning ? "RUNNING" : "IDLE");

                    if (autoSourceState.launcherState >= 0)
                    {
                        ImGui::Text("Freemode host: %d | source launcher: %d",
                            autoSourceState.launcherState,
                            autoSourceState.launcherIndex);
                    }

                    if (autoSourceState.pending)
                        ImGui::TextDisabled("Checking Enhanced Vehicle Cargo source route...");
                    else
                        ImGui::TextWrapped("%s", autoSourceState.message.c_str());

                    ImGui::Spacing();
                    ImGui::TextDisabled("Enhanced source route: mission 178 -> TU event 1613825825 -> freemode host -> launcher 73 -> GB_VEHICLE_EXPORT.");
                    ImGui::TextWrapped("For back-to-back sourcing without Rockstar's normal steal cooldown, set Steal cooldown to 0 below and apply the Vehicle Cargo globals.");
                }
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

                ImGui::SeparatorText("Mission Launch Safety");
                ImGui::TextWrapped("Normal Source mode mirrors Rockstar's Enhanced freemode launch path. Instant Source + Delivery still uses a real activity-178 source mission, verifies the exact Import/Export model and unique plate, acquires only that mission vehicle, and lets Rockstar own the final warehouse save. Sell/export activity 188 is never touched.");
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        SetV11Description("Vehicle Cargo / Import Export includes Enhanced source missions, fully automatic instant sourcing and Rockstar-backed warehouse delivery, script-state monitoring and verified 1.73 tuning globals.");
    }
}
