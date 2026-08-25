#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/native/EnhancedUtilityRuntime.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstdio>

namespace Tutones::UI
{
    namespace EnhancedUtilityPanelDetail
    {
        inline bool g_Open{};
        inline float g_VehicleSpeed{35.0f};
        inline char g_PlateText[16] = "TUTONES";
        inline int g_ExtraIndex{1};
        inline bool g_ExtraEnabled{true};
        inline int g_WindowTint{};
        inline bool g_GravityEnabled{true};
        inline int g_HydraulicState{};

        inline int g_WeatherIndex{};
        inline int g_Hour{12};
        inline int g_Minute{};
        inline int g_Second{};
        inline bool g_Blackout{};

        inline constexpr std::array<const char*, 10> WeatherNames{{
            "CLEAR",
            "EXTRASUNNY",
            "CLOUDS",
            "OVERCAST",
            "RAIN",
            "THUNDER",
            "SMOG",
            "FOGGY",
            "SNOWLIGHT",
            "BLIZZARD",
        }};

        inline void RenderVehicleUtilities(Game::NativeTools::EnhancedUtilityRuntime& runtime, const Game::NativeTools::EnhancedUtilitySnapshot& snapshot)
        {
            ImGui::TextColored(V11Theme::Accent, "Enhanced Vehicle Utilities");
            ImGui::TextWrapped("These actions acquire and verify network control of the current vehicle before applying the Enhanced native.");
            ImGui::Spacing();

            ImGui::BeginDisabled(snapshot.pending);
            if (ImGui::Button("Full Vehicle Repair", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueRepairCurrentVehicle());
            ImGui::EndDisabled();
            DescribeLastV11Item("Repair engine/body state, clear deformation and repair the standard tyre indices after network control is confirmed.");

            ImGui::SeparatorText("Motion");
            ImGui::SliderFloat("Forward Speed (m/s)", &g_VehicleSpeed, -50.0f, 150.0f, "%.1f");
            ImGui::BeginDisabled(snapshot.pending);
            if (ImGui::Button("Apply Vehicle Speed", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueSetVehicleSpeed(g_VehicleSpeed));
            ImGui::EndDisabled();
            DescribeLastV11Item("Apply SET_VEHICLE_FORWARD_SPEED to the vehicle you are currently driving.");

            ImGui::SeparatorText("Plate / Extras / Tint");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##enhanced_plate", "Plate text (8 chars max)", g_PlateText, sizeof(g_PlateText));
            ImGui::BeginDisabled(snapshot.pending || g_PlateText[0] == '\0');
            if (ImGui::Button("Apply Plate Text", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueSetPlateText(g_PlateText));
            ImGui::EndDisabled();

            ImGui::SliderInt("Extra Index", &g_ExtraIndex, 0, 20);
            ImGui::Checkbox("Extra Enabled", &g_ExtraEnabled);
            ImGui::BeginDisabled(snapshot.pending);
            if (ImGui::Button("Apply Vehicle Extra", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueSetVehicleExtra(g_ExtraIndex, g_ExtraEnabled));
            ImGui::EndDisabled();
            DescribeLastV11Item("DOES_EXTRA_EXIST is checked before SET_VEHICLE_EXTRA is dispatched.");

            ImGui::SliderInt("Window Tint", &g_WindowTint, -1, 6);
            ImGui::BeginDisabled(snapshot.pending);
            if (ImGui::Button("Apply Window Tint", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueSetWindowTint(g_WindowTint));
            ImGui::EndDisabled();

            ImGui::SeparatorText("Physics / Suspension");
            ImGui::Checkbox("Vehicle Gravity", &g_GravityEnabled);
            ImGui::BeginDisabled(snapshot.pending);
            if (ImGui::Button("Apply Gravity State", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueSetVehicleGravity(g_GravityEnabled));
            ImGui::EndDisabled();

            ImGui::SliderInt("Hydraulic State", &g_HydraulicState, 0, 3);
            ImGui::BeginDisabled(snapshot.pending);
            if (ImGui::Button("Apply Hydraulic State", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueSetHydraulicState(g_HydraulicState));
            ImGui::EndDisabled();
            DescribeLastV11Item("Uses the Enhanced hydraulic state native. Unsupported vehicles simply reject or ignore the state.");

            ImGui::BeginDisabled(snapshot.pending);
            if (ImGui::Button("Read Native Suspension Lowering", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueReadSuspensionLowering());
            ImGui::EndDisabled();
            if (snapshot.haveSuspensionReadback)
                ImGui::Text("GTA suspension lowering: %.6f", snapshot.suspensionLowering);
            else
                ImGui::TextDisabled("GTA suspension lowering: not sampled");
            DescribeLastV11Item("Reads GET_FAKE_SUSPENSION_LOWERING_AMOUNT so the native lowering value can be compared with Tutones' extra ride-height backend.");
        }

        inline void RenderWorldUtilities(Game::NativeTools::EnhancedUtilityRuntime& runtime, const Game::NativeTools::EnhancedUtilitySnapshot& snapshot)
        {
            ImGui::TextColored(V11Theme::Accent, "Enhanced World Environment");

            g_WeatherIndex = std::clamp(g_WeatherIndex, 0, static_cast<int>(WeatherNames.size()) - 1);
            ImGui::Combo("Weather", &g_WeatherIndex, WeatherNames.data(), static_cast<int>(WeatherNames.size()));
            ImGui::BeginDisabled(snapshot.pending);
            if (ImGui::Button("Apply Weather Override", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueSetWeather(WeatherNames[static_cast<std::size_t>(g_WeatherIndex)]));
            ImGui::EndDisabled();
            DescribeLastV11Item("Applies the selected weather through the current Enhanced SET_OVERRIDE_WEATHER handler.");

            ImGui::SeparatorText("Clock");
            ImGui::SliderInt("Hour", &g_Hour, 0, 23);
            ImGui::SliderInt("Minute", &g_Minute, 0, 59);
            ImGui::SliderInt("Second", &g_Second, 0, 59);
            ImGui::BeginDisabled(snapshot.pending);
            if (ImGui::Button("Apply Clock Override", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueSetClock(g_Hour, g_Minute, g_Second));
            ImGui::EndDisabled();

            ImGui::SeparatorText("Lighting");
            ImGui::Checkbox("Blackout", &g_Blackout);
            ImGui::BeginDisabled(snapshot.pending);
            if (ImGui::Button("Apply Blackout State", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueSetBlackout(g_Blackout));
            ImGui::EndDisabled();
            DescribeLastV11Item("Toggles SET_ARTIFICIAL_LIGHTS_STATE for local world lighting.");
        }

        inline void RenderStatus(const Game::NativeTools::EnhancedUtilitySnapshot& snapshot)
        {
            ImGui::SeparatorText("Runtime Status");
            ImGui::Text("Native backend: %s", snapshot.nativeReady ? "READY" : "WAITING");
            if (snapshot.controlAttempts > 0 && snapshot.pending)
                ImGui::Text("Network control attempts: %d", snapshot.controlAttempts);
            if (snapshot.pending)
                ImGui::TextDisabled("%s", snapshot.message.c_str());
            else if (snapshot.haveResult)
                ImGui::TextWrapped("%s: %s", snapshot.lastSucceeded ? "Success" : "Failed", snapshot.message.c_str());
            else
                ImGui::TextDisabled("%s", snapshot.message.c_str());
        }
    }

    inline void RenderEnhancedUtilityLauncher(std::size_t page) noexcept
    {
        using namespace EnhancedUtilityPanelDetail;

        if (page == 1 || page == 2)
        {
            ImGui::SetCursorPos(ImVec2(724.0f, 326.0f));
            ImGui::PushStyleColor(ImGuiCol_Button, V11Theme::PanelBg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, V11Theme::HoverBg);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, V11Theme::ActiveBg);
            if (ImGui::Button("Enhanced Utility Suite", ImVec2(284.0f, 34.0f)))
                g_Open = true;
            ImGui::PopStyleColor(3);
        }

        if (!g_Open)
            return;

        ImGui::SetNextWindowSize(ImVec2(610.0f, 650.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Tutones Enhanced Utilities", &g_Open))
        {
            ImGui::End();
            return;
        }

        auto& runtime = Game::NativeTools::EnhancedUtilityRuntime::Get();
        const auto snapshot = runtime.Snapshot();

        if (ImGui::BeginTabBar("##enhanced_utility_tabs"))
        {
            if (ImGui::BeginTabItem("Vehicle"))
            {
                RenderVehicleUtilities(runtime, snapshot);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("World"))
            {
                RenderWorldUtilities(runtime, snapshot);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        RenderStatus(snapshot);
        ImGui::End();
    }
}
