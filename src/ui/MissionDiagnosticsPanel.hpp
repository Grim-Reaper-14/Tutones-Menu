#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/game/MissionDiagnosticsRuntime.hpp"

#include <imgui.h>

namespace Tutones::UI
{
    namespace MissionDiagnosticsDetail
    {
        [[nodiscard]] inline const char* LaunchStateName(int state) noexcept
        {
            switch (state)
            {
            case 0: return "WAITING";
            case 1: return "SETUP";
            case 2: return "ACTIVE";
            case 7: return "CLEANUP";
            default: return "INTERMEDIATE";
            }
        }

        [[nodiscard]] inline const char* EntityPhaseName(int phase) noexcept
        {
            switch (phase)
            {
            case 0: return "WAITING ENTITIES";
            case 1: return "ENTITY ACTIVE";
            case 2: return "OCCUPANT ACTIVE";
            case 3: return "TRANSITION";
            case 4: return "COMPLETE";
            default: return "UNKNOWN";
            }
        }
    }

    inline void RenderMissionDiagnosticsPanel() noexcept
    {
        using namespace MissionDiagnosticsDetail;

        auto& runtime = Game::MissionDiagnostics::Runtime::Get();
        const auto snapshot = runtime.GetSnapshot();

        ImGui::TextWrapped(
            "Read-only am_mission_launch state from the Enhanced decompile. Tutones does not write mission locals from this page.");
        ImGui::Spacing();

        ImGui::BeginDisabled(snapshot.pending);
        if (ImGui::Button(snapshot.pending ? "Refreshing Mission..." : "Refresh Mission State", ImVec2(-1.0f, 28.0f)))
            static_cast<void>(runtime.QueueRefresh());
        ImGui::EndDisabled();
        DescribeLastV11Item("Read the validated am_mission_launch locals through Tutones' shared ScriptRuntime.");

        if (!snapshot.haveResult)
        {
            ImGui::TextDisabled("Refresh while a mission-launch controller is active.");
            SetV11Description("Mission Control - read-only Enhanced mission launch state.");
            return;
        }

        if (!snapshot.threadFound)
        {
            ImGui::TextDisabled("am_mission_launch: IDLE");
            ImGui::TextWrapped("%s", snapshot.message.c_str());
            SetV11Description("Mission Control - am_mission_launch is not currently running.");
            return;
        }

        if (!snapshot.lastSucceeded)
        {
            ImGui::TextWrapped("%s", snapshot.message.c_str());
            ImGui::TextDisabled("Mission-local writes remain disabled.");
            SetV11Description("Mission Control - Enhanced mission layout failed validation.");
            return;
        }

        if (ImGui::BeginTable(
                "##mission_launch_state",
                2,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthStretch, 1.4f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 1.6f);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Launch phase");
            ImGui::TableSetColumnIndex(1); ImGui::TextColored(V11Theme::Accent, "%s (%d)", LaunchStateName(snapshot.launchState), snapshot.launchState);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Entity phase");
            ImGui::TableSetColumnIndex(1); ImGui::Text("%s (%d)", EntityPhaseName(snapshot.entityPhase), snapshot.entityPhase);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Mission variant");
            ImGui::TableSetColumnIndex(1); ImGui::Text("%d", snapshot.missionVariant);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("State flags");
            ImGui::TableSetColumnIndex(1); ImGui::Text("0x%08X", snapshot.flags);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Thread");
            ImGui::TableSetColumnIndex(1); ImGui::Text("%u | PC %u", snapshot.threadId, snapshot.programCounter);

            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Host-controlled state machine; values are observed, never forced.");
        SetV11Description("Mission Control - live Enhanced launch phase, entity phase, variant and state flags.");
    }
}
