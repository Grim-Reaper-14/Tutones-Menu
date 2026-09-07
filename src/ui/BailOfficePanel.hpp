#pragma once

#include "V11Theme.hpp"
#include "../features/business/BailOfficeRuntime.hpp"

#include <imgui.h>

namespace Tutones::UI
{
    inline void RenderBailOfficePanel() noexcept
    {
        using namespace Game::Business::BailOfficeEnhanced173;
        using Game::Business::BailOfficeRuntime;

        auto& runtime = BailOfficeRuntime::Get();
        const auto state = runtime.Snapshot();

        ImGui::SetCursorPos(ImVec2(226.0f, 52.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##bail_office_panel", ImVec2(490.0f, 394.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Bail Office");
            ImGui::SameLine();
            ImGui::TextDisabled("Bottom Dollar Bounties");
            ImGui::Separator();
            ImGui::TextDisabled("Enhanced source: appbailoffice.c");
            ImGui::TextDisabled("Target rotation is read from the same GPBD flow and packed stats used by the in-game Bail Office app.");

            ImGui::BeginDisabled(state.pending);
            if (ImGui::Button("Refresh Bail Office", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueRefresh());
            ImGui::EndDisabled();

            ImGui::SeparatorText("Standard Targets");
            for (std::size_t index = 0; index < state.standardTargets.size(); ++index)
            {
                const auto& target = state.standardTargets[index];
                ImGui::PushID(static_cast<int>(index));
                if (target.readable)
                {
                    ImGui::Text("Slot %d: %s", static_cast<int>(index) + 1, TargetName(target.target));
                    ImGui::TextDisabled(
                        "Mission %d | Target %d | Reward $%d | %s",
                        target.mission,
                        target.target,
                        target.reward,
                        target.completed ? "Completed" : "Available");
                }
                else
                {
                    ImGui::Text("Slot %d: --", static_cast<int>(index) + 1);
                }
                ImGui::PopID();
            }

            ImGui::SeparatorText("Most Wanted");
            if (state.mostWantedReadable)
            {
                ImGui::Text("Daily rotation ID: %d", state.mostWantedRotation);
                ImGui::TextDisabled("The decompile stores the Most Wanted rotation in packed int 19014. Name mapping stays read-only until its mission-to-target table is fully pinned.");
            }
            else
            {
                ImGui::Text("Daily rotation: --");
            }

            ImGui::SeparatorText("Runtime");
            ImGui::Text("Session: %s", state.sessionStarted ? "Online" : "Offline");
            ImGui::Text("Bail Office app: %s", state.appRunning ? "Running" : "Idle");
            ImGui::Text("Script globals: %s", state.globalsReady ? "Ready" : "Unavailable");

            if (state.pending)
                ImGui::TextDisabled("%s", state.message.c_str());
            else if (state.haveResult)
                ImGui::TextDisabled("%s", state.message.c_str());
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
