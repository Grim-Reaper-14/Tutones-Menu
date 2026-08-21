#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/world/TeleportRuntime.hpp"
#include "../features/world/WorldRuntime.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <string>

namespace Tutones::UI
{
    namespace WorldPanelDetail
    {
        inline int g_SetHour{12};
        inline int g_SetMinute{};
        inline int g_WeatherIndex{};
        inline float g_ClearRadius{50.0f};
        inline std::chrono::steady_clock::time_point g_NextClockSample{};

        inline constexpr std::array<const char*, 15> WeatherNames{{
            "EXTRASUNNY", "CLEAR", "CLOUDS", "SMOG", "FOGGY",
            "OVERCAST", "RAIN", "THUNDER", "CLEARING", "NEUTRAL",
            "SNOW", "BLIZZARD", "SNOWLIGHT", "XMAS", "HALLOWEEN",
        }};

        inline bool RenderToggleSwitch(const char* label, bool& value) noexcept
        {
            ImGui::PushID(label);

            const float height = ImGui::GetFrameHeight();
            const float width = height * 1.75f;
            const float radius = height * 0.5f;
            const ImVec2 position = ImGui::GetCursorScreenPos();

            const bool pressed = ImGui::InvisibleButton("##switch", ImVec2(width, height));
            if (pressed)
                value = !value;

            const bool hovered = ImGui::IsItemHovered();
            const ImVec4 track = value
                ? (hovered ? V11Theme::AccentHover : V11Theme::Accent)
                : (hovered ? V11Theme::ControlHover : V11Theme::ControlBg);

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(
                position,
                ImVec2(position.x + width, position.y + height),
                ImGui::GetColorU32(track),
                radius);

            const float knobX = value ? position.x + width - radius : position.x + radius;
            drawList->AddCircleFilled(
                ImVec2(knobX, position.y + radius),
                std::max(2.0f, radius - 2.0f),
                ImGui::GetColorU32(ImVec4(0.94f, 0.97f, 1.0f, 1.0f)),
                24);

            ImGui::SameLine(0.0f, 8.0f);
            ImGui::TextUnformatted(label);
            ImGui::PopID();
            return pressed;
        }

        inline void RenderRuntimeStatus(const Game::World::WorldSnapshot& snapshot) noexcept
        {
            ImGui::Spacing();
            ImGui::SeparatorText("Status");
            if (snapshot.actionPending)
                ImGui::TextDisabled("%s", snapshot.message.c_str());
            else if (snapshot.haveResult)
                ImGui::TextWrapped("%s: %s", snapshot.lastSucceeded ? "Success" : "Failed", snapshot.message.c_str());
            else
                ImGui::TextDisabled("%s", snapshot.message.c_str());
        }

        inline void RenderGeneral() noexcept
        {
            auto& runtime = Game::World::WorldRuntime::Get();
            const auto snapshot = runtime.Snapshot();

            ImGui::TextColored(V11Theme::Accent, "Population density");
            ImGui::TextWrapped("Density overrides are applied every GTA script tick only while a value differs from 1.00. Resetting to normal stops the loop automatically.");
            ImGui::Separator();

            float pedDensity = snapshot.pedDensity;
            if (ImGui::SliderFloat("Ambient peds", &pedDensity, 0.0f, 1.0f, "%.2f"))
                runtime.SetPedDensity(pedDensity);
            DescribeLastV11Item("Scale ambient pedestrian population for the current local world frame.");

            float scenarioDensity = snapshot.scenarioPedDensity;
            if (ImGui::SliderFloat("Scenario peds", &scenarioDensity, 0.0f, 1.0f, "%.2f"))
                runtime.SetScenarioPedDensity(scenarioDensity);
            DescribeLastV11Item("Scale scenario-driven pedestrians for both interior and exterior world population.");

            float vehicleDensity = snapshot.vehicleDensity;
            if (ImGui::SliderFloat("Traffic", &vehicleDensity, 0.0f, 1.0f, "%.2f"))
                runtime.SetVehicleDensity(vehicleDensity);
            DescribeLastV11Item("Scale the main ambient traffic density multiplier for the current frame.");

            float randomVehicleDensity = snapshot.randomVehicleDensity;
            if (ImGui::SliderFloat("Random traffic", &randomVehicleDensity, 0.0f, 1.0f, "%.2f"))
                runtime.SetRandomVehicleDensity(randomVehicleDensity);
            DescribeLastV11Item("Scale randomly generated traffic independently from the main vehicle density.");

            float parkedDensity = snapshot.parkedVehicleDensity;
            if (ImGui::SliderFloat("Parked vehicles", &parkedDensity, 0.0f, 1.0f, "%.2f"))
                runtime.SetParkedVehicleDensity(parkedDensity);
            DescribeLastV11Item("Scale ambient parked-vehicle generation for the current world frame.");

            ImGui::Spacing();
            if (ImGui::Button("Quiet World", ImVec2(154.0f, 0.0f)))
            {
                runtime.SetPedDensity(0.15f);
                runtime.SetScenarioPedDensity(0.15f);
                runtime.SetVehicleDensity(0.20f);
                runtime.SetRandomVehicleDensity(0.20f);
                runtime.SetParkedVehicleDensity(0.25f);
            }
            DescribeLastV11Item("Apply a low-population preset without completely emptying the map.");
            ImGui::SameLine();
            if (ImGui::Button("Empty Streets", ImVec2(154.0f, 0.0f)))
            {
                runtime.SetPedDensity(0.0f);
                runtime.SetScenarioPedDensity(0.0f);
                runtime.SetVehicleDensity(0.0f);
                runtime.SetRandomVehicleDensity(0.0f);
                runtime.SetParkedVehicleDensity(0.0f);
            }
            DescribeLastV11Item("Set all ambient population multipliers to zero while leaving scripted entities under their own game logic.");
            ImGui::SameLine();
            if (ImGui::Button("Normal", ImVec2(-1.0f, 0.0f)))
                runtime.ResetDensity();
            DescribeLastV11Item("Restore all population multipliers to GTA's normal 1.00 values and stop the density loop.");

            ImGui::TextDisabled("Density loop: %s", snapshot.densityLoopRunning ? "active" : "idle");
        }

        inline void RenderTimeWeather() noexcept
        {
            auto& runtime = Game::World::WorldRuntime::Get();
            const auto now = std::chrono::steady_clock::now();
            if (g_NextClockSample == std::chrono::steady_clock::time_point{} || now >= g_NextClockSample)
            {
                runtime.RequestClockSample();
                g_NextClockSample = now + std::chrono::milliseconds(250);
            }

            const auto snapshot = runtime.Snapshot();
            ImGui::TextColored(V11Theme::Accent, "Time");
            if (snapshot.clockHour >= 0 && snapshot.clockMinute >= 0)
                ImGui::Text("Current local clock: %02d:%02d", snapshot.clockHour, snapshot.clockMinute);
            else
                ImGui::TextDisabled("Current local clock: unavailable");

            ImGui::SliderInt("Hour", &g_SetHour, 0, 23);
            DescribeLastV11Item("Choose the local GTA world hour to apply.");
            ImGui::SliderInt("Minute", &g_SetMinute, 0, 59);
            DescribeLastV11Item("Choose the local GTA world minute to apply.");

            ImGui::BeginDisabled(snapshot.actionPending);
            if (ImGui::Button("Apply Time", ImVec2(-1.0f, 0.0f)))
                runtime.QueueSetTime(g_SetHour, g_SetMinute);
            ImGui::EndDisabled();
            DescribeLastV11Item("Apply the selected local world clock time through the current Enhanced native mapping.");

            bool freezeClock = snapshot.freezeClock;
            ImGui::BeginDisabled(snapshot.actionPending);
            if (RenderToggleSwitch("Freeze Time", freezeClock))
                runtime.QueueFreezeClock(freezeClock);
            ImGui::EndDisabled();
            DescribeLastV11Item("Pause or resume the local GTA world clock. Disable before unloading if you want normal time progression immediately restored.");

            ImGui::Separator();
            ImGui::TextColored(V11Theme::Accent, "Weather & lighting");
            ImGui::Combo("Weather", &g_WeatherIndex, WeatherNames.data(), static_cast<int>(WeatherNames.size()));
            DescribeLastV11Item("Choose a GTA weather type for the local world.");

            ImGui::BeginDisabled(snapshot.actionPending);
            if (ImGui::Button("Apply Weather", ImVec2(-1.0f, 0.0f)))
                runtime.QueueWeather(WeatherNames[static_cast<std::size_t>(g_WeatherIndex)]);
            ImGui::EndDisabled();
            DescribeLastV11Item("Apply and persist the selected weather type locally until GTA or another script changes it.");

            bool blackout = snapshot.blackout;
            ImGui::BeginDisabled(snapshot.actionPending);
            if (RenderToggleSwitch("Blackout", blackout))
                runtime.QueueBlackout(blackout);
            ImGui::EndDisabled();
            DescribeLastV11Item("Toggle local artificial-light blackout state using the verified Enhanced mapping.");

            RenderRuntimeStatus(snapshot);
        }

        inline void RenderTeleport() noexcept
        {
            auto& runtime = Game::World::TeleportRuntime::Get();
            const auto snapshot = runtime.Snapshot();

            ImGui::TextUnformatted("Waypoint");
            ImGui::Separator();

            ImGui::BeginDisabled(!snapshot.nativeReady || snapshot.actionPending);
            if (ImGui::Button("Teleport to Waypoint", ImVec2(-1.0f, 0.0f)))
                runtime.QueueWaypoint();
            DescribeLastV11Item("Teleport the local player, or the vehicle they are currently using, to the active map waypoint. Ground height is resolved over GTA script ticks before the final placement.");
            ImGui::EndDisabled();

            bool autoWaypoint = snapshot.autoWaypointEnabled;
            ImGui::BeginDisabled(!snapshot.nativeReady);
            if (RenderToggleSwitch("Auto Teleport to Waypoint", autoWaypoint))
                runtime.SetAutoWaypoint(autoWaypoint);
            DescribeLastV11Item("Automatically teleport once when a waypoint is created or moved. Removing and placing the waypoint again will trigger another teleport.");
            ImGui::EndDisabled();

            ImGui::Spacing();
            ImGui::SeparatorText("Map Destinations");
            ImGui::TextDisabled("Store buttons cycle through every active map blip of that type.");

            const auto& groups = Game::World::TeleportData::Groups;
            for (std::size_t index = 0; index < groups.size(); ++index)
            {
                ImGui::PushID(static_cast<int>(index));
                ImGui::BeginDisabled(!snapshot.nativeReady || snapshot.actionPending);

                const char* prefix = groups[index].cycle ? "Next " : "";
                if (ImGui::Button((std::string(prefix) + groups[index].label).c_str(), ImVec2(-1.0f, 0.0f)))
                    runtime.QueueGroup(index);

                ImGui::EndDisabled();
                ImGui::PopID();
            }

            ImGui::Spacing();
            ImGui::SeparatorText("Status");
            ImGui::Text("Native runtime: %s", snapshot.nativeReady ? "ready" : "unavailable");
            ImGui::Text("Auto waypoint: %s", snapshot.autoWaypointEnabled ? "on" : "off");
            if (snapshot.actionPending)
                ImGui::TextDisabled("%s", snapshot.message.c_str());
            else if (snapshot.haveResult)
                ImGui::TextWrapped("%s: %s", snapshot.lastSucceeded ? "Success" : "Failed", snapshot.message.c_str());
            else
                ImGui::TextDisabled("%s", snapshot.message.c_str());
        }

        inline void RenderEntities() noexcept
        {
            auto& runtime = Game::World::WorldRuntime::Get();
            const auto snapshot = runtime.Snapshot();

            ImGui::TextColored(V11Theme::Accent, "Local area cleanup");
            ImGui::TextWrapped("These are local clear-area commands centered on your current player position. Avoid using them around mission content you want to keep loaded.");
            ImGui::Separator();

            ImGui::SliderFloat("Radius", &g_ClearRadius, 5.0f, 200.0f, "%.0f m");
            DescribeLastV11Item("Set the radius used by the local ambient-entity cleanup commands.");

            ImGui::BeginDisabled(snapshot.actionPending);
            if (ImGui::Button("Clear Nearby Peds", ImVec2(-1.0f, 0.0f)))
                runtime.QueueClearPeds(g_ClearRadius);
            DescribeLastV11Item("Clear nearby ambient pedestrians around your current local position.");

            if (ImGui::Button("Clear Ambient Vehicles", ImVec2(-1.0f, 0.0f)))
                runtime.QueueClearVehicles(g_ClearRadius);
            DescribeLastV11Item("Clear nearby ambient vehicles with the standard local clear-area vehicle native. Scripted/mission behavior remains game-controlled, so use this away from active missions.");

            if (ImGui::Button("Clear Nearby Objects", ImVec2(-1.0f, 0.0f)))
                runtime.QueueClearObjects(g_ClearRadius);
            DescribeLastV11Item("Clear nearby local world objects inside the selected radius.");

            ImGui::Separator();
            if (ImGui::Button("Clear Ambient Area", ImVec2(-1.0f, 0.0f)))
                runtime.QueueClearAmbient(g_ClearRadius);
            DescribeLastV11Item("Run the pedestrian, object and ambient-vehicle cleanup commands together at the selected radius.");
            ImGui::EndDisabled();

            RenderRuntimeStatus(snapshot);
        }
    }

    inline void RenderWorldPanel(std::size_t subtab) noexcept
    {
        const std::size_t index = subtab < 4 ? subtab : 0;
        constexpr const char* names[] = {"General", "Time & Weather", "Teleport", "Entities"};

        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##world_panel", ImVec2(490.0f, 430.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "World");
            ImGui::SameLine();
            ImGui::TextDisabled("%s", names[index]);
            ImGui::Separator();

            if (index == 0)
                WorldPanelDetail::RenderGeneral();
            else if (index == 1)
                WorldPanelDetail::RenderTimeWeather();
            else if (index == 2)
                WorldPanelDetail::RenderTeleport();
            else
                WorldPanelDetail::RenderEntities();
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
