#pragma once

#include "LscBypassWidget.hpp"
#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/vehicle/VehicleModificationRuntime.hpp"
#include "../features/vehicle/VehiclePaintRuntime.hpp"
#include "../game/vehicle/VehicleCatalogs.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <string>

namespace Tutones::UI
{
    namespace VehicleCustomizationV2Detail
    {
        using Game::Paint::PaintPalette;
        using Game::Paint::RgbColor;
        using Game::VehicleCatalogs::IndexedName;

        inline constexpr std::array<const char*, 50> ModNames{{
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

        inline constexpr std::array<PaintPalette, 7> PaintPalettes{{
            PaintPalette::Chrome,
            PaintPalette::Classic,
            PaintPalette::Matte,
            PaintPalette::Metals,
            PaintPalette::Utility,
            PaintPalette::Worn,
            PaintPalette::Chameleon,
        }};

        inline constexpr std::array<const char*, 7> PaintPaletteNames{{
            "Chrome", "Classic", "Matte", "Metals", "Utility", "Worn", "Chameleon",
        }};

        inline int g_ModType{11};
        inline int g_ModIndex{};
        inline int g_LastModVehicle{};
        inline int g_LastObserved{-1};
        inline int g_LastCurrentMod{std::numeric_limits<int>::min()};
        inline int g_LastWheelType{std::numeric_limits<int>::min()};
        inline bool g_CustomTires{};

        inline int g_PrimaryPalette{1};
        inline int g_SecondaryPalette{1};
        inline int g_PrimaryColor{};
        inline int g_SecondaryColor{};
        inline int g_Pearlescent{};
        inline int g_WheelColor{};
        inline int g_WheelColorFamily{};
        inline float g_CustomPrimary[3]{};
        inline float g_CustomSecondary[3]{};
        inline Game::Paint::VehicleHandle g_LastPaintVehicle{};

        inline int g_XenonColorIndex{};
        inline float g_NeonRgb[3]{222.0f / 255.0f, 222.0f / 255.0f, 1.0f};
        inline float g_SmokeRgb[3]{1.0f, 1.0f, 1.0f};
        inline std::string g_ActionMessage{"Ready"};

        [[nodiscard]] inline bool IsToggleSlot(int modType) noexcept
        {
            return modType == 18 || modType == 20 || modType == 22;
        }

        [[nodiscard]] inline int ToByte(float value) noexcept
        {
            return static_cast<int>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
        }

        [[nodiscard]] inline RgbColor ToRgb(const float color[3]) noexcept
        {
            return {
                static_cast<std::uint8_t>(ToByte(color[0])),
                static_cast<std::uint8_t>(ToByte(color[1])),
                static_cast<std::uint8_t>(ToByte(color[2])),
            };
        }

        [[nodiscard]] inline bool ContainsColor(std::span<const IndexedName> colors, int value) noexcept
        {
            for (const auto& entry : colors)
            {
                if (entry.value == value)
                    return true;
            }
            return false;
        }

        [[nodiscard]] inline int PaletteIndexFromColor(int value) noexcept
        {
            for (std::size_t i = 0; i < PaintPalettes.size(); ++i)
            {
                if (ContainsColor(Game::VehicleCatalogs::ColorsForPalette(PaintPalettes[i]), value))
                    return static_cast<int>(i);
            }
            return 1;
        }

        [[nodiscard]] inline const char* ColorPreview(std::span<const IndexedName> colors, int value) noexcept
        {
            for (const auto& entry : colors)
            {
                if (entry.value == value)
                    return entry.name;
            }
            return "Choose color";
        }

        inline bool ColorCombo(const char* id, std::span<const IndexedName> colors, int& value) noexcept
        {
            bool changed = false;
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo(id, ColorPreview(colors, value)))
            {
                for (const auto& entry : colors)
                {
                    const bool selected = entry.value == value;
                    if (ImGui::Selectable(entry.name, selected))
                    {
                        value = entry.value;
                        changed = true;
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            return changed;
        }

        inline void ResetColorForPalette(PaintPalette palette, int& color) noexcept
        {
            const auto colors = Game::VehicleCatalogs::ColorsForPalette(palette);
            if (!colors.empty())
                color = colors.front().value;
        }

        inline void SyncModificationState(const Game::Mods::VehicleModificationSnapshot& snapshot) noexcept
        {
            if (!snapshot.valid)
            {
                g_LastModVehicle = 0;
                g_LastObserved = -1;
                g_LastCurrentMod = std::numeric_limits<int>::min();
                g_LastWheelType = std::numeric_limits<int>::min();
                return;
            }

            if (snapshot.vehicle != g_LastModVehicle)
            {
                g_LastModVehicle = snapshot.vehicle;
                g_LastObserved = -1;
                g_LastCurrentMod = std::numeric_limits<int>::min();
                g_LastWheelType = std::numeric_limits<int>::min();
                g_CustomTires = snapshot.customTires;
                g_XenonColorIndex = std::clamp(snapshot.xenonColor + 1, 0, static_cast<int>(Game::VehicleCatalogs::HeadlightColors.size()) - 1);
                g_NeonRgb[0] = static_cast<float>(snapshot.neonRed) / 255.0f;
                g_NeonRgb[1] = static_cast<float>(snapshot.neonGreen) / 255.0f;
                g_NeonRgb[2] = static_cast<float>(snapshot.neonBlue) / 255.0f;
                g_SmokeRgb[0] = static_cast<float>(snapshot.tireSmokeRed) / 255.0f;
                g_SmokeRgb[1] = static_cast<float>(snapshot.tireSmokeGreen) / 255.0f;
                g_SmokeRgb[2] = static_cast<float>(snapshot.tireSmokeBlue) / 255.0f;
            }

            if (snapshot.wheelType != g_LastWheelType)
                g_LastWheelType = snapshot.wheelType;

            if (snapshot.observedModType == g_ModType
                && (snapshot.observedModType != g_LastObserved || snapshot.currentMod != g_LastCurrentMod))
            {
                g_LastObserved = snapshot.observedModType;
                g_LastCurrentMod = snapshot.currentMod;
                g_ModIndex = std::max(0, snapshot.currentMod);
                g_CustomTires = snapshot.customTires;
            }
        }

        inline void SyncPaintState(const Game::Paint::PaintServiceSnapshot& snapshot) noexcept
        {
            const auto& paint = snapshot.paint;
            if (!paint.valid)
            {
                g_LastPaintVehicle = 0;
                return;
            }
            if (paint.vehicle == g_LastPaintVehicle)
                return;

            g_LastPaintVehicle = paint.vehicle;
            g_PrimaryPalette = PaletteIndexFromColor(paint.primaryColor);
            g_SecondaryPalette = PaletteIndexFromColor(paint.secondaryColor);
            g_PrimaryColor = paint.primaryColor;
            g_SecondaryColor = paint.secondaryColor;
            g_Pearlescent = paint.pearlescentColor;
            g_WheelColor = paint.wheelColor;
            g_WheelColorFamily = paint.wheelColor >= 161 ? 1 : 0;
            g_CustomPrimary[0] = static_cast<float>(paint.customPrimary.red) / 255.0f;
            g_CustomPrimary[1] = static_cast<float>(paint.customPrimary.green) / 255.0f;
            g_CustomPrimary[2] = static_cast<float>(paint.customPrimary.blue) / 255.0f;
            g_CustomSecondary[0] = static_cast<float>(paint.customSecondary.red) / 255.0f;
            g_CustomSecondary[1] = static_cast<float>(paint.customSecondary.green) / 255.0f;
            g_CustomSecondary[2] = static_cast<float>(paint.customSecondary.blue) / 255.0f;
        }

        inline void RenderQuickPerformance(
            Game::Mods::VehicleModificationRuntime& runtime,
            const Game::Mods::VehicleModificationSnapshot& snapshot) noexcept
        {
            ImGui::SeparatorText("Quick Performance");

            if (ImGui::BeginTable("##vehicle_quick_performance", 2, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                bool turbo = snapshot.turbo;
                if (ImGui::Checkbox("Turbo", &turbo))
                    g_ActionMessage = runtime.QueueToggleMod(18, turbo) ? "Turbo change queued" : "Turbo change rejected";
                DescribeLastV11Item("Enable or disable the current vehicle's turbo modification.");

                ImGui::TableSetColumnIndex(1);
                if (ImGui::Button("Max Vehicle", ImVec2(-1.0f, 0.0f)))
                    g_ActionMessage = runtime.QueueMaxVehicle() ? "Max Vehicle queued" : "Max Vehicle rejected";
                DescribeLastV11Item("Apply the runtime's supported maximum vehicle upgrade package.");
                ImGui::EndTable();
            }
        }

        inline void RenderModSlots(
            Game::Mods::VehicleModificationRuntime& runtime,
            const Game::Mods::VehicleModificationSnapshot& snapshot) noexcept
        {
            ImGui::SeparatorText("Mod Slots");
            ImGui::TextDisabled("Body, performance, interior and wheel slots all live in this one editor.");
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::Combo("##vehicle_mod_slot_v2", &g_ModType, ModNames.data(), static_cast<int>(ModNames.size())))
            {
                runtime.SetObservedModType(g_ModType);
                g_LastObserved = -1;
                g_LastCurrentMod = std::numeric_limits<int>::min();
            }
            DescribeLastV11Item("Choose any GTA vehicle modification slot without leaving the Customization page.");

            if (snapshot.observedModType != g_ModType)
            {
                ImGui::TextDisabled("Refreshing %s...", ModNames[static_cast<std::size_t>(g_ModType)]);
                return;
            }

            ImGui::TextDisabled("Available: %d   Installed: %d", snapshot.modCount, snapshot.currentMod);

            if (IsToggleSlot(g_ModType))
            {
                bool enabled = g_ModType == 18 ? snapshot.turbo : (g_ModType == 20 ? snapshot.tireSmoke : snapshot.xenon);
                if (ImGui::Checkbox("Enabled", &enabled))
                    g_ActionMessage = runtime.QueueToggleMod(g_ModType, enabled) ? "Toggle queued" : "Toggle rejected";
                DescribeLastV11Item("Toggle the selected GTA modification slot.");
                return;
            }

            if (snapshot.modCount <= 0)
            {
                ImGui::TextDisabled("This vehicle has no options for the selected slot.");
                return;
            }

            g_ModIndex = std::clamp(g_ModIndex, 0, snapshot.modCount - 1);
            const char* preview = g_ModIndex < static_cast<int>(snapshot.modDisplayNames.size())
                ? snapshot.modDisplayNames[static_cast<std::size_t>(g_ModIndex)].c_str()
                : "Choose option";

            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##vehicle_mod_choice_v2", preview))
            {
                for (int i = 0; i < snapshot.modCount; ++i)
                {
                    const std::string fallback = "Option " + std::to_string(i + 1);
                    const char* name = i < static_cast<int>(snapshot.modDisplayNames.size())
                        ? snapshot.modDisplayNames[static_cast<std::size_t>(i)].c_str()
                        : fallback.c_str();
                    const bool selected = i == g_ModIndex;
                    if (ImGui::Selectable(name, selected))
                        g_ModIndex = i;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            DescribeLastV11Item("Choose the installed option for the selected vehicle modification slot.");

            if (g_ModType == 23 || g_ModType == 24)
            {
                ImGui::Checkbox("Custom tire / whitewall variant", &g_CustomTires);
                DescribeLastV11Item("Use GTA's custom-tire variation for the selected front or rear wheel style.");
            }

            if (ImGui::BeginTable("##vehicle_mod_actions_v2", 2, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (ImGui::Button("Apply Selected", ImVec2(-1.0f, 0.0f)))
                    g_ActionMessage = runtime.QueueSetMod(g_ModType, g_ModIndex, g_CustomTires) ? "Modification queued" : "Modification rejected";
                DescribeLastV11Item("Apply the selected modification and verify it through the vehicle runtime.");

                ImGui::TableSetColumnIndex(1);
                if (ImGui::Button("Restore Stock", ImVec2(-1.0f, 0.0f)))
                    g_ActionMessage = runtime.QueueRemoveMod(g_ModType) ? "Stock modification queued" : "Stock restore rejected";
                DescribeLastV11Item("Remove the selected modification slot and restore its stock state.");
                ImGui::EndTable();
            }
        }

        inline void RenderWheelSetup(
            Game::Mods::VehicleModificationRuntime& runtime,
            const Game::Mods::VehicleModificationSnapshot& snapshot) noexcept
        {
            ImGui::SeparatorText("Wheels");
            int wheelType = std::clamp(snapshot.wheelType, 0, static_cast<int>(Game::VehicleCatalogs::WheelTypes.size()) - 1);
            const char* preview = Game::VehicleCatalogs::WheelTypes[static_cast<std::size_t>(wheelType)].name;
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##vehicle_wheel_family_v2", preview))
            {
                for (const auto& entry : Game::VehicleCatalogs::WheelTypes)
                {
                    const bool selected = entry.value == wheelType;
                    if (ImGui::Selectable(entry.name, selected))
                    {
                        wheelType = entry.value;
                        g_ActionMessage = runtime.QueueWheelType(wheelType) ? "Wheel family queued" : "Wheel family rejected";
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            DescribeLastV11Item("Set the GTA wheel family. Front and rear wheel styles remain available in Mod Slots on this same page.");
            ImGui::TextDisabled("Wheel styles: choose Front Wheels or Rear Wheels above.");
        }

        inline void RenderPaint(
            Game::Paint::VehiclePaintRuntime& paintRuntime,
            const Game::Paint::PaintServiceSnapshot& snapshot) noexcept
        {
            ImGui::SeparatorText("Paint & Colors");
            if (!paintRuntime.IsRunning())
            {
                ImGui::TextDisabled("Vehicle paint runtime is offline.");
                return;
            }
            if (!snapshot.paint.valid)
            {
                ImGui::TextDisabled("Paint state is refreshing for the current vehicle.");
                return;
            }

            auto& service = paintRuntime.Service();

            if (ImGui::TreeNodeEx("Primary Paint", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGui::Combo("Primary family", &g_PrimaryPalette, PaintPaletteNames.data(), static_cast<int>(PaintPaletteNames.size())))
                {
                    g_PrimaryPalette = std::clamp(g_PrimaryPalette, 0, static_cast<int>(PaintPalettes.size()) - 1);
                    ResetColorForPalette(PaintPalettes[static_cast<std::size_t>(g_PrimaryPalette)], g_PrimaryColor);
                }
                const auto palette = PaintPalettes[static_cast<std::size_t>(g_PrimaryPalette)];
                static_cast<void>(ColorCombo("##primary_color_v2", Game::VehicleCatalogs::ColorsForPalette(palette), g_PrimaryColor));
                if (ImGui::Button("Apply Primary", ImVec2(-1.0f, 0.0f)))
                    g_ActionMessage = service.QueuePrimary({palette, g_PrimaryColor}) ? "Primary paint queued" : "Primary paint rejected";

                ImGui::ColorEdit3("Custom Primary RGB", g_CustomPrimary, ImGuiColorEditFlags_NoAlpha);
                if (ImGui::BeginTable("##primary_custom_actions_v2", 2, ImGuiTableFlags_SizingStretchSame))
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    if (ImGui::Button("Apply Primary RGB", ImVec2(-1.0f, 0.0f)))
                        g_ActionMessage = service.QueueCustomPrimary(ToRgb(g_CustomPrimary)) ? "Custom primary queued" : "Custom primary rejected";
                    ImGui::TableSetColumnIndex(1);
                    if (ImGui::Button("Clear Primary RGB", ImVec2(-1.0f, 0.0f)))
                        g_ActionMessage = service.QueueClearCustomPrimary() ? "Clear primary queued" : "Clear primary rejected";
                    ImGui::EndTable();
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("Secondary Paint", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGui::Combo("Secondary family", &g_SecondaryPalette, PaintPaletteNames.data(), static_cast<int>(PaintPaletteNames.size())))
                {
                    g_SecondaryPalette = std::clamp(g_SecondaryPalette, 0, static_cast<int>(PaintPalettes.size()) - 1);
                    ResetColorForPalette(PaintPalettes[static_cast<std::size_t>(g_SecondaryPalette)], g_SecondaryColor);
                }
                const auto palette = PaintPalettes[static_cast<std::size_t>(g_SecondaryPalette)];
                static_cast<void>(ColorCombo("##secondary_color_v2", Game::VehicleCatalogs::ColorsForPalette(palette), g_SecondaryColor));
                if (ImGui::Button("Apply Secondary", ImVec2(-1.0f, 0.0f)))
                    g_ActionMessage = service.QueueSecondary({palette, g_SecondaryColor}) ? "Secondary paint queued" : "Secondary paint rejected";

                ImGui::ColorEdit3("Custom Secondary RGB", g_CustomSecondary, ImGuiColorEditFlags_NoAlpha);
                if (ImGui::BeginTable("##secondary_custom_actions_v2", 2, ImGuiTableFlags_SizingStretchSame))
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    if (ImGui::Button("Apply Secondary RGB", ImVec2(-1.0f, 0.0f)))
                        g_ActionMessage = service.QueueCustomSecondary(ToRgb(g_CustomSecondary)) ? "Custom secondary queued" : "Custom secondary rejected";
                    ImGui::TableSetColumnIndex(1);
                    if (ImGui::Button("Clear Secondary RGB", ImVec2(-1.0f, 0.0f)))
                        g_ActionMessage = service.QueueClearCustomSecondary() ? "Clear secondary queued" : "Clear secondary rejected";
                    ImGui::EndTable();
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("Pearlescent & Wheel Color", ImGuiTreeNodeFlags_DefaultOpen))
            {
                static_cast<void>(ColorCombo("##pearlescent_color_v2", Game::VehicleCatalogs::ClassicColors, g_Pearlescent));
                if (ImGui::Button("Apply Pearlescent", ImVec2(-1.0f, 0.0f)))
                    g_ActionMessage = service.QueuePearlescent(g_Pearlescent) ? "Pearlescent queued" : "Pearlescent rejected";

                const char* families[] = {"LSC / Classic", "Chameleon"};
                if (ImGui::Combo("Wheel color family", &g_WheelColorFamily, families, 2))
                {
                    const auto colors = g_WheelColorFamily == 1
                        ? std::span<const IndexedName>(Game::VehicleCatalogs::ChameleonColors)
                        : std::span<const IndexedName>(Game::VehicleCatalogs::ClassicColors);
                    if (!colors.empty())
                        g_WheelColor = colors.front().value;
                }
                const auto colors = g_WheelColorFamily == 1
                    ? std::span<const IndexedName>(Game::VehicleCatalogs::ChameleonColors)
                    : std::span<const IndexedName>(Game::VehicleCatalogs::ClassicColors);
                static_cast<void>(ColorCombo("##wheel_color_v2", colors, g_WheelColor));
                if (ImGui::Button("Apply Wheel Color", ImVec2(-1.0f, 0.0f)))
                    g_ActionMessage = service.QueueWheel(g_WheelColor) ? "Wheel color queued" : "Wheel color rejected";
                ImGui::TreePop();
            }
        }

        inline void RenderLighting(
            Game::Mods::VehicleModificationRuntime& runtime,
            const Game::Mods::VehicleModificationSnapshot& snapshot) noexcept
        {
            ImGui::SeparatorText("Lighting & Neon");

            bool xenon = snapshot.xenon;
            if (ImGui::Checkbox("Xenon Headlights", &xenon))
                g_ActionMessage = runtime.QueueToggleMod(22, xenon) ? "Xenon change queued" : "Xenon change rejected";

            g_XenonColorIndex = std::clamp(g_XenonColorIndex, 0, static_cast<int>(Game::VehicleCatalogs::HeadlightColors.size()) - 1);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##headlight_color_v2", Game::VehicleCatalogs::HeadlightColors[static_cast<std::size_t>(g_XenonColorIndex)].name))
            {
                for (std::size_t i = 0; i < Game::VehicleCatalogs::HeadlightColors.size(); ++i)
                {
                    const bool selected = static_cast<int>(i) == g_XenonColorIndex;
                    if (ImGui::Selectable(Game::VehicleCatalogs::HeadlightColors[i].name, selected))
                        g_XenonColorIndex = static_cast<int>(i);
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (ImGui::Button("Apply Headlight Color", ImVec2(-1.0f, 0.0f)))
                g_ActionMessage = runtime.QueueXenonColor(Game::VehicleCatalogs::HeadlightColors[static_cast<std::size_t>(g_XenonColorIndex)].value)
                    ? "Headlight color queued" : "Headlight color rejected";

            ImGui::TextDisabled("Neon sides");
            if (ImGui::BeginTable("##neon_sides_v2", 4, ImGuiTableFlags_SizingStretchSame))
            {
                static constexpr std::array<const char*, 4> names{{"Left", "Right", "Front", "Back"}};
                ImGui::TableNextRow();
                for (int i = 0; i < 4; ++i)
                {
                    ImGui::TableSetColumnIndex(i);
                    bool enabled = snapshot.neonEnabled[static_cast<std::size_t>(i)];
                    ImGui::PushID(i);
                    if (ImGui::Checkbox(names[static_cast<std::size_t>(i)], &enabled))
                        g_ActionMessage = runtime.QueueNeonEnabled(i, enabled) ? "Neon side queued" : "Neon side rejected";
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }

            ImGui::ColorEdit3("Neon RGB", g_NeonRgb, ImGuiColorEditFlags_NoAlpha);
            if (ImGui::Button("Apply Neon Color", ImVec2(-1.0f, 0.0f)))
                g_ActionMessage = runtime.QueueNeonColor(ToByte(g_NeonRgb[0]), ToByte(g_NeonRgb[1]), ToByte(g_NeonRgb[2]))
                    ? "Neon color queued" : "Neon color rejected";
        }

        inline void RenderTires(
            Game::Mods::VehicleModificationRuntime& runtime,
            const Game::Mods::VehicleModificationSnapshot& snapshot) noexcept
        {
            ImGui::SeparatorText("Tires & Smoke");

            bool smoke = snapshot.tireSmoke;
            if (ImGui::Checkbox("Tire Smoke", &smoke))
                g_ActionMessage = runtime.QueueToggleMod(20, smoke) ? "Tire smoke queued" : "Tire smoke rejected";

            ImGui::ColorEdit3("Tire Smoke RGB", g_SmokeRgb, ImGuiColorEditFlags_NoAlpha);
            if (ImGui::Button("Apply Tire Smoke Color", ImVec2(-1.0f, 0.0f)))
                g_ActionMessage = runtime.QueueTireSmokeColor(ToByte(g_SmokeRgb[0]), ToByte(g_SmokeRgb[1]), ToByte(g_SmokeRgb[2]))
                    ? "Tire smoke color queued" : "Tire smoke color rejected";

            if (ImGui::BeginTable("##tire_behavior_v2", 2, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                bool bulletproof = !snapshot.tyresCanBurst;
                if (ImGui::Checkbox("Bulletproof Tires", &bulletproof))
                    g_ActionMessage = runtime.QueueTyresCanBurst(!bulletproof) ? "Tire durability queued" : "Tire durability rejected";

                ImGui::TableSetColumnIndex(1);
                bool lowGrip = snapshot.driftTyres;
                if (ImGui::Checkbox("Low Grip", &lowGrip))
                    g_ActionMessage = runtime.QueueDriftTyres(lowGrip) ? "Low-grip state queued" : "Low-grip state rejected";
                ImGui::EndTable();
            }
        }

        inline void RenderStatus(
            const Game::Mods::VehicleModificationSnapshot& mods,
            const Game::Paint::PaintServiceSnapshot& paint) noexcept
        {
            ImGui::SeparatorText("Live Status");
            ImGui::Text("Vehicle: %d", mods.vehicle);
            ImGui::TextDisabled("Mod slot %d (%s): %d / %d", g_ModType, ModNames[static_cast<std::size_t>(g_ModType)], mods.currentMod, mods.modCount);
            ImGui::TextDisabled("Wheel family: %d   Turbo: %s   Xenon: %s", mods.wheelType, mods.turbo ? "ON" : "OFF", mods.xenon ? "ON" : "OFF");
            if (paint.paint.valid)
                ImGui::TextDisabled("Paint P/S/Pearl/Wheel: %d / %d / %d / %d", paint.paint.primaryColor, paint.paint.secondaryColor, paint.paint.pearlescentColor, paint.paint.wheelColor);
            ImGui::TextWrapped("Last UI action: %s", g_ActionMessage.c_str());
            ImGui::TextDisabled("All customization controls remain on this page; there is no separate Paint or Workshop mode.");
            RenderLscBypassWidget();
        }
    }

    inline void RenderVehicleCustomizationV2Panel() noexcept
    {
        using namespace VehicleCustomizationV2Detail;

        auto& modsRuntime = Game::Mods::VehicleModificationRuntime::Get();
        modsRuntime.SetObservedModType(g_ModType);
        const auto mods = modsRuntime.Snapshot();

        auto& paintRuntime = Game::Paint::VehiclePaintRuntime::Get();
        const auto paint = paintRuntime.Snapshot();

        SyncModificationState(mods);
        SyncPaintState(paint);

        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild(
                "##vehicle_customization_v2",
                ImVec2(490.0f, 430.0f),
                true,
                ImGuiWindowFlags_AlwaysVerticalScrollbar))
        {
            ImGui::TextColored(V11Theme::Accent, "Vehicle Customization");
            ImGui::SameLine();
            ImGui::TextDisabled("Unified Vehicle Editor");
            ImGui::Separator();

            if (!modsRuntime.IsRunning())
            {
                ImGui::TextDisabled("Vehicle modification runtime is offline.");
            }
            else if (!mods.valid)
            {
                ImGui::TextDisabled("Enter a vehicle to edit its complete customization state.");
            }
            else
            {
                RenderQuickPerformance(modsRuntime, mods);
                RenderModSlots(modsRuntime, mods);
                RenderWheelSetup(modsRuntime, mods);
                RenderPaint(paintRuntime, paint);
                RenderLighting(modsRuntime, mods);
                RenderTires(modsRuntime, mods);
                RenderStatus(mods, paint);
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
