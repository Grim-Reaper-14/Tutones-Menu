#include "VehicleGeneralPanel.hpp"

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/vehicle/VehicleModificationRuntime.hpp"
#include "../game/GameState.hpp"
#include "../game/VehicleNatives.hpp"
#include "../game/vehicle/VehicleCatalogs.hpp"
#include "../game/vehicle/VehicleModels.hpp"
#include "../runtime/GameRuntime.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
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

        char g_PlateText[9]{};
        int g_PlateStyle{};
        constexpr std::array<const char*, 13> PlateStyles{{
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
            case VehicleModAction::SetStealthMode: return "Vehicle stealth";
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

            ImGui::TextColored(V11Theme::Accent, "Vehicle Spawner");
            ImGui::SameLine();
            ImGui::TextDisabled("%zu vehicles", Game::VehicleCatalogs::VehicleModels.size());
            if (catalog.ready < catalog.total)
                ImGui::TextDisabled("Catalog loading: %zu / %zu", catalog.ready, catalog.total);

            ImGui::SeparatorText("Browse Vehicles");
            if (ImGui::BeginTable("##vehicle_spawn_filters", 2, ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Search", ImGuiTableColumnFlags_WidthStretch, 2.0f);
                ImGui::TableSetupColumn("Class", ImGuiTableColumnFlags_WidthStretch, 1.0f);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("Search");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextDisabled("Vehicle Class");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::InputTextWithHint("##vehicle_search", "Make, name, or model ID", g_Search, sizeof(g_Search));
                DescribeLastV11Item("Filter the built-in vehicle catalog by display name, make, or model ID.");

                ImGui::TableSetColumnIndex(1);
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
                DescribeLastV11Item("Limit the vehicle browser to one GTA vehicle class, or show every class.");
                ImGui::EndTable();
            }

            if (ImGui::BeginListBox("##vehicle_catalog", ImVec2(-1.0f, 148.0f)))
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

            ImGui::SeparatorText("Vehicle to Spawn");
            if (g_SelectedModel >= 0
                && g_SelectedModel < static_cast<int>(Game::VehicleCatalogs::VehicleModels.size()))
            {
                const auto selectedIndex = static_cast<std::size_t>(g_SelectedModel);
                const char* selectedDisplay = selectedIndex < catalog.displayNames.size()
                    && !catalog.displayNames[selectedIndex].empty()
                    ? catalog.displayNames[selectedIndex].c_str()
                    : Game::VehicleCatalogs::VehicleModels[selectedIndex];
                ImGui::Text("Selected: %s", selectedDisplay);
            }
            else
            {
                ImGui::TextDisabled("No catalog vehicle selected; enter a model name below.");
            }

            ImGui::TextDisabled("Model name / add-on");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##direct_model", "Vehicle model name", g_SpawnModel, sizeof(g_SpawnModel));
            DescribeLastV11Item("Enter a model name directly, including supported add-on model names not found in the built-in catalog.");

            ImGui::SeparatorText("Spawn Options");
            if (ImGui::BeginTable("##vehicle_spawn_options", 2, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Checkbox("Spawn inside", &g_SpawnInside);
                DescribeLastV11Item("Place your player directly into the newly spawned vehicle when the spawn succeeds.");

                ImGui::TableSetColumnIndex(1);
                ImGui::Checkbox("Spawn maxed", &g_SpawnMaxed);
                DescribeLastV11Item("Apply the runtime's supported maximum vehicle modifications after the vehicle is spawned.");
                ImGui::EndTable();
            }

            ImGui::BeginDisabled(snapshot.spawnPending);
            if (ImGui::Button(snapshot.spawnPending ? "Loading vehicle..." : "Spawn Vehicle", ImVec2(-1.0f, 0.0f)))
            {
                const bool queued = runtime.QueueSpawnVehicle(g_SpawnModel, g_SpawnInside, g_SpawnMaxed);
                g_Message = queued ? "Spawn request queued" : "Spawn request rejected";
            }
            ImGui::EndDisabled();
            DescribeLastV11Item("Queue the selected or directly entered model for spawning on the GTA game thread.");

            if (snapshot.spawnPending)
                ImGui::TextDisabled("Loading model 0x%08X...", snapshot.pendingSpawnModel);
        }

        void RenderCurrent(
            Game::Mods::VehicleModificationRuntime& runtime,
            const Game::Mods::VehicleModificationSnapshot& snapshot,
            const Game::GameSnapshot& gameState)
        {
            ImGui::TextColored(V11Theme::Accent, "Current Vehicle");
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

            ImGui::SeparatorText("License Plate");
            ImGui::SetNextItemWidth(150.0f);
            ImGui::InputTextWithHint("##plate_text", "Plate Number", g_PlateText, sizeof(g_PlateText));
            DescribeLastV11Item("GTA license plates support up to 8 characters, matching YimMenuV2's vehicle editor.");
            ImGui::SameLine();
            if (ImGui::Button("Change Plate"))
            {
                const Game::Vehicle vehicle = gameState.vehicle;
                const std::string plate(g_PlateText);
                const bool queued = Runtime::GameRuntime::Get().Enqueue([vehicle, plate] {
                    static_cast<void>(Game::VehicleNatives::SetVehicleNumberPlateText(vehicle, plate));
                });
                g_Message = queued ? "Plate text queued" : "Plate text rejected";
            }
            DescribeLastV11Item("Apply the entered license plate text to the current vehicle on the GTA game thread.");

            ImGui::SetNextItemWidth(260.0f);
            if (ImGui::BeginCombo("Plate Style", PlateStyles[static_cast<std::size_t>(g_PlateStyle)]))
            {
                for (std::size_t i = 0; i < PlateStyles.size(); ++i)
                {
                    const bool selected = g_PlateStyle == static_cast<int>(i);
                    if (ImGui::Selectable(PlateStyles[i], selected))
                    {
                        g_PlateStyle = static_cast<int>(i);
                        const Game::Vehicle vehicle = gameState.vehicle;
                        const int style = g_PlateStyle;
                        const bool queued = Runtime::GameRuntime::Get().Enqueue([vehicle, style] {
                            static_cast<void>(Game::VehicleNatives::SetVehicleNumberPlateTextIndex(vehicle, style));
                        });
                        g_Message = queued ? "Plate style queued" : "Plate style rejected";
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            DescribeLastV11Item("Choose one of the 13 plate styles exposed by YimMenuV2, including Yankton, Ecola, Liberty City, Car Meet, Pounders and Sprunk.");

            ImGui::SeparatorText("Vehicle Actions");
            if (ImGui::Button("Repair", ImVec2(150.0f, 0.0f))) static_cast<void>(runtime.QueueRepair());
            DescribeLastV11Item("Repair the current vehicle through the verified vehicle modification runtime.");
            ImGui::SameLine();
            if (ImGui::Button("Clean", ImVec2(150.0f, 0.0f))) static_cast<void>(runtime.QueueClean());
            DescribeLastV11Item("Clean dirt and visible grime from the current vehicle.");
            ImGui::SameLine();
            if (ImGui::Button("Flip upright", ImVec2(-1.0f, 0.0f))) static_cast<void>(runtime.QueueFlipUpright());
            DescribeLastV11Item("Rotate the current vehicle upright if it has rolled or landed awkwardly.");
            if (ImGui::Button("Max all supported LSC mods", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueMaxVehicle());
            DescribeLastV11Item("Apply the highest supported native LSC option to each modification slot exposed by this vehicle.");

            ImGui::BeginDisabled(snapshot.stealthPending);
            if (ImGui::Button("Enable stealth", ImVec2(220.0f, 0.0f)))
                g_Message = runtime.QueueStealthMode(true) ? "Vehicle stealth queued" : "Vehicle stealth rejected";
            DescribeLastV11Item("Acquire network control, close the supported stealth hardware, and verify GTA's Enhanced vehicle_stealth_mode state.");
            ImGui::SameLine();
            if (ImGui::Button("Disable stealth", ImVec2(-1.0f, 0.0f)))
                g_Message = runtime.QueueStealthMode(false) ? "Vehicle stealth disable queued" : "Vehicle stealth disable rejected";
            DescribeLastV11Item("Deploy the supported stealth hardware and verify that GTA cleared its vehicle-stealth state.");
            ImGui::EndDisabled();

            ImGui::Separator();
            ImGui::TextDisabled("LSC restrictions: Tutones applies supported native mod slots directly; rank/purchase gates are not part of this path.");
        }

        void RenderClone(Game::Mods::VehicleModificationRuntime& runtime)
        {
            ImGui::TextColored(V11Theme::Accent, "Clone Nearest Vehicle");
            ImGui::TextWrapped("Copies the nearest supported vehicle's model, paint/custom RGB, mod slots, wheel family/variants, toggles, xenon/neon, tire smoke and tire settings, then spawns the clone in front of you.");
            ImGui::Checkbox("Enter cloned vehicle", &g_CloneInside);
            DescribeLastV11Item("Enter the cloned vehicle automatically after the clone is created.");
            if (ImGui::Button("Clone nearest (30m)", ImVec2(-1.0f, 0.0f)))
            {
                const bool queued = runtime.QueueCloneNearest(g_CloneInside);
                g_Message = queued ? "Nearest clone queued" : "Clone request rejected";
            }
            DescribeLastV11Item("Copy the nearest supported vehicle within 30 meters and reproduce its supported customization state.");
        }

        void RenderSavedGarage(Game::Mods::VehicleModificationRuntime& runtime, const Game::Mods::VehicleModificationSnapshot& snapshot)
        {
            ImGui::TextColored(V11Theme::Accent, "Tutones Saved Garage");
            ImGui::TextDisabled("Local full-custom presets. This is separate from Rockstar Online personal-garage script globals.");
            ImGui::SetNextItemWidth(270.0f);
            ImGui::InputTextWithHint("##preset_name", "Preset name", g_PresetName, sizeof(g_PresetName));
            DescribeLastV11Item("Choose the local preset name used when saving the current vehicle customization.");
            ImGui::SameLine();
            if (ImGui::Button("Save current", ImVec2(-1.0f, 0.0f)))
            {
                const bool queued = runtime.QueueSaveCurrentPreset(g_PresetName);
                g_Message = queued ? "Full vehicle preset queued for save" : "Save rejected (enter a vehicle first)";
            }
            DescribeLastV11Item("Save the current vehicle as a local Tutones preset. This does not write a Rockstar Online personal vehicle.");

            if (ImGui::Button("Refresh saved garage", ImVec2(-1.0f, 0.0f)))
                RefreshSaved(runtime);
            DescribeLastV11Item("Rescan the Tutones local saved-vehicle preset folder and refresh this list.");
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
            DescribeLastV11Item("Enter the vehicle automatically when spawning the selected local saved preset.");
            if (g_SelectedPreset >= 0 && g_SelectedPreset < static_cast<int>(g_SavedPresets.size()))
            {
                if (ImGui::Button("Spawn saved vehicle", ImVec2(-1.0f, 0.0f)))
                {
                    const bool queued = runtime.QueueLoadPreset(
                        g_SavedPresets[static_cast<std::size_t>(g_SelectedPreset)], g_LoadPresetInside);
                    g_Message = queued ? "Saved vehicle queued" : "Saved vehicle load rejected";
                }
                DescribeLastV11Item("Spawn the selected local preset and reapply its saved supported customization state.");
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
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##vehicle_general", ImVec2(490.0f, 430.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Vehicle Hub");
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
                ImGui::EndTabBar();
            }

            ImGui::Separator();
            ImGui::Text("Last action: %s", ActionName(snapshot.lastAction));
            if (snapshot.lastAction != Game::Mods::VehicleModAction::None)
            {
                if (snapshot.stealthPending && snapshot.lastAction == Game::Mods::VehicleModAction::SetStealthMode)
                    ImGui::SameLine(), ImGui::Text("(pending)");
                else if (snapshot.lastActionRejectedAsStale)
                    ImGui::TextDisabled("Dropped because you switched vehicles.");
                else
                    ImGui::SameLine(), ImGui::Text("(%s)", snapshot.lastActionSucceeded ? "success" : "failed");

                if (!snapshot.lastActionMessage.empty())
                    ImGui::TextWrapped("%s", snapshot.lastActionMessage.c_str());
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
