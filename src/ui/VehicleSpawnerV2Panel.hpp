#pragma once

#include "Input.hpp"
#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "VehicleThumbnailCache.hpp"
#include "../features/vehicle/DlcVehicleRuntime.hpp"
#include "../features/vehicle/PersonalVehicleRuntime.hpp"
#include "../features/vehicle/SavePersonalVehicleRuntime.hpp"
#include "../features/vehicle/VehicleModificationRuntime.hpp"
#include "../game/vehicle/VehicleCatalogs.hpp"
#include "../game/vehicle/VehicleModels.hpp"

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>

namespace Tutones::UI
{
    namespace VehicleSpawnerV2Detail
    {
        inline char g_SpawnModel[48] = "adder";
        inline char g_Search[64]{};
        inline int g_ClassFilter{-1};
        inline int g_SelectedModel{-1};
        inline bool g_SpawnInside{true};
        inline bool g_SpawnMaxed{};
        inline bool g_CloneInside{true};
        inline bool g_ShowVisualCategories{true};
        inline int g_SelectedPersonalVehicle{-1};
        inline std::string g_Message{"Ready"};

        enum class CloneCurrentStage : unsigned char
        {
            Idle,
            WaitingForSave,
            WaitingForSpawn,
        };

        inline CloneCurrentStage g_CloneStage{CloneCurrentStage::Idle};
        inline std::string g_CloneScratchName{};

        [[nodiscard]] inline bool SearchMatches(std::string_view model, std::string_view display) noexcept
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

        [[nodiscard]] inline std::filesystem::path CloneScratchPath(std::string_view name)
        {
            const char* localAppData = std::getenv("LOCALAPPDATA");
            std::filesystem::path root = (localAppData && *localAppData) ? localAppData : ".";
            return root / "TutonesMenu" / "saved_vehicles" / (std::string(name) + ".tutcar");
        }

        inline void CleanupCloneScratch() noexcept
        {
            if (g_CloneScratchName.empty())
                return;
            std::error_code error;
            std::filesystem::remove(CloneScratchPath(g_CloneScratchName), error);
            g_CloneScratchName.clear();
        }

        inline void PumpCloneCurrent(
            Game::Mods::VehicleModificationRuntime& runtime,
            const Game::Mods::VehicleModificationSnapshot& snapshot) noexcept
        {
            using Game::Mods::VehicleModAction;

            if (g_CloneStage == CloneCurrentStage::WaitingForSave
                && snapshot.lastAction == VehicleModAction::SavePreset
                && snapshot.lastSavedPreset == g_CloneScratchName)
            {
                if (!snapshot.lastActionSucceeded)
                {
                    g_Message = "Clone Current failed while capturing vehicle state";
                    CleanupCloneScratch();
                    g_CloneStage = CloneCurrentStage::Idle;
                    return;
                }

                if (runtime.QueueLoadPreset(g_CloneScratchName, g_CloneInside))
                {
                    g_Message = "Clone Current captured - spawning clone";
                    g_CloneStage = CloneCurrentStage::WaitingForSpawn;
                }
                else
                {
                    g_Message = "Clone Current captured but spawn queue failed";
                    CleanupCloneScratch();
                    g_CloneStage = CloneCurrentStage::Idle;
                }
                return;
            }

            if (g_CloneStage == CloneCurrentStage::WaitingForSpawn
                && snapshot.lastAction == VehicleModAction::LoadPreset
                && snapshot.lastSavedPreset == g_CloneScratchName)
            {
                g_Message = snapshot.lastActionSucceeded
                    ? "Current vehicle cloned successfully"
                    : "Clone Current spawn failed";
                CleanupCloneScratch();
                g_CloneStage = CloneCurrentStage::Idle;
            }
        }

        inline void BeginCloneCurrent(Game::Mods::VehicleModificationRuntime& runtime) noexcept
        {
            if (g_CloneStage != CloneCurrentStage::Idle)
                return;

            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            g_CloneScratchName = "clone_current_" + std::to_string(static_cast<unsigned long long>(stamp));
            if (runtime.QueueSaveCurrentPreset(g_CloneScratchName))
            {
                g_Message = "Capturing current vehicle for clone";
                g_CloneStage = CloneCurrentStage::WaitingForSave;
            }
            else
            {
                g_Message = "Clone Current rejected - enter a vehicle first";
                CleanupCloneScratch();
            }
        }

        template <typename CatalogSnapshot>
        inline void EnsureVehicleSelection(const CatalogSnapshot& catalog) noexcept
        {
            if (g_SelectedModel >= 0
                && g_SelectedModel < static_cast<int>(Game::VehicleCatalogs::VehicleModels.size()))
                return;

            for (std::size_t i = 0; i < Game::VehicleCatalogs::VehicleModels.size(); ++i)
            {
                if (std::string_view(Game::VehicleCatalogs::VehicleModels[i]) == std::string_view(g_SpawnModel))
                {
                    g_SelectedModel = static_cast<int>(i);
                    return;
                }
            }

            if (!Game::VehicleCatalogs::VehicleModels.empty())
            {
                g_SelectedModel = 0;
                std::snprintf(
                    g_SpawnModel,
                    sizeof(g_SpawnModel),
                    "%s",
                    Game::VehicleCatalogs::VehicleModels.front());
            }

            static_cast<void>(catalog);
        }

        inline bool CategoryCard(int classIndex, const char* label, const VehicleThumbnailView& thumbnail) noexcept
        {
            constexpr float CardWidth = 103.0f;
            constexpr float CardHeight = 77.0f;
            constexpr float ImageHeight = 51.0f;

            ImGui::PushID(classIndex);
            const ImVec2 start = ImGui::GetCursorScreenPos();
            const bool selected = g_ClassFilter == classIndex;
            ImGui::InvisibleButton("##vehicle_category_card", ImVec2(CardWidth, CardHeight));
            const bool pressed = ImGui::IsItemClicked();
            const bool hovered = ImGui::IsItemHovered();

            auto* draw = ImGui::GetWindowDrawList();
            const ImU32 background = ImGui::GetColorU32(
                selected ? V11Theme::AccentDark : (hovered ? V11Theme::ControlHover : V11Theme::ControlBg));
            const ImU32 border = ImGui::GetColorU32(selected ? V11Theme::Accent : V11Theme::PanelBorder);
            draw->AddRectFilled(start, ImVec2(start.x + CardWidth, start.y + CardHeight), background, 4.0f);
            draw->AddRect(start, ImVec2(start.x + CardWidth, start.y + CardHeight), border, 4.0f, 0, selected ? 2.0f : 1.0f);

            if (thumbnail.Valid())
            {
                draw->AddImage(
                    thumbnail.texture,
                    ImVec2(start.x + 3.0f, start.y + 3.0f),
                    ImVec2(start.x + CardWidth - 3.0f, start.y + ImageHeight));
            }

            const ImVec2 textSize = ImGui::CalcTextSize(label);
            draw->AddText(
                ImVec2(start.x + std::max(5.0f, (CardWidth - textSize.x) * 0.5f), start.y + 57.0f),
                ImGui::GetColorU32(selected ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : V11Theme::MutedText),
                label);

            ImGui::PopID();
            return pressed;
        }

        inline void RenderVisualCategories() noexcept
        {
            auto& thumbnails = VehicleThumbnailCache::Get();

            ImGui::Checkbox("Visual vehicle categories", &g_ShowVisualCategories);
            DescribeLastV11Item("Show GTA vehicle classes as picture cards. Built-in artwork is always available and can be replaced with your own class_0.png through class_22.png files.");
            if (!g_ShowVisualCategories)
                return;

            if (ImGui::Button("All Vehicles", ImVec2(118.0f, 27.0f)))
                g_ClassFilter = -1;
            DescribeLastV11Item("Clear the class filter and browse the full vehicle catalog.");
            ImGui::SameLine();
            if (ImGui::Button("Refresh Images", ImVec2(118.0f, 27.0f)))
                thumbnails.Refresh();
            DescribeLastV11Item("Reload class artwork and the selected vehicle image from the Tutones vehicle_thumbnails folder.");
            ImGui::SameLine();
            ImGui::TextDisabled(g_ClassFilter < 0 ? "ALL" : Game::VehicleCatalogs::VehicleClassNames[static_cast<std::size_t>(g_ClassFilter)]);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 6.0f));
            if (ImGui::BeginChild("##visual_vehicle_categories", ImVec2(-1.0f, 174.0f), true))
            {
                constexpr int Columns = 4;
                for (std::size_t i = 0; i < Game::VehicleCatalogs::VehicleClassNames.size(); ++i)
                {
                    const auto thumbnail = thumbnails.ClassThumbnail(static_cast<int>(i));
                    if (CategoryCard(
                            static_cast<int>(i),
                            Game::VehicleCatalogs::VehicleClassNames[i],
                            thumbnail))
                    {
                        g_ClassFilter = static_cast<int>(i);
                    }

                    if ((i % Columns) != Columns - 1 && i + 1 < Game::VehicleCatalogs::VehicleClassNames.size())
                        ImGui::SameLine();
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
        }

        template <typename CatalogSnapshot>
        inline void RenderSelectedVehiclePreview(const CatalogSnapshot& catalog) noexcept
        {
            if (g_SelectedModel < 0
                || g_SelectedModel >= static_cast<int>(Game::VehicleCatalogs::VehicleModels.size()))
            {
                ImGui::TextDisabled("Select a vehicle to preview it.");
                return;
            }

            const auto index = static_cast<std::size_t>(g_SelectedModel);
            const char* model = Game::VehicleCatalogs::VehicleModels[index];
            const char* display = index < catalog.displayNames.size() && !catalog.displayNames[index].empty()
                ? catalog.displayNames[index].c_str()
                : model;
            const int classIndex = index < catalog.classes.size() ? catalog.classes[index] : -1;
            const auto thumbnail = VehicleThumbnailCache::Get().VehicleThumbnail(model, classIndex);

            ImGui::TextColored(V11Theme::Accent, "%s", display);
            ImGui::TextDisabled("Model: %s", model);
            if (classIndex >= 0 && classIndex < static_cast<int>(Game::VehicleCatalogs::VehicleClassNames.size()))
                ImGui::TextDisabled("Class: %s", Game::VehicleCatalogs::VehicleClassNames[static_cast<std::size_t>(classIndex)]);
            else
                ImGui::TextDisabled("Class: unknown");

            const ImVec2 start = ImGui::GetCursorScreenPos();
            constexpr float PreviewWidthUi = 166.0f;
            constexpr float PreviewHeightUi = 83.0f;
            ImGui::InvisibleButton("##selected_vehicle_preview", ImVec2(PreviewWidthUi, PreviewHeightUi));
            auto* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(start, ImVec2(start.x + PreviewWidthUi, start.y + PreviewHeightUi), ImGui::GetColorU32(V11Theme::ControlBg), 4.0f);
            if (thumbnail.Valid())
            {
                draw->AddImage(
                    thumbnail.texture,
                    ImVec2(start.x + 3.0f, start.y + 3.0f),
                    ImVec2(start.x + PreviewWidthUi - 3.0f, start.y + PreviewHeightUi - 3.0f));
            }
            draw->AddRect(start, ImVec2(start.x + PreviewWidthUi, start.y + PreviewHeightUi), ImGui::GetColorU32(V11Theme::PanelBorder), 4.0f);

            ImGui::TextDisabled(thumbnail.custom ? "Vehicle artwork: custom image" : "Vehicle artwork: class preview");
            if (!thumbnail.custom)
                DescribeLastV11Item("Drop a picture named after the model, for example adder.png, into %LOCALAPPDATA%\\TutonesMenu\\vehicle_thumbnails and press Refresh Images to use the real vehicle artwork.");
        }

        inline void RenderCatalogSpawner(
            Game::Mods::VehicleModificationRuntime& runtime,
            const Game::Mods::VehicleModificationSnapshot& snapshot) noexcept
        {
            const auto catalog = runtime.CatalogSnapshot();
            EnsureVehicleSelection(catalog);

            ImGui::SeparatorText("Spawn Vehicle");
            RenderVisualCategories();

            if (ImGui::BeginTable("##vehicle_spawn_filters_v2", 2, ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Search", ImGuiTableColumnFlags_WidthStretch, 2.0f);
                ImGui::TableSetupColumn("Class", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::InputTextWithHint("##vehicle_search_v2", "Search vehicle", g_Search, sizeof(g_Search));
                DescribeLastV11Item("Search the built-in GTA vehicle catalog by display name or model name.");

                ImGui::TableSetColumnIndex(1);
                const char* classPreview = g_ClassFilter < 0
                    ? "All classes"
                    : Game::VehicleCatalogs::VehicleClassNames[static_cast<std::size_t>(g_ClassFilter)];
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::BeginCombo("##vehicle_class_v2", classPreview))
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
                DescribeLastV11Item("Filter the vehicle catalog by GTA vehicle class. The picture cards above update the same filter.");
                ImGui::EndTable();
            }

            if (ImGui::BeginTable("##vehicle_catalog_visual_browser", 2, ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Vehicle list", ImGuiTableColumnFlags_WidthStretch, 1.65f);
                ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthFixed, 176.0f);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                if (ImGui::BeginListBox("##vehicle_catalog_v2", ImVec2(-1.0f, 154.0f)))
                {
                    for (std::size_t i = 0; i < Game::VehicleCatalogs::VehicleModels.size(); ++i)
                    {
                        const char* model = Game::VehicleCatalogs::VehicleModels[i];
                        const std::string_view display = i < catalog.displayNames.size()
                            ? std::string_view(catalog.displayNames[i])
                            : std::string_view(model);
                        if (!SearchMatches(model, display))
                            continue;
                        if (g_ClassFilter >= 0 && (i >= catalog.classes.size() || catalog.classes[i] != g_ClassFilter))
                            continue;

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

                ImGui::TableSetColumnIndex(1);
                RenderSelectedVehiclePreview(catalog);
                ImGui::EndTable();
            }

            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##spawn_model_v2", "Model name / add-on model", g_SpawnModel, sizeof(g_SpawnModel));
            DescribeLastV11Item("Enter a GTA model name directly or use the visual vehicle browser above.");

            if (ImGui::BeginTable("##spawn_options_v2", 2, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Checkbox("Spawn inside", &g_SpawnInside);
                ImGui::TableSetColumnIndex(1);
                ImGui::Checkbox("Spawn maxed", &g_SpawnMaxed);
                ImGui::EndTable();
            }

            ImGui::BeginDisabled(snapshot.spawnPending);
            if (ImGui::Button(snapshot.spawnPending ? "Loading Vehicle..." : "Spawn Vehicle", ImVec2(-1.0f, 30.0f)))
            {
                g_Message = runtime.QueueSpawnVehicle(g_SpawnModel, g_SpawnInside, g_SpawnMaxed)
                    ? "Vehicle spawn queued"
                    : "Vehicle spawn rejected";
            }
            ImGui::EndDisabled();
            DescribeLastV11Item("Spawn the selected vehicle on the GTA game thread using Tutones' vehicle runtime.");
        }

        inline void RenderCloneTools(
            Game::Mods::VehicleModificationRuntime& runtime,
            const Game::Mods::VehicleModificationSnapshot& snapshot) noexcept
        {
            ImGui::SeparatorText("Clone Vehicle");
            ImGui::Checkbox("Enter cloned vehicle", &g_CloneInside);
            DescribeLastV11Item("Automatically enter a cloned vehicle after it spawns.");

            const bool cloneBusy = g_CloneStage != CloneCurrentStage::Idle || snapshot.spawnPending;
            ImGui::BeginDisabled(cloneBusy);
            if (ImGui::BeginTable("##clone_vehicle_v2", 2, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (ImGui::Button("Clone Current Vehicle", ImVec2(-1.0f, 30.0f)))
                    BeginCloneCurrent(runtime);
                DescribeLastV11Item("Clone the vehicle you are currently driving, including its supported paint, RGB, mods, wheels, lighting and tire state.");

                ImGui::TableSetColumnIndex(1);
                if (ImGui::Button("Clone Nearest Vehicle", ImVec2(-1.0f, 30.0f)))
                {
                    g_Message = runtime.QueueCloneNearest(g_CloneInside)
                        ? "Nearest vehicle clone queued"
                        : "Nearest vehicle clone rejected";
                }
                DescribeLastV11Item("Clone the nearest supported vehicle within the runtime's 30 meter search radius.");
                ImGui::EndTable();
            }
            ImGui::EndDisabled();
        }

        inline std::string PersonalVehicleLabel(const Game::PersonalVehicles::PersonalVehicleEntry& vehicle)
        {
            std::string label = vehicle.displayName.empty() ? "Personal Vehicle" : vehicle.displayName;
            if (!vehicle.plate.empty())
                label += " | " + vehicle.plate;
            if (!vehicle.garage.empty())
                label += " | " + vehicle.garage;
            if (vehicle.destroyed)
                label += " | DESTROYED";
            if (vehicle.impounded)
                label += " | IMPOUNDED";
            return label;
        }

        inline void ValidatePersonalVehicleSelection(const Game::PersonalVehicles::PersonalVehicleSnapshot& snapshot) noexcept
        {
            const auto selected = std::find_if(snapshot.vehicles.begin(), snapshot.vehicles.end(), [](const auto& vehicle) {
                return vehicle.id == g_SelectedPersonalVehicle;
            });
            if (selected != snapshot.vehicles.end())
                return;

            if (snapshot.currentVehicleId >= 0)
                g_SelectedPersonalVehicle = snapshot.currentVehicleId;
            else if (!snapshot.vehicles.empty())
                g_SelectedPersonalVehicle = snapshot.vehicles.front().id;
            else
                g_SelectedPersonalVehicle = -1;
        }

        inline void RenderPersonalVehicles() noexcept
        {
            using Game::PersonalVehicles::PersonalVehicleRuntime;
            using Game::PersonalVehicles::SavePersonalVehicleRuntime;

            auto& personalRuntime = PersonalVehicleRuntime::Get();
            auto& saveRuntime = SavePersonalVehicleRuntime::Get();
            const auto personal = personalRuntime.Snapshot();
            const auto save = saveRuntime.Snapshot();
            ValidatePersonalVehicleSelection(personal);

            ImGui::SeparatorText("Personal Vehicle / Garage");

            const auto selected = std::find_if(personal.vehicles.begin(), personal.vehicles.end(), [](const auto& vehicle) {
                return vehicle.id == g_SelectedPersonalVehicle;
            });
            const std::string preview = selected != personal.vehicles.end()
                ? PersonalVehicleLabel(*selected)
                : std::string("Choose personal vehicle");

            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##personal_vehicle_v2", preview.c_str()))
            {
                for (const auto& vehicle : personal.vehicles)
                {
                    const std::string label = PersonalVehicleLabel(vehicle);
                    const bool selectedItem = vehicle.id == g_SelectedPersonalVehicle;
                    if (ImGui::Selectable(label.c_str(), selectedItem))
                        g_SelectedPersonalVehicle = vehicle.id;
                    if (selectedItem)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            DescribeLastV11Item("Choose one of your Rockstar personal vehicles discovered from the Enhanced MPSV/Freemode backend.");

            const bool canRequest = personal.running
                && personal.sessionStarted
                && personal.scriptGlobalsReady
                && personal.requestSupported
                && !personal.actionPending;

            ImGui::BeginDisabled(!canRequest || g_SelectedPersonalVehicle < 0);
            if (ImGui::Button("Spawn Selected Personal Vehicle", ImVec2(-1.0f, 30.0f)))
            {
                g_Message = personalRuntime.QueueRequest(g_SelectedPersonalVehicle)
                    ? "Personal vehicle request queued"
                    : "Personal vehicle request rejected";
            }
            ImGui::EndDisabled();
            DescribeLastV11Item("Request the selected owned personal vehicle through the Enhanced Freemode personal-vehicle state machine.");

            ImGui::BeginDisabled(!canRequest || personal.currentVehicleId < 0);
            if (ImGui::Button("Spawn Current / Last Personal Vehicle", ImVec2(-1.0f, 30.0f)))
            {
                g_Message = personalRuntime.QueueRequest(personal.currentVehicleId)
                    ? "Current personal vehicle request queued"
                    : "Current personal vehicle request rejected";
            }
            ImGui::EndDisabled();
            DescribeLastV11Item("Request your current or last Rockstar personal vehicle without selecting it from the list.");

            ImGui::BeginDisabled(!personal.sessionStarted || save.pending);
            if (ImGui::Button("Save Current Vehicle to Personal Garage", ImVec2(-1.0f, 32.0f)))
            {
                if (saveRuntime.QueueSaveCurrent())
                {
                    g_Message = "GTA garage selector opened - Tutones released input";
                    Input::Get().SetMenuOpen(false);
                }
                else
                {
                    g_Message = "Save to personal garage request rejected";
                }
            }
            ImGui::EndDisabled();
            DescribeLastV11Item("Open GTA Online's personal-garage selector for the vehicle you are currently driving. Tutones closes so GTA can receive the garage-selection input.");

            if (save.pending || save.haveResult)
                ImGui::TextDisabled("Garage save: %s", save.message.c_str());
            if (personal.actionPending)
                ImGui::TextDisabled("Personal vehicle action is pending...");
        }

        inline void RenderDlcVehicleSupport() noexcept
        {
            auto& runtime = Game::VehicleFeatures::DlcVehicleRuntime::Get();
            const auto state = runtime.Snapshot();

            ImGui::SeparatorText("Vehicle Website Support");
            bool enabled = state.enabled;
            ImGui::BeginDisabled(!state.running);
            if (ImGui::Checkbox("Enable All DLC Vehicles", &enabled))
                runtime.SetEnabled(enabled);
            ImGui::EndDisabled();
            DescribeLastV11Item("Use the verified Enhanced appinternet shadow-script patches for vehicle availability, price and purchase checks.");
            ImGui::TextDisabled("appinternet: %s | script checks: %s",
                state.programLoaded ? "LOADED" : "WAITING",
                state.vehicleAvailabilitySupported && state.priceGateSupported && state.purchaseGateSupported ? "READY" : "WAITING");
        }
    }

    inline void RenderVehicleSpawnerV2Panel() noexcept
    {
        using namespace VehicleSpawnerV2Detail;

        auto& runtime = Game::Mods::VehicleModificationRuntime::Get();
        const auto snapshot = runtime.Snapshot();
        PumpCloneCurrent(runtime, snapshot);

        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##vehicle_spawner_v2", ImVec2(490.0f, 430.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Vehicle Spawner");
            ImGui::SameLine();
            ImGui::TextDisabled("%s", g_Message.c_str());
            ImGui::Separator();

            if (!runtime.IsRunning())
            {
                ImGui::TextDisabled("Vehicle runtime is offline.");
            }
            else
            {
                RenderCatalogSpawner(runtime, snapshot);
                RenderCloneTools(runtime, snapshot);
                RenderPersonalVehicles();
                RenderDlcVehicleSupport();
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);

        SetV11Description("Vehicle Spawner - visual picture categories and selected-car previews plus spawning, cloning, Rockstar personal vehicles, garage save and DLC website support.");
    }
}
