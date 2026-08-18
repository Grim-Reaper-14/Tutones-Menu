#include "VehicleModificationPanel.hpp"

#include "../features/vehicle/VehicleModificationRuntime.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstddef>

namespace Tutones::UI
{
    namespace
    {
        constexpr std::array<const char*, 50> ModNames{{
            "Spoiler", "Front Bumper", "Rear Bumper", "Side Skirt", "Exhaust",
            "Chassis", "Grille", "Hood / Bonnet", "Left Fender", "Right Fender",
            "Roof", "Engine", "Brakes", "Transmission", "Horn",
            "Suspension", "Armor", "Nitrous", "Turbo", "Subwoofer",
            "Tire Smoke", "Hydraulics", "Xenon", "Front Wheels", "Rear Wheels",
            "Plate Holder", "Vanity Plate", "Interior 1", "Interior 2", "Interior 3",
            "Interior 4", "Interior 5", "Seats", "Steering Wheel", "Shift Lever",
            "Plaques", "ICE", "Trunk", "Hydraulics 2", "Engine Bay 1",
            "Engine Bay 2", "Engine Bay 3", "Chassis 2", "Chassis 3", "Chassis 4",
            "Chassis 5", "Door Left", "Door Right", "Livery", "Lightbar",
        }};

        constexpr std::array<const char*, 13> WheelTypeNames{{
            "Sport", "Muscle", "Lowrider", "SUV", "Offroad", "Tuner", "Bike Wheels",
            "High End", "Bennys Original", "Bennys Bespoke", "Open Wheel", "Street", "Track",
        }};

        constexpr std::array<const char*, 2> WheelAxleNames{{"Front Wheels", "Rear Wheels"}};

        int g_ModType{11};
        int g_ModIndex{};
        int g_WheelType{};
        int g_WheelAxle{};
        int g_WheelStyle{};
        bool g_CustomTires{};
        int g_LastVehicle{};
        int g_LastObserved{-1};
        const char* g_WheelMessage{"Choose a wheel family, then a wheel style."};

        [[nodiscard]] bool IsToggleSlot(int modType) noexcept
        {
            return modType >= 17 && modType <= 22;
        }

        void SyncFromSnapshot(const Game::Mods::VehicleModificationSnapshot& snapshot) noexcept
        {
            if (!snapshot.valid)
            {
                g_LastVehicle = 0;
                return;
            }

            if (snapshot.vehicle == g_LastVehicle && snapshot.observedModType == g_LastObserved)
                return;

            g_LastVehicle = snapshot.vehicle;
            g_LastObserved = snapshot.observedModType;
            g_ModType = std::clamp(snapshot.observedModType, 0, 49);
            g_ModIndex = std::max(0, snapshot.currentMod);
            g_WheelStyle = std::max(0, snapshot.currentMod);
            g_WheelType = std::clamp(snapshot.wheelType, 0, 12);
            g_CustomTires = snapshot.customTires;
        }
    }

    void RenderVehicleModificationPanel() noexcept
    {
        auto& runtime = Game::Mods::VehicleModificationRuntime::Get();
        runtime.SetObservedModType(g_ModType);
        const auto snapshot = runtime.Snapshot();
        SyncFromSnapshot(snapshot);

        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(24.0f / 255.0f, 24.0f / 255.0f, 26.0f / 255.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.04f));

        if (ImGui::BeginChild("##vehicle_modifications", ImVec2(490.0f, 430.0f), true))
        {
            ImGui::TextUnformatted("Vehicle Modifications");
            ImGui::Separator();

            if (!runtime.IsRunning())
                ImGui::TextDisabled("Vehicle modification runtime is offline.");
            else if (!snapshot.valid)
                ImGui::TextDisabled("Enter a vehicle to read and apply modifications.");
            else if (ImGui::BeginTabBar("##vehicle_mod_tabs"))
            {
                if (ImGui::BeginTabItem("Mods"))
                {
                    if (ImGui::Combo("Mod slot", &g_ModType, ModNames.data(), static_cast<int>(ModNames.size())))
                    {
                        runtime.SetObservedModType(g_ModType);
                        g_LastObserved = -1;
                    }

                    ImGui::Text("Available: %d", snapshot.modCount);
                    ImGui::Text("Installed index: %d", snapshot.currentMod);

                    if (IsToggleSlot(g_ModType))
                    {
                        bool enabled = false;
                        if (g_ModType == 18) enabled = snapshot.turbo;
                        else if (g_ModType == 20) enabled = snapshot.tireSmoke;
                        else if (g_ModType == 22) enabled = snapshot.xenon;

                        if (g_ModType == 18 || g_ModType == 20 || g_ModType == 22)
                        {
                            if (ImGui::Checkbox("Enabled", &enabled))
                                static_cast<void>(runtime.QueueToggleMod(g_ModType, enabled));
                        }
                        else
                        {
                            if (ImGui::Button("Enable", ImVec2(120.0f, 0.0f)))
                                static_cast<void>(runtime.QueueToggleMod(g_ModType, true));
                            ImGui::SameLine();
                            if (ImGui::Button("Disable", ImVec2(120.0f, 0.0f)))
                                static_cast<void>(runtime.QueueToggleMod(g_ModType, false));
                        }
                    }
                    else if (snapshot.modCount > 0)
                    {
                        g_ModIndex = std::clamp(g_ModIndex, 0, snapshot.modCount - 1);
                        ImGui::SliderInt("Mod index", &g_ModIndex, 0, snapshot.modCount - 1);
                        if (g_ModType == 23 || g_ModType == 24)
                            ImGui::Checkbox("Custom tires", &g_CustomTires);

                        if (ImGui::Button("Apply mod", ImVec2(180.0f, 0.0f)))
                            static_cast<void>(runtime.QueueSetMod(g_ModType, g_ModIndex, g_CustomTires));
                        ImGui::SameLine();
                        if (ImGui::Button("Stock / remove", ImVec2(-1.0f, 0.0f)))
                            static_cast<void>(runtime.QueueRemoveMod(g_ModType));
                    }
                    else
                    {
                        ImGui::TextDisabled("This vehicle exposes no choices for the selected slot.");
                    }
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Wheels"))
                {
                    const int wheelSlot = g_WheelAxle == 0 ? 23 : 24;
                    if (g_ModType != wheelSlot)
                    {
                        g_ModType = wheelSlot;
                        runtime.SetObservedModType(wheelSlot);
                        g_LastObserved = -1;
                    }

                    ImGui::TextUnformatted("Wheel family");
                    ImGui::Combo("Category", &g_WheelType, WheelTypeNames.data(), static_cast<int>(WheelTypeNames.size()));
                    if (ImGui::Button("Apply wheel family", ImVec2(-1.0f, 0.0f)))
                    {
                        const bool queued = runtime.QueueWheelType(g_WheelType);
                        g_WheelMessage = queued ? "Wheel family queued; refreshing styles." : "Wheel family rejected.";
                        runtime.SetObservedModType(wheelSlot);
                        g_LastObserved = -1;
                    }
                    ImGui::TextDisabled("%s", g_WheelMessage);

                    ImGui::Separator();
                    if (ImGui::Combo("Axle", &g_WheelAxle, WheelAxleNames.data(), static_cast<int>(WheelAxleNames.size())))
                    {
                        const int nextSlot = g_WheelAxle == 0 ? 23 : 24;
                        g_ModType = nextSlot;
                        runtime.SetObservedModType(nextSlot);
                        g_LastObserved = -1;
                    }

                    if (snapshot.observedModType != wheelSlot)
                    {
                        ImGui::TextDisabled("Refreshing wheel choices...");
                    }
                    else if (snapshot.modCount <= 0)
                    {
                        ImGui::TextDisabled("This vehicle exposes no wheel styles for this axle/category.");
                    }
                    else
                    {
                        g_WheelStyle = std::clamp(g_WheelStyle, 0, snapshot.modCount - 1);
                        ImGui::Text("Available styles: %d", snapshot.modCount);
                        ImGui::SliderInt("Wheel style", &g_WheelStyle, 0, snapshot.modCount - 1);
                        ImGui::Checkbox("Custom tires", &g_CustomTires);

                        if (ImGui::Button("Apply wheel style", ImVec2(180.0f, 0.0f)))
                            static_cast<void>(runtime.QueueSetMod(wheelSlot, g_WheelStyle, g_CustomTires));
                        ImGui::SameLine();
                        if (ImGui::Button("Stock wheels", ImVec2(-1.0f, 0.0f)))
                            static_cast<void>(runtime.QueueRemoveMod(wheelSlot));
                    }

                    ImGui::Separator();
                    bool turbo = snapshot.turbo;
                    bool tireSmoke = snapshot.tireSmoke;
                    bool xenon = snapshot.xenon;
                    if (ImGui::Checkbox("Turbo", &turbo))
                        static_cast<void>(runtime.QueueToggleMod(18, turbo));
                    if (ImGui::Checkbox("Tire smoke", &tireSmoke))
                        static_cast<void>(runtime.QueueToggleMod(20, tireSmoke));
                    if (ImGui::Checkbox("Xenon", &xenon))
                        static_cast<void>(runtime.QueueToggleMod(22, xenon));
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Status"))
                {
                    ImGui::Text("Vehicle: %d", snapshot.vehicle);
                    ImGui::Text("Slot: %d - %s", snapshot.observedModType, ModNames[static_cast<std::size_t>(snapshot.observedModType)]);
                    ImGui::Text("Count: %d", snapshot.modCount);
                    ImGui::Text("Current index: %d", snapshot.currentMod);
                    ImGui::Text("Wheel family: %d - %s", snapshot.wheelType, WheelTypeNames[static_cast<std::size_t>(snapshot.wheelType)]);
                    ImGui::Text("Turbo: %s", snapshot.turbo ? "on" : "off");
                    ImGui::Text("Tire smoke: %s", snapshot.tireSmoke ? "on" : "off");
                    ImGui::Text("Xenon: %s", snapshot.xenon ? "on" : "off");
                    if (snapshot.lastAction == Game::Mods::VehicleModAction::None)
                        ImGui::TextDisabled("No modification action has run yet.");
                    else if (snapshot.lastActionRejectedAsStale)
                        ImGui::TextDisabled("Last action was dropped because the vehicle changed.");
                    else
                        ImGui::Text("Last action: %s", snapshot.lastActionSucceeded ? "success" : "failed");
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
