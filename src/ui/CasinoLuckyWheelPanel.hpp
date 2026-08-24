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

        // V12 inner navigation: large dashboard tabs, separate from Recovery tabs.
        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);

        const auto pageButton = [&](const char* label, int page, float width)
        {
            const bool selected = casinoPage == page;
            if (selected)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(
                    V11Theme::Accent.x,
                    V11Theme::Accent.y,
                    V11Theme::Accent.z,
                    0.28f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(
                    V11Theme::Accent.x,
                    V11Theme::Accent.y,
                    V11Theme::Accent.z,
                    0.82f));
            }

            if (ImGui::Button(label, ImVec2(width, 34.0f)))
                casinoPage = page;

            if (selected)
                ImGui::PopStyleColor(2);
        };

        pageButton("Lucky Wheel", 0, 170.0f);
        ImGui::SameLine();
        pageButton("Rig Slot Machines", 1, 190.0f);
        ImGui::PopStyleVar(2);

        ImGui::SetCursorPos(ImVec2(226.0f, 60.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##casino_v12_panel", ImVec2(780.0f, 500.0f), true))
        {
            if (casinoPage == 0)
            {
                const auto state = wheelRuntime.Snapshot();

                ImGui::TextColored(V11Theme::Accent, "LUCKY WHEEL");
                ImGui::SameLine();
                ImGui::TextDisabled("Enhanced 1.73 / b1158.13");
                ImGui::TextDisabled("Pick the reward, write it to the active wheel script, then spin.");
                ImGui::Separator();

                if (ImGui::BeginTable("##lucky_wheel_v12_columns", 2, ImGuiTableFlags_SizingStretchSame))
                {
                    ImGui::TableNextColumn();
                    if (ImGui::BeginChild("##lucky_wheel_actions", ImVec2(0.0f, 382.0f), true))
                    {
                        ImGui::TextColored(V11Theme::Accent, "PRIZE CONTROL");
                        ImGui::TextDisabled("Choose any wheel outcome from 0 through 19.");
                        ImGui::Spacing();

                        ImGui::SetNextItemWidth(-1.0f);
                        ImGui::Combo("##lucky_wheel_prize", &selectedPrize, PrizeNames, IM_ARRAYSIZE(PrizeNames));
                        DescribeLastV11Item("Select the exact Lucky Wheel prize outcome value from 0 through 19.");

                        ImGui::TextDisabled("Selected reward");
                        ImGui::TextWrapped("%s", PrizeNames[selectedPrize]);
                        ImGui::Spacing();

                        ImGui::BeginDisabled(state.pending);
                        if (ImGui::Button("APPLY SELECTED PRIZE", ImVec2(-1.0f, 38.0f)))
                            static_cast<void>(wheelRuntime.QueueSetPrize(selectedPrize));
                        ImGui::EndDisabled();
                        DescribeLastV11Item("Enable the required additional-spin globals and write the selected reward into the active per-player casino_lucky_wheel prize local with read-back verification.");

                        ImGui::Spacing();
                        ImGui::SeparatorText("Wheel Globals");
                        ImGui::BeginDisabled(state.pending);
                        if (ImGui::Button("Apply Enhanced Wheel Globals", ImVec2(-1.0f, 0.0f)))
                            static_cast<void>(wheelRuntime.QueueApplySuppliedGlobals());
                        ImGui::EndDisabled();
                        DescribeLastV11Item("Apply Global_262145.f_26855=1, f_26856=1 and f_37458=2 using the supplied Enhanced 1.73 mapping.");
                    }
                    ImGui::EndChild();

                    ImGui::TableNextColumn();
                    if (ImGui::BeginChild("##lucky_wheel_runtime", ImVec2(0.0f, 382.0f), true))
                    {
                        ImGui::TextColored(V11Theme::Accent, "LIVE WHEEL STATUS");
                        ImGui::TextDisabled("Read the same prize local that the setter writes.");
                        ImGui::Spacing();

                        ImGui::BeginDisabled(state.pending);
                        if (ImGui::Button("Refresh Current Prize", ImVec2(-1.0f, 0.0f)))
                            static_cast<void>(wheelRuntime.QueueInspectPlayerLocal());
                        ImGui::EndDisabled();
                        DescribeLastV11Item("Read the active casino_lucky_wheel per-player prize outcome local without changing it.");

                        ImGui::SeparatorText("Current State");
                        if (state.localAvailable)
                        {
                            ImGui::Text("PLAYER_ID");
                            ImGui::SameLine(170.0f);
                            ImGui::Text("%d", state.playerId);
                            ImGui::Text("Prize local");
                            ImGui::SameLine(170.0f);
                            ImGui::Text("%zu", state.localIndex);
                            ImGui::Text("Current reward");
                            if (state.localValue >= CasinoLuckyWheelRuntime::MinPrize
                                && state.localValue <= CasinoLuckyWheelRuntime::MaxPrize)
                            {
                                ImGui::TextWrapped("%s", PrizeNames[state.localValue]);
                            }
                            else
                            {
                                ImGui::Text("Raw value: %d", state.localValue);
                            }
                        }
                        else
                        {
                            ImGui::TextDisabled("Wheel script/local not resolved yet.");
                        }

                        ImGui::SeparatorText("Status");
                        if (state.pending)
                            ImGui::TextWrapped("Working: %s", state.message.c_str());
                        else if (state.haveResult)
                            ImGui::TextWrapped("%s: %s", state.lastSucceeded ? "Success" : "Failed", state.message.c_str());
                        else
                            ImGui::TextDisabled("Approach/open the Lucky Wheel so casino_lucky_wheel is active.");
                    }
                    ImGui::EndChild();

                    ImGui::EndTable();
                }

                SetV11Description("Lucky Wheel V12: choose any reward from the 0-19 list, write it to the active Enhanced wheel player local, refresh the live value and verify the operation before spinning.");
            }
            else
            {
                const auto state = slotRuntime.Snapshot();
                bool enabled = state.enabled;

                ImGui::TextColored(V11Theme::Accent, "RIG SLOT MACHINES");
                ImGui::SameLine();
                ImGui::TextDisabled("DIRECT READ / WRITE");
                ImGui::TextDisabled("Use explicit table reads/writes or keep the win table continuously forced.");
                ImGui::Separator();

                if (ImGui::BeginTable("##slot_v12_columns", 2, ImGuiTableFlags_SizingStretchSame))
                {
                    ImGui::TableNextColumn();
                    if (ImGui::BeginChild("##slot_write_controls", ImVec2(0.0f, 382.0f), true))
                    {
                        ImGui::TextColored(V11Theme::Accent, "RESULT TABLE CONTROL");
                        ImGui::TextDisabled("casino_slots locals 1357 + [3..196]");
                        ImGui::Spacing();

                        ImGui::BeginDisabled(state.taskQueued);
                        if (ImGui::Button("READ CURRENT TABLE", ImVec2(-1.0f, 36.0f)))
                            static_cast<void>(slotRuntime.QueueReadResults());
                        DescribeLastV11Item("Read every supported casino_slots result entry and report how many currently equal win result 6.");

                        if (ImGui::Button("WRITE WIN TABLE NOW", ImVec2(-1.0f, 36.0f)))
                            static_cast<void>(slotRuntime.QueueWriteWinResults());
                        DescribeLastV11Item("Immediately write result 6 to every supported result entry and verify all values by reading the table back.");

                        if (ImGui::Button("RESET RESULT TABLE", ImVec2(-1.0f, 36.0f)))
                            static_cast<void>(slotRuntime.QueueResetResults());
                        DescribeLastV11Item("Replace the forced result table with normal 3-9 values and verify the table remains readable.");
                        ImGui::EndDisabled();

                        ImGui::Spacing();
                        ImGui::SeparatorText("Continuous Write");
                        if (ImGui::Checkbox("Keep Win Table Forced", &enabled))
                            slotRuntime.SetEnabled(enabled);
                        DescribeLastV11Item("Continuously rewrite supported result entries to 6 whenever the game changes them. The spin-state value is telemetry and does not block writes.");
                    }
                    ImGui::EndChild();

                    ImGui::TableNextColumn();
                    if (ImGui::BeginChild("##slot_live_status", ImVec2(0.0f, 382.0f), true))
                    {
                        ImGui::TextColored(V11Theme::Accent, "LIVE TABLE STATUS");
                        ImGui::Spacing();

                        ImGui::Text("Script active");
                        ImGui::SameLine(180.0f);
                        ImGui::TextColored(state.scriptActive ? ImVec4(0.20f, 0.88f, 0.42f, 1.0f) : V11Theme::MutedText, "%s", state.scriptActive ? "YES" : "NO");

                        if (state.scriptActive)
                        {
                            ImGui::Text("Spin state");
                            ImGui::SameLine(180.0f);
                            ImGui::Text("%d", state.spinState);
                            ImGui::Text("8 / 14 telemetry");
                            ImGui::SameLine(180.0f);
                            ImGui::Text("%s", state.safeSpinState ? "YES" : "NO");
                        }

                        ImGui::SeparatorText("Result Table");
                        if (state.tableReadable)
                        {
                            ImGui::Text("Readable entries");
                            ImGui::SameLine(180.0f);
                            ImGui::Text("%zu", state.tableEntryCount);
                            ImGui::Text("Win result 6");
                            ImGui::SameLine(180.0f);
                            ImGui::Text("%zu / %zu", state.forcedWinCount, state.tableEntryCount);
                        }
                        else
                        {
                            ImGui::TextDisabled("Result table has not been read yet.");
                        }

                        ImGui::Text("Last writes");
                        ImGui::SameLine(180.0f);
                        ImGui::Text("%zu", state.lastWriteCount);
                        ImGui::Text("Reset pending");
                        ImGui::SameLine(180.0f);
                        ImGui::Text("%s", state.restorePending ? "YES" : "NO");

                        ImGui::SeparatorText("Status");
                        if (state.taskQueued)
                            ImGui::TextWrapped("Working: %s", state.message.c_str());
                        else if (state.haveResult)
                            ImGui::TextWrapped("%s: %s", state.lastSucceeded ? "Success" : "Failed", state.message.c_str());
                        else
                            ImGui::TextDisabled("Sit at/use a casino slot machine so casino_slots is active.");
                    }
                    ImGui::EndChild();

                    ImGui::EndTable();
                }

                SetV11Description("Rig Slot Machines V12: direct live result-table read/write controls on the left and script/table verification telemetry on the right, plus optional continuous forced-win writes.");
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
    }
}
