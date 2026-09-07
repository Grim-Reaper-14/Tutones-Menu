#pragma once

#include "V11Theme.hpp"
#include "../features/business/GarmentFactoryRuntime.hpp"

#include <imgui.h>

namespace Tutones::UI
{
    inline void RenderGarmentFactoryPanel() noexcept
    {
        using Game::Business::GarmentFactoryRuntime;

        auto& runtime = GarmentFactoryRuntime::Get();
        const auto state = runtime.Snapshot();

        ImGui::SetCursorPos(ImVec2(226.0f, 52.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##garment_factory_panel", ImVec2(490.0f, 394.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Garment Factory");
            ImGui::SameLine();
            ImGui::TextDisabled("Agents of Sabotage / Hacker24 flow");
            ImGui::Separator();
            ImGui::TextDisabled("Enhanced app: apphackertruck.c");
            ImGui::TextWrapped("This first pass is live telemetry only. The app does not directly expose a safe shared-flow write path, so Tutones does not guess FIB File completion values.");

            ImGui::BeginDisabled(state.pending);
            if (ImGui::Button("Refresh Garment Factory", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueRefresh());
            ImGui::EndDisabled();

            ImGui::SeparatorText("FIB File Flow");
            ImGui::Text("Active robbery/file ID: %d", state.activeRobbery);
            ImGui::Text("General flags: 0x%08X", state.generalFlags);
            ImGui::Text("Instance flags: 0x%08X", state.instanceFlags);
            ImGui::Text("Hacker flags: 0x%08X", state.hackerFlags);
            ImGui::Text("Unknown slot 3: %d", state.unknown3);
            ImGui::Text("Unknown slot 5: %d", state.unknown5);

            ImGui::SeparatorText("Current Packed State");
            ImGui::Text("Packed bool 51273: %s", state.packedBool51273 ? "Set" : "Clear");
            ImGui::Text("Packed bool 51274: %s", state.packedBool51274 ? "Set" : "Clear");
            ImGui::Text("Packed bool 51275: %s", state.packedBool51275 ? "Set" : "Clear");
            ImGui::TextDisabled("The current Enhanced typed flow identifies these slots, but their exact gameplay meaning remains intentionally unmapped here.");

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
