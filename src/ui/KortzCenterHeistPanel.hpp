#pragma once

#include "../features/heist/KortzCenterHeistRuntime.hpp"

#include <imgui.h>

#include <algorithm>

namespace Tutones::UI
{
    inline void RenderKortzCenterHeistPanel() noexcept
    {
        using namespace Game::Heist::KortzCenterEnhanced;
        using Game::Heist::KortzCenterHeistRuntime;

        auto& runtime = KortzCenterHeistRuntime::Get();
        const auto state = runtime.Snapshot();
        static int selectedTarget = 0;
        static bool targetInitialized = false;
        if (!targetInitialized && state.setupReady && state.target >= 0 && state.target < TargetCount)
        {
            selectedTarget = state.target;
            targetInitialized = true;
        }
        selectedTarget = std::clamp(selectedTarget, 0, TargetCount - 1);

        ImGui::SeparatorText("Kortz Center Heist");

        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("##kortz_target", TargetName(selectedTarget)))
        {
            for (int target = 0; target < TargetCount; ++target)
            {
                const bool selected = selectedTarget == target;
                if (ImGui::Selectable(TargetName(target), selected))
                    selectedTarget = target;
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::BeginDisabled(state.pending);
        if (ImGui::Button("Complete Kortz Setup", ImVec2(-1.0f, 0.0f)))
            static_cast<void>(runtime.QueueSetup(selectedTarget));
        if (ImGui::Button("Refresh Kortz State", ImVec2(-1.0f, 0.0f)))
        {
            targetInitialized = false;
            static_cast<void>(runtime.QueueRefresh());
        }
        ImGui::EndDisabled();

        ImGui::SeparatorText("In-Heist Controls");
        ImGui::BeginDisabled(state.pending || !state.controllerRunning);
        if (ImGui::Button("Skip Fingerprint Hack", ImVec2(-1.0f, 0.0f)))
            static_cast<void>(runtime.QueueAction(Action::SkipFingerprint));
        if (ImGui::Button("Skip Signal Nodes", ImVec2(-1.0f, 0.0f)))
            static_cast<void>(runtime.QueueAction(Action::SkipSignalNodes));
        if (ImGui::Button("Skip Data Crack", ImVec2(-1.0f, 0.0f)))
            static_cast<void>(runtime.QueueAction(Action::SkipDataCrack));
        if (ImGui::Button("Cut Glass", ImVec2(-1.0f, 0.0f)))
            static_cast<void>(runtime.QueueAction(Action::CutGlass));
        if (ImGui::Button("Disable Laser Grid", ImVec2(-1.0f, 0.0f)))
            static_cast<void>(runtime.QueueAction(Action::DisableLaserGrid));
        if (ImGui::Button("Take Primary Target", ImVec2(-1.0f, 0.0f)))
            static_cast<void>(runtime.QueueAction(Action::TakePrimaryTarget));
        if (ImGui::Button("Take Secondary Target", ImVec2(-1.0f, 0.0f)))
            static_cast<void>(runtime.QueueAction(Action::TakeSecondaryTarget));
        ImGui::EndDisabled();

        ImGui::SeparatorText("Current State");
        ImGui::Text("Finale: %s", state.controllerRunning ? "Running" : "Idle");
        if (state.setupReady)
        {
            ImGui::Text("Target: %s", TargetName(state.target));
            ImGui::Text("Setup: %s", state.robberyProgress == -1 ? "Complete" : "Custom");
            ImGui::Text("Scoping: %s", state.scopingBits == -1 ? "Complete" : "Custom");
            ImGui::Text("POIs: %s", state.poiBits == -1 ? "Complete" : "Custom");
        }

        if (state.pending || state.haveResult)
            ImGui::TextDisabled("%s", state.message.c_str());
    }
}
