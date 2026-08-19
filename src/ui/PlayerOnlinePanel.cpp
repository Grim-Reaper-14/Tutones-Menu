#include "PlayerOnlinePanel.hpp"

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/player/OffRadarRuntime.hpp"

#include <imgui.h>

namespace Tutones::UI
{
    namespace
    {
        const ImVec4 Accent = V11Theme::Accent;
    }

    void RenderPlayerOnlinePanel() noexcept
    {
        auto& runtime = Game::PlayerFeatures::OffRadarRuntime::Get();
        const auto state = runtime.Snapshot();

        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##player_online_panel", ImVec2(490.0f, 430.0f), true))
        {
            ImGui::TextColored(Accent, "Player");
            ImGui::SameLine();
            ImGui::TextDisabled("Online");
            ImGui::Separator();

            bool enabled = state.enabled;
            ImGui::BeginDisabled(!runtime.IsRunning());
            if (ImGui::Checkbox("Off Radar", &enabled))
                runtime.SetEnabled(enabled);
            ImGui::EndDisabled();
            DescribeLastV11Item("Toggle the verified GTA Online Freemode broadcast-global Off Radar state. Tutones does not substitute local HUD or blip hiding when the required online script state is unavailable.");

            if (!runtime.IsRunning())
                ImGui::TextDisabled("Off Radar runtime is offline.");
            else if (state.enabled && !state.applied)
                ImGui::TextDisabled("Off Radar requested; waiting for a safe Freemode broadcast state.");
            else if (state.applied)
                ImGui::Text("Off Radar broadcast flag: applied");
            else
                ImGui::TextDisabled("Off Radar broadcast flag: not applied");

            ImGui::Spacing();
            ImGui::SeparatorText("Backend state");
            ImGui::Text("Script globals: %s", state.scriptGlobalsReady ? "ready" : "unavailable");
            ImGui::Text("Online session: %s", state.sessionStarted ? "started" : "not started");
            ImGui::Text("Freemode thread: %s", state.freemodeReady ? "ready" : "unavailable");
            ImGui::Text("Safe to modify: %s", state.safeToModify ? "yes" : "no");
            ImGui::Text("Network time: %u", state.networkTime);
            if (state.lastRefreshTime != 0)
                ImGui::Text("Last Off Radar refresh: %u", state.lastRefreshTime);

            ImGui::Spacing();
            ImGui::TextWrapped("This uses the GTA Online Freemode broadcast globals. The menu does not substitute local blip or HUD hiding when those globals are unavailable.");
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
