#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/world/TeleportRuntime.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstddef>
#include <string>

namespace Tutones::UI
{
    namespace WorldPanelDetail
    {
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

        inline void RenderPlaceholder(const char* title, const char* message) noexcept
        {
            ImGui::TextUnformatted(title);
            ImGui::Separator();
            ImGui::TextWrapped("%s", message);
        }
    }

    inline void RenderWorldPanel(std::size_t subtab) noexcept
    {
        const std::size_t index = subtab < 3 ? subtab : 0;

        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##world_panel", ImVec2(490.0f, 430.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "World");
            ImGui::SameLine();
            ImGui::TextDisabled("%s", index == 0 ? "General" : (index == 1 ? "Time & Weather" : "Teleport"));
            ImGui::Separator();

            if (index == 0)
                WorldPanelDetail::RenderPlaceholder("General", "World general controls remain a workspace for the next feature pass.");
            else if (index == 1)
                WorldPanelDetail::RenderPlaceholder("Time & Weather", "Time and weather controls remain pending until their Enhanced paths are verified.");
            else
                WorldPanelDetail::RenderTeleport();
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
