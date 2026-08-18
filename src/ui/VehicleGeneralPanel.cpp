#include "VehicleGeneralPanel.hpp"

#include "../features/vehicle/VehicleModificationRuntime.hpp"
#include "../game/GameState.hpp"
#include "../game/vehicle/VehicleCatalogs.hpp"
#include "../game/vehicle/VehicleModels.hpp"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

namespace Tutones::UI
{
    namespace
    {
        char g_SpawnModel[48] = "adder";
        char g_Search[64]{};
        int g_ClassFilter{-1};
        int g_SelectedModel{-1};
        bool g_SpawnInside{true};
        bool g_SpawnMaxed{};
        bool g_CloneInside{};
        char g_PresetName[48] = "my_vehicle";
        bool g_LoadPresetInside{true};
        int g_SelectedPreset{-1};
        std::vector<std::string> g_SavedPresets{};
        const char* g_Message{"Ready"};

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
            case VehicleModAction::CloneNearest: return "Clone nearest";
            case VehicleModAction::SavePreset: return "Save preset";
            case VehicleModAction::LoadPreset: return "Load preset";
            case VehicleModAction::SetMod: return "Set mod";
            case VehicleModAction::RemoveMod: return "Remove mod";
            case VehicleModAction::ToggleMod: return "Toggle mod";
            case VehicleModAction::SetWheelType: return "Wheel family";
            case VehicleModAction::SetTireSmokeColor: return "Tire smoke color";
            case VehicleModAction::SetXenonColor: return "Xenon color";
            case VehicleModAction::SetNeonColor: return "Neon color";
            case VehicleModAction::SetNeonEnabled: return "Neon side";
            case VehicleModAction::SetTyresCanBurst: return "Tire durability";
            case VehicleModAction::SetDriftTyres: return "Low grip tires";
            }
            return "Unknown";
        }

        [[nodiscard]] bool SearchMatches(std::string_view model, std::string_view display) noexcept
        {
            if (g_Search[0] == '\0')
                return true;
            std::string haystack;
            haystack.reserve(model.size() + display.size() + 1);
            haystack.append(display);
            haystack.push_back(' ');
            haystack.append(model);
            std::string needle(g_Search);
            std::transform(haystack.begin(), haystack.end(), haystack.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return haystack.find(needle) != std::string::npos;
        }

        void RefreshSaved(Game::Mods::VehicleModificationRuntime& runtime)
        {
            g_SavedPresets = runtime.SavedPresetNames();
            if (g_SavedPresets.empty())
                g_SelectedPreset = -1;
            else
                g_SelectedPreset = std::clamp(g_SelectedPreset, 0, static_cast<int>(g_SavedPresets.size()) - 1);
        }

        void RenderSpawner(Game::Mods::VehicleModificationRuntime& runtime, const Game::Mods::VehicleModificationSnapshot& snapshot)
        {
            const auto catalog = runtime.CatalogSnapshot();
            ImGui::Text("Vehicle catalog: %zu models", Game::VehicleCatalogs::VehicleModels.size());
            if (catalog.ready < catalog.total)
                ImGui::TextDisabled("Classifying models on GTA thread: %zu / %zu", catalog.ready, catalog.total);

            ImGui::SetNextItemWidth(230.0f);
            ImGui::InputTextWithHint("##vehicle_search", "Search make, name, or model ID", g_Search, sizeof(g_Search));
            ImGui::SameLine();
            const char* classPreview = g_ClassFilter < 0
                ? "All classes"
                : Game::VehicleCatalogs::VehicleClassNames[static_cast<std::size_t>(g_ClassFilter)];
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##vehicle_class", classPreview))
            {
                if (ImGui::Selectable("All classes", g_ClassFilter == -1))
                    g_ClassFilter = -1;
                for (std::size_t i = 0; i < Game::VehicleCatalogs::VehicleClassNames.size(); ++i)
                {
                    const bool selected = g_ClassFilter == static_cast<int>(i);
                    if (ImGui::Selectable(Game::VehicleCatalogs::VehicleClassNames[i], selected))
                        g_ClassFilter = static_cast<int>(i);
                }
                ImGui::EndCombo();
            }

            if (ImGui::BeginListBox("##vehicle_catalog", ImVec2(-1.0f, 188.0f)))
            {
                for (std::size_t i = 0; i < Game::VehicleCatalogs::VehicleModels.size(); ++i)
                {
                    const char* model = Game::VehicleCatalogs::VehicleModels[i];
                    const std::string_view display = i < catalog.displayNames.size()
                        ? std::string_view(catalog.displayNames[i])
                        : std::string_view(model);
                    if (!SearchMatches(model, display))
                        continue;
                    if (g_ClassFilter >= 0)
                    {
                        if (i >= catalog.classes.size() || catalog.classes[i] != g_ClassFilter)
                            continue;
                    }

                    const bool selected = g_SelectedModel == static_cast<int>(i);
                    const char* label = i < catalog.displayNames.size() && !catalog.displayNames[i].empty()
                        ? catalog.displayNames[i].c_str()
                        : model;
                    if (ImGui::Selectable(label, selected))
                    {
                        g_SelectedModel = static_cast<int>(i);
                        std::snprintf(g_SpawnModel, sizeof(g_SpawnModel), "%s", model);
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndListBox();
            }

            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##direct_model", "Direct model / add-on fallback", g_SpawnModel, sizeof(g_SpawnModel));
            ImGui::Checkbox("Spawn inside", &g_SpawnInside);
            ImGui::SameLine();
            ImGui::Checkbox("Spawn maxed", &g_SpawnMaxed);
            if (ImGui::Button("Spawn selected vehicle", ImVec2(-1.0f, 0.0f)))
            {
                const bool queued = runtime.QueueSpawnVehicle(g_SpawnModel, g_SpawnInside, g_SpawnMaxed);
                g_Message = queued ? "Spawn request queued" : "Spawn request rejected";
            }
            if (snapshot.spawnPending)
                ImGui::Text("Loading model 0x%08X...", snapshot.pendingSpawnModel);
        }

        void RenderCurrent(
            Game::Mods::VehicleModificationRuntime& runtime,
            const Game::Mods::VehicleModificationSnapshot& snapshot,
            const Game::GameSnapshot& gameState)
        {
            ImGui::Text("Player ped: %d", gameState.playerPed);
            ImGui::Text("Vehicle handle: %d", gameState.vehicle);
            ImGui::Text("In vehicle: %s", gameState.inVehicle ? "yes" : "no");
            if (!gameState.inVehicle || gameState.vehicle == 0)
            {
                ImGui::TextDisabled("No current vehicle detected.");
                return;
            }
            if (!snapshot.valid)
                ImGui::TextDisabled("Vehicle detected; workshop state is refreshing.");

            if (ImGui::Button("Repair", ImVec2(150.0f, 0.0f))) static_cast<void>(runtime.QueueRepair());
            ImGui::SameLine();
            if (ImGui::Button("Clean", ImVec2(150.0f, 0.0f))) static_cast<void>(runtime.QueueClean());
            ImGui::SameLine();
            if (ImGui::Button("Flip upright", ImVec2(-1.0f, 0.0f))) static_cast<void>(runtime.QueueFlipUpright());
            if (ImGui::Button("Max all supported LSC mods", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueMaxVehicle());

            ImGui::Separator();
            ImGui::TextDisabled("LSC restrictions: Tutones applies supported native mod slots directly; rank/purchase gates are not part of this path.");
        }

        void RenderClone(Game::Mods::VehicleModificationRuntime& runtime)
        {
            ImGui::TextUnformatted("Clone nearest vehicle");
            ImGui::TextWrapped("Copies the nearest supported vehicle's model, paint/custom RGB, mod slots, wheel family/variants, toggles, xenon/neon, tire smoke and tire settings, then spawns the clone in front of you.");
            ImGui::Checkbox("Enter cloned vehicle", &g_CloneInside);
            if (ImGui::Button("Clone nearest (30m)", ImVec2(-1.0f, 0.0f)))
            {
                const bool queued = runtime.QueueCloneNearest(g_CloneInside);
                g_Message = queued ? "Nearest clone queued" : "Clone request rejected";
            }
        }

        void RenderSavedGarage(Game::Mods::VehicleModificationRuntime& runtime, const Game::Mods::VehicleModificationSnapshot& snapshot)
        {
            ImGui::TextUnformatted("Tutones Saved Garage");
            ImGui::TextDisabled("Local full-custom presets. This is separate from Rockstar Online personal-garage script globals.");
            ImGui::SetNextItemWidth(270.0f);
            ImGui::InputTextWithHint("##preset_name", "Preset name", g_PresetName, sizeof(g_PresetName));
            ImGui::SameLine();
            if (ImGui::Button("Save current", ImVec2(-1.0f, 0.0f)))
            {
                const bool queued = runtime.QueueSaveCurrentPreset(g_PresetName);
                g_Message = queued ? "Full vehicle preset queued for save" : "Save rejected (enter a vehicle first)";
            }

            if (ImGui::Button("Refresh saved garage", ImVec2(-1.0f, 0.0f)))
                RefreshSaved(runtime);
            if (g_SavedPresets.empty())
            {
                ImGui::TextDisabled("No local saved vehicles yet.");
            }
            else if (ImGui::BeginListBox("##saved_vehicles", ImVec2(-1.0f, 145.0f)))
            {
                for (std::size_t i = 0; i < g_SavedPresets.size(); ++i)
                {
                    const bool selected = g_SelectedPreset == static_cast<int>(i);
                    if (ImGui::Selectable(g_SavedPresets[i].c_str(), selected))
                        g_SelectedPreset = static_cast<int>(i);
                }
                ImGui::EndListBox();
            }

            ImGui::Checkbox("Spawn loaded preset inside", &g_LoadPresetInside);
            if (g_SelectedPreset >= 0 && g_SelectedPreset < static_cast<int>(g_SavedPresets.size()))
            {
                if (ImGui::Button("Spawn saved vehicle", ImVec2(-1.0f, 0.0f)))
                {
                    const bool queued = runtime.QueueLoadPreset(
                        g_SavedPresets[static_cast<std::size_t>(g_SelectedPreset)], g_LoadPresetInside);
                    g_Message = queued ? "Saved vehicle queued" : "Saved vehicle load rejected";
                }
            }
            if (!snapshot.lastSavedPreset.empty())
                ImGui::Text("Last preset: %s", snapshot.lastSavedPreset.c_str());
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
            ImGui::TextUnformatted("Vehicle Hub");
            ImGui::SameLine();
            ImGui::TextDisabled("%s", g_Message);
            ImGui::Separator();

            if (!runtime.IsRunning())
            {
                ImGui::TextDisabled("Vehicle runtime is offline.");
            }
            else if (ImGui::BeginTabBar("##vehicle_general_tabs"))
            {
                if (ImGui::BeginTabItem("Spawner"))
                {
                    RenderSpawner(runtime, snapshot);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Current"))
                {
                    RenderCurrent(runtime, snapshot, gameState);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Clone"))
                {
                    RenderClone(runtime);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Saved Garage"))
                {
                    RenderSavedGarage(runtime, snapshot);
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }

            ImGui::Separator();
            ImGui::Text("Last action: %s", ActionName(snapshot.lastAction));
            if (snapshot.lastAction != Game::Mods::VehicleModAction::None)
            {
                if (snapshot.lastActionRejectedAsStale)
                    ImGui::TextDisabled("Dropped because you switched vehicles.");
                else
                    ImGui::SameLine(), ImGui::Text("(%s)", snapshot.lastActionSucceeded ? "success" : "failed");
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
