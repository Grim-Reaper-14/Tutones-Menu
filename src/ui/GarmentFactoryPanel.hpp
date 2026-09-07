#pragma once

#include "V11Theme.hpp"
#include "../features/business/GarmentFactoryRuntime.hpp"

#include <imgui.h>

#include <algorithm>

namespace Tutones::UI
{
    inline void RenderGarmentFactoryPanel() noexcept
    {
        using namespace Game::Business::GarmentFactoryEnhanced173;
        using Game::Business::GarmentFactoryRuntime;

        auto& runtime = GarmentFactoryRuntime::Get();
        const auto state = runtime.Snapshot();
        static int selectedFile = 0;
        selectedFile = std::clamp(selectedFile, 0, FileCount - 1);

        ImGui::SetCursorPos(ImVec2(226.0f, 52.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##garment_factory_panel", ImVec2(490.0f, 394.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Garment Factory");
            ImGui::SameLine();
            ImGui::TextDisabled("Agents of Sabotage / FIB Files");
            ImGui::Separator();
            ImGui::TextDisabled("HACKER24 persistent stat controls + live Enhanced flow");

            ImGui::SeparatorText("FIB File Editor");
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##fib_file_select", FibFileName(selectedFile)))
            {
                for (int file = 0; file < FileCount; ++file)
                {
                    const bool selected = file == selectedFile;
                    if (ImGui::Selectable(FibFileName(file), selected))
                        selectedFile = file;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::BeginDisabled(state.pending);
            if (ImGui::Button("Set Active FIB File", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueSetActiveFile(selectedFile));

            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float halfWidth = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;
            if (ImGui::Button("Complete All Preps", ImVec2(halfWidth, 0.0f)))
                static_cast<void>(runtime.QueueSetPrepsComplete(true));
            ImGui::SameLine();
            if (ImGui::Button("Reset Preps", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueSetPrepsComplete(false));

            if (ImGui::Button("Unbrick Computer", ImVec2(halfWidth, 0.0f)))
                static_cast<void>(runtime.QueueUnbrickComputer());
            ImGui::SameLine();
            if (ImGui::Button("Collect Factory Safe", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueCollectSafe());

            if (ImGui::Button("Refresh Garment Factory", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueRefresh());
            ImGui::EndDisabled();

            ImGui::TextDisabled("After changing file or prep state, reopen the Garment Factory computer if its board was already loaded.");
            ImGui::TextDisabled("Unbrick Computer resets HACKER24_GEN_BS to the known working recovery value; use it only when the computer is stuck.");

            ImGui::SeparatorText("Current FIB File State");
            ImGui::Text("Persistent active file: %s", FibFileName(state.activeRobberyStat));
            ImGui::Text("Live flow active file: %s", FibFileName(state.activeRobbery));
            ImGui::Text("Preps: %s", state.prepsComplete ? "COMPLETE" : "INCOMPLETE");
            ImGui::Text("HACKER24_GEN_BS: %d / 0x%08X", state.generalBitsStat, static_cast<std::uint32_t>(state.generalBitsStat));
            ImGui::Text("Factory safe cash: $%d", state.safeCash);

            ImGui::SeparatorText("Live Hacker24 Flow");
            ImGui::Text("General flags: 0x%08X", state.generalFlags);
            ImGui::Text("Instance flags: 0x%08X", state.instanceFlags);
            ImGui::Text("Hacker flags: 0x%08X", state.hackerFlags);
            ImGui::Text("Unknown slot 3: %d", state.unknown3);
            ImGui::Text("Unknown slot 5: %d", state.unknown5);
            ImGui::Text("Packed 51273/4/5: %s / %s / %s",
                state.packedBool51273 ? "SET" : "CLEAR",
                state.packedBool51274 ? "SET" : "CLEAR",
                state.packedBool51275 ? "SET" : "CLEAR");

            ImGui::SeparatorText("Runtime");
            ImGui::Text("Session: %s", state.sessionStarted ? "Online" : "Offline");
            ImGui::Text("Hacker app: %s", state.hackerTruckAppRunning ? "Running" : "Idle");
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
