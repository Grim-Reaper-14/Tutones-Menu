#pragma once

#include "V11Theme.hpp"
#include "../features/business/AgencyRuntime.hpp"

#include <imgui.h>

#include <algorithm>

namespace Tutones::UI
{
    inline void RenderAgencyBusinessPanel() noexcept
    {
        using namespace Game::Business::AgencyEnhanced173;
        using Game::Business::AgencyRuntime;

        auto& runtime = AgencyRuntime::Get();
        const auto state = runtime.Snapshot();
        static int selectedSlot = 0;
        static int selectedType = 0;
        static int selectedDifficulty = 0;
        static int selectedStoryIndex = 11;
        static int selectedPayout = 1000000;
        static bool payoutInitialized = false;

        selectedStoryIndex = std::clamp(selectedStoryIndex, 0, static_cast<int>(StoryContractValues.size()) - 1);
        selectedPayout = std::clamp(selectedPayout, 0, MaximumFinalePayout);
        if (!payoutInitialized && state.finalePayoutReadable)
        {
            selectedPayout = std::clamp(state.finalePayout, 0, MaximumFinalePayout);
            payoutInitialized = true;
        }

        ImGui::SetCursorPos(ImVec2(226.0f, 52.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##agency_business_panel", ImVec2(490.0f, 394.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Agency");
            ImGui::SameLine();
            ImGui::TextDisabled("The Contract / Fixer flow");
            ImGui::Separator();
            ImGui::TextDisabled("Enhanced Fixer stats, board globals and central tunables");

            ImGui::SeparatorText("Dr. Dre Contract / Preps");
            const int storyValue = StoryContractValues[static_cast<std::size_t>(selectedStoryIndex)];
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##agency_story_contract", StoryContractName(storyValue)))
            {
                for (int index = 0; index < static_cast<int>(StoryContractValues.size()); ++index)
                {
                    const int value = StoryContractValues[static_cast<std::size_t>(index)];
                    const bool selected = index == selectedStoryIndex;
                    if (ImGui::Selectable(StoryContractName(value), selected))
                        selectedStoryIndex = index;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::BeginDisabled(state.pending);
            if (ImGui::Button("Set Dre Contract + Complete Preps", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueApplyStoryContract(storyValue));

            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputInt("Finale payout##agency_payout", &selectedPayout, 50000, 250000);
            selectedPayout = std::clamp(selectedPayout, 0, MaximumFinalePayout);
            if (ImGui::Button("Apply Finale Payout", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueSetFinalePayout(selectedPayout));

            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float halfWidth = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;
            if (ImGui::Button("Kill All Cooldowns", ImVec2(halfWidth, 0.0f)))
                static_cast<void>(runtime.QueueKillCooldowns());
            ImGui::SameLine();
            if (ImGui::Button("Collect Agency Safe", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueCollectSafe());
            ImGui::EndDisabled();

            ImGui::TextDisabled("Kill All Cooldowns clears Dre replay, Security Contract and Payphone cooldowns using the current Enhanced tunables.");

            ImGui::SeparatorText("Security Contract Board Editor");
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##agency_slot", selectedSlot == 0 ? "Board Slot 1" : selectedSlot == 1 ? "Board Slot 2" : "Board Slot 3"))
            {
                if (ImGui::Selectable("Board Slot 1", selectedSlot == 0))
                    selectedSlot = 0;
                if (ImGui::Selectable("Board Slot 2", selectedSlot == 1))
                    selectedSlot = 1;
                if (ImGui::Selectable("Board Slot 3", selectedSlot == 2))
                    selectedSlot = 2;
                ImGui::EndCombo();
            }

            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##agency_type", SecurityContractName(selectedType)))
            {
                for (int type = 0; type < ContractTypes; ++type)
                {
                    const bool selected = type == selectedType;
                    if (ImGui::Selectable(SecurityContractName(type), selected))
                        selectedType = type;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##agency_difficulty", SecurityContractDifficultyName(selectedDifficulty)))
            {
                for (int difficulty = 0; difficulty < ContractDifficulties; ++difficulty)
                {
                    const bool selected = difficulty == selectedDifficulty;
                    if (ImGui::Selectable(SecurityContractDifficultyName(difficulty), selected))
                        selectedDifficulty = difficulty;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::BeginDisabled(state.pending);
            if (ImGui::Button("Apply Security Contract Slot", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueSetContractSlot(selectedSlot, selectedType, selectedDifficulty));

            if (ImGui::Button("Clear Current Board Delay", ImVec2(halfWidth, 0.0f)))
                static_cast<void>(runtime.QueueClearSecurityContractDelay());
            ImGui::SameLine();
            if (ImGui::Button("Clear Dre Flow Timestamp", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueClearStoryCooldown());

            if (ImGui::Button("Refresh Agency State", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueRefresh());
            ImGui::EndDisabled();
            ImGui::TextDisabled("If The Contract app was already open, back out and reopen it after changing board or story state.");

            ImGui::SeparatorText("Current Security Contract Board");
            for (std::size_t index = 0; index < state.contracts.size(); ++index)
            {
                const auto& contract = state.contracts[index];
                ImGui::PushID(static_cast<int>(index));
                if (contract.readable)
                {
                    ImGui::Text("Slot %d: %s", static_cast<int>(index) + 1, SecurityContractName(contract.type));
                    ImGui::TextDisabled("%s | Reward $%d", SecurityContractDifficultyName(contract.difficulty), contract.reward);
                }
                else
                {
                    ImGui::Text("Slot %d: --", static_cast<int>(index) + 1);
                }
                ImGui::PopID();
            }

            ImGui::SeparatorText("Agency Progress");
            ImGui::Text("Persistent Dre state: %s", StoryContractName(state.persistentStoryBits));
            ImGui::Text("Persistent strand: %d", state.persistentStoryStrand);
            ImGui::Text("Persistent general/completed: 0x%08X / 0x%08X",
                static_cast<std::uint32_t>(state.persistentGeneralBits),
                static_cast<std::uint32_t>(state.persistentCompletedBits));
            ImGui::Text("Security contracts completed: %d", state.contractCount);
            ImGui::Text("Agency earnings: $%d", state.earnings);
            ImGui::Text("Agency safe: $%d", state.safeCash);
            ImGui::Text("Flow story strand/cooldown: %d / %d", state.storyStrand, state.storyCooldown);
            ImGui::Text("Persistent story cooldown: %d", state.persistentStoryCooldown);
            ImGui::Text("Contract short delay: %s", state.securityContractDelayActive ? "ACTIVE" : "CLEAR");
            if (state.finalePayoutReadable)
                ImGui::Text("Current finale payout tunable: $%d", state.finalePayout);
            else
                ImGui::TextDisabled("Finale payout tunable: waiting for Tunable Registry");

            ImGui::SeparatorText("Live Fixer Flags");
            ImGui::Text("General: 0x%08X", state.generalFlags);
            ImGui::Text("Completed: 0x%08X", state.completedFlags);
            ImGui::Text("Story: 0x%08X", state.storyFlags);
            ImGui::Text("Fixer: 0x%08X", state.fixerFlags);
            ImGui::Text("Payphone bonus method: %d", state.payphoneBonusMethod);
            ImGui::Text("Short Trips flags: 0x%08X", state.shortTrips);

            ImGui::SeparatorText("Runtime");
            ImGui::Text("Session: %s", state.sessionStarted ? "Online" : "Offline");
            ImGui::Text("Security app: %s", state.securityAppRunning ? "Running" : "Idle");
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
