#include "PersonalVehiclePanel.hpp"

#include "Input.hpp"
#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/vehicle/PersonalVehicleRuntime.hpp"
#include "../features/vehicle/SavePersonalVehicleRuntime.hpp"
#include "../features/vehicle/TeleportPersonalVehicleRuntime.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <string>

namespace Tutones::UI
{
    namespace
    {
        using Game::PersonalVehicles::PersonalVehicleAction;
        using Game::PersonalVehicles::PersonalVehicleEntry;
        using Game::PersonalVehicles::PersonalVehicleRuntime;
        using Game::PersonalVehicles::PersonalVehicleSnapshot;
        using Game::PersonalVehicles::SavePersonalVehicleRuntime;
        using Game::PersonalVehicles::TeleportPersonalVehicleRuntime;

        int g_SelectedVehicleId{-1};
        std::string g_GarageFilter;
        std::uint64_t g_LastRevision{};
        const char* g_Message{"Ready"};

        [[nodiscard]] bool VehicleVisible(const PersonalVehicleEntry& vehicle) noexcept
        {
            return g_GarageFilter.empty() || vehicle.garage == g_GarageFilter;
        }

        [[nodiscard]] const char* ActionName(PersonalVehicleAction action) noexcept
        {
            switch (action)
            {
            case PersonalVehicleAction::Repair: return "Repair";
            case PersonalVehicleAction::Request: return "Request";
            case PersonalVehicleAction::None: break;
            }
            return "None";
        }

        void ValidateSelection(const PersonalVehicleSnapshot& snapshot) noexcept
        {
            if (g_LastRevision == snapshot.revision)
                return;

            g_LastRevision = snapshot.revision;
            if (!g_GarageFilter.empty()
                && std::find(snapshot.garages.begin(), snapshot.garages.end(), g_GarageFilter) == snapshot.garages.end())
            {
                g_GarageFilter.clear();
            }

            const auto selected = std::find_if(snapshot.vehicles.begin(), snapshot.vehicles.end(), [](const auto& vehicle) {
                return vehicle.id == g_SelectedVehicleId;
            });
            if (selected == snapshot.vehicles.end() || !VehicleVisible(*selected))
                g_SelectedVehicleId = -1;
        }
    }

    void RenderPersonalVehiclePanel() noexcept
    {
        auto& runtime = PersonalVehicleRuntime::Get();
        auto& saveRuntime = SavePersonalVehicleRuntime::Get();
        auto& teleportRuntime = TeleportPersonalVehicleRuntime::Get();
        const PersonalVehicleSnapshot snapshot = runtime.Snapshot();
        const auto saveSnapshot = saveRuntime.Snapshot();
        const auto teleportSnapshot = teleportRuntime.Snapshot();
        ValidateSelection(snapshot);

        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##personal_vehicle_panel", ImVec2(490.0f, 430.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Personal Vehicles");
            ImGui::SameLine();
            ImGui::TextDisabled("Enhanced MPSV runtime");
            ImGui::Separator();

            ImGui::TextColored(V11Theme::Accent, "Save Current Vehicle");
            const bool saveEnabled = snapshot.sessionStarted && snapshot.nativeReady && !saveSnapshot.pending;
            ImGui::BeginDisabled(!saveEnabled);
            if (ImGui::Button("Save Current to Personal Garage", ImVec2(-1.0f, 0.0f)))
            {
                if (saveRuntime.QueueSaveCurrent())
                {
                    // Tutones normally consumes keyboard, mouse and raw input while open.
                    // GTA's AM_MP_VEHICLE_REWARD selector needs those controls, so hand
                    // input back to the game immediately after the save flow is accepted.
                    g_Message = "GTA garage selector owns input - press F4 after it closes";
                    Input::Get().SetMenuOpen(false);
                }
                else
                {
                    g_Message = "Personal-garage save request was rejected";
                }
            }
            ImGui::EndDisabled();
            DescribeLastV11Item("Open GTA Online's AM_MP_VEHICLE_REWARD garage selector for the current vehicle. Tutones closes automatically after the request is queued so GTA receives the selector's navigation and confirm controls.");

            if (saveSnapshot.pending)
                ImGui::TextDisabled("%s", saveSnapshot.message.c_str());
            else if (saveSnapshot.haveResult)
                ImGui::TextDisabled("Save result: %s - %s", saveSnapshot.lastSucceeded ? "success" : "failed", saveSnapshot.message.c_str());
            else if (!snapshot.sessionStarted)
                ImGui::TextDisabled("Save Personal Vehicle requires an active GTA Online session.");
            else
                ImGui::TextDisabled("Tutones releases input while GTA's garage selector is active; reopen with F4 after choosing or backing out.");

            ImGui::Spacing();
            ImGui::TextColored(V11Theme::Accent, "Quick Personal Vehicle Action");
            ImGui::BeginDisabled(teleportSnapshot.pending || !snapshot.sessionStarted || !snapshot.scriptGlobalsReady);
            if (ImGui::Button(teleportSnapshot.pending ? "Teleporting..." : "Teleport Into Personal Vehicle", ImVec2(-1.0f, 0.0f)))
                g_Message = teleportRuntime.QueueTeleport() ? "Personal vehicle teleport queued" : "Personal vehicle teleport rejected";
            ImGui::EndDisabled();
            DescribeLastV11Item("Enhanced 1.73 b1158.13: set Global_2640101.f_8 = 1 on the GTA script thread to teleport into the currently active personal vehicle.");

            if (teleportSnapshot.pending)
                ImGui::TextDisabled("Teleport request is running on the GTA script thread...");
            else if (teleportSnapshot.haveResult)
                ImGui::TextDisabled("Teleport result: %s - %s", teleportSnapshot.lastSucceeded ? "success" : "failed", teleportSnapshot.message.c_str());
            else
                ImGui::TextDisabled("Targets GTA Online's currently active personal vehicle.");

            ImGui::Separator();

            if (!snapshot.running)
            {
                ImGui::TextDisabled("Personal vehicle runtime is offline.");
            }
            else if (!snapshot.scriptGlobalsReady)
            {
                ImGui::TextDisabled("Enhanced script globals are unavailable; no personal vehicle data is being shown.");
            }
            else if (!snapshot.nativeReady)
            {
                ImGui::TextDisabled("Native table is not ready; model validation is unavailable.");
            }
            else
            {
                ImGui::TextColored(V11Theme::Accent, "Garage Browser");
                ImGui::Text("MPSV source slots: %zu", snapshot.sourceArraySize);
                ImGui::SameLine();
                ImGui::TextDisabled("Vehicles: %zu   Garages: %zu", snapshot.vehicles.size(), snapshot.garages.size());

                if (snapshot.garageOwnershipStatsReady)
                {
                    ImGui::TextDisabled(
                        "Ownership gate: active for MP%d   Owned garage sources: %zu",
                        snapshot.garageCharacterIndex,
                        snapshot.ownedGarageSources);
                }
                else
                {
                    ImGui::TextDisabled(
                        "Ownership gate: partial - unreadable property stats are excluded; verified special service garages remain eligible.");
                }

                const char* preview = g_GarageFilter.empty() ? "All garages" : g_GarageFilter.c_str();
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::BeginCombo("##personal_vehicle_garage", preview))
                {
                    if (ImGui::Selectable("All garages", g_GarageFilter.empty()))
                    {
                        g_GarageFilter.clear();
                        g_SelectedVehicleId = -1;
                    }
                    for (const auto& garage : snapshot.garages)
                    {
                        const bool selected = garage == g_GarageFilter;
                        if (ImGui::Selectable(garage.c_str(), selected))
                        {
                            g_GarageFilter = garage;
                            g_SelectedVehicleId = -1;
                        }
                    }
                    ImGui::EndCombo();
                }
                DescribeLastV11Item("Filter the Enhanced personal-vehicle snapshot to one garage that passed the current ownership gate, or show vehicles from every resolved owned garage.");

                if (ImGui::BeginListBox("##personal_vehicles", ImVec2(-1.0f, 118.0f)))
                {
                    for (const auto& vehicle : snapshot.vehicles)
                    {
                        if (!VehicleVisible(vehicle))
                            continue;

                        char label[256]{};
                        if (vehicle.plate.empty())
                            std::snprintf(label, sizeof(label), "%s##pv_%d", vehicle.displayName.c_str(), vehicle.id);
                        else
                            std::snprintf(label, sizeof(label), "%s  [%s]##pv_%d", vehicle.displayName.c_str(), vehicle.plate.c_str(), vehicle.id);

                        const bool selected = g_SelectedVehicleId == vehicle.id;
                        if (ImGui::Selectable(label, selected))
                            g_SelectedVehicleId = vehicle.id;
                        DescribeLastV11Item("Select this Rockstar personal-vehicle entry to inspect its MPSV identity, plate, ownership-gated garage slot, and available actions.");
                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndListBox();
                }

                const auto selected = std::find_if(snapshot.vehicles.begin(), snapshot.vehicles.end(), [](const auto& vehicle) {
                    return vehicle.id == g_SelectedVehicleId;
                });
                if (selected != snapshot.vehicles.end())
                {
                    ImGui::TextColored(V11Theme::Accent, "Selected Vehicle");
                    ImGui::Text("ID %d   Model 0x%08X", selected->id, static_cast<unsigned int>(selected->model));
                    ImGui::Text("Plate: %s", selected->plate.empty() ? "(none)" : selected->plate.c_str());
                    ImGui::Text("Garage: %s", selected->garage.empty() ? "Unresolved" : selected->garage.c_str());
                    ImGui::TextDisabled("State: %s%s%s",
                        selected->destroyed ? "Destroyed " : "",
                        selected->insured ? "Insured " : "",
                        selected->impounded ? "Impounded" : "");

                    const bool repairEnabled = !snapshot.actionPending && selected->destroyed && selected->insured;
                    ImGui::BeginDisabled(!repairEnabled);
                    if (ImGui::Button("Repair", ImVec2(150.0f, 0.0f)))
                        g_Message = runtime.QueueRepair(selected->id) ? "Repair queued" : "Repair rejected";
                    ImGui::EndDisabled();
                    DescribeLastV11Item("Repair an insured destroyed personal vehicle by clearing the verified destroyed/impounded MPSV repair state.");

                    ImGui::SameLine();
                    const bool requestEnabled = !snapshot.actionPending
                        && snapshot.requestSupported
                        && snapshot.sessionStarted
                        && snapshot.requestedVehicleId == -1;
                    ImGui::BeginDisabled(!requestEnabled);
                    if (ImGui::Button("Request", ImVec2(-1.0f, 0.0f)))
                        g_Message = runtime.QueueRequest(selected->id) ? "Request queued" : "Request rejected";
                    ImGui::EndDisabled();
                    DescribeLastV11Item("Request this selected personal vehicle through the verified Enhanced Freemode personal-vehicle request state machine.");
                }
                else
                {
                    ImGui::TextDisabled("Select a personal vehicle to inspect its MPSV identity and garage slot.");
                }

                if (snapshot.actionPending)
                    ImGui::TextDisabled("Personal vehicle action is running on the GTA script thread...");
                else if (snapshot.lastAction != PersonalVehicleAction::None)
                    ImGui::TextDisabled("Last action: %s ID %d - %s",
                        ActionName(snapshot.lastAction),
                        snapshot.lastActionVehicleId,
                        snapshot.lastActionSucceeded ? "success" : "failed");
                else
                    ImGui::TextDisabled("%s", g_Message);

                if (!snapshot.requestSupported && snapshot.sessionStarted)
                    ImGui::TextDisabled("Request support is unavailable because the shared script runtime is not ready.");
                else if (snapshot.requestedVehicleId != -1)
                    ImGui::TextDisabled("GTA already has personal vehicle request ID %d in progress.", snapshot.requestedVehicleId);
            }

            ImGui::Separator();
            ImGui::TextDisabled("Rockstar Personal Garage uses GTA's vehicle-reward script; Tutones Saved Garage remains local-only preset storage.");
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
