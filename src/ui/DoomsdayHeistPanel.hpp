#pragma once

#include "V11Theme.hpp"
#include "../features/heist/DoomsdayHeistRuntime.hpp"

#include <imgui.h>

#include <algorithm>

namespace Tutones::UI
{
    inline void RenderDoomsdayHeistPanel() noexcept
    {
        using namespace Game::Heist::DoomsdayHeistEnhanced173;
        using Game::Heist::DoomsdayHeistRuntime;

        auto& runtime = DoomsdayHeistRuntime::Get();
        const auto state = runtime.Snapshot();

        static int selectedAct = 0;
        static CutArray cuts{{100, 0, 0, 0}};
        static bool setupInitialized = false;
        static bool cutsInitialized = false;

        if (!setupInitialized && state.setupReady && state.act >= 0 && state.act < ActCount)
        {
            selectedAct = state.act;
            setupInitialized = true;
        }
        if (!cutsInitialized && state.cutsReady)
        {
            cuts = state.cuts;
            cutsInitialized = true;
        }

        selectedAct = std::clamp(selectedAct, 0, ActCount - 1);

        ImGui::SeparatorText("Doomsday Heist");
        ImGui::TextDisabled("Enhanced source: gb_gang_ops_planning.c");
        ImGui::TextDisabled("Planning stats and cuts use read-back verification and rollback. Mission-controller instant-finish writes are intentionally excluded.");

        ImGui::SeparatorText("Act Setup");
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("##doomsday_act", ActName(selectedAct)))
        {
            for (int act = 0; act < ActCount; ++act)
            {
                const bool selected = act == selectedAct;
                if (ImGui::Selectable(ActName(act), selected))
                    selectedAct = act;
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::BeginDisabled(state.pending);
        if (ImGui::Button("Complete Selected Act Setup", ImVec2(-1.0f, 0.0f)))
            static_cast<void>(runtime.QueueSetup(selectedAct));
        if (ImGui::Button("Refresh Doomsday State", ImVec2(-1.0f, 0.0f)))
        {
            setupInitialized = false;
            cutsInitialized = false;
            static_cast<void>(runtime.QueueRefresh());
        }
        ImGui::EndDisabled();
        ImGui::TextDisabled("After applying setup, close and reopen the Facility planning screen to force its normal redraw path.");

        ImGui::SeparatorText("Player Pay Cuts");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputInt("Player 1 %##doomsday_cut1", &cuts[0], 1, 5);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputInt("Player 2 %##doomsday_cut2", &cuts[1], 1, 5);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputInt("Player 3 %##doomsday_cut3", &cuts[2], 1, 5);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputInt("Player 4 %##doomsday_cut4", &cuts[3], 1, 5);
        for (int& cut : cuts)
            cut = std::clamp(cut, 0, MaxCut);

        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float thirdWidth = (ImGui::GetContentRegionAvail().x - (spacing * 2.0f)) / 3.0f;
        if (ImGui::Button("Solo 100##doomsday", ImVec2(thirdWidth, 0.0f)))
            cuts = CutArray{{100, 0, 0, 0}};
        ImGui::SameLine();
        if (ImGui::Button("25 Each##doomsday", ImVec2(thirdWidth, 0.0f)))
            cuts = CutArray{{25, 25, 25, 25}};
        ImGui::SameLine();
        if (ImGui::Button("100 Each##doomsday", ImVec2(-1.0f, 0.0f)))
            cuts = CutArray{{100, 100, 100, 100}};

        ImGui::TextDisabled("Configured total: %d%%", cuts[0] + cuts[1] + cuts[2] + cuts[3]);
        ImGui::BeginDisabled(state.pending);
        if (ImGui::Button("Apply Doomsday Cuts", ImVec2(-1.0f, 0.0f)))
            static_cast<void>(runtime.QueueCuts(cuts));
        ImGui::EndDisabled();

        ImGui::SeparatorText("Live Planning State");
        ImGui::Text("Session: %s", state.sessionStarted ? "Online" : "Offline");
        ImGui::Text("Planning script: %s", state.planningRunning ? "Running" : "Idle");
        ImGui::Text("Script globals: %s", state.globalsReady ? "Ready" : "Unavailable");
        if (state.setupReady)
        {
            ImGui::Text("Detected act: %s", ActName(state.act));
            ImGui::Text("Mission progress: %d", state.missionProgress);
            ImGui::Text("Heist status: %d", state.heistStatus);
            ImGui::Text("Notifications: %d", state.notifications);
        }
        if (state.cutsReady)
        {
            ImGui::Text(
                "Current cuts: P1 %d%% | P2 %d%% | P3 %d%% | P4 %d%%",
                state.cuts[0], state.cuts[1], state.cuts[2], state.cuts[3]);
        }

        if (state.pending)
            ImGui::TextDisabled("%s", state.message.c_str());
        else if (state.haveResult)
            ImGui::TextDisabled("%s", state.message.c_str());
    }
}
