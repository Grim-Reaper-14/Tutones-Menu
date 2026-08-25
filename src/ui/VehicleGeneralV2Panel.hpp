#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/vehicle/HornBoostRuntime.hpp"
#include "../features/vehicle/VehicleLoopFeatures.hpp"
#include "../features/vehicle/VehicleModificationRuntime.hpp"
#include "../game/GameState.hpp"

#include <imgui.h>

namespace Tutones::UI
{
    inline void RenderVehicleGeneralV2Panel() noexcept
    {
        auto& loop = Game::Mods::VehicleLoopFeatures::Get();
        auto& hornBoost = Game::Mods::HornBoostRuntime::Get();
        auto& runtime = Game::Mods::VehicleModificationRuntime::Get();
        const auto vehicleState = runtime.Snapshot();
        const auto gameState = Game::GameState::Get().Snapshot();

        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##vehicle_general_v2", ImVec2(490.0f, 430.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Vehicle General");
            ImGui::SameLine();
            ImGui::TextDisabled(gameState.inVehicle && gameState.vehicle != 0 ? "Current vehicle active" : "Enter a vehicle");
            ImGui::Separator();

            ImGui::SeparatorText("Persistent Vehicle Features");

            bool godMode = loop.VehicleGodMode();
            if (ImGui::Checkbox("Vehicle God Mode", &godMode))
                loop.SetVehicleGodMode(godMode);
            DescribeLastV11Item("Keep the vehicle you are currently driving invincible. Protection follows you when you switch vehicles and is restored when disabled.");

            bool keepClean = loop.KeepVehicleClean();
            if (ImGui::Checkbox("Keep Vehicle Clean", &keepClean))
                loop.SetKeepVehicleClean(keepClean);
            DescribeLastV11Item("Continuously remove dirt and visible vehicle decals from the vehicle you are currently driving.");

            bool hornBoostEnabled = hornBoost.Enabled();
            if (ImGui::Checkbox("Horn Boost", &hornBoostEnabled))
                hornBoost.SetEnabled(hornBoostEnabled);
            DescribeLastV11Item("Hold the normal vehicle horn while driving to progressively accelerate the vehicle forward. The boost resets when the horn is released.");

            bool loweredStance = loop.LoweredStance();
            if (ImGui::Checkbox("Lower Vehicle Stance", &loweredStance))
                loop.SetLoweredStance(loweredStance);
            DescribeLastV11Item("Continuously apply GTA's reduced-suspension state to supported vehicles and restore the previous vehicle when disabled.");

            ImGui::SeparatorText("Quick Vehicle Actions");
            ImGui::BeginDisabled(!runtime.IsRunning() || !gameState.inVehicle || gameState.vehicle == 0);
            if (ImGui::BeginTable("##vehicle_general_actions", 3, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (ImGui::Button("Repair", ImVec2(-1.0f, 30.0f)))
                    static_cast<void>(runtime.QueueRepair());
                DescribeLastV11Item("Repair the current vehicle through the verified vehicle runtime.");

                ImGui::TableSetColumnIndex(1);
                if (ImGui::Button("Clean Now", ImVec2(-1.0f, 30.0f)))
                    static_cast<void>(runtime.QueueClean());
                DescribeLastV11Item("Immediately clear the current vehicle's dirt level.");

                ImGui::TableSetColumnIndex(2);
                if (ImGui::Button("Set Upright", ImVec2(-1.0f, 30.0f)))
                    static_cast<void>(runtime.QueueFlipUpright());
                DescribeLastV11Item("Place a rolled or awkwardly landed vehicle upright on the ground.");
                ImGui::EndTable();
            }

            if (ImGui::BeginTable("##vehicle_general_stealth", 2, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (ImGui::Button("Enable Vehicle Stealth", ImVec2(-1.0f, 30.0f)))
                    static_cast<void>(runtime.QueueStealthMode(true));
                DescribeLastV11Item("Enable the supported stealth state on vehicles such as the Akula, Annihilator2 and Raiju.");

                ImGui::TableSetColumnIndex(1);
                if (ImGui::Button("Disable Vehicle Stealth", ImVec2(-1.0f, 30.0f)))
                    static_cast<void>(runtime.QueueStealthMode(false));
                DescribeLastV11Item("Restore the supported stealth vehicle's normal wing or missile-bay state.");
                ImGui::EndTable();
            }
            ImGui::EndDisabled();

            ImGui::SeparatorText("Current Vehicle Status");
            ImGui::Text("Vehicle: %d", gameState.vehicle);
            ImGui::Text("In vehicle: %s", gameState.inVehicle ? "YES" : "NO");
            ImGui::Text("God Mode: %s", godMode ? "ON" : "OFF");
            ImGui::SameLine();
            ImGui::Text("  Clean: %s", keepClean ? "ON" : "OFF");
            ImGui::Text("Horn Boost: %s", hornBoostEnabled ? "ON" : "OFF");
            ImGui::SameLine();
            ImGui::Text("  Stance: %s", loweredStance ? "LOW" : "NORMAL");
            if (vehicleState.lastAction != Game::Mods::VehicleModAction::None)
                ImGui::TextDisabled("Last vehicle action: %s", vehicleState.lastActionSucceeded ? "SUCCESS" : "FAILED");
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);

        SetV11Description("Vehicle General - active-vehicle behavior only: God Mode, Keep Vehicle Clean, Horn Boost, lowered stance, repair, clean, upright and stealth controls.");
    }
}
