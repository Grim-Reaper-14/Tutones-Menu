#include "VehicleGeneralPanel.hpp"

#include "../features/vehicle/VehicleModificationRuntime.hpp"
#include "../game/GameState.hpp"

#include <imgui.h>

namespace Tutones::UI
{
    namespace
    {
        char g_SpawnModel[32] = "adder";
        bool g_SpawnInside = true;
        bool g_SpawnMaxed = false;
        const char* g_SpawnMessage = "Ready";

        const char* ActionName(Game::Mods::VehicleModAction action) noexcept
        {
            using Game::Mods::VehicleModAction;
            switch (action)
            {
            case VehicleModAction::None: return "None";
            case VehicleModAction::Repair: return "Repair";
            case VehicleModAction::Clean: return "Clean";
            case VehicleModAction::FlipUpright: return "Flip upright";
            case VehicleModAction::MaxVehicle: return "Max vehicle";
            case VehicleModAction::SpawnVehicle: return "Spawn vehicle";
            case VehicleModAction::SetMod: return "Set mod";
            case VehicleModAction::RemoveMod: return "Remove mod";
            case VehicleModAction::ToggleMod: return "Toggle mod";
            case VehicleModAction::SetWheelType: return "Wheel category";
            }
            return "Unknown";
        }
    }

    void RenderVehicleGeneralPanel() noexcept
    {
        auto& runtime = Game::Mods::VehicleModificationRuntime::Get();
        const auto snapshot = runtime.Snapshot();
        const auto gameState = Game::GameState::Get().Snapshot();

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
            {
                ImGui::TextDisabled("Vehicle runtime is offline.");
            }
            else if (ImGui::BeginTabBar("##vehicle_general_tabs"))
            {
                if (ImGui::BeginTabItem("Current Vehicle"))
                {
                    ImGui::Text("Player ped: %d", gameState.playerPed);
                    ImGui::Text("GameState vehicle: %d", gameState.vehicle);
                    ImGui::Text("In vehicle: %s", gameState.inVehicle ? "yes" : "no");

                    if (!gameState.inVehicle || gameState.vehicle == 0)
                    {
                        ImGui::Spacing();
                        ImGui::TextDisabled("No current vehicle handle detected yet.");
                    }
                    else if (!snapshot.valid)
                    {
                        ImGui::Spacing();
                        ImGui::TextDisabled("Vehicle handle is detected; modification state is still refreshing.");
                    }
                    else
                    {
                        ImGui::Spacing();
                        if (ImGui::Button("Repair vehicle", ImVec2(-1.0f, 0.0f)))
                            static_cast<void>(runtime.QueueRepair());
                        if (ImGui::Button("Clean vehicle", ImVec2(-1.0f, 0.0f)))
                            static_cast<void>(runtime.QueueClean());
                        if (ImGui::Button("Flip / place upright", ImVec2(-1.0f, 0.0f)))
                            static_cast<void>(runtime.QueueFlipUpright());
                        if (ImGui::Button("Max vehicle", ImVec2(-1.0f, 0.0f)))
                            static_cast<void>(runtime.QueueMaxVehicle());
                    }

                    ImGui::Separator();
                    ImGui::Text("Last action: %s", ActionName(snapshot.lastAction));
                    if (snapshot.lastAction != Game::Mods::VehicleModAction::None)
                    {
                        if (snapshot.lastActionRejectedAsStale)
                            ImGui::TextDisabled("Dropped because you switched vehicles.");
                        else
                            ImGui::Text("Result: %s", snapshot.lastActionSucceeded ? "success" : "failed");
                    }
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Spawn"))
                {
                    ImGui::TextUnformatted("Spawn by GTA model name");
                    ImGui::TextDisabled("Examples: adder, sultanrs, banshee2, dominator10");
                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::InputText("##vehicle_model", g_SpawnModel, sizeof(g_SpawnModel));
                    ImGui::Checkbox("Spawn inside", &g_SpawnInside);
                    ImGui::Checkbox("Spawn maxed", &g_SpawnMaxed);

                    if (ImGui::Button("Spawn vehicle", ImVec2(-1.0f, 0.0f)))
                    {
                        const bool queued = runtime.QueueSpawnVehicle(g_SpawnModel, g_SpawnInside, g_SpawnMaxed);
                        g_SpawnMessage = queued ? "Spawn request queued" : "Spawn request rejected";
                    }

                    ImGui::Spacing();
                    ImGui::TextDisabled("%s", g_SpawnMessage);
                    if (snapshot.spawnPending)
                        ImGui::Text("Loading model: 0x%08X", snapshot.pendingSpawnModel);
                    if (snapshot.lastSpawnedVehicle != 0)
                        ImGui::Text("Last spawned vehicle: %d", snapshot.lastSpawnedVehicle);
                    if (snapshot.lastAction == Game::Mods::VehicleModAction::SpawnVehicle && !snapshot.spawnPending)
                        ImGui::Text("Last spawn: %s", snapshot.lastActionSucceeded ? "success" : "failed");

                    ImGui::Separator();
                    ImGui::TextWrapped("Spawner follows the same model validation -> request/load -> create -> optional driver-seat flow used by current GTA scripting examples, but executes through Tutones' GTA script queue.");
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
