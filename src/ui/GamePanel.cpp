#include "GamePanel.hpp"

#include "V11Theme.hpp"
#include "../features/game/GameSessionRuntime.hpp"
#include "../features/game/GameSessionTypes.hpp"

#include <imgui.h>

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
        const char* g_SessionMessage{"Ready"};

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
            ImGui::TextUnformatted("GTA Online session transition");
            ImGui::Separator();

            if (g_SelectedJoinType >= OnlineJoinTypes.size())
                g_SelectedJoinType = 0;

            if (ImGui::BeginCombo("Session Type", OnlineJoinTypes[g_SelectedJoinType].label))
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

            const bool canQueue = snapshot.scriptRuntimeReady && !snapshot.actionPending;
            ImGui::BeginDisabled(!canQueue);
            if (ImGui::Button("Join Session", ImVec2(220.0f, 0.0f)))
                g_SessionMessage = runtime.QueueJoin(OnlineJoinTypes[g_SelectedJoinType].value)
                    ? "Session transition queued"
                    : "Session transition rejected";
            ImGui::SameLine();
            if (ImGui::Button("Leave Online", ImVec2(-1.0f, 0.0f)))
                g_SessionMessage = runtime.QueueLeaveOnline()
                    ? "Leave Online queued"
                    : "Leave Online rejected";
            ImGui::EndDisabled();

            if (!snapshot.scriptRuntimeReady)
                ImGui::TextDisabled("Shared script runtime is unavailable.");
            else if (snapshot.actionPending)
                ImGui::TextDisabled("A session transition is queued on the GTA script thread.");
            else
                ImGui::TextDisabled("%s", g_SessionMessage);

            ImGui::Spacing();
            ImGui::SeparatorText("Transition backend");
            ImGui::Text("Script runtime: %s", snapshot.scriptRuntimeReady ? "ready" : "unavailable");
            if (!snapshot.capabilityProbed)
                ImGui::TextDisabled("shop_controller SendToClouds path has not been exercised yet.");
            else
                ImGui::Text("shop_controller path: %s", snapshot.shopControllerReady ? "ready" : "unavailable");

            if (snapshot.hasLastAction)
            {
                ImGui::Text("Last action: %s", JoinTypeLabel(snapshot.lastRequested));
                ImGui::Text("Last result: %s", snapshot.lastActionSucceeded ? "transition launched" : "failed");
            }

            ImGui::Spacing();
            ImGui::TextWrapped("Join and Leave Online use the current Enhanced SendToClouds script-function path followed by join-type global 1575048.");
        }

        void RenderCreator(GameSessionRuntime& runtime) noexcept
        {
            ImGui::TextUnformatted("Creator");
            ImGui::Separator();
            ImGui::TextDisabled("Creator transition is intentionally unimplemented.");
            ImGui::Spacing();
            ImGui::TextWrapped("V11 will not expose a working Creator button until its exact GTA Enhanced transition path is verified independently from the normal online session join path.");
            ImGui::Spacing();
            ImGui::Text("Capability: %s", runtime.CreatorSupported() ? "ready" : "not implemented");
        }
    }

    void RenderGamePanel(std::size_t subtab) noexcept
    {
        auto& runtime = GameSessionRuntime::Get();
        const auto snapshot = runtime.Snapshot();
        const std::size_t index = subtab < 2 ? subtab : 0;

        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##game_panel", ImVec2(490.0f, 430.0f), true))
        {
            ImGui::TextColored(Accent, "Game");
            ImGui::SameLine();
            ImGui::TextDisabled("%s", index == 0 ? "Session" : "Creator");
            ImGui::Separator();

            if (index == 0)
                RenderSession(runtime, snapshot);
            else
                RenderCreator(runtime);
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
