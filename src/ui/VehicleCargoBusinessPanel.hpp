#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/business/BusinessScriptMonitorRuntime.hpp"
#include "../features/business/VehicleCargoAutoSourceRuntime.hpp"
#include "../features/business/VehicleCargoTuningRuntime.hpp"

#include <imgui.h>

#include <algorithm>

namespace Tutones::UI
{
    inline void RenderVehicleCargoBusinessPanel() noexcept
    {
        using Game::Business::BusinessScriptMonitorRuntime;
        using Game::Business::VehicleCargoAutoSourceRuntime;
        using Game::Business::VehicleCargoTuningProfile;
        using Game::Business::VehicleCargoTuningRuntime;

        auto& monitor = BusinessScriptMonitorRuntime::Get();
        const auto monitorState = monitor.Snapshot();
        auto& autoSource = VehicleCargoAutoSourceRuntime::Get();
        const auto autoSourceState = autoSource.Snapshot();
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
                "Instant Auto Source now uses the same direct-storage idea as Special Cargo: Tutones writes Rockstar's persistent Vehicle Warehouse slot, verifies it, and mirrors the live warehouse cache. It does not start activity 178, spawn a source mission car, or touch the garage entrance.");
            ImGui::Spacing();

            if (ImGui::CollapsingHeader("Instant Warehouse Source", ImGuiTreeNodeFlags_DefaultOpen))
            {
                bool enabled = autoSourceState.enabled;
                if (ImGui::Checkbox("Auto Source straight to warehouse", &enabled))
                    autoSource.SetEnabled(enabled);
                DescribeLastV11Item(
                    "Adds one valid non-duplicate Import/Export vehicle to the next free Vehicle Warehouse slot every cycle. No steal mission is launched, so the warehouse entrance remains usable.");

                ImGui::SameLine();
                ImGui::BeginDisabled(autoSourceState.pending);
                if (ImGui::Button("Source one now", ImVec2(-1.0f, 0.0f)))
                    static_cast<void>(autoSource.QueueSourceNow());
                ImGui::EndDisabled();
                DescribeLastV11Item(
                    "Adds exactly one Vehicle Cargo ID directly to the next free persistent warehouse slot and updates the live warehouse cache.");

                ImGui::SeparatorText("Direct Storage Status");
                ImGui::Text("Runtime: %s", autoSourceState.pending ? "WRITING" : (autoSourceState.enabled ? "AUTO" : "IDLE"));
                ImGui::Text("Session: %s", autoSourceState.sessionReady ? "READY" : "WAITING");
                ImGui::Text("Vehicle Warehouse: %s", autoSourceState.warehouseReady ? "READY" : "WAITING");

                if (autoSourceState.warehouseProperty != 0)
                    ImGui::Text("Warehouse property/index: %d", autoSourceState.warehouseProperty);
                ImGui::Text("Warehouse stock: %d / 40", autoSourceState.warehouseStock);

                if (autoSourceState.lastVehicleId > 0)
                {
                    ImGui::Text("Last sourced Vehicle ID: %d", autoSourceState.lastVehicleId);
                    ImGui::Text("Saved warehouse slot: %d", autoSourceState.lastWarehouseSlot);
                }

                ImGui::TextWrapped("%s", autoSourceState.message.c_str());

                ImGui::Spacing();
                ImGui::TextDisabled(
                    "Direct-source contract: no activity 178, no gb_vehicle_export launch, no source blip scanning, no network-control loop, and no warehouse teleport. If a normal Vehicle Cargo mission is already active, Tutones refuses the direct write until that mission ends.");
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
                    "Instant Auto Source owns only persistent Vehicle Warehouse inventory and its local live cache. It never starts or manipulates a Vehicle Cargo steal mission. Normal Rockstar missions remain independent diagnostics only.");
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        SetV11Description(
            "Vehicle Cargo / Import Export uses direct warehouse sourcing like Special Cargo: persistent slot write, read-back verification, and live warehouse cache refresh without activity 178.");
    }
}
