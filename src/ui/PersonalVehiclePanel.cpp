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
        int g_SelectedVehicleId{-1};
        std::string g_GarageFilter;
        std::uint64_t g_LastRevision{};

        [[nodiscard]] bool VehicleVisible(
            const Game::PersonalVehicles::PersonalVehicleEntry& vehicle) noexcept
        {
            return g_GarageFilter.empty() || vehicle.garage == g_GarageFilter;
        }

        void ValidateSelection(const Game::PersonalVehicles::PersonalVehicleSnapshot& snapshot) noexcept
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
        const auto snapshot = Game::PersonalVehicles::PersonalVehicleRuntime::Get().Snapshot();
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
            ImGui::TextDisabled("Enhanced MPSV reader");
            ImGui::Separator();

            if (!snapshot.running)
            {
                ImGui::TextDisabled("Personal vehicle reader is offline.");
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

                if (ImGui::BeginListBox("##personal_vehicles", ImVec2(-1.0f, 235.0f)))
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
                }
                else
                {
                    ImGui::TextDisabled("Select a personal vehicle to inspect its MPSV identity and garage slot.");
                }
            }

            ImGui::Separator();
            ImGui::TextDisabled("Read-only snapshot; refreshes every 10 seconds on the GTA script thread.");
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
