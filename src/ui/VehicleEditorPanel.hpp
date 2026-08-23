#pragma once

#include "VehicleModificationPanel.hpp"
#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/vehicle/VehicleAppearanceRuntime.hpp"
#include "../game/GameState.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <string>

namespace Tutones::UI
{
    namespace VehicleEditorPanelDetail
    {
        inline bool g_ShowAppearance{};
        inline Game::Vehicle g_AppearanceEditVehicle{};
        inline char g_PlateText[9]{};
        inline int g_InteriorColor{};
        inline int g_DashboardColor{};

        inline constexpr std::array<const char*, 13> PlateStyles{{
            "Blue on White 1",
            "Yellow on Black",
            "Yellow on Blue",
            "Blue on White 2",
            "Blue on White 3",
            "Yankton",
            "Ecola",
            "Las Venturas",
            "Liberty City",
            "Los Santos Car Meet",
            "Los Santos Panic",
            "Los Santos Pounders",
            "Sprunk",
        }};

        inline constexpr std::array<const char*, 7> WindowTints{{
            "None",
            "Black",
            "Dark Smoke",
            "Light Smoke",
            "Stock",
            "Limo",
            "Green",
        }};

        inline void RenderReturnButton(const char* label, bool appearanceValue) noexcept
        {
            ImGui::SetCursorPos(ImVec2(226.0f + 354.0f, 20.0f));
            if (ImGui::Button(label, ImVec2(122.0f, 28.0f)))
                g_ShowAppearance = appearanceValue;
        }

        inline void SyncAppearanceInputs(
            Game::Vehicle vehicle,
            const Game::Mods::VehicleAppearanceSnapshot& snapshot) noexcept
        {
            if (vehicle == 0 || !snapshot.ready || snapshot.vehicle != vehicle)
                return;
            if (g_AppearanceEditVehicle == vehicle)
                return;

            g_AppearanceEditVehicle = vehicle;
            std::snprintf(g_PlateText, sizeof(g_PlateText), "%s", snapshot.plateText.c_str());
            g_InteriorColor = std::clamp(snapshot.interiorColor, 0, 160);
            g_DashboardColor = std::clamp(snapshot.dashboardColor, 0, 160);
        }

        inline void RenderAppearancePanel() noexcept
        {
            const auto gameState = Game::GameState::Get().Snapshot();
            const Game::Vehicle vehicle = gameState.nativeRuntimeReady && gameState.inVehicle
                ? gameState.vehicle
                : 0;

            auto& runtime = Game::Mods::VehicleAppearanceRuntime::Get();
            runtime.RequestRefresh(vehicle);
            const auto snapshot = runtime.Snapshot();
            SyncAppearanceInputs(vehicle, snapshot);

            if (vehicle == 0)
                g_AppearanceEditVehicle = 0;

            ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
            ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

            if (ImGui::BeginChild("##vehicle_appearance", ImVec2(490.0f, 430.0f), true))
            {
                ImGui::TextColored(V11Theme::Accent, "Vehicle Appearance");
                ImGui::SameLine();
                ImGui::TextDisabled("Enhanced native controls");
                ImGui::Separator();

                if (vehicle == 0)
                {
                    ImGui::TextDisabled("Enter a vehicle to edit its appearance.");
                }
                else if (!snapshot.ready || snapshot.vehicle != vehicle)
                {
                    ImGui::TextDisabled("Reading the current vehicle appearance from GTA...");
                }
                else
                {
                    int plateStyle = std::clamp(snapshot.plateStyle, 0, static_cast<int>(PlateStyles.size()) - 1);
                    int windowTint = std::clamp(snapshot.windowTint, 0, static_cast<int>(WindowTints.size()) - 1);

                    ImGui::BeginDisabled(snapshot.pending);

                    ImGui::SeparatorText("License Plate");
                    ImGui::SetNextItemWidth(190.0f);
                    ImGui::InputTextWithHint("##appearance_plate_text", "Plate Number", g_PlateText, sizeof(g_PlateText));
                    DescribeLastV11Item("Set up to eight license-plate characters through the Enhanced plate-text native path.");
                    ImGui::SameLine();
                    if (ImGui::Button("Apply Plate", ImVec2(-1.0f, 0.0f)))
                        static_cast<void>(runtime.QueuePlateText(vehicle, std::string(g_PlateText)));
                    DescribeLastV11Item("Queue the plate text on GTA's game thread and verify it with GET_VEHICLE_NUMBER_PLATE_TEXT.");
                    ImGui::TextDisabled("Current: %s", snapshot.plateText.empty() ? "(blank)" : snapshot.plateText.c_str());

                    ImGui::SetNextItemWidth(-1.0f);
                    if (ImGui::BeginCombo("Plate Style", PlateStyles[static_cast<std::size_t>(plateStyle)]))
                    {
                        for (std::size_t i = 0; i < PlateStyles.size(); ++i)
                        {
                            const bool selected = static_cast<int>(i) == plateStyle;
                            if (ImGui::Selectable(PlateStyles[i], selected))
                                static_cast<void>(runtime.QueuePlateStyle(vehicle, static_cast<int>(i)));
                            if (selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    DescribeLastV11Item("Change the vehicle plate background/style and verify the selected index by read-back.");
                    ImGui::TextDisabled("Current style index: %d", snapshot.plateStyle);

                    ImGui::SeparatorText("Cabin Colors");
                    ImGui::SetNextItemWidth(300.0f);
                    ImGui::SliderInt("Interior Color", &g_InteriorColor, 0, 160);
                    DescribeLastV11Item("Choose the indexed interior trim color used by Enhanced SET_VEHICLE_EXTRA_COLOUR_5.");
                    ImGui::SameLine();
                    if (ImGui::Button("Apply##interior"))
                        static_cast<void>(runtime.QueueInteriorColor(vehicle, g_InteriorColor));
                    ImGui::TextDisabled("Current interior index: %d", snapshot.interiorColor);

                    ImGui::SetNextItemWidth(300.0f);
                    ImGui::SliderInt("Dashboard Color", &g_DashboardColor, 0, 160);
                    DescribeLastV11Item("Choose the indexed dashboard color used by Enhanced SET_VEHICLE_EXTRA_COLOUR_6.");
                    ImGui::SameLine();
                    if (ImGui::Button("Apply##dashboard"))
                        static_cast<void>(runtime.QueueDashboardColor(vehicle, g_DashboardColor));
                    ImGui::TextDisabled("Current dashboard index: %d", snapshot.dashboardColor);

                    ImGui::SeparatorText("Window Tint");
                    ImGui::SetNextItemWidth(-1.0f);
                    if (ImGui::BeginCombo("##window_tint", WindowTints[static_cast<std::size_t>(windowTint)]))
                    {
                        for (std::size_t i = 0; i < WindowTints.size(); ++i)
                        {
                            const bool selected = static_cast<int>(i) == windowTint;
                            if (ImGui::Selectable(WindowTints[i], selected))
                                static_cast<void>(runtime.QueueWindowTint(vehicle, static_cast<int>(i)));
                            if (selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    DescribeLastV11Item("Change the current vehicle's window tint and verify the selected tint by read-back.");
                    ImGui::TextDisabled("Current tint index: %d", snapshot.windowTint);

                    ImGui::EndDisabled();

                    ImGui::SeparatorText("Status");
                    if (snapshot.pending)
                        ImGui::TextDisabled("%s", snapshot.message.c_str());
                    else if (snapshot.haveResult)
                        ImGui::TextWrapped("%s: %s", snapshot.lastSucceeded ? "Verified" : "Failed", snapshot.message.c_str());
                    else
                        ImGui::TextDisabled("%s", snapshot.message.c_str());

                    if (ImGui::Button("Reload values from vehicle", ImVec2(-1.0f, 0.0f)))
                    {
                        g_AppearanceEditVehicle = 0;
                        runtime.RequestRefresh(vehicle);
                    }
                    DescribeLastV11Item("Discard local appearance edits and reload the current plate and cabin color values from GTA.");

                    ImGui::Spacing();
                    ImGui::TextWrapped(
                        "Plate text/style, interior color, dashboard color and window tint use dedicated Enhanced native handlers. "
                        "Writes stay on the GTA game thread and every appearance change is checked with a read-back before it is reported as successful.");
                }
            }

            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(2);
            RenderReturnButton("Workshop", false);
        }
    }

    inline void RenderVehicleEditorPanel() noexcept
    {
        using namespace VehicleEditorPanelDetail;
        if (g_ShowAppearance)
        {
            RenderAppearancePanel();
            return;
        }

        RenderVehicleModificationPanel();
        RenderReturnButton("Appearance", true);
    }
}
