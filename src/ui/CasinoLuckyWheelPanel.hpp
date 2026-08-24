#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/recovery/CasinoLuckyWheelRuntime.hpp"

#include <imgui.h>

namespace Tutones::UI
{
    inline void RenderCasinoLuckyWheelPanel() noexcept
    {
        using Game::Recovery::CasinoLuckyWheelRuntime;

        auto& runtime = CasinoLuckyWheelRuntime::Get();
        const auto state = runtime.Snapshot();

        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##casino_lucky_wheel_panel", ImVec2(490.0f, 430.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Casino Lucky Wheel");
            ImGui::SameLine();
            ImGui::TextDisabled("Enhanced 1.73 / b1158.13");
            ImGui::Separator();

            ImGui::TextWrapped("Uses the supplied Enhanced Lucky Wheel globals and validates each write with read-back verification.");
            ImGui::Spacing();

            ImGui::BeginDisabled(state.pending);
            if (ImGui::Button("Apply supplied Lucky Wheel globals", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueApplySuppliedGlobals());
            ImGui::EndDisabled();
            DescribeLastV11Item("Apply Global_262145.f_26855=1, f_26856=1 and f_37458=2 using the supplied Enhanced 1.73 mapping.");

            ImGui::SeparatorText("casino_lucky_wheel Player Local");
            ImGui::TextDisabled("Supplied layout: local 150 + (PLAYER_ID * 5)");
            ImGui::BeginDisabled(state.pending);
            if (ImGui::Button("Inspect active player local", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueInspectPlayerLocal());
            ImGui::EndDisabled();
            DescribeLastV11Item("Resolve casino_lucky_wheel, PLAYER_ID and the supplied player-local index on the GTA script thread. This inspector is intentionally read-only because no local write value was supplied.");

            if (state.localAvailable)
            {
                ImGui::Text("PLAYER_ID: %d", state.playerId);
                ImGui::Text("Resolved local: %zu", state.localIndex);
                ImGui::Text("Raw value: %d", state.localValue);
            }

            ImGui::SeparatorText("Status");
            if (state.pending)
                ImGui::TextDisabled("%s", state.message.c_str());
            else if (state.haveResult)
                ImGui::TextDisabled("%s: %s", state.lastSucceeded ? "Success" : "Failed", state.message.c_str());
            else
                ImGui::TextDisabled("Ready. Local inspection only runs while casino_lucky_wheel is active.");
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        SetV11Description("Casino Lucky Wheel tools for the supplied Enhanced 1.73 globals plus a validated read-only casino_lucky_wheel player-local inspector.");
    }
}
