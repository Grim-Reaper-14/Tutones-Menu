#include "GamePanel.hpp"

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/game/GameSessionRuntime.hpp"
#include "../features/game/GameSessionTypes.hpp"
#include "../features/game/NoIdleRuntime.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstddef>

namespace Tutones::UI
{
    namespace
    {
        using Game::SessionFeatures::GameSessionRuntime;
        using Game::SessionFeatures::GameSessionSnapshot;
        using Game::SessionFeatures::JoinType;
        using Game::SessionFeatures::OnlineJoinTypes;

        const ImVec4 Accent = V11Theme::Accent;
        std::size_t g_SelectedJoinType{};
        bool g_NoIdle{};
        const char* g_SessionMessage{"Ready"};

        bool RenderToggleSwitch(const char* label, bool& value) noexcept
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

        const char* JoinTypeLabel(JoinType type) noexcept
        {
            if (type == JoinType::LeaveOnline)
                return "Leave Online";
            for (const auto& entry : OnlineJoinTypes)
                if (entry.value == type)
                    return entry.label;
            return "Unknown";
        }

        void RenderSession(GameSessionRuntime& runtime, const GameSessionSnapshot& snapshot) noexcept
        {
            if (g_SelectedJoinType >= OnlineJoinTypes.size())
                g_SelectedJoinType = 0;

            if (ImGui::BeginTable("##session_v12_columns", 2, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextColumn();
                if (ImGui::BeginChild("##session_transition_card", ImVec2(0.0f, 365.0f), true))
                {
                    ImGui::TextColored(Accent, "SESSION TRANSITION");
                    ImGui::TextDisabled("Choose where Tutones should send your Online session.");
                    ImGui::Separator();

                    ImGui::SetNextItemWidth(-1.0f);
                    if (ImGui::BeginCombo("##session_type", OnlineJoinTypes[g_SelectedJoinType].label))
                    {
                        for (std::size_t index = 0; index < OnlineJoinTypes.size(); ++index)
                        {
                            const bool selected = index == g_SelectedJoinType;
                            if (ImGui::Selectable(OnlineJoinTypes[index].label, selected))
                                g_SelectedJoinType = index;
                            if (selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    DescribeLastV11Item("Choose the verified GTA Online join type that Tutones will pass through the current Enhanced session-transition path.");

                    const bool canQueue = snapshot.scriptRuntimeReady && !snapshot.actionPending;
                    ImGui::BeginDisabled(!canQueue);
                    if (ImGui::Button("JOIN SELECTED SESSION", ImVec2(-1.0f, 38.0f)))
                        g_SessionMessage = runtime.QueueJoin(OnlineJoinTypes[g_SelectedJoinType].value)
                            ? "Session transition queued"
                            : "Session transition rejected";
                    DescribeLastV11Item("Queue the selected Online join type through the verified Enhanced shop_controller SendToClouds transition path.");

                    if (ImGui::Button("LEAVE GTA ONLINE", ImVec2(-1.0f, 38.0f)))
                        g_SessionMessage = runtime.QueueLeaveOnline()
                            ? "Leave Online queued"
                            : "Leave Online rejected";
                    DescribeLastV11Item("Queue the verified Leave Online transition through the same Enhanced script-runtime session path.");
                    ImGui::EndDisabled();

                    ImGui::Spacing();
                    ImGui::SeparatorText("Status");
                    if (!snapshot.scriptRuntimeReady)
                        ImGui::TextDisabled("Shared script runtime is unavailable.");
                    else if (snapshot.actionPending)
                        ImGui::TextDisabled("A session transition is queued on the GTA script thread.");
                    else
                        ImGui::TextWrapped("%s", g_SessionMessage);
                }
                ImGui::EndChild();

                ImGui::TableNextColumn();
                if (ImGui::BeginChild("##session_qol_card", ImVec2(0.0f, 365.0f), true))
                {
                    ImGui::TextColored(Accent, "QUALITY OF LIFE");
                    ImGui::TextDisabled("Session utilities and transition backend state");
                    ImGui::Separator();

                    if (ImGui::Button("Skip Cutscene", ImVec2(-1.0f, 0.0f)))
                        g_SessionMessage = runtime.QueueSkipCutscene()
                            ? "Skip Cutscene queued"
                            : "Skip Cutscene unavailable";
                    DescribeLastV11Item("Immediately stop the currently playing GTA cutscene using the verified Enhanced STOP_CUTSCENE_IMMEDIATELY native.");

                    if (ImGui::Button("Skip Conversation", ImVec2(-1.0f, 0.0f)))
                        g_SessionMessage = runtime.QueueSkipConversation()
                            ? "Skip Conversation queued"
                            : "Skip Conversation unavailable";
                    DescribeLastV11Item("Skip to the next scripted phone/conversation line using the same native path used by YimMenuV2.");

                    auto& noIdle = Game::SessionFeatures::NoIdleRuntime::Get();
                    const auto idle = noIdle.Snapshot();
                    g_NoIdle = idle.enabled;
                    if (RenderToggleSwitch("No Idle Kick", g_NoIdle))
                    {
                        noIdle.SetEnabled(g_NoIdle);
                        g_SessionMessage = g_NoIdle ? "No Idle enabled" : "No Idle disabled";
                    }
                    DescribeLastV11Item("Yim-style No Idle: service the idle-warning and kick tunables while enabled and restore their original values when disabled.");

                    ImGui::Spacing();
                    ImGui::SeparatorText("Backend");
                    ImGui::Text("Script runtime");
                    ImGui::SameLine(180.0f);
                    ImGui::TextColored(snapshot.scriptRuntimeReady ? ImVec4(0.20f, 0.88f, 0.42f, 1.0f) : V11Theme::MutedText,
                        "%s", snapshot.scriptRuntimeReady ? "READY" : "UNAVAILABLE");
                    ImGui::Text("shop_controller path");
                    ImGui::SameLine(180.0f);
                    ImGui::Text("%s", snapshot.capabilityProbed ? (snapshot.shopControllerReady ? "READY" : "UNAVAILABLE") : "NOT PROBED");
                    ImGui::Text("No Idle");
                    ImGui::SameLine(180.0f);
                    ImGui::Text("%s", idle.enabled ? idle.message.c_str() : "OFF");

                    if (snapshot.hasLastAction)
                    {
                        ImGui::Spacing();
                        ImGui::SeparatorText("Last Transition");
                        ImGui::Text("%s", JoinTypeLabel(snapshot.lastRequested));
                        ImGui::TextDisabled("%s", snapshot.lastActionSucceeded ? "Transition launched" : "Transition failed");
                    }
                }
                ImGui::EndChild();

                ImGui::EndTable();
            }
        }

        void RenderCreator(GameSessionRuntime& runtime) noexcept
        {
            if (ImGui::BeginChild("##creator_v12_card", ImVec2(0.0f, 365.0f), true))
            {
                ImGui::TextColored(Accent, "CREATOR");
                ImGui::TextDisabled("Capability placeholder");
                ImGui::Separator();
                ImGui::TextWrapped("Creator transition remains intentionally unavailable until its exact GTA Enhanced transition path is independently verified from the normal Online session join path.");
                ImGui::Spacing();
                ImGui::Text("Capability");
                ImGui::SameLine(180.0f);
                ImGui::Text("%s", runtime.CreatorSupported() ? "READY" : "NOT IMPLEMENTED");
            }
            ImGui::EndChild();
        }
    }

    void RenderGamePanel(std::size_t subtab) noexcept
    {
        auto& runtime = GameSessionRuntime::Get();
        const auto snapshot = runtime.Snapshot();
        const std::size_t index = subtab < 2 ? subtab : 0;

        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##game_v12_panel", ImVec2(780.0f, 500.0f), true))
        {
            ImGui::TextColored(Accent, "ONLINE");
            ImGui::SameLine();
            ImGui::TextDisabled("%s", index == 0 ? "SESSION" : "CREATOR");
            ImGui::TextDisabled("Session transitions and Online quality-of-life controls.");
            ImGui::Separator();

            if (index == 0)
                RenderSession(runtime, snapshot);
            else
                RenderCreator(runtime);
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
    }
}
