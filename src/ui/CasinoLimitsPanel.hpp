#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/recovery/CasinoLimitsRuntime.hpp"

#include <imgui.h>

namespace Tutones::UI
{
    inline void RenderCasinoLimitsPanel() noexcept
    {
        auto& runtime = Game::Recovery::CasinoLimitsRuntime::Get();
        const auto state = runtime.Snapshot();

        ImGui::TextColored(V11Theme::Accent, "CASINO LIMITS");
        ImGui::SameLine();
        ImGui::TextDisabled("READ-ONLY DIAGNOSTICS");
        ImGui::TextDisabled("Reads the current casino win counter, timestamp and named Rockstar tunables without changing them.");
        ImGui::Separator();

        if (ImGui::BeginTable("##casino_limits_columns", 2, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn();
            if (ImGui::BeginChild("##casino_limits_source", ImVec2(0.0f, 382.0f), true))
            {
                ImGui::TextColored(V11Theme::Accent, "DAILY LIMIT SOURCES");
                ImGui::Spacing();

                ImGui::BeginDisabled(state.pending);
                if (ImGui::Button("REFRESH CASINO LIMITS", ImVec2(-1.0f, 38.0f)))
                    static_cast<void>(runtime.QueueRefresh());
                ImGui::EndDisabled();
                DescribeLastV11Item("Read MPPLY_CASINO_CHIPS_WON_GD, MPPLY_CASINO_CHIPS_WONTIM and resolve the named daily-win/cooldown tunables through the central tunable registry. This page does not write casino values.");

                ImGui::Spacing();
                ImGui::SeparatorText("Runtime");
                ImGui::Text("GTA Online session");
                ImGui::SameLine(190.0f);
                ImGui::TextColored(
                    state.sessionStarted ? ImVec4(0.20f, 0.88f, 0.42f, 1.0f) : V11Theme::MutedText,
                    "%s",
                    state.sessionStarted ? "YES" : "NO");

                ImGui::Text("Tunable registry");
                ImGui::SameLine(190.0f);
                ImGui::TextColored(
                    state.tunableRegistryReady ? ImVec4(0.20f, 0.88f, 0.42f, 1.0f) : V11Theme::MutedText,
                    "%s",
                    state.tunableRegistryReady ? "READY" : "WAITING");

                ImGui::SeparatorText("Rockstar Stats");
                ImGui::Text("CHIPS_WON_GD");
                ImGui::SameLine(190.0f);
                if (state.chipsWonReadable)
                    ImGui::Text("%d", state.chipsWon);
                else
                    ImGui::TextDisabled("unresolved");

                ImGui::Text("CHIPS_WONTIM");
                ImGui::SameLine(190.0f);
                if (state.winTimestampReadable)
                    ImGui::Text("%d", state.winTimestamp);
                else
                    ImGui::TextDisabled("unresolved");

                ImGui::SeparatorText("Named Tunables");
                ImGui::TextWrapped("VC_CASINO_CHIP_MAX_WIN_DAILY");
                if (state.maxDailyWinReadable)
                    ImGui::Text("Value: %d chips", state.maxDailyWin);
                else
                    ImGui::TextDisabled("Not resolved by tunable registry yet.");

                ImGui::TextWrapped("VC_CASINO_CHIP_MAX_WIN_LOSS_COOLDOWN");
                if (state.cooldownReadable)
                    ImGui::Text("Value: %d seconds", state.cooldownSeconds);
                else
                    ImGui::TextDisabled("Not resolved by tunable registry yet.");
            }
            ImGui::EndChild();

            ImGui::TableNextColumn();
            if (ImGui::BeginChild("##casino_limits_state", ImVec2(0.0f, 382.0f), true))
            {
                ImGui::TextColored(V11Theme::Accent, "CALCULATED STATE");
                ImGui::TextDisabled("Cooldown timing is estimated against the local POSIX clock for this diagnostic pass.");
                ImGui::Spacing();

                if (state.chipsWonReadable && state.maxDailyWinReadable)
                {
                    ImGui::Text("Daily wins");
                    ImGui::SameLine(185.0f);
                    ImGui::Text("%d / %d", state.chipsWon, state.maxDailyWin);

                    ImGui::Text("Remaining to limit");
                    ImGui::SameLine(185.0f);
                    ImGui::Text("%lld", static_cast<long long>(state.winRemaining));

                    ImGui::Text("Win limit reached");
                    ImGui::SameLine(185.0f);
                    ImGui::TextColored(
                        state.winLimitReached ? ImVec4(0.95f, 0.55f, 0.24f, 1.0f) : ImVec4(0.20f, 0.88f, 0.42f, 1.0f),
                        "%s",
                        state.winLimitReached ? "YES" : "NO");
                }
                else
                {
                    ImGui::TextDisabled("Daily win state unavailable until the stat and tunable both resolve.");
                }

                ImGui::SeparatorText("Cooldown Estimate");
                if (state.winTimestampReadable && state.cooldownReadable)
                {
                    ImGui::Text("Elapsed");
                    ImGui::SameLine(185.0f);
                    ImGui::Text("%.2f min", static_cast<double>(state.elapsedSeconds) / 60.0);

                    ImGui::Text("Remaining");
                    ImGui::SameLine(185.0f);
                    ImGui::Text("%.2f min", static_cast<double>(state.remainingSeconds) / 60.0);

                    ImGui::Text("Cooldown active");
                    ImGui::SameLine(185.0f);
                    ImGui::TextColored(
                        state.cooldownActive ? ImVec4(0.95f, 0.55f, 0.24f, 1.0f) : ImVec4(0.20f, 0.88f, 0.42f, 1.0f),
                        "%s",
                        state.cooldownActive ? "YES" : "NO");
                }
                else
                {
                    ImGui::TextDisabled("Cooldown estimate unavailable until timestamp and cooldown resolve.");
                }

                ImGui::SeparatorText("Status");
                if (state.pending)
                    ImGui::TextWrapped("Working: %s", state.message.c_str());
                else if (state.haveResult)
                    ImGui::TextWrapped("%s: %s", state.lastSucceeded ? "Success" : "Partial/Failed", state.message.c_str());
                else
                    ImGui::TextDisabled("Press Refresh Casino Limits after joining GTA Online.");
            }
            ImGui::EndChild();

            ImGui::EndTable();
        }

        SetV11Description("Casino Limits is a read-only Enhanced diagnostic page. It reads the daily casino win stat/timestamp and resolves Rockstar's named maximum-win and win/loss-cooldown tunables at runtime, avoiding stale hard-coded Global_262145 offsets.");
    }
}
