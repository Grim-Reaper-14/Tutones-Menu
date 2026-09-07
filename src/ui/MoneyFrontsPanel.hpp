#pragma once

#include "V11Theme.hpp"
#include "../features/business/MoneyFrontsRuntime.hpp"

#include <imgui.h>

namespace Tutones::UI
{
    inline void RenderMoneyFrontsPanel() noexcept
    {
        using Game::Business::MoneyFrontsRuntime;

        auto& runtime = MoneyFrontsRuntime::Get();
        const auto state = runtime.Snapshot();

        ImGui::SetCursorPos(ImVec2(226.0f, 52.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##money_fronts_panel", ImVec2(490.0f, 394.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Money Fronts");
            ImGui::SameLine();
            ImGui::TextDisabled("Money Fronts / M25 flow");
            ImGui::Separator();

            ImGui::TextWrapped("Current Enhanced player-flow telemetry for the Money Fronts DLC. This first pass is deliberately read-only until the decompile exposes the exact meaning of every M25 bit and mission state.");

            ImGui::BeginDisabled(state.pending);
            if (ImGui::Button("Refresh Money Fronts", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueRefresh());
            ImGui::EndDisabled();

            ImGui::SeparatorText("M25 Live Flow");
            ImGui::Text("Current mission: %d", state.currentMission);
            ImGui::Text("General bits: 0x%08X", state.generalBits);
            ImGui::Text("Flags: 0x%08X", state.flags);
            ImGui::TextDisabled("Raw fields stay visible so new decompile mappings can be verified in-game without guessing writes.");

            ImGui::SeparatorText("Known Money Fronts Properties");
            ImGui::BulletText("Hands On Car Wash");
            ImGui::BulletText("Smoke on the Water");
            ImGui::TextDisabled("Property names are decompile-backed; operational bit meanings remain locked until verified.");

            ImGui::SeparatorText("Runtime");
            ImGui::Text("Session: %s", state.sessionStarted ? "Online" : "Offline");
            ImGui::Text("Native backend: %s", state.nativeReady ? "Ready" : "Unavailable");
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
