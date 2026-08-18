#include "VehicleGeneralPanel.hpp"

#include "../features/vehicle/VehicleModificationRuntime.hpp"

#include <imgui.h>

namespace Tutones::UI
{
    void RenderVehicleGeneralPanel() noexcept
    {
        auto& runtime = Game::Mods::VehicleModificationRuntime::Get();
        const auto snapshot = runtime.Snapshot();

        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(24.0f / 255.0f, 24.0f / 255.0f, 26.0f / 255.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.04f));

        if (ImGui::BeginChild("##vehicle_general", ImVec2(490.0f, 430.0f), true))
        {
            ImGui::TextUnformatted("Vehicle General");
            ImGui::Separator();

            if (!runtime.IsRunning())
                ImGui::TextDisabled("Vehicle runtime is offline.");
            else if (!snapshot.valid)
                ImGui::TextDisabled("Enter a vehicle to enable vehicle actions.");
            else
            {
                ImGui::Text("Current vehicle handle: %d", snapshot.vehicle);
                ImGui::Spacing();

                if (ImGui::Button("Repair vehicle", ImVec2(-1.0f, 0.0f)))
                    static_cast<void>(runtime.QueueRepair());
                if (ImGui::Button("Clean vehicle", ImVec2(-1.0f, 0.0f)))
                    static_cast<void>(runtime.QueueClean());
                if (ImGui::Button("Flip / place upright", ImVec2(-1.0f, 0.0f)))
                    static_cast<void>(runtime.QueueFlipUpright());

                ImGui::Separator();
                if (snapshot.lastAction == Game::Mods::VehicleModAction::None)
                    ImGui::TextDisabled("No vehicle action has run yet.");
                else if (snapshot.lastActionRejectedAsStale)
                    ImGui::TextDisabled("Last action was dropped because you switched vehicles.");
                else
                    ImGui::Text("Last action: %s", snapshot.lastActionSucceeded ? "success" : "failed");
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
