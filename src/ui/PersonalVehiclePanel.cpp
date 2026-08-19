#include "PersonalVehiclePanel.hpp"

#include "V11Theme.hpp"
#include "../features/vehicle/PersonalVehicleRuntime.hpp"

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
        const PersonalVehicleSnapshot snapshot = runtime.Snapshot();
        ValidateSelection(snapshot);

        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##personal_vehicle_panel", ImVec2(490.0f, 430.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Personal Vehicles");
            ImGui::SameLine();
            ImGui::TextDisabled("Enhanced MPSV runtime");
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
                ImGui::Text("MPSV source slots: %zu", snapshot.sourceArraySize);
                ImGui::SameLine();
                ImGui::TextDisabled("Vehicles: %zu   Garages: %zu", snapshot.vehicles.size(), snapshot.garages.size());

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

                if (ImGui::BeginListBox("##personal_vehicles", ImVec2(-1.0f, 176.0f)))
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

                    ImGui::SameLine();
                    const bool requestEnabled = !snapshot.actionPending
                        && snapshot.requestSupported
                        && snapshot.sessionStarted
                        && snapshot.requestedVehicleId == -1;
                    ImGui::BeginDisabled(!requestEnabled);
                    if (ImGui::Button("Request", ImVec2(-1.0f, 0.0f)))
                        g_Message = runtime.QueueRequest(selected->id) ? "Request queued" : "Request rejected";
                    ImGui::EndDisabled();
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

                if (!snapshot.sessionStarted)
                    ImGui::TextDisabled("Request requires an active GTA Online session.");
                else if (!snapshot.requestSupported)
                    ImGui::TextDisabled("Request support is unavailable because the shared script runtime is not ready.");
                else if (snapshot.requestedVehicleId != -1)
                    ImGui::TextDisabled("GTA already has personal vehicle request ID %d in progress.", snapshot.requestedVehicleId);
            }

            ImGui::Separator();
            ImGui::TextDisabled("Repair/Request use verified Enhanced MPSV/Freemode state; Bring and Save are not enabled here.");
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
