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
        static int selectedPrize = 18;
        static constexpr const char* PrizeNames[] = {
            "0 - Clothing",
            "1 - 2,500 RP",
            "2 - $20,000",
            "3 - 10,000 Chips",
            "4 - 10% Discount Voucher",
            "5 - 5,000 RP",
            "6 - $30,000",
            "7 - 15,000 Chips",
            "8 - Clothing",
            "9 - 7,500 RP",
            "10 - 20,000 Chips",
            "11 - Mystery Prize",
            "12 - Clothing",
            "13 - 10,000 RP",
            "14 - $40,000",
            "15 - 25,000 Chips",
            "16 - Clothing",
            "17 - 15,000 RP",
            "18 - Podium Vehicle",
            "19 - $50,000",
        };

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

                ImGui::TextWrapped("Choose the exact wheel reward, apply it while casino_lucky_wheel is active, then spin the wheel.");
                ImGui::Spacing();

                ImGui::SeparatorText("Prize Selector");
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::Combo("##lucky_wheel_prize", &selectedPrize, PrizeNames, IM_ARRAYSIZE(PrizeNames));
                DescribeLastV11Item("Select the exact Lucky Wheel prize outcome value from 0 through 19.");

                ImGui::BeginDisabled(state.pending);
                if (ImGui::Button("Apply Selected Prize", ImVec2(-1.0f, 0.0f)))
                    static_cast<void>(wheelRuntime.QueueSetPrize(selectedPrize));
                ImGui::EndDisabled();
                DescribeLastV11Item("Enable the supplied additional-spin globals and write the selected prize into the active casino_lucky_wheel per-player prize local with read-back verification.");

                ImGui::TextDisabled("Selected: %s", PrizeNames[selectedPrize]);

                ImGui::SeparatorText("Wheel Globals");
                ImGui::BeginDisabled(state.pending);
                if (ImGui::Button("Apply supplied Lucky Wheel globals", ImVec2(-1.0f, 0.0f)))
                    static_cast<void>(wheelRuntime.QueueApplySuppliedGlobals());
                ImGui::EndDisabled();
                DescribeLastV11Item("Apply Global_262145.f_26855=1, f_26856=1 and f_37458=2 using the supplied Enhanced 1.73 mapping.");

                ImGui::SeparatorText("Current Prize Local");
                ImGui::BeginDisabled(state.pending);
                if (ImGui::Button("Refresh Current Prize", ImVec2(-1.0f, 0.0f)))
                    static_cast<void>(wheelRuntime.QueueInspectPlayerLocal());
                ImGui::EndDisabled();
                DescribeLastV11Item("Read the active casino_lucky_wheel per-player prize outcome local without changing it.");

                if (state.localAvailable)
                {
                    ImGui::Text("PLAYER_ID: %d", state.playerId);
                    ImGui::Text("Resolved prize local: %zu", state.localIndex);
                    if (state.localValue >= CasinoLuckyWheelRuntime::MinPrize
                        && state.localValue <= CasinoLuckyWheelRuntime::MaxPrize)
                    {
                        ImGui::Text("Current prize: %s", PrizeNames[state.localValue]);
                    }
                    else
                    {
                        ImGui::Text("Current raw prize value: %d", state.localValue);
                    }
                }

                ImGui::SeparatorText("Status");
                if (state.pending)
                    ImGui::TextDisabled("%s", state.message.c_str());
                else if (state.haveResult)
                    ImGui::TextDisabled("%s: %s", state.lastSucceeded ? "Success" : "Failed", state.message.c_str());
                else
                    ImGui::TextDisabled("Ready. Open/approach the Lucky Wheel so casino_lucky_wheel is active before applying a prize.");

                SetV11Description("Select any Lucky Wheel reward from the 0-19 prize list and apply it directly to the active Enhanced casino_lucky_wheel player prize local before spinning.");
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
