#include "VehicleModificationPanel.hpp"

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/vehicle/VehicleAppearanceRuntime.hpp"
#include "../features/vehicle/VehicleModificationRuntime.hpp"
#include "../features/vehicle/VehicleWorkshopRuntime.hpp"
#include "../game/GameState.hpp"
#include "../game/vehicle/VehicleCatalogs.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <string>

namespace Tutones::UI
{
    namespace
    {
        enum class WorkshopPage : int
        {
            Home,
            Performance,
            Appearance,
            Customization,
        };

        constexpr std::array<const char*, 5> PerformanceNames{{
            "Engine",
            "Brakes",
            "Transmission",
            "Suspension",
            "Armor",
        }};

        constexpr std::array<const char*, 7> WindowTintNames{{
            "None",
            "Pure Black",
            "Dark Smoke",
            "Light Smoke",
            "Stock",
            "Limo",
            "Green",
        }};

        constexpr std::array<const char*, 13> PlateStyleNames{{
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

        WorkshopPage g_WorkshopPage{WorkshopPage::Home};

        [[nodiscard]] const char* WorkshopActionName(Game::Mods::VehicleWorkshopAction action) noexcept
        {
            switch (action)
            {
            case Game::Mods::VehicleWorkshopAction::SetPerformanceLevel: return "Performance level";
            case Game::Mods::VehicleWorkshopAction::SetTurbo: return "Turbo";
            case Game::Mods::VehicleWorkshopAction::MaxPerformance: return "Max performance";
            case Game::Mods::VehicleWorkshopAction::StockPerformance: return "Restore stock";
            default: return "None";
            }
        }

        [[nodiscard]] const char* WorkshopResultName(Game::Mods::VehicleWorkshopResult result) noexcept
        {
            switch (result)
            {
            case Game::Mods::VehicleWorkshopResult::Queued: return "Queued";
            case Game::Mods::VehicleWorkshopResult::Verified: return "Verified";
            case Game::Mods::VehicleWorkshopResult::Failed: return "Failed verification";
            case Game::Mods::VehicleWorkshopResult::Stale: return "Vehicle changed";
            default: return "Idle";
            }
        }

        [[nodiscard]] const char* VehicleClassName(int vehicleClass) noexcept
        {
            if (vehicleClass < 0 || vehicleClass >= static_cast<int>(Game::VehicleCatalogs::VehicleClassNames.size()))
                return "Unknown";
            return Game::VehicleCatalogs::VehicleClassNames[static_cast<std::size_t>(vehicleClass)];
        }

        [[nodiscard]] bool SupportsSlot(
            const Game::Mods::VehicleWorkshopSnapshot& snapshot,
            int modType) noexcept
        {
            return modType >= 0
                && modType < static_cast<int>(snapshot.modCounts.size())
                && snapshot.modCounts[static_cast<std::size_t>(modType)] > 0;
        }

        void RenderVehicleHeader(const Game::Mods::VehicleWorkshopSnapshot& snapshot) noexcept
        {
            ImGui::TextColored(V11Theme::Accent, "%s", snapshot.displayName.c_str());
            if (!snapshot.modelCode.empty())
            {
                ImGui::SameLine();
                ImGui::TextDisabled("[%s]", snapshot.modelCode.c_str());
            }

            if (ImGui::BeginTable("##vehicle_identity", 2, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("Class");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextDisabled("Plate");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(VehicleClassName(snapshot.vehicleClass));
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(snapshot.plate.empty() ? "---" : snapshot.plate.c_str());
                ImGui::EndTable();
            }
        }

        void RenderBackToHome() noexcept
        {
            if (ImGui::Button("< Vehicle Home", ImVec2(132.0f, 0.0f)))
                g_WorkshopPage = WorkshopPage::Home;
            ImGui::Separator();
        }

        void RenderHome(
            Game::Mods::VehicleModificationRuntime& modificationRuntime,
            const Game::Mods::VehicleWorkshopSnapshot& snapshot) noexcept
        {
            RenderVehicleHeader(snapshot);
            ImGui::SeparatorText("What do you want to change?");

            if (ImGui::BeginTable("##vehicle_editor_sections", 3, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                if (ImGui::Button("Performance", ImVec2(-1.0f, 42.0f)))
                    g_WorkshopPage = WorkshopPage::Performance;
                DescribeLastV11Item("Engine, brakes, transmission, suspension, armor and turbo.");

                ImGui::TableSetColumnIndex(1);
                if (ImGui::Button("Appearance", ImVec2(-1.0f, 42.0f)))
                    g_WorkshopPage = WorkshopPage::Appearance;
                DescribeLastV11Item("Window tint and license-plate style.");

                ImGui::TableSetColumnIndex(2);
                if (ImGui::Button("Customization", ImVec2(-1.0f, 42.0f)))
                    g_WorkshopPage = WorkshopPage::Customization;
                DescribeLastV11Item("Body parts, interior, wheels, lighting, tires and other supported vehicle mods.");

                ImGui::EndTable();
            }

            ImGui::TextDisabled("Performance");
            ImGui::SameLine();
            ImGui::TextUnformatted("Engine, brakes, transmission, suspension, armor, turbo");
            ImGui::TextDisabled("Appearance");
            ImGui::SameLine();
            ImGui::TextUnformatted("Window tint and plate style");
            ImGui::TextDisabled("Customization");
            ImGui::SameLine();
            ImGui::TextUnformatted("Body, interior, wheels, lights, tires and detailed mod slots");

            ImGui::SeparatorText("Quick Actions");
            if (ImGui::BeginTable("##vehicle_quick_service", 3, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (ImGui::Button("Repair", ImVec2(-1.0f, 0.0f)))
                    static_cast<void>(modificationRuntime.QueueRepair());
                DescribeLastV11Item("Repair the current vehicle.");

                ImGui::TableSetColumnIndex(1);
                if (ImGui::Button("Clean", ImVec2(-1.0f, 0.0f)))
                    static_cast<void>(modificationRuntime.QueueClean());
                DescribeLastV11Item("Remove dirt from the current vehicle.");

                ImGui::TableSetColumnIndex(2);
                if (ImGui::Button("Set Upright", ImVec2(-1.0f, 0.0f)))
                    static_cast<void>(modificationRuntime.QueueFlipUpright());
                DescribeLastV11Item("Place the current vehicle properly on the ground.");
                ImGui::EndTable();
            }

            if (snapshot.capabilitiesReady)
            {
                ImGui::SeparatorText("Vehicle Support");
                ImGui::TextDisabled(
                    "%d supported mod slots / %d available choices",
                    snapshot.supportedModSlots,
                    snapshot.availableModOptions);

                if (ImGui::BeginTable("##vehicle_support_summary", 3, ImGuiTableFlags_SizingStretchSame))
                {
                    const auto supportCell = [&](const char* name, bool supported) {
                        ImGui::TableNextColumn();
                        ImGui::Text("%s: %s", name, supported ? "Ready" : "N/A");
                    };

                    ImGui::TableNextRow();
                    supportCell("Performance", SupportsSlot(snapshot, 11)
                        || SupportsSlot(snapshot, 12)
                        || SupportsSlot(snapshot, 13)
                        || SupportsSlot(snapshot, 15)
                        || SupportsSlot(snapshot, 16)
                        || SupportsSlot(snapshot, Game::Mods::VehicleWorkshopRuntime::TurboSlot));
                    supportCell("Body", SupportsSlot(snapshot, 0)
                        || SupportsSlot(snapshot, 1)
                        || SupportsSlot(snapshot, 2)
                        || SupportsSlot(snapshot, 3)
                        || SupportsSlot(snapshot, 7));
                    supportCell("Wheels", SupportsSlot(snapshot, 23) || SupportsSlot(snapshot, 24));
                    ImGui::EndTable();
                }
            }
            else
            {
                ImGui::TextDisabled("Reading supported modifications for this vehicle...");
            }
        }

        void RenderPerformanceLevel(
            Game::Mods::VehicleWorkshopRuntime& runtime,
            const Game::Mods::VehicleWorkshopSnapshot& snapshot,
            int modType,
            const char* name) noexcept
        {
            const int count = snapshot.modCounts[static_cast<std::size_t>(modType)];
            const int current = snapshot.currentMods[static_cast<std::size_t>(modType)];

            ImGui::PushID(modType);
            ImGui::TextDisabled("%s", name);
            ImGui::SetNextItemWidth(-1.0f);

            if (count <= 0)
            {
                ImGui::BeginDisabled();
                int unsupported{};
                ImGui::Combo("##performance_level", &unsupported, "Not supported\0\0");
                ImGui::EndDisabled();
                ImGui::PopID();
                return;
            }

            std::string preview = current < 0
                ? "Stock"
                : "Level " + std::to_string(current + 1) + " / " + std::to_string(count);

            if (ImGui::BeginCombo("##performance_level", preview.c_str()))
            {
                const bool stockSelected = current < 0;
                if (ImGui::Selectable("Stock", stockSelected))
                    static_cast<void>(runtime.QueueSetPerformanceLevel(modType, -1));
                if (stockSelected)
                    ImGui::SetItemDefaultFocus();

                for (int index = 0; index < count; ++index)
                {
                    const std::string label = "Level " + std::to_string(index + 1);
                    const bool selected = current == index;
                    if (ImGui::Selectable(label.c_str(), selected))
                        static_cast<void>(runtime.QueueSetPerformanceLevel(modType, index));
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            DescribeLastV11Item("Apply a supported performance level and verify the installed value from GTA.");
            ImGui::PopID();
        }

        void RenderPerformance(
            Game::Mods::VehicleWorkshopRuntime& runtime,
            const Game::Mods::VehicleWorkshopSnapshot& snapshot) noexcept
        {
            RenderBackToHome();
            ImGui::TextColored(V11Theme::Accent, "Performance");
            ImGui::TextDisabled("Power, handling and protection upgrades");
            ImGui::Separator();

            for (std::size_t i = 0; i < Game::Mods::VehicleWorkshopRuntime::PerformanceSlots.size(); ++i)
            {
                RenderPerformanceLevel(
                    runtime,
                    snapshot,
                    Game::Mods::VehicleWorkshopRuntime::PerformanceSlots[i],
                    PerformanceNames[i]);
            }

            ImGui::TextDisabled("Turbo");
            bool turbo = snapshot.turbo;
            const bool turboSupported = SupportsSlot(snapshot, Game::Mods::VehicleWorkshopRuntime::TurboSlot);
            if (!turboSupported)
                ImGui::BeginDisabled();
            if (ImGui::Checkbox("Enabled##workshop_turbo", &turbo))
                static_cast<void>(runtime.QueueTurbo(turbo));
            if (!turboSupported)
                ImGui::EndDisabled();
            DescribeLastV11Item("Enable or disable turbo and verify the result from the current vehicle.");

            ImGui::Spacing();
            if (ImGui::BeginTable("##performance_package_actions", 2, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (ImGui::Button("Max Performance", ImVec2(-1.0f, 0.0f)))
                    static_cast<void>(runtime.QueueMaxPerformance());
                DescribeLastV11Item("Install the highest available performance levels and enable turbo.");

                ImGui::TableSetColumnIndex(1);
                if (ImGui::Button("Restore Stock", ImVec2(-1.0f, 0.0f)))
                    static_cast<void>(runtime.QueueStockPerformance());
                DescribeLastV11Item("Return supported performance upgrades and turbo to stock.");
                ImGui::EndTable();
            }

            ImGui::SeparatorText("Last Change");
            ImGui::Text("Action: %s", WorkshopActionName(snapshot.lastAction));
            ImGui::Text("Result: %s", WorkshopResultName(snapshot.lastResult));
            if (snapshot.lastSlot >= 0)
            {
                ImGui::Text("Requested: %d", snapshot.lastRequested);
                if (snapshot.lastObserved >= -1)
                    ImGui::Text("Installed: %d", snapshot.lastObserved);
            }
        }

        void RenderAppearance(Game::Vehicle vehicle) noexcept
        {
            RenderBackToHome();

            auto& runtime = Game::Mods::VehicleAppearanceRuntime::Get();
            runtime.RequestRefresh(vehicle);
            const auto snapshot = runtime.Snapshot();

            ImGui::TextColored(V11Theme::Accent, "Appearance");
            ImGui::TextDisabled("Glass and license-plate styling");
            ImGui::Separator();

            if (!snapshot.ready || snapshot.vehicle != vehicle)
            {
                ImGui::TextDisabled("Reading appearance from the current vehicle...");
                return;
            }

            const int currentTint = std::clamp(snapshot.windowTint, 0, static_cast<int>(WindowTintNames.size()) - 1);
            ImGui::TextDisabled("Window Tint");
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##workshop_window_tint", WindowTintNames[static_cast<std::size_t>(currentTint)]))
            {
                for (std::size_t i = 0; i < WindowTintNames.size(); ++i)
                {
                    const bool selected = static_cast<int>(i) == currentTint;
                    if (ImGui::Selectable(WindowTintNames[i], selected))
                        static_cast<void>(runtime.QueueWindowTint(vehicle, static_cast<int>(i)));
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            DescribeLastV11Item("Set the current vehicle's window tint and verify it by reading the value back from GTA.");

            const int currentPlate = std::clamp(snapshot.plateStyle, 0, static_cast<int>(PlateStyleNames.size()) - 1);
            ImGui::Spacing();
            ImGui::TextDisabled("Plate Style");
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##workshop_plate_style", PlateStyleNames[static_cast<std::size_t>(currentPlate)]))
            {
                for (std::size_t i = 0; i < PlateStyleNames.size(); ++i)
                {
                    const bool selected = static_cast<int>(i) == currentPlate;
                    if (ImGui::Selectable(PlateStyleNames[i], selected))
                        static_cast<void>(runtime.QueuePlateStyle(vehicle, static_cast<int>(i)));
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            DescribeLastV11Item("Set the current vehicle's number-plate background/style and verify it by read-back.");

            ImGui::SeparatorText("Status");
            if (snapshot.pending)
                ImGui::TextDisabled("%s", snapshot.message.c_str());
            else if (snapshot.haveResult)
                ImGui::TextWrapped("%s: %s", snapshot.lastSucceeded ? "Verified" : "Failed", snapshot.message.c_str());
            else
                ImGui::TextDisabled("Ready");
        }

        void RenderCustomizationReturnOverlay(const ImVec2& hostWindowPosition) noexcept
        {
            ImGui::SetNextWindowPos(
                ImVec2(hostWindowPosition.x + 226.0f + 342.0f, hostWindowPosition.y + 20.0f),
                ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(138.0f, 34.0f), ImGuiCond_Always);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 4.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 3.0f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, V11Theme::PanelBg);
            constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
                | ImGuiWindowFlags_NoSavedSettings
                | ImGuiWindowFlags_NoNav
                | ImGuiWindowFlags_NoMove;
            if (ImGui::Begin("##vehicle_customization_return", nullptr, flags))
            {
                if (ImGui::Button("Vehicle Home", ImVec2(-1.0f, 0.0f)))
                    g_WorkshopPage = WorkshopPage::Home;
            }
            ImGui::End();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);
        }
    }

    void RenderVehicleModificationPanel() noexcept
    {
        const ImVec2 hostWindowPosition = ImGui::GetWindowPos();
        if (g_WorkshopPage == WorkshopPage::Customization)
        {
            RenderLegacyVehicleModificationPanel();
            RenderCustomizationReturnOverlay(hostWindowPosition);
            return;
        }

        auto& modificationRuntime = Game::Mods::VehicleModificationRuntime::Get();
        auto& workshopRuntime = Game::Mods::VehicleWorkshopRuntime::Get();
        const auto gameState = Game::GameState::Get().Snapshot();
        const Game::Vehicle currentVehicle = gameState.nativeRuntimeReady && gameState.inVehicle
            ? gameState.vehicle
            : 0;

        workshopRuntime.RequestRefresh(currentVehicle);
        const auto workshop = workshopRuntime.Snapshot();

        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##vehicle_editor", ImVec2(490.0f, 430.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Vehicle Editor");
            ImGui::SameLine();
            ImGui::TextDisabled("simple vehicle customization");
            ImGui::Separator();

            if (!modificationRuntime.IsRunning())
            {
                ImGui::TextDisabled("Vehicle modification runtime is offline.");
            }
            else if (currentVehicle == 0)
            {
                ImGui::TextDisabled("Enter a vehicle to use the editor.");
            }
            else if (g_WorkshopPage == WorkshopPage::Appearance)
            {
                RenderAppearance(currentVehicle);
            }
            else if (!workshop.valid || workshop.vehicle != currentVehicle)
            {
                ImGui::TextDisabled("Reading the current vehicle...");
            }
            else if (g_WorkshopPage == WorkshopPage::Home)
            {
                RenderHome(modificationRuntime, workshop);
            }
            else
            {
                RenderPerformance(workshopRuntime, workshop);
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
