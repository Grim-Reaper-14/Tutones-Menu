#pragma once

#include "../features/heist/ApartmentHeistRuntime.hpp"

#include <imgui.h>

#include <algorithm>

namespace Tutones::UI
{
    inline void RenderApartmentHeistPanel() noexcept
    {
        using namespace Game::Heist::ApartmentHeistEnhanced173;
        using Game::Heist::ApartmentHeistRuntime;

        auto& runtime = ApartmentHeistRuntime::Get();
        const auto state = runtime.Snapshot();

        static CutArray cuts{{100, 0, 0, 0}};
        static bool cutsInitialized = false;
        if (!cutsInitialized && state.cutsReady)
        {
            cuts = state.cuts;
            cutsInitialized = true;
        }

        ImGui::SeparatorText("Apartment Heists");

        ImGui::BeginDisabled(state.pending);
        if (ImGui::Button("Complete Current Setup", ImVec2(-1.0f, 0.0f)))
            static_cast<void>(runtime.QueueCompleteSetup());
        if (ImGui::Button("Refresh Apartment State", ImVec2(-1.0f, 0.0f)))
        {
            cutsInitialized = false;
            static_cast<void>(runtime.QueueRefresh());
        }
        ImGui::EndDisabled();

        ImGui::SeparatorText("Player Pay Cuts");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputInt("Player 1 %##apartment_cut1", &cuts[0], 1, 5);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputInt("Player 2 %##apartment_cut2", &cuts[1], 1, 5);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputInt("Player 3 %##apartment_cut3", &cuts[2], 1, 5);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputInt("Player 4 %##apartment_cut4", &cuts[3], 1, 5);
        for (int& cut : cuts)
            cut = std::clamp(cut, 0, MaxCut);

        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float thirdWidth = (ImGui::GetContentRegionAvail().x - (spacing * 2.0f)) / 3.0f;
        if (ImGui::Button("Solo 100##apartment", ImVec2(thirdWidth, 0.0f)))
            cuts = CutArray{{100, 0, 0, 0}};
        ImGui::SameLine();
        if (ImGui::Button("25 Each##apartment", ImVec2(thirdWidth, 0.0f)))
            cuts = CutArray{{25, 25, 25, 25}};
        ImGui::SameLine();
        if (ImGui::Button("100 Each##apartment", ImVec2(-1.0f, 0.0f)))
            cuts = CutArray{{100, 100, 100, 100}};

        ImGui::BeginDisabled(state.pending);
        if (ImGui::Button("Apply Apartment Cuts", ImVec2(-1.0f, 0.0f)))
            static_cast<void>(runtime.QueueCuts(cuts));
        ImGui::EndDisabled();

        ImGui::SeparatorText("In-Heist Controls");
        ImGui::BeginDisabled(state.pending || !state.controllerRunning);
        if (ImGui::Button("Skip Hacking", ImVec2(-1.0f, 0.0f)))
            static_cast<void>(runtime.QueueSkipHacking());
        if (ImGui::Button("Skip Drilling", ImVec2(-1.0f, 0.0f)))
            static_cast<void>(runtime.QueueSkipDrilling());
        if (ImGui::Button("Skip Card Swipe", ImVec2(-1.0f, 0.0f)))
            static_cast<void>(runtime.QueueSkipSwiping());
        ImGui::EndDisabled();

        ImGui::SeparatorText("Current State");
        ImGui::Text("Mission: %s", state.controllerRunning ? "Running" : "Idle");
        if (state.setupReady)
            ImGui::Text("Planning stage: %d", state.planningStage);
        if (state.cutsReady)
        {
            ImGui::Text(
                "Cuts: P1 %d%% | P2 %d%% | P3 %d%% | P4 %d%%",
                state.cuts[0], state.cuts[1], state.cuts[2], state.cuts[3]);
        }

        if (state.pending || state.haveResult)
            ImGui::TextDisabled("%s", state.message.c_str());
    }
}
