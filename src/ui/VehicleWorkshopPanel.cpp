#include "VehicleModificationPanel.hpp"

#include "V11Description.hpp"
#include "V11Theme.hpp"
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
            Overview,
            Performance,
            Advanced,
        };

        constexpr std::array<const char*, 5> PerformanceNames{{
            "Engine",
            "Brakes",
            "Transmission",
            "Suspension",
            "Armor",
        }};

        WorkshopPage g_WorkshopPage{WorkshopPage::Overview};

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

        void DrawWorkshopNavigation() noexcept
        {
            constexpr std::array<const char*, 3> labels{{"Overview", "Performance", "Advanced"}};
            if (!ImGui::BeginTable("##vehicle_workshop_navigation", 3, ImGuiTableFlags_SizingStretchSame))
                return;

            ImGui::TableNextRow();
            for (int i = 0; i < static_cast<int>(labels.size()); ++i)
            {
                ImGui::TableSetColumnIndex(i);
                const bool active = static_cast<int>(g_WorkshopPage) == i;
                if (active)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, V11Theme::AccentDark);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, V11Theme::AccentHover);
                }

                if (ImGui::Button(labels[static_cast<std::size_t>(i)], ImVec2(-1.0f, 0.0f)))
                    g_WorkshopPage = static_cast<WorkshopPage>(i);

                if (active)
                    ImGui::PopStyleColor(2);
            }
            ImGui::EndTable();
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

        void RenderOverview(
            Game::Mods::VehicleModificationRuntime& modificationRuntime,
            const Game::Mods::VehicleWorkshopSnapshot& snapshot) noexcept
        {
            RenderVehicleHeader(snapshot);

            ImGui::SeparatorText("Workshop Capability");
            if (!snapshot.capabilitiesReady)
            {
                ImGui::TextDisabled("Scanning this vehicle's supported modification slots...");
                return;
            }

            if (ImGui::BeginTable("##vehicle_capability_summary", 2, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("Supported slots");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextDisabled("Available choices");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%d / 50", snapshot.supportedModSlots);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%d", snapshot.availableModOptions);
                ImGui::EndTable();
            }

            ImGui::Spacing();
            if (ImGui::BeginTable("##vehicle_capability_flags", 3, ImGuiTableFlags_SizingStretchSame))
            {
                const auto capabilityCell = [&](const char* name, bool supported) {
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("%s", name);
                    ImGui::SameLine();
                    ImGui::TextColored(
                        supported ? V11Theme::Accent : V11Theme::MutedText,
                        "%s",
                        supported ? "Ready" : "N/A");
                };

                ImGui::TableNextRow();
                capabilityCell("Performance", SupportsSlot(snapshot, 11)
                    || SupportsSlot(snapshot, 12)
                    || SupportsSlot(snapshot, 13)
                    || SupportsSlot(snapshot, 15)
                    || SupportsSlot(snapshot, 16)
                    || SupportsSlot(snapshot, Game::Mods::VehicleWorkshopRuntime::TurboSlot));
                capabilityCell("Body", SupportsSlot(snapshot, 0)
                    || SupportsSlot(snapshot, 1)
                    || SupportsSlot(snapshot, 2)
                    || SupportsSlot(snapshot, 3)
                    || SupportsSlot(snapshot, 7));
                capabilityCell("Wheels", SupportsSlot(snapshot, 23) || SupportsSlot(snapshot, 24));

                ImGui::TableNextRow();
                capabilityCell("Interior", SupportsSlot(snapshot, 27)
                    || SupportsSlot(snapshot, 28)
                    || SupportsSlot(snapshot, 29)
                    || SupportsSlot(snapshot, 32)
                    || SupportsSlot(snapshot, 33));
                capabilityCell("Livery", SupportsSlot(snapshot, 48));
                capabilityCell("Lightbar", SupportsSlot(snapshot, 49));
                ImGui::EndTable();
            }

            ImGui::SeparatorText("Quick Service");
            if (ImGui::BeginTable("##vehicle_quick_service", 3, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (ImGui::Button("Repair", ImVec2(-1.0f, 0.0f)))
                    static_cast<void>(modificationRuntime.QueueRepair());
                DescribeLastV11Item("Repair the current vehicle using the managed vehicle runtime.");

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

            ImGui::TextWrapped(
                "The capability map is read from the current vehicle when you enter it. "
                "Unsupported workshop controls can now be hidden or disabled instead of guessing by vehicle type.");
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
            RenderVehicleHeader(snapshot);
            ImGui::SeparatorText("Performance Package");

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
                DescribeLastV11Item("Install the highest available engine, brake, transmission, suspension and armor levels, then enable turbo.");

                ImGui::TableSetColumnIndex(1);
                if (ImGui::Button("Restore Stock", ImVec2(-1.0f, 0.0f)))
                    static_cast<void>(runtime.QueueStockPerformance());
                DescribeLastV11Item("Return supported performance slots and turbo to stock state.");
                ImGui::EndTable();
            }

            ImGui::SeparatorText("Last Performance Change");
            ImGui::Text("Action: %s", WorkshopActionName(snapshot.lastAction));
            ImGui::Text("Result: %s", WorkshopResultName(snapshot.lastResult));
            if (snapshot.lastSlot >= 0)
            {
                ImGui::Text("Slot: %d", snapshot.lastSlot);
                ImGui::Text("Requested: %d", snapshot.lastRequested);
                if (snapshot.lastObserved >= -1)
                    ImGui::Text("Observed: %d", snapshot.lastObserved);
            }
            ImGui::TextWrapped("Package actions verify every supported performance slot after writing it. A vehicle switch is rejected as stale instead of modifying the wrong vehicle.");
        }

        void RenderAdvancedReturnOverlay(const ImVec2& hostWindowPosition) noexcept
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
            if (ImGui::Begin("##vehicle_workshop_return", nullptr, flags))
            {
                if (ImGui::Button("Workshop", ImVec2(-1.0f, 0.0f)))
                    g_WorkshopPage = WorkshopPage::Overview;
            }
            ImGui::End();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);
        }
    }

    void RenderVehicleModificationPanel() noexcept
    {
        const ImVec2 hostWindowPosition = ImGui::GetWindowPos();
        if (g_WorkshopPage == WorkshopPage::Advanced)
        {
            RenderLegacyVehicleModificationPanel();
            RenderAdvancedReturnOverlay(hostWindowPosition);
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

        if (ImGui::BeginChild("##vehicle_workshop", ImVec2(490.0f, 430.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Vehicle Workshop");
            ImGui::SameLine();
            ImGui::TextDisabled("capability-aware editor");
            ImGui::Separator();
            DrawWorkshopNavigation();
            ImGui::Separator();

            if (!modificationRuntime.IsRunning())
            {
                ImGui::TextDisabled("Vehicle modification runtime is offline.");
            }
            else if (currentVehicle == 0)
            {
                ImGui::TextDisabled("Enter a vehicle to open the workshop.");
            }
            else if (!workshop.valid || workshop.vehicle != currentVehicle)
            {
                ImGui::TextDisabled("Scanning the current vehicle and building its capability map...");
            }
            else if (g_WorkshopPage == WorkshopPage::Overview)
            {
                RenderOverview(modificationRuntime, workshop);
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
