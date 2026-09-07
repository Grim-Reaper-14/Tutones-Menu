#pragma once

#include "V11Theme.hpp"
#include "../features/business/AgencyRuntime.hpp"

#include <imgui.h>

namespace Tutones::UI
{
    inline void RenderAgencyBusinessPanel() noexcept
    {
        using namespace Game::Business::AgencyEnhanced173;
        using Game::Business::AgencyRuntime;

        auto& runtime = AgencyRuntime::Get();
        const auto state = runtime.Snapshot();

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
            ImGui::TextDisabled("Enhanced source: appfixersecurity.c");

            ImGui::BeginDisabled(state.pending);
            if (ImGui::Button("Refresh Agency State", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueRefresh());
            ImGui::EndDisabled();

            ImGui::SeparatorText("Security Contract Board");
            for (std::size_t index = 0; index < state.contracts.size(); ++index)
            {
                const auto& contract = state.contracts[index];
                ImGui::PushID(static_cast<int>(index));
                if (contract.readable)
                {
                    ImGui::Text("Slot %d: %s", static_cast<int>(index) + 1, SecurityContractName(contract.type));
                    ImGui::TextDisabled(
                        "Type %d | Difficulty %d | Reward $%d",
                        contract.type,
                        contract.difficulty,
                        contract.reward);
                }
                else
                {
                    ImGui::Text("Slot %d: --", static_cast<int>(index) + 1);
                }
                ImGui::PopID();
            }

            ImGui::SeparatorText("Agency Progress");
            ImGui::Text("Security contracts completed: %d", state.contractCount);
            ImGui::Text("Agency earnings: $%d", state.earnings);
            ImGui::Text("Story strand: %d", state.storyStrand);
            ImGui::Text("Story cooldown: %d", state.storyCooldown);
            ImGui::Text("Payphone bonus method: %d", state.payphoneBonusMethod);
            ImGui::Text("Short Trips flags: 0x%08X", state.shortTrips);

            ImGui::SeparatorText("Live Fixer Flags");
            ImGui::Text("General: 0x%08X", state.generalFlags);
            ImGui::Text("Completed: 0x%08X", state.completedFlags);
            ImGui::Text("Story: 0x%08X", state.storyFlags);
            ImGui::Text("Fixer: 0x%08X", state.fixerFlags);
            ImGui::TextDisabled("These fields stay read-only here until each write path and cooldown semantic is verified independently.");

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
