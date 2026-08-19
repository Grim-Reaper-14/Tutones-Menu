#include "VehicleModificationPanel.hpp"

#include "LscBypassWidget.hpp"
#include "../features/vehicle/VehicleModificationRuntime.hpp"
#include "../game/vehicle/VehicleCatalogs.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace Tutones::UI
{
    namespace
    {
        constexpr std::array<const char*, 50> ModNames{{
            "Spoiler", "Front Bumper", "Rear Bumper", "Side Skirt", "Exhaust",
            "Chassis", "Grille", "Hood / Bonnet", "Left Fender", "Right Fender",
            "Roof", "Engine", "Brakes", "Transmission", "Horns",
            "Suspension", "Armor", "Nitrous", "Turbo", "Subwoofer",
            "Tire Smoke", "Hydraulics", "Xenon", "Front Wheels", "Rear Wheels",
            "Plate Holder", "Vanity Plate", "Trim Design", "Ornaments", "Dashboard",
            "Dials", "Door Speakers", "Seats", "Steering Wheel", "Shift Lever",
            "Plaques", "Speakers / ICE", "Trunk", "Hydraulics 2", "Engine Block",
            "Air Filter", "Struts", "Arch Cover", "Aerials", "Trim",
            "Tank", "Windows", "Doors", "Livery", "Lightbar",
        }};

        constexpr std::array<const char*, 2> WheelAxleNames{{"Front Wheels", "Rear Wheels"}};
        constexpr std::array<const char*, 4> NeonSideNames{{"Left", "Right", "Front", "Back"}};

        int g_ModType{11};
        int g_ModIndex{};
        int g_WheelType{};
        int g_WheelAxle{};
        int g_WheelStyle{};
        bool g_CustomTires{};
        int g_LastVehicle{};
        int g_LastObserved{-1};
        int g_XenonColor{};
        int g_NeonPreset{};
        int g_SmokePreset{};
        float g_NeonRgb[3]{222.0f / 255.0f, 222.0f / 255.0f, 1.0f};
        float g_SmokeRgb[3]{1.0f, 1.0f, 1.0f};
        const char* g_WheelMessage{"Choose a wheel family, then a named wheel."};

        [[nodiscard]] bool IsToggleSlot(int modType) noexcept
        {
            return modType >= 17 && modType <= 22;
        }

        [[nodiscard]] int ToByte(float value) noexcept
        {
            return static_cast<int>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
        }

        void SyncFromSnapshot(const Game::Mods::VehicleModificationSnapshot& snapshot) noexcept
        {
            if (!snapshot.valid)
            {
                g_LastVehicle = 0;
                return;
            }

            if (snapshot.vehicle != g_LastVehicle)
            {
                g_LastVehicle = snapshot.vehicle;
                g_XenonColor = std::clamp(snapshot.xenonColor + 1, 0, 13);
                g_NeonRgb[0] = static_cast<float>(snapshot.neonRed) / 255.0f;
                g_NeonRgb[1] = static_cast<float>(snapshot.neonGreen) / 255.0f;
                g_NeonRgb[2] = static_cast<float>(snapshot.neonBlue) / 255.0f;
                g_SmokeRgb[0] = static_cast<float>(snapshot.tireSmokeRed) / 255.0f;
                g_SmokeRgb[1] = static_cast<float>(snapshot.tireSmokeGreen) / 255.0f;
                g_SmokeRgb[2] = static_cast<float>(snapshot.tireSmokeBlue) / 255.0f;
            }

            if (snapshot.observedModType == g_LastObserved)
                return;

            g_LastObserved = snapshot.observedModType;
            g_ModType = std::clamp(snapshot.observedModType, 0, 49);
            g_ModIndex = std::max(0, snapshot.currentMod);
            g_WheelStyle = std::max(0, snapshot.currentMod);
            g_WheelType = std::clamp(snapshot.wheelType, 0, 12);
            g_CustomTires = snapshot.customTires;
        }

        bool NamedModCombo(const char* label, const Game::Mods::VehicleModificationSnapshot& snapshot, int& index) noexcept
        {
            if (snapshot.modCount <= 0)
                return false;
            index = std::clamp(index, 0, snapshot.modCount - 1);
            const char* preview = index < static_cast<int>(snapshot.modDisplayNames.size())
                ? snapshot.modDisplayNames[static_cast<std::size_t>(index)].c_str()
                : "Choose option";
            bool changed = false;
            if (ImGui::BeginCombo(label, preview))
            {
                for (int i = 0; i < snapshot.modCount; ++i)
                {
                    const std::string fallback = "Option " + std::to_string(i + 1);
                    const char* name = i < static_cast<int>(snapshot.modDisplayNames.size())
                        ? snapshot.modDisplayNames[static_cast<std::size_t>(i)].c_str()
                        : fallback.c_str();
                    const bool selected = i == index;
                    if (ImGui::Selectable(name, selected))
                    {
                        index = i;
                        changed = true;
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            return changed;
        }

        void ApplyRgbPreset(const Game::VehicleCatalogs::RgbName& preset, float rgb[3]) noexcept
        {
            rgb[0] = static_cast<float>(preset.red) / 255.0f;
            rgb[1] = static_cast<float>(preset.green) / 255.0f;
            rgb[2] = static_cast<float>(preset.blue) / 255.0f;
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
            ImGui::TextUnformatted("LSC Vehicle Workshop");
            ImGui::Separator();

            if (!runtime.IsRunning())
                ImGui::TextDisabled("Vehicle modification runtime is offline.");
            else if (!snapshot.valid)
                ImGui::TextDisabled("Enter a vehicle to read and apply modifications.");
            else if (ImGui::BeginTabBar("##vehicle_mod_tabs"))
            {
                if (ImGui::BeginTabItem("LSC Mods"))
                {
                    if (ImGui::Combo("Mod slot", &g_ModType, ModNames.data(), static_cast<int>(ModNames.size())))
                    {
                        runtime.SetObservedModType(g_ModType);
                        g_LastObserved = -1;
                    }

                    ImGui::TextDisabled("Direct native workshop controls still enforce the vehicle-supported mod count.");
                    RenderLscBypassWidget();
                    ImGui::Text("Available: %d | Installed: %d", snapshot.modCount, snapshot.currentMod);

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
                        static_cast<void>(NamedModCombo("Option", snapshot, g_ModIndex));
                        if (g_ModType == 23 || g_ModType == 24)
                            ImGui::Checkbox("Custom tires", &g_CustomTires);

                        if (ImGui::Button("Apply", ImVec2(180.0f, 0.0f)))
                            static_cast<void>(runtime.QueueSetMod(g_ModType, g_ModIndex, g_CustomTires));
                        ImGui::SameLine();
                        if (ImGui::Button("Stock / remove", ImVec2(-1.0f, 0.0f)))
                            static_cast<void>(runtime.QueueRemoveMod(g_ModType));
                    }
                    else
                    {
                        ImGui::TextDisabled("This vehicle exposes no choices for this LSC slot.");
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

                    const char* wheelPreview = Game::VehicleCatalogs::WheelTypes[static_cast<std::size_t>(g_WheelType)].name;
                    if (ImGui::BeginCombo("Wheel family", wheelPreview))
                    {
                        for (const auto& wheelType : Game::VehicleCatalogs::WheelTypes)
                        {
                            const bool selected = wheelType.value == g_WheelType;
                            if (ImGui::Selectable(wheelType.name, selected))
                                g_WheelType = wheelType.value;
                            if (selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    if (ImGui::Button("Apply family / refresh wheel list", ImVec2(-1.0f, 0.0f)))
                    {
                        const bool queued = runtime.QueueWheelType(g_WheelType);
                        g_WheelMessage = queued ? "Wheel family queued; named styles will refresh." : "Wheel family rejected.";
                        runtime.SetObservedModType(wheelSlot);
                        g_LastObserved = -1;
                    }
                    ImGui::TextDisabled("%s", g_WheelMessage);

                    if (ImGui::Combo("Axle", &g_WheelAxle, WheelAxleNames.data(), static_cast<int>(WheelAxleNames.size())))
                    {
                        const int nextSlot = g_WheelAxle == 0 ? 23 : 24;
                        g_ModType = nextSlot;
                        runtime.SetObservedModType(nextSlot);
                        g_LastObserved = -1;
                    }

                    if (snapshot.observedModType == wheelSlot && snapshot.modCount > 0)
                    {
                        static_cast<void>(NamedModCombo("Wheel", snapshot, g_WheelStyle));
                        ImGui::Checkbox("Custom tire / whitewall variant", &g_CustomTires);
                        if (ImGui::Button("Apply wheel", ImVec2(180.0f, 0.0f)))
                            static_cast<void>(runtime.QueueSetMod(wheelSlot, g_WheelStyle, g_CustomTires));
                        ImGui::SameLine();
                        if (ImGui::Button("Stock wheels", ImVec2(-1.0f, 0.0f)))
                            static_cast<void>(runtime.QueueRemoveMod(wheelSlot));
                    }
                    else
                    {
                        ImGui::TextDisabled("Refreshing named wheel choices for this family/axle...");
                    }
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Lights / Tires"))
                {
                    bool xenon = snapshot.xenon;
                    if (ImGui::Checkbox("Xenon headlights", &xenon))
                        static_cast<void>(runtime.QueueToggleMod(22, xenon));

                    const int xenonPos = std::clamp(g_XenonColor, 0, static_cast<int>(Game::VehicleCatalogs::HeadlightColors.size()) - 1);
                    const char* xenonPreview = Game::VehicleCatalogs::HeadlightColors[static_cast<std::size_t>(xenonPos)].name;
                    if (ImGui::BeginCombo("Xenon color", xenonPreview))
                    {
                        for (std::size_t i = 0; i < Game::VehicleCatalogs::HeadlightColors.size(); ++i)
                        {
                            const bool selected = static_cast<int>(i) == g_XenonColor;
                            if (ImGui::Selectable(Game::VehicleCatalogs::HeadlightColors[i].name, selected))
                                g_XenonColor = static_cast<int>(i);
                            if (selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    if (ImGui::Button("Apply xenon color", ImVec2(-1.0f, 0.0f)))
                        static_cast<void>(runtime.QueueXenonColor(
                            Game::VehicleCatalogs::HeadlightColors[static_cast<std::size_t>(g_XenonColor)].value));

                    ImGui::Separator();
                    ImGui::TextUnformatted("Neon kit");
                    for (int i = 0; i < 4; ++i)
                    {
                        bool enabled = snapshot.neonEnabled[static_cast<std::size_t>(i)];
                        ImGui::PushID(i);
                        if (ImGui::Checkbox(NeonSideNames[static_cast<std::size_t>(i)], &enabled))
                            static_cast<void>(runtime.QueueNeonEnabled(i, enabled));
                        if (i != 3) ImGui::SameLine();
                        ImGui::PopID();
                    }
                    g_NeonPreset = std::clamp(g_NeonPreset, 0, static_cast<int>(Game::VehicleCatalogs::NeonColors.size()) - 1);
                    if (ImGui::BeginCombo("Neon preset", Game::VehicleCatalogs::NeonColors[static_cast<std::size_t>(g_NeonPreset)].name))
                    {
                        for (std::size_t i = 0; i < Game::VehicleCatalogs::NeonColors.size(); ++i)
                        {
                            const bool selected = static_cast<int>(i) == g_NeonPreset;
                            if (ImGui::Selectable(Game::VehicleCatalogs::NeonColors[i].name, selected))
                            {
                                g_NeonPreset = static_cast<int>(i);
                                ApplyRgbPreset(Game::VehicleCatalogs::NeonColors[i], g_NeonRgb);
                            }
                            if (selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::ColorEdit3("Custom neon RGB", g_NeonRgb, ImGuiColorEditFlags_NoAlpha);
                    if (ImGui::Button("Apply neon color", ImVec2(-1.0f, 0.0f)))
                        static_cast<void>(runtime.QueueNeonColor(ToByte(g_NeonRgb[0]), ToByte(g_NeonRgb[1]), ToByte(g_NeonRgb[2])));

                    ImGui::Separator();
                    bool smokeEnabled = snapshot.tireSmoke;
                    if (ImGui::Checkbox("Tire smoke", &smokeEnabled))
                        static_cast<void>(runtime.QueueToggleMod(20, smokeEnabled));
                    g_SmokePreset = std::clamp(g_SmokePreset, 0, static_cast<int>(Game::VehicleCatalogs::TireSmokeColors.size()) - 1);
                    if (ImGui::BeginCombo("Smoke preset", Game::VehicleCatalogs::TireSmokeColors[static_cast<std::size_t>(g_SmokePreset)].name))
                    {
                        for (std::size_t i = 0; i < Game::VehicleCatalogs::TireSmokeColors.size(); ++i)
                        {
                            const bool selected = static_cast<int>(i) == g_SmokePreset;
                            if (ImGui::Selectable(Game::VehicleCatalogs::TireSmokeColors[i].name, selected))
                            {
                                g_SmokePreset = static_cast<int>(i);
                                ApplyRgbPreset(Game::VehicleCatalogs::TireSmokeColors[i], g_SmokeRgb);
                            }
                            if (selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::ColorEdit3("Custom smoke RGB", g_SmokeRgb, ImGuiColorEditFlags_NoAlpha);
                    if (ImGui::Button("Apply tire smoke", ImVec2(-1.0f, 0.0f)))
                        static_cast<void>(runtime.QueueTireSmokeColor(ToByte(g_SmokeRgb[0]), ToByte(g_SmokeRgb[1]), ToByte(g_SmokeRgb[2])));

                    bool bulletproof = !snapshot.tyresCanBurst;
                    if (ImGui::Checkbox("Bulletproof tires", &bulletproof))
                        static_cast<void>(runtime.QueueTyresCanBurst(!bulletproof));
                    ImGui::SameLine();
                    bool lowGrip = snapshot.driftTyres;
                    if (ImGui::Checkbox("Low grip / drift tires", &lowGrip))
                        static_cast<void>(runtime.QueueDriftTyres(lowGrip));
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Status"))
                {
                    ImGui::Text("Vehicle: %d", snapshot.vehicle);
                    ImGui::Text("Slot: %d - %s", snapshot.observedModType, ModNames[static_cast<std::size_t>(snapshot.observedModType)]);
                    ImGui::Text("Count / installed: %d / %d", snapshot.modCount, snapshot.currentMod);
                    ImGui::Text("Wheel type: %d", snapshot.wheelType);
                    ImGui::Text("Turbo: %s", snapshot.turbo ? "on" : "off");
                    ImGui::Text("Tire smoke RGB: %d, %d, %d", snapshot.tireSmokeRed, snapshot.tireSmokeGreen, snapshot.tireSmokeBlue);
                    ImGui::Text("Xenon: %s / color %d", snapshot.xenon ? "on" : "off", snapshot.xenonColor);
                    ImGui::Text("Neon RGB: %d, %d, %d", snapshot.neonRed, snapshot.neonGreen, snapshot.neonBlue);
                    ImGui::Text("Tires can burst: %s", snapshot.tyresCanBurst ? "yes" : "no");
                    ImGui::Text("Low grip: %s", snapshot.driftTyres ? "yes" : "no");
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
