#pragma once

#include "MissionDiagnosticsPanel.hpp"
#include "ServicesDiagnosticsPanel.hpp"
#include "StreetDealerPanel.hpp"
#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/game/EnhancedScriptCenterRuntime.hpp"

#include <imgui.h>

#include <cstddef>
#include <cstdio>

namespace Tutones::UI
{
    namespace EnhancedControlCenterDetail
    {
        using Game::EnhancedScripts::Area;
        using Game::EnhancedScripts::Definitions;
        using Game::EnhancedScripts::Runtime;
        using Game::EnhancedScripts::Snapshot;

        [[nodiscard]] inline const char* ThreadStateName(Game::Types::ScriptThreadState state) noexcept
        {
            switch (state)
            {
            case Game::Types::ScriptThreadState::Idle: return "IDLE";
            case Game::Types::ScriptThreadState::Running: return "RUNNING";
            case Game::Types::ScriptThreadState::Killed: return "KILLED";
            case Game::Types::ScriptThreadState::Paused: return "PAUSED";
            default: return "UNKNOWN";
            }
        }

        inline void RenderAreaTable(Area area, const Snapshot& snapshot) noexcept
        {
            if (!snapshot.haveResult)
            {
                ImGui::TextDisabled("Refresh the catalog to resolve this Enhanced script group.");
                return;
            }

            if (ImGui::BeginTable(
                    "##enhanced_script_area",
                    4,
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Feature", ImGuiTableColumnFlags_WidthStretch, 1.7f);
                ImGui::TableSetupColumn("Script", ImGuiTableColumnFlags_WidthStretch, 1.7f);
                ImGui::TableSetupColumn("Thread", ImGuiTableColumnFlags_WidthStretch, 0.9f);
                ImGui::TableSetupColumn("Program", ImGuiTableColumnFlags_WidthStretch, 0.9f);
                ImGui::TableHeadersRow();

                for (std::size_t index = 0; index < Definitions.size(); ++index)
                {
                    const auto& definition = Definitions[index];
                    if (definition.area != area)
                        continue;

                    const auto& state = snapshot.scripts[index];
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(definition.label);
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted(definition.semanticName);
                        ImGui::TextDisabled("Decompile: %s", definition.decompileFile);
                        if (state.programLoaded)
                        {
                            ImGui::Separator();
                            ImGui::Text("Code: %u bytes", state.codeSize);
                            ImGui::Text("Locals: %u", state.localCount);
                            ImGui::Text("Globals: %u", state.globalCount);
                            ImGui::Text("Natives: %u", state.nativeCount);
                        }
                        if (state.threadFound)
                        {
                            ImGui::Text("PC: %u", state.programCounter);
                            ImGui::Text("Stack: %u", state.stackSize);
                        }
                        ImGui::EndTooltip();
                    }

                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextDisabled("%s", definition.script);

                    ImGui::TableSetColumnIndex(2);
                    if (!state.threadFound)
                    {
                        ImGui::TextDisabled("IDLE");
                    }
                    else if (state.running)
                    {
                        ImGui::TextColored(V11Theme::Accent, "RUNNING");
                    }
                    else
                    {
                        ImGui::TextDisabled("%s", ThreadStateName(state.threadState));
                    }

                    ImGui::TableSetColumnIndex(3);
                    if (state.programLoaded)
                        ImGui::TextColored(V11Theme::Accent, "LOADED");
                    else
                        ImGui::TextDisabled("NOT LOADED");
                }

                ImGui::EndTable();
            }
        }

        inline void RenderResolver(const Snapshot& snapshot) noexcept
        {
            ImGui::TextWrapped(
                "Semantic names are the stable Tutones-facing layer. Raw Global_ and local indexes should sit behind a build-specific resolver and stay disabled until their current Enhanced layout is validated.");
            ImGui::Spacing();

            if (ImGui::BeginTable(
                    "##enhanced_semantic_resolver",
                    3,
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Tutones key", ImGuiTableColumnFlags_WidthStretch, 2.2f);
                ImGui::TableSetupColumn("Rockstar script", ImGuiTableColumnFlags_WidthStretch, 1.6f);
                ImGui::TableSetupColumn("Resolved", ImGuiTableColumnFlags_WidthStretch, 0.8f);
                ImGui::TableHeadersRow();

                for (std::size_t index = 0; index < Definitions.size(); ++index)
                {
                    const auto& definition = Definitions[index];
                    const auto& state = snapshot.scripts[index];
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(definition.semanticName);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextDisabled("%s", definition.script);
                    ImGui::TableSetColumnIndex(2);
                    if (snapshot.haveResult && (state.threadFound || state.programLoaded))
                        ImGui::TextColored(V11Theme::Accent, "YES");
                    else
                        ImGui::TextDisabled(snapshot.haveResult ? "NO" : "UNKNOWN");
                }
                ImGui::EndTable();
            }
        }
    }

    inline void RenderEnhancedControlCenterPanel() noexcept
    {
        using namespace EnhancedControlCenterDetail;

        auto& runtime = Runtime::Get();
        const auto snapshot = runtime.GetSnapshot();

        ImGui::SetCursorPos(ImVec2(226.0f, 52.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##enhanced_control_center", ImVec2(490.0f, 394.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Enhanced Control Center");
            ImGui::SameLine();
            ImGui::TextDisabled("Acid Labs decompile map");
            ImGui::Separator();

            ImGui::Text("Script runtime: %s", snapshot.scriptRuntimeReady ? "READY" : "WAITING");
            ImGui::SameLine();
            ImGui::TextDisabled("| Online: %s", snapshot.sessionStarted ? "YES" : "NO");

            ImGui::BeginDisabled(snapshot.pending);
            if (ImGui::Button(snapshot.pending ? "Refreshing..." : "Refresh Enhanced Catalog", ImVec2(-1.0f, 28.0f)))
                static_cast<void>(runtime.QueueRefresh());
            ImGui::EndDisabled();
            DescribeLastV11Item("Resolve the known Enhanced controllers through Tutones' shared ScriptRuntime on the GTA game thread.");

            if (snapshot.haveResult || snapshot.pending)
                ImGui::TextDisabled("%s", snapshot.message.c_str());

            ImGui::Spacing();
            if (ImGui::BeginTabBar("##enhanced_control_tabs"))
            {
                if (ImGui::BeginTabItem("Business"))
                {
                    RenderAreaTable(Area::Business, snapshot);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Dealer"))
                {
                    RenderStreetDealerPanel();
                    ImGui::SeparatorText("Controller");
                    RenderAreaTable(Area::StreetDealer, snapshot);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Services"))
                {
                    RenderServicesDiagnosticsPanel();
                    ImGui::SeparatorText("Controller");
                    RenderAreaTable(Area::Services, snapshot);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Mission"))
                {
                    RenderMissionDiagnosticsPanel();
                    ImGui::SeparatorText("Controllers");
                    RenderAreaTable(Area::Mission, snapshot);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Today"))
                {
                    ImGui::TextWrapped("Foundation for rotating daily activities, dealer state and reset tracking without exposing unverified locals.");
                    RenderAreaTable(Area::Daily, snapshot);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Properties"))
                {
                    RenderAreaTable(Area::Property, snapshot);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Latest DLC"))
                {
                    RenderAreaTable(Area::Dlc, snapshot);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Resolver"))
                {
                    RenderResolver(snapshot);
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        SetV11Description("Enhanced Control Center: semantic script resolver and live controller diagnostics sourced from the GTA V Enhanced decompile layout.");
    }
}
