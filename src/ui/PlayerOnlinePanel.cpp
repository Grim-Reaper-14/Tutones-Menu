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
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##player_online_v12_panel", ImVec2(780.0f, 500.0f), true))
        {
            ImGui::TextColored(Accent, "SELF / ONLINE");
            ImGui::TextDisabled("Local Online state controls and live Freemode backend telemetry.");
            ImGui::Separator();

            if (ImGui::BeginTable("##player_online_v12_columns", 2, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextColumn();
                if (ImGui::BeginChild("##off_radar_control_card", ImVec2(0.0f, 360.0f), true))
                {
                    ImGui::TextColored(Accent, "OFF RADAR");
                    ImGui::TextDisabled("Freemode broadcast-global control");
                    ImGui::Separator();

                    bool enabled = state.enabled;
                    ImGui::BeginDisabled(!runtime.IsRunning());
                    if (ImGui::Checkbox("Enable Off Radar", &enabled))
                        runtime.SetEnabled(enabled);
                    ImGui::EndDisabled();
                    DescribeLastV11Item("Toggle the verified GTA Online Freemode broadcast-global Off Radar state. Tutones does not substitute local HUD or blip hiding when the required online script state is unavailable.");

                    ImGui::Spacing();
                    ImGui::SeparatorText("Current State");
                    if (!runtime.IsRunning())
                        ImGui::TextDisabled("Off Radar runtime is offline.");
                    else if (state.enabled && !state.applied)
                        ImGui::TextWrapped("Requested; waiting for a safe Freemode broadcast state.");
                    else if (state.applied)
                        ImGui::TextColored(ImVec4(0.20f, 0.88f, 0.42f, 1.0f), "BROADCAST FLAG APPLIED");
                    else
                        ImGui::TextDisabled("Broadcast flag not applied.");

                    ImGui::Spacing();
                    ImGui::TextWrapped("This is a real Online script-state write. No local-only HUD or blip hiding is used as a substitute.");
                }
                ImGui::EndChild();

                ImGui::TableNextColumn();
                if (ImGui::BeginChild("##off_radar_backend_card", ImVec2(0.0f, 360.0f), true))
                {
                    ImGui::TextColored(Accent, "BACKEND STATE");
                    ImGui::TextDisabled("Live read-back and safety conditions");
                    ImGui::Separator();

                    ImGui::Text("Script globals");
                    ImGui::SameLine(180.0f);
                    ImGui::Text("%s", state.scriptGlobalsReady ? "READY" : "UNAVAILABLE");
                    ImGui::Text("Online session");
                    ImGui::SameLine(180.0f);
                    ImGui::Text("%s", state.sessionStarted ? "STARTED" : "NOT STARTED");
                    ImGui::Text("Freemode thread");
                    ImGui::SameLine(180.0f);
                    ImGui::Text("%s", state.freemodeReady ? "READY" : "UNAVAILABLE");
                    ImGui::Text("Safe to modify");
                    ImGui::SameLine(180.0f);
                    ImGui::TextColored(state.safeToModify ? ImVec4(0.20f, 0.88f, 0.42f, 1.0f) : V11Theme::MutedText,
                        "%s", state.safeToModify ? "YES" : "NO");

                    ImGui::Spacing();
                    ImGui::SeparatorText("Timing");
                    ImGui::Text("Network time");
                    ImGui::SameLine(180.0f);
                    ImGui::Text("%u", state.networkTime);
                    ImGui::Text("Last refresh");
                    ImGui::SameLine(180.0f);
                    if (state.lastRefreshTime != 0)
                        ImGui::Text("%u", state.lastRefreshTime);
                    else
                        ImGui::TextDisabled("N/A");
                }
                ImGui::EndChild();

                ImGui::EndTable();
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
    }
}
