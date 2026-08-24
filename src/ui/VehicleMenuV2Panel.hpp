#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/vehicle/VehicleModificationRuntime.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace Tutones::UI
{
    namespace VehicleMenuV2Detail
    {
        inline int g_CustomizeMode{}; // 0 = paint, 1 = workshop
        inline int g_GarageMode{};    // 0 = Rockstar, 1 = Tutones saved

        inline char g_PresetName[48] = "my_vehicle";
        inline bool g_LoadPresetInside{true};
        inline int g_SelectedPreset{-1};
        inline bool g_SavedGarageLoaded{};
        inline std::vector<std::string> g_SavedPresets{};
        inline std::string g_SavedGarageMessage{"Ready"};

        inline void RefreshSavedGarage(Game::Mods::VehicleModificationRuntime& runtime)
        {
            g_SavedPresets = runtime.SavedPresetNames();
            if (g_SavedPresets.empty())
                g_SelectedPreset = -1;
            else
                g_SelectedPreset = std::clamp(g_SelectedPreset, 0, static_cast<int>(g_SavedPresets.size()) - 1);
            g_SavedGarageLoaded = true;
        }

        inline void DrawModeSelector(const char* id, int& mode, const char* left, const char* right) noexcept
        {
            ImGui::SetCursorPos(ImVec2(226.0f, 454.0f));
            ImGui::PushID(id);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, V11Theme::PanelBg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, V11Theme::AccentDark);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, V11Theme::AccentDark);

            ImGui::TextDisabled("SECTION");
            ImGui::SameLine(78.0f);
            if (ImGui::Button(left, ImVec2(194.0f, 28.0f)))
                mode = 0;
            ImGui::SameLine();
            if (ImGui::Button(right, ImVec2(194.0f, 28.0f)))
                mode = 1;

            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();
            ImGui::PopID();
        }

        inline void RenderSavedGaragePanel() noexcept
        {
            auto& runtime = Game::Mods::VehicleModificationRuntime::Get();
            const auto snapshot = runtime.Snapshot();
            if (!g_SavedGarageLoaded)
                RefreshSavedGarage(runtime);

            ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
            ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

            if (ImGui::BeginChild("##vehicle_saved_garage_v2", ImVec2(490.0f, 430.0f), true))
            {
                ImGui::TextColored(V11Theme::Accent, "Tutones Saved Garage");
                ImGui::SameLine();
                ImGui::TextDisabled("Local vehicle presets");
                ImGui::Separator();

                if (!runtime.IsRunning())
                {
                    ImGui::TextDisabled("Vehicle runtime is offline.");
                }
                else
                {
                    ImGui::TextWrapped("Save and respawn complete local vehicle presets here. Rockstar Online personal vehicles stay in the Rockstar Garage section.");
                    ImGui::SeparatorText("Save Current Vehicle");
                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::InputTextWithHint("##saved_garage_name", "Preset name", g_PresetName, sizeof(g_PresetName));
                    DescribeLastV11Item("Choose the local Tutones preset name for the current vehicle and its supported customization state.");

                    if (ImGui::Button("Save Current Vehicle", ImVec2(-1.0f, 0.0f)))
                    {
                        const bool queued = runtime.QueueSaveCurrentPreset(g_PresetName);
                        g_SavedGarageMessage = queued
                            ? "Vehicle preset queued for save"
                            : "Save rejected - enter a vehicle first";
                    }
                    DescribeLastV11Item("Save the current vehicle as a local Tutones preset without changing Rockstar personal-garage data.");

                    ImGui::SeparatorText("Saved Vehicles");
                    if (ImGui::Button("Refresh Saved Vehicles", ImVec2(-1.0f, 0.0f)))
                        RefreshSavedGarage(runtime);
                    DescribeLastV11Item("Refresh the local Tutones saved-vehicle preset list.");

                    if (g_SavedPresets.empty())
                    {
                        ImGui::TextDisabled("No local saved vehicles yet.");
                    }
                    else if (ImGui::BeginListBox("##saved_garage_list", ImVec2(-1.0f, 130.0f)))
                    {
                        for (std::size_t i = 0; i < g_SavedPresets.size(); ++i)
                        {
                            const bool selected = g_SelectedPreset == static_cast<int>(i);
                            if (ImGui::Selectable(g_SavedPresets[i].c_str(), selected))
                                g_SelectedPreset = static_cast<int>(i);
                            if (selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndListBox();
                    }

                    ImGui::Checkbox("Spawn inside saved vehicle", &g_LoadPresetInside);
                    DescribeLastV11Item("Enter the spawned saved vehicle automatically after its preset is restored.");

                    ImGui::BeginDisabled(g_SelectedPreset < 0 || g_SelectedPreset >= static_cast<int>(g_SavedPresets.size()));
                    if (ImGui::Button("Spawn Selected Saved Vehicle", ImVec2(-1.0f, 0.0f)))
                    {
                        const bool queued = runtime.QueueLoadPreset(
                            g_SavedPresets[static_cast<std::size_t>(g_SelectedPreset)],
                            g_LoadPresetInside);
                        g_SavedGarageMessage = queued ? "Saved vehicle queued" : "Saved vehicle load rejected";
                    }
                    ImGui::EndDisabled();
                    DescribeLastV11Item("Spawn the selected Tutones preset and restore its supported paint, wheels, modifications, lighting and tire state.");

                    if (!snapshot.lastSavedPreset.empty())
                        ImGui::Text("Last preset: %s", snapshot.lastSavedPreset.c_str());
                    ImGui::TextDisabled("%s", g_SavedGarageMessage.c_str());
                }
            }

            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(2);
        }
    }

    inline void RenderVehicleCustomizeV2() noexcept
    {
        using namespace VehicleMenuV2Detail;
        if (g_CustomizeMode == 0)
            RenderV12VehiclePaintPanel();
        else
            RenderV12VehicleModificationPanel();

        DrawModeSelector(
            "vehicle_customize_mode",
            g_CustomizeMode,
            g_CustomizeMode == 0 ? "PAINT & COLORS" : "Paint & Colors",
            g_CustomizeMode == 1 ? "MODS / WHEELS / LIGHTS" : "Mods / Wheels / Lights");

        SetV11Description(g_CustomizeMode == 0
            ? "Vehicle Customize - primary/secondary paint, custom RGB, pearlescent and wheel colors in the same customization hub as the workshop."
            : "Vehicle Customize - LSC modifications, wheels, lighting, neon, tire smoke and tire behavior in one workshop.");
    }

    inline void RenderVehicleGarageV2() noexcept
    {
        using namespace VehicleMenuV2Detail;
        if (g_GarageMode == 0)
            RenderV12PersonalVehiclePanel();
        else
            RenderV12PageSurface([] { RenderSavedGaragePanel(); });

        DrawModeSelector(
            "vehicle_garage_mode",
            g_GarageMode,
            g_GarageMode == 0 ? "ROCKSTAR GARAGE" : "Rockstar Garage",
            g_GarageMode == 1 ? "TUTONES SAVED" : "Tutones Saved");

        SetV11Description(g_GarageMode == 0
            ? "Vehicle Garage - Rockstar personal vehicles, owned garage slots, request, repair, save-current and teleport-to-personal-vehicle actions."
            : "Vehicle Garage - local Tutones saved vehicle presets, kept together with the rest of the garage tools.");
    }
}
