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

namespace Tutones::UI
{
    namespace VehicleEditorPanelDetail
    {
        inline bool g_ShowAppearance{};

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

        inline void RenderAppearancePanel() noexcept
        {
            const auto gameState = Game::GameState::Get().Snapshot();
            const Game::Vehicle vehicle = gameState.nativeRuntimeReady && gameState.inVehicle
                ? gameState.vehicle
                : 0;

            auto& runtime = Game::Mods::VehicleAppearanceRuntime::Get();
            runtime.RequestRefresh(vehicle);
            const auto snapshot = runtime.Snapshot();

            ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
            ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

            if (ImGui::BeginChild("##vehicle_appearance", ImVec2(490.0f, 430.0f), true))
            {
                ImGui::TextColored(V11Theme::Accent, "Vehicle Appearance");
                ImGui::SameLine();
                ImGui::TextDisabled("YimMenuV2-style custom slots");
                ImGui::Separator();

                if (vehicle == 0)
                {
                    ImGui::TextDisabled("Enter a vehicle to edit plate style and window tint.");
                }
                else if (!snapshot.ready || snapshot.vehicle != vehicle)
                {
                    ImGui::TextDisabled("Reading the current vehicle appearance from GTA...");
                }
                else
                {
                    int plateStyle = std::clamp(snapshot.plateStyle, 0, static_cast<int>(PlateStyles.size()) - 1);
                    int windowTint = std::clamp(snapshot.windowTint, 0, static_cast<int>(WindowTints.size()) - 1);

                    ImGui::SeparatorText("Plate Style");
                    ImGui::SetNextItemWidth(-1.0f);
                    if (ImGui::BeginCombo("##plate_style", PlateStyles[static_cast<std::size_t>(plateStyle)]))
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
                    DescribeLastV11Item("Change the vehicle plate background/style using GTA's dedicated plate-style native and verify the value by reading it back.");
                    ImGui::TextDisabled("Current index: %d", snapshot.plateStyle);

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
                    DescribeLastV11Item("Change the current vehicle's window tint using GTA's dedicated tint native and verify the selected tint by read-back.");
                    ImGui::TextDisabled("Current index: %d", snapshot.windowTint);

                    ImGui::SeparatorText("Status");
                    if (snapshot.pending)
                        ImGui::TextDisabled("%s", snapshot.message.c_str());
                    else if (snapshot.haveResult)
                        ImGui::TextWrapped("%s: %s", snapshot.lastSucceeded ? "Verified" : "Failed", snapshot.message.c_str());
                    else
                        ImGui::TextDisabled("%s", snapshot.message.c_str());

                    ImGui::Spacing();
                    ImGui::TextWrapped(
                        "Plate Style and Window Tint are dedicated GTA customization values, not normal mod slots 0-49. "
                        "They are exposed here separately just like YimMenuV2 does in its Vehicle Editor.");
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
