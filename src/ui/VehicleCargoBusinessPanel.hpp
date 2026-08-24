#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/business/BusinessScriptMonitorRuntime.hpp"
#include "../features/business/VehicleCargoTuningRuntime.hpp"

#include <imgui.h>

#include <algorithm>

namespace Tutones::UI
{
    inline void RenderVehicleCargoBusinessPanel() noexcept
    {
        using Game::Business::BusinessScriptMonitorRuntime;
        using Game::Business::VehicleCargoTuningProfile;
        using Game::Business::VehicleCargoTuningRuntime;

        auto& monitor = BusinessScriptMonitorRuntime::Get();
        const auto monitorState = monitor.Snapshot();
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

            ImGui::TextWrapped("Vehicle Cargo tunables use the supplied Global_262145 offsets. Writes run on Tutones' GTA game-thread queue and are verified by immediate read-back.");
            ImGui::Spacing();

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
                DescribeLastV11Item("Resolve the Enhanced gb_vehicle_export thread through Tutones' shared script runtime without writing unverified mission locals.");

                if (monitorState.pending)
                    ImGui::TextDisabled("%s", monitorState.message.c_str());
                else if (monitorState.haveResult)
                    ImGui::TextDisabled("%s: %s", monitorState.lastSucceeded ? "Success" : "Failed", monitorState.message.c_str());

                ImGui::SeparatorText("Mission Local Guard");
                ImGui::TextWrapped("Source/sell mission locals remain protected until their current Enhanced 1.73 indices and expected values are independently verified. The tunables above are separate Global_262145 values.");
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        SetV11Description("Vehicle Cargo / Import Export now includes supplied Enhanced 1.73 steal cooldown, sell cooldown and vehicle-range sell-price tunables with live read-back verification.");
    }
}
