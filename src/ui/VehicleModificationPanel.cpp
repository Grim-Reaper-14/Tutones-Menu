#include "VehicleModificationPanel.hpp"

#include "LscBypassWidget.hpp"
#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/vehicle/VehicleModificationRuntime.hpp"
#include "../game/Natives.hpp"
#include "../game/PlayerNatives.hpp"
#include "../game/VehicleNatives.hpp"
#include "../game/vehicle/VehicleCatalogs.hpp"
#include "../runtime/GameRuntime.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
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
        constexpr int TurboModType = 18;
        constexpr int TireSmokeModType = 20;
        constexpr int XenonModType = 22;

        enum class VerifiedAction : int
        {
            None,
            SetMod,
            RemoveMod,
            ToggleMod,
            SetWheelType,
        };

        enum class VerifiedResult : int
        {
            Idle,
            Queued,
            Success,
            Failed,
            Stale,
        };

        int g_ModType{11};
        int g_ModIndex{};
        int g_WheelType{};
        int g_WheelAxle{};
        int g_WheelStyle{};
        bool g_CustomTires{};
        int g_LastVehicle{};
        int g_LastObserved{-1};
        int g_LastCurrentMod{std::numeric_limits<int>::min()};
        int g_LastWheelType{std::numeric_limits<int>::min()};
        int g_XenonColor{};
        int g_NeonPreset{};
        int g_SmokePreset{};
        float g_NeonRgb[3]{222.0f / 255.0f, 222.0f / 255.0f, 1.0f};
        float g_SmokeRgb[3]{1.0f, 1.0f, 1.0f};
        const char* g_WheelMessage{"Choose a wheel family, then a named wheel."};

        std::atomic<VerifiedAction> g_VerifiedAction{VerifiedAction::None};
        std::atomic<VerifiedResult> g_VerifiedResult{VerifiedResult::Idle};
        std::atomic<int> g_VerifiedSlot{-1};
        std::atomic<int> g_VerifiedRequested{};
        std::atomic<int> g_VerifiedObserved{std::numeric_limits<int>::min()};

        [[nodiscard]] bool IsToggleSlot(int modType) noexcept
        {
            return modType == TurboModType || modType == TireSmokeModType || modType == XenonModType;
        }

        [[nodiscard]] int ToByte(float value) noexcept
        {
            return static_cast<int>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
        }

        [[nodiscard]] Game::Vehicle LiveVehicleOnGameThread() noexcept
        {
            const auto ped = Game::PlayerNatives::PlayerPedId();
            if (!ped || *ped == 0)
                return 0;

            auto vehicle = Game::VehicleNatives::GetVehiclePedIsUsing(*ped);
            if (vehicle && *vehicle != 0)
                return *vehicle;

            vehicle = Game::Natives::GetVehiclePedIsIn(*ped, false);
            return vehicle ? *vehicle : 0;
        }

        void PublishVerifiedAction(VerifiedAction action, int slot, int requested) noexcept
        {
            g_VerifiedAction.store(action, std::memory_order_release);
            g_VerifiedSlot.store(slot, std::memory_order_release);
            g_VerifiedRequested.store(requested, std::memory_order_release);
            g_VerifiedObserved.store(std::numeric_limits<int>::min(), std::memory_order_release);
            g_VerifiedResult.store(VerifiedResult::Queued, std::memory_order_release);
        }

        bool QueueSetModVerified(Game::Vehicle vehicle, int modType, int modIndex, bool customTires)
        {
            if (vehicle == 0 || modType < 0 || modType >= static_cast<int>(ModNames.size()) || modIndex < -1)
                return false;

            const auto action = modIndex < 0 ? VerifiedAction::RemoveMod : VerifiedAction::SetMod;
            PublishVerifiedAction(action, modType, modIndex);

            const bool queued = Runtime::GameRuntime::Get().Enqueue([vehicle, modType, modIndex, customTires] {
                if (LiveVehicleOnGameThread() != vehicle)
                {
                    g_VerifiedResult.store(VerifiedResult::Stale, std::memory_order_release);
                    return;
                }

                if (!Game::Natives::SetVehicleModKit(vehicle, 0))
                {
                    g_VerifiedResult.store(VerifiedResult::Failed, std::memory_order_release);
                    return;
                }

                bool success = false;
                if (modIndex < 0)
                {
                    const bool dispatched = Game::Natives::RemoveVehicleMod(vehicle, modType);
                    const auto current = Game::Natives::GetVehicleMod(vehicle, modType);
                    if (current)
                        g_VerifiedObserved.store(*current, std::memory_order_release);
                    success = dispatched && current && *current == -1;
                }
                else
                {
                    const auto count = Game::Natives::GetNumVehicleMods(vehicle, modType);
                    if (count && modIndex < *count)
                    {
                        const bool dispatched = Game::Natives::SetVehicleMod(vehicle, modType, modIndex, customTires);
                        const auto current = Game::Natives::GetVehicleMod(vehicle, modType);
                        if (current)
                            g_VerifiedObserved.store(*current, std::memory_order_release);
                        success = dispatched && current && *current == modIndex;
                        if (success && (modType == 23 || modType == 24))
                        {
                            const auto variation = Game::Natives::GetVehicleModVariation(vehicle, modType);
                            success = variation && *variation == customTires;
                        }
                    }
                }

                g_VerifiedResult.store(success ? VerifiedResult::Success : VerifiedResult::Failed, std::memory_order_release);
            });

            if (!queued)
                g_VerifiedResult.store(VerifiedResult::Failed, std::memory_order_release);
            return queued;
        }

        bool QueueToggleVerified(Game::Vehicle vehicle, int modType, bool enabled)
        {
            if (vehicle == 0 || !IsToggleSlot(modType))
                return false;

            PublishVerifiedAction(VerifiedAction::ToggleMod, modType, enabled ? 1 : 0);
            const bool queued = Runtime::GameRuntime::Get().Enqueue([vehicle, modType, enabled] {
                if (LiveVehicleOnGameThread() != vehicle)
                {
                    g_VerifiedResult.store(VerifiedResult::Stale, std::memory_order_release);
                    return;
                }

                const bool dispatched = Game::Natives::SetVehicleModKit(vehicle, 0)
                    && Game::Natives::ToggleVehicleMod(vehicle, modType, enabled);
                const auto current = Game::Natives::IsToggleModOn(vehicle, modType);
                if (current)
                    g_VerifiedObserved.store(*current ? 1 : 0, std::memory_order_release);
                const bool success = dispatched && current && *current == enabled;
                g_VerifiedResult.store(success ? VerifiedResult::Success : VerifiedResult::Failed, std::memory_order_release);
            });

            if (!queued)
                g_VerifiedResult.store(VerifiedResult::Failed, std::memory_order_release);
            return queued;
        }

        bool QueueWheelTypeVerified(Game::Vehicle vehicle, int wheelType)
        {
            if (vehicle == 0 || wheelType < 0 || wheelType > 12)
                return false;

            PublishVerifiedAction(VerifiedAction::SetWheelType, -1, wheelType);
            const bool queued = Runtime::GameRuntime::Get().Enqueue([vehicle, wheelType] {
                if (LiveVehicleOnGameThread() != vehicle)
                {
                    g_VerifiedResult.store(VerifiedResult::Stale, std::memory_order_release);
                    return;
                }

                bool success = Game::Natives::SetVehicleModKit(vehicle, 0)
                    && Game::Natives::SetVehicleWheelType(vehicle, wheelType);

                for (const int wheelSlot : {23, 24})
                {
                    const auto count = Game::Natives::GetNumVehicleMods(vehicle, wheelSlot);
                    if (count && *count > 0)
                        success = Game::Natives::SetVehicleMod(vehicle, wheelSlot, 0, false) && success;
                }

                const auto current = Game::Natives::GetVehicleWheelType(vehicle);
                if (current)
                    g_VerifiedObserved.store(*current, std::memory_order_release);
                success = success && current && *current == wheelType;
                g_VerifiedResult.store(success ? VerifiedResult::Success : VerifiedResult::Failed, std::memory_order_release);
            });

            if (!queued)
                g_VerifiedResult.store(VerifiedResult::Failed, std::memory_order_release);
            return queued;
        }

        void SyncFromSnapshot(const Game::Mods::VehicleModificationSnapshot& snapshot) noexcept
        {
            if (!snapshot.valid)
            {
                g_LastVehicle = 0;
                g_LastObserved = -1;
                g_LastCurrentMod = std::numeric_limits<int>::min();
                g_LastWheelType = std::numeric_limits<int>::min();
                return;
            }

            if (snapshot.vehicle != g_LastVehicle)
            {
                g_LastVehicle = snapshot.vehicle;
                g_LastObserved = -1;
                g_LastCurrentMod = std::numeric_limits<int>::min();
                g_LastWheelType = std::numeric_limits<int>::min();
                g_XenonColor = std::clamp(snapshot.xenonColor + 1, 0, 13);
                g_NeonRgb[0] = static_cast<float>(snapshot.neonRed) / 255.0f;
                g_NeonRgb[1] = static_cast<float>(snapshot.neonGreen) / 255.0f;
                g_NeonRgb[2] = static_cast<float>(snapshot.neonBlue) / 255.0f;
                g_SmokeRgb[0] = static_cast<float>(snapshot.tireSmokeRed) / 255.0f;
                g_SmokeRgb[1] = static_cast<float>(snapshot.tireSmokeGreen) / 255.0f;
                g_SmokeRgb[2] = static_cast<float>(snapshot.tireSmokeBlue) / 255.0f;
            }

            if (snapshot.observedModType != g_ModType)
                return;

            if (snapshot.observedModType == g_LastObserved
                && snapshot.currentMod == g_LastCurrentMod
                && snapshot.wheelType == g_LastWheelType)
            {
                return;
            }

            g_LastObserved = snapshot.observedModType;
            g_LastCurrentMod = snapshot.currentMod;
            g_LastWheelType = snapshot.wheelType;
            g_ModIndex = std::max(0, snapshot.currentMod);
            g_WheelStyle = std::max(0, snapshot.currentMod);
            g_WheelType = std::clamp(snapshot.wheelType, 0, 12);
            g_CustomTires = snapshot.customTires;
        }

        bool NamedModCombo(
            const char* label,
            const Game::Mods::VehicleModificationSnapshot& snapshot,
            int& index,
            const char* description) noexcept
        {
            if (snapshot.modCount <= 0)
                return false;

            index = std::clamp(index, 0, snapshot.modCount - 1);
            const char* preview = index < static_cast<int>(snapshot.modDisplayNames.size())
                ? snapshot.modDisplayNames[static_cast<std::size_t>(index)].c_str()
                : "Choose option";

            bool changed = false;
            ImGui::SetNextItemWidth(-1.0f);
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
            DescribeLastV11Item(description);
            return changed;
        }

        void ApplyRgbPreset(const Game::VehicleCatalogs::RgbName& preset, float rgb[3]) noexcept
        {
            rgb[0] = static_cast<float>(preset.red) / 255.0f;
            rgb[1] = static_cast<float>(preset.green) / 255.0f;
            rgb[2] = static_cast<float>(preset.blue) / 255.0f;
        }

        const char* VerifiedActionName(VerifiedAction action) noexcept
        {
            switch (action)
            {
            case VerifiedAction::SetMod: return "Set mod";
            case VerifiedAction::RemoveMod: return "Stock / remove";
            case VerifiedAction::ToggleMod: return "Toggle mod";
            case VerifiedAction::SetWheelType: return "Wheel family";
            default: return "None";
            }
        }

        const char* VerifiedResultName(VerifiedResult result) noexcept
        {
            switch (result)
            {
            case VerifiedResult::Queued: return "Queued";
            case VerifiedResult::Success: return "Verified";
            case VerifiedResult::Failed: return "Failed verification";
            case VerifiedResult::Stale: return "Vehicle changed";
            default: return "Idle";
            }
        }

        void RenderModsTab(
            Game::Mods::VehicleModificationRuntime& runtime,
            const Game::Mods::VehicleModificationSnapshot& snapshot) noexcept
        {
            ImGui::SeparatorText("Modification Slot");
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::Combo("##mod_slot", &g_ModType, ModNames.data(), static_cast<int>(ModNames.size())))
            {
                runtime.SetObservedModType(g_ModType);
                g_LastObserved = -1;
                g_LastCurrentMod = std::numeric_limits<int>::min();
            }
            DescribeLastV11Item("Choose the GTA vehicle modification slot to edit.");

            ImGui::TextDisabled("Available: %d   Installed index: %d", snapshot.modCount, snapshot.currentMod);

            ImGui::SeparatorText("Selected Option");
            if (IsToggleSlot(g_ModType))
            {
                bool enabled = g_ModType == TurboModType ? snapshot.turbo
                    : (g_ModType == TireSmokeModType ? snapshot.tireSmoke : snapshot.xenon);
                if (ImGui::Checkbox("Enabled", &enabled))
                    static_cast<void>(QueueToggleVerified(snapshot.vehicle, g_ModType, enabled));
                DescribeLastV11Item("Enable or disable this toggle-style modification and verify the result from GTA.");
            }
            else if (snapshot.modCount > 0 && snapshot.observedModType == g_ModType)
            {
                if (NamedModCombo(
                        "##mod_option",
                        snapshot,
                        g_ModIndex,
                        "Choose a named vehicle modification. Selecting it applies the option immediately."))
                {
                    static_cast<void>(QueueSetModVerified(snapshot.vehicle, g_ModType, g_ModIndex, g_CustomTires));
                }

                if (g_ModType == 23 || g_ModType == 24)
                {
                    if (ImGui::Checkbox("Custom tires / whitewall", &g_CustomTires) && snapshot.currentMod >= 0)
                        static_cast<void>(QueueSetModVerified(snapshot.vehicle, g_ModType, snapshot.currentMod, g_CustomTires));
                    DescribeLastV11Item("Reapply the selected wheel with GTA's custom-tire variation flag.");
                }

                if (ImGui::BeginTable("##mod_actions", 2, ImGuiTableFlags_SizingStretchSame))
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    if (ImGui::Button("Reapply Selected", ImVec2(-1.0f, 0.0f)))
                        static_cast<void>(QueueSetModVerified(snapshot.vehicle, g_ModType, g_ModIndex, g_CustomTires));
                    DescribeLastV11Item("Reapply the selected option and verify the installed value.");

                    ImGui::TableSetColumnIndex(1);
                    if (ImGui::Button("Stock / Remove", ImVec2(-1.0f, 0.0f)))
                        static_cast<void>(QueueSetModVerified(snapshot.vehicle, g_ModType, -1, false));
                    DescribeLastV11Item("Restore this modification slot to stock and verify the result.");
                    ImGui::EndTable();
                }
            }
            else
            {
                ImGui::TextDisabled(snapshot.observedModType == g_ModType
                    ? "This vehicle has no choices for this modification slot."
                    : "Reading the selected slot from GTA...");
            }

            ImGui::SeparatorText("LSC Access");
            ImGui::TextDisabled("Mod kit 0 is prepared before writes and installed values are read back after changes.");
            RenderLscBypassWidget();
        }

        void RenderWheelsTab(
            Game::Mods::VehicleModificationRuntime& runtime,
            const Game::Mods::VehicleModificationSnapshot& snapshot) noexcept
        {
            const int wheelSlot = g_WheelAxle == 0 ? 23 : 24;
            if (g_ModType != wheelSlot)
            {
                g_ModType = wheelSlot;
                runtime.SetObservedModType(wheelSlot);
                g_LastObserved = -1;
                g_LastCurrentMod = std::numeric_limits<int>::min();
            }

            ImGui::SeparatorText("Wheel Setup");
            if (ImGui::BeginTable("##wheel_setup", 2, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("Wheel Family");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextDisabled("Axle");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                const char* wheelPreview = Game::VehicleCatalogs::WheelTypes[static_cast<std::size_t>(g_WheelType)].name;
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::BeginCombo("##wheel_family", wheelPreview))
                {
                    for (const auto& wheelType : Game::VehicleCatalogs::WheelTypes)
                    {
                        const bool selected = wheelType.value == g_WheelType;
                        if (ImGui::Selectable(wheelType.name, selected))
                        {
                            g_WheelType = wheelType.value;
                            const bool queued = QueueWheelTypeVerified(snapshot.vehicle, g_WheelType);
                            g_WheelMessage = queued
                                ? "Wheel family queued; GTA read-back will verify it."
                                : "Wheel family rejected.";
                        }
                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                DescribeLastV11Item("Choose and immediately apply a GTA wheel family.");

                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::Combo("##wheel_axle", &g_WheelAxle, WheelAxleNames.data(), static_cast<int>(WheelAxleNames.size())))
                {
                    const int nextSlot = g_WheelAxle == 0 ? 23 : 24;
                    g_ModType = nextSlot;
                    runtime.SetObservedModType(nextSlot);
                    g_LastObserved = -1;
                    g_LastCurrentMod = std::numeric_limits<int>::min();
                }
                DescribeLastV11Item("Choose whether the wheel style editor targets the front or rear wheel slot.");
                ImGui::EndTable();
            }

            ImGui::TextDisabled("%s", g_WheelMessage);

            ImGui::SeparatorText("Wheel Style");
            if (snapshot.observedModType == wheelSlot && snapshot.modCount > 0)
            {
                if (NamedModCombo(
                        "##wheel_style",
                        snapshot,
                        g_WheelStyle,
                        "Choose a named wheel style. Selecting it applies the wheel immediately."))
                {
                    static_cast<void>(QueueSetModVerified(snapshot.vehicle, wheelSlot, g_WheelStyle, g_CustomTires));
                }

                if (ImGui::Checkbox("Custom tire / whitewall variant", &g_CustomTires) && snapshot.currentMod >= 0)
                    static_cast<void>(QueueSetModVerified(snapshot.vehicle, wheelSlot, snapshot.currentMod, g_CustomTires));
                DescribeLastV11Item("Apply GTA's custom-tire variation to the selected wheel and verify it.");

                if (ImGui::Button("Restore Stock Wheels", ImVec2(-1.0f, 0.0f)))
                    static_cast<void>(QueueSetModVerified(snapshot.vehicle, wheelSlot, -1, false));
                DescribeLastV11Item("Restore the selected axle's stock wheel and verify the result.");
            }
            else
            {
                ImGui::TextDisabled("Refreshing named wheel choices for this family and axle...");
            }
        }

        void RenderLightingTab(
            Game::Mods::VehicleModificationRuntime& runtime,
            const Game::Mods::VehicleModificationSnapshot& snapshot) noexcept
        {
            ImGui::SeparatorText("Headlights");
            bool xenon = snapshot.xenon;
            if (ImGui::Checkbox("Xenon headlights", &xenon))
                static_cast<void>(QueueToggleVerified(snapshot.vehicle, XenonModType, xenon));
            DescribeLastV11Item("Enable or disable xenon headlights and verify the toggle state.");

            g_XenonColor = std::clamp(
                g_XenonColor,
                0,
                static_cast<int>(Game::VehicleCatalogs::HeadlightColors.size()) - 1);
            const char* xenonPreview = Game::VehicleCatalogs::HeadlightColors[static_cast<std::size_t>(g_XenonColor)].name;
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##xenon_color", xenonPreview))
            {
                for (std::size_t i = 0; i < Game::VehicleCatalogs::HeadlightColors.size(); ++i)
                {
                    const bool selected = static_cast<int>(i) == g_XenonColor;
                    if (ImGui::Selectable(Game::VehicleCatalogs::HeadlightColors[i].name, selected))
                        g_XenonColor = static_cast<int>(i);
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            DescribeLastV11Item("Choose a supported GTA xenon headlight color.");

            if (ImGui::Button("Apply Headlight Color", ImVec2(-1.0f, 0.0f)))
            {
                static_cast<void>(runtime.QueueXenonColor(
                    Game::VehicleCatalogs::HeadlightColors[static_cast<std::size_t>(g_XenonColor)].value));
            }
            DescribeLastV11Item("Apply the selected xenon headlight color to the current vehicle.");

            ImGui::SeparatorText("Neon Kit");
            ImGui::TextDisabled("Sides");
            if (ImGui::BeginTable("##neon_sides", 4, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextRow();
                for (int i = 0; i < 4; ++i)
                {
                    ImGui::TableSetColumnIndex(i);
                    bool enabled = snapshot.neonEnabled[static_cast<std::size_t>(i)];
                    ImGui::PushID(i);
                    if (ImGui::Checkbox(NeonSideNames[static_cast<std::size_t>(i)], &enabled))
                        static_cast<void>(runtime.QueueNeonEnabled(i, enabled));
                    DescribeLastV11Item("Enable or disable this side of the vehicle's neon kit.");
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }

            g_NeonPreset = std::clamp(
                g_NeonPreset,
                0,
                static_cast<int>(Game::VehicleCatalogs::NeonColors.size()) - 1);
            ImGui::TextDisabled("Color Preset");
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##neon_preset", Game::VehicleCatalogs::NeonColors[static_cast<std::size_t>(g_NeonPreset)].name))
            {
                for (std::size_t i = 0; i < Game::VehicleCatalogs::NeonColors.size(); ++i)
                {
                    const bool selected = static_cast<int>(i) == g_NeonPreset;
                    if (ImGui::Selectable(Game::VehicleCatalogs::NeonColors[i].name, selected))
                    {
                        g_NeonPreset = static_cast<int>(i);
                        ApplyRgbPreset(Game::VehicleCatalogs::NeonColors[i], g_NeonRgb);
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            DescribeLastV11Item("Choose a named neon preset and copy its RGB values into the custom editor.");

            ImGui::ColorEdit3("Custom Neon RGB", g_NeonRgb, ImGuiColorEditFlags_NoAlpha);
            DescribeLastV11Item("Choose an exact RGB color for the vehicle's neon lighting.");

            if (ImGui::Button("Apply Neon Color", ImVec2(-1.0f, 0.0f)))
            {
                static_cast<void>(runtime.QueueNeonColor(
                    ToByte(g_NeonRgb[0]),
                    ToByte(g_NeonRgb[1]),
                    ToByte(g_NeonRgb[2])));
            }
            DescribeLastV11Item("Apply the selected custom or preset neon color.");
        }

        void RenderTiresTab(
            Game::Mods::VehicleModificationRuntime& runtime,
            const Game::Mods::VehicleModificationSnapshot& snapshot) noexcept
        {
            ImGui::SeparatorText("Tire Smoke");
            bool smokeEnabled = snapshot.tireSmoke;
            if (ImGui::Checkbox("Enable tire smoke", &smokeEnabled))
                static_cast<void>(QueueToggleVerified(snapshot.vehicle, TireSmokeModType, smokeEnabled));
            DescribeLastV11Item("Enable or disable tire smoke and verify the toggle state.");

            g_SmokePreset = std::clamp(
                g_SmokePreset,
                0,
                static_cast<int>(Game::VehicleCatalogs::TireSmokeColors.size()) - 1);
            ImGui::TextDisabled("Smoke Preset");
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##smoke_preset", Game::VehicleCatalogs::TireSmokeColors[static_cast<std::size_t>(g_SmokePreset)].name))
            {
                for (std::size_t i = 0; i < Game::VehicleCatalogs::TireSmokeColors.size(); ++i)
                {
                    const bool selected = static_cast<int>(i) == g_SmokePreset;
                    if (ImGui::Selectable(Game::VehicleCatalogs::TireSmokeColors[i].name, selected))
                    {
                        g_SmokePreset = static_cast<int>(i);
                        ApplyRgbPreset(Game::VehicleCatalogs::TireSmokeColors[i], g_SmokeRgb);
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            DescribeLastV11Item("Choose a tire-smoke preset and copy its RGB values into the custom editor.");

            ImGui::ColorEdit3("Custom Smoke RGB", g_SmokeRgb, ImGuiColorEditFlags_NoAlpha);
            DescribeLastV11Item("Choose an exact RGB color for tire smoke.");

            if (ImGui::Button("Apply Tire Smoke Color", ImVec2(-1.0f, 0.0f)))
            {
                static_cast<void>(runtime.QueueTireSmokeColor(
                    ToByte(g_SmokeRgb[0]),
                    ToByte(g_SmokeRgb[1]),
                    ToByte(g_SmokeRgb[2])));
            }
            DescribeLastV11Item("Apply the selected tire-smoke color to the current vehicle.");

            ImGui::SeparatorText("Tire Behavior");
            if (ImGui::BeginTable("##tire_behavior", 2, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                bool bulletproof = !snapshot.tyresCanBurst;
                if (ImGui::Checkbox("Bulletproof tires", &bulletproof))
                    static_cast<void>(runtime.QueueTyresCanBurst(!bulletproof));
                DescribeLastV11Item("Prevent the current vehicle's tires from bursting, or restore normal tire behavior.");

                ImGui::TableSetColumnIndex(1);
                bool lowGrip = snapshot.driftTyres;
                if (ImGui::Checkbox("Low grip / drift", &lowGrip))
                    static_cast<void>(runtime.QueueDriftTyres(lowGrip));
                DescribeLastV11Item("Enable or disable GTA's low-grip tire state.");
                ImGui::EndTable();
            }
        }

        void RenderStatusTab(const Game::Mods::VehicleModificationSnapshot& snapshot) noexcept
        {
            ImGui::SeparatorText("Current Vehicle State");
            if (ImGui::BeginTable("##vehicle_editor_state", 2, ImGuiTableFlags_SizingStretchProp))
            {
                const auto row = [](const char* label, const char* value) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextDisabled("%s", label);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(value);
                };

                char buffer[96]{};
                std::snprintf(buffer, sizeof(buffer), "%d", snapshot.vehicle);
                row("Vehicle", buffer);
                std::snprintf(buffer, sizeof(buffer), "%d - %s", g_ModType, ModNames[static_cast<std::size_t>(std::clamp(g_ModType, 0, 49))]);
                row("Requested slot", buffer);
                std::snprintf(buffer, sizeof(buffer), "%d", snapshot.observedModType);
                row("Observed slot", buffer);
                std::snprintf(buffer, sizeof(buffer), "%d / %d", snapshot.modCount, snapshot.currentMod);
                row("Available / installed", buffer);
                std::snprintf(buffer, sizeof(buffer), "%d", snapshot.wheelType);
                row("Wheel type", buffer);
                row("Turbo", snapshot.turbo ? "On" : "Off");
                std::snprintf(buffer, sizeof(buffer), "%d, %d, %d", snapshot.tireSmokeRed, snapshot.tireSmokeGreen, snapshot.tireSmokeBlue);
                row("Tire smoke RGB", buffer);
                std::snprintf(buffer, sizeof(buffer), "%s / color %d", snapshot.xenon ? "On" : "Off", snapshot.xenonColor);
                row("Xenon", buffer);
                std::snprintf(buffer, sizeof(buffer), "%d, %d, %d", snapshot.neonRed, snapshot.neonGreen, snapshot.neonBlue);
                row("Neon RGB", buffer);
                row("Tires can burst", snapshot.tyresCanBurst ? "Yes" : "No");
                row("Low grip", snapshot.driftTyres ? "Yes" : "No");
                ImGui::EndTable();
            }

            ImGui::SeparatorText("Last Verified Change");
            const auto verifiedAction = g_VerifiedAction.load(std::memory_order_acquire);
            const auto verifiedResult = g_VerifiedResult.load(std::memory_order_acquire);
            ImGui::Text("Action: %s", VerifiedActionName(verifiedAction));
            ImGui::Text("Result: %s", VerifiedResultName(verifiedResult));
            if (verifiedAction != VerifiedAction::None)
            {
                ImGui::Text("Slot: %d", g_VerifiedSlot.load(std::memory_order_acquire));
                ImGui::Text("Requested: %d", g_VerifiedRequested.load(std::memory_order_acquire));
                const int observed = g_VerifiedObserved.load(std::memory_order_acquire);
                if (observed != std::numeric_limits<int>::min())
                    ImGui::Text("Observed: %d", observed);
            }
            ImGui::TextWrapped("Modification, toggle, and wheel-family changes report success only after GTA read-back matches the requested value.");
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
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##vehicle_modifications", ImVec2(490.0f, 430.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Vehicle Editor");
            ImGui::SameLine();
            ImGui::TextDisabled("Vehicle %d", snapshot.vehicle);
            ImGui::Separator();

            if (!runtime.IsRunning())
            {
                ImGui::TextDisabled("Vehicle modification runtime is offline.");
            }
            else if (!snapshot.valid)
            {
                ImGui::TextDisabled("Enter a vehicle to read and apply modifications.");
            }
            else if (ImGui::BeginTabBar("##vehicle_mod_tabs"))
            {
                if (ImGui::BeginTabItem("Mods"))
                {
                    RenderModsTab(runtime, snapshot);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Wheels"))
                {
                    RenderWheelsTab(runtime, snapshot);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Lighting"))
                {
                    RenderLightingTab(runtime, snapshot);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Tires"))
                {
                    RenderTiresTab(runtime, snapshot);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Status"))
                {
                    RenderStatusTab(snapshot);
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
