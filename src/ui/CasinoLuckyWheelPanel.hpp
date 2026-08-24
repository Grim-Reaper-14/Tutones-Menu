#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/recovery/CasinoLuckyWheelRuntime.hpp"
#include "../features/recovery/CasinoSlotMachineRuntime.hpp"

#include <imgui.h>

namespace Tutones::UI
{
    inline void RenderCasinoLuckyWheelPanel() noexcept
    {
        using Game::Recovery::CasinoLuckyWheelRuntime;
        using Game::Recovery::CasinoSlotMachineRuntime;

        static int casinoPage = 0;

        auto& wheelRuntime = CasinoLuckyWheelRuntime::Get();
        auto& slotRuntime = CasinoSlotMachineRuntime::Get();
        slotRuntime.Tick();

        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 4.0f));
        if (casinoPage == 0)
            ImGui::PushStyleColor(ImGuiCol_Button, V11Theme::AccentDark);
        if (ImGui::Button("Lucky Wheel", ImVec2(150.0f, 30.0f)))
            casinoPage = 0;
        if (casinoPage == 0)
            ImGui::PopStyleColor();

        ImGui::SameLine();
        if (casinoPage == 1)
            ImGui::PushStyleColor(ImGuiCol_Button, V11Theme::AccentDark);
        if (ImGui::Button("Rig Slot Machines", ImVec2(180.0f, 30.0f)))
            casinoPage = 1;
        if (casinoPage == 1)
            ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        ImGui::SetCursorPos(ImVec2(226.0f, 54.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##casino_tools_panel", ImVec2(620.0f, 390.0f), true))
        {
            if (casinoPage == 0)
            {
                const auto state = wheelRuntime.Snapshot();

                ImGui::TextColored(V11Theme::Accent, "Casino Lucky Wheel");
                ImGui::SameLine();
                ImGui::TextDisabled("Enhanced 1.73 / b1158.13");
                ImGui::Separator();

                ImGui::TextWrapped("Uses the supplied Enhanced Lucky Wheel globals and validates each write with read-back verification.");
                ImGui::Spacing();

                ImGui::BeginDisabled(state.pending);
                if (ImGui::Button("Apply supplied Lucky Wheel globals", ImVec2(-1.0f, 0.0f)))
                    static_cast<void>(wheelRuntime.QueueApplySuppliedGlobals());
                ImGui::EndDisabled();
                DescribeLastV11Item("Apply Global_262145.f_26855=1, f_26856=1 and f_37458=2 using the supplied Enhanced 1.73 mapping.");

                ImGui::SeparatorText("casino_lucky_wheel Player Local");
                ImGui::TextDisabled("Supplied layout: local 150 + (PLAYER_ID * 5)");
                ImGui::BeginDisabled(state.pending);
                if (ImGui::Button("Inspect active player local", ImVec2(-1.0f, 0.0f)))
                    static_cast<void>(wheelRuntime.QueueInspectPlayerLocal());
                ImGui::EndDisabled();
                DescribeLastV11Item("Resolve casino_lucky_wheel, PLAYER_ID and the supplied player-local index on the GTA script thread.");

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

                SetV11Description("Casino Lucky Wheel tools for Enhanced 1.73 globals plus the validated casino_lucky_wheel player-local inspector.");
            }
            else
            {
                auto state = slotRuntime.Snapshot();
                bool enabled = state.enabled;

                ImGui::TextColored(V11Theme::Accent, "Rig Slot Machines");
                ImGui::SameLine();
                ImGui::TextDisabled("casino_slots");
                ImGui::Separator();

                ImGui::TextWrapped("Forces the current Enhanced casino_slots random-result table to result 6 while the script is active and its spin state is safe.");
                ImGui::Spacing();

                if (ImGui::Checkbox("Rig Slot Machines", &enabled))
                    slotRuntime.SetEnabled(enabled);
                DescribeLastV11Item("When enabled, write result 6 to casino_slots locals 1357 + [3..196], excluding 9, 21, 22, 87 and 152, only at spin states 8 or 14.");

                ImGui::SeparatorText("Runtime");
                ImGui::Text("Script active: %s", state.scriptActive ? "Yes" : "No");
                if (state.scriptActive)
                {
                    ImGui::Text("Spin state: %d", state.spinState);
                    ImGui::Text("Safe write state: %s", state.safeSpinState ? "Yes" : "No");
                    ImGui::Text("Last writes: %zu", state.lastWriteCount);
                }
                ImGui::Text("Reset pending: %s", state.restorePending ? "Yes" : "No");

                ImGui::SeparatorText("Status");
                if (state.haveResult)
                    ImGui::TextDisabled("%s: %s", state.lastSucceeded ? "Success" : "Waiting/Failed", state.message.c_str());
                else
                    ImGui::TextDisabled("%s", state.message.c_str());

                ImGui::Spacing();
                ImGui::TextWrapped("Disabling waits for a safe spin state, then replaces the forced result table with normal 3-9 values instead of leaving forced wins behind.");

                SetV11Description("Rig Slot Machines uses the current Enhanced casino_slots result-table mapping with script/thread checks, safe spin-state gating, read-back validation and cleanup on disable.");
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
