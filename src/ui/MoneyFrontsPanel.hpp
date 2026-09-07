#pragma once

#include "V11Theme.hpp"
#include "../features/business/MoneyFrontsRuntime.hpp"

#include <imgui.h>

#include <algorithm>

namespace Tutones::UI
{
    inline void RenderMoneyFrontsPanel() noexcept
    {
        using namespace Game::Business::MoneyFrontsEnhanced173;
        using Game::Business::MoneyFrontsRuntime;

        auto& runtime = MoneyFrontsRuntime::Get();
        const auto state = runtime.Snapshot();
        static int selectedFront = 0;
        static int selectedHeat = 0;
        selectedFront = std::clamp(selectedFront, 0, FrontCount - 1);
        selectedHeat = std::clamp(selectedHeat, MinimumHeat, MaximumHeat);

        ImGui::SetCursorPos(ImVec2(226.0f, 52.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##money_fronts_panel", ImVec2(490.0f, 394.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Money Fronts");
            ImGui::SameLine();
            ImGui::TextDisabled("Money Fronts / M25 flow");
            ImGui::Separator();
            ImGui::TextDisabled("Enhanced packed-stat and safe-collect controls");

            ImGui::SeparatorText("Front Heat Control");
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##money_front", FrontName(selectedFront)))
            {
                for (int index = 0; index < FrontCount; ++index)
                {
                    const bool selected = index == selectedFront;
                    if (ImGui::Selectable(FrontName(index), selected))
                    {
                        selectedFront = index;
                        if (state.heat[static_cast<std::size_t>(index)] >= 0)
                            selectedHeat = state.heat[static_cast<std::size_t>(index)];
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::SetNextItemWidth(-1.0f);
            ImGui::SliderInt("Heat##money_front_heat", &selectedHeat, MinimumHeat, MaximumHeat, "%d%%");

            ImGui::BeginDisabled(state.pending);
            if (ImGui::Button("Apply Selected Front Heat", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueSetHeat(selectedFront, selectedHeat));

            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float halfWidth = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;
            if (ImGui::Button("All Heat 0", ImVec2(halfWidth, 0.0f)))
                static_cast<void>(runtime.QueueSetAllHeat(0));
            ImGui::SameLine();
            if (ImGui::Button("All Heat 100", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueSetAllHeat(100));

            ImGui::SeparatorText("Hands On Car Wash Safe");
            ImGui::Text("Safe cash: $%d", state.carWashSafeCash);
            if (ImGui::Button("Collect Car Wash Safe", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueCollectCarWashSafe());

            if (ImGui::Button("Refresh Money Fronts", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueRefresh());
            ImGui::EndDisabled();

            ImGui::SeparatorText("Live Front State");
            for (int index = 0; index < FrontCount; ++index)
            {
                const auto slot = static_cast<std::size_t>(index);
                ImGui::Text("%s", FrontName(index));
                ImGui::SameLine();
                if (state.heat[slot] >= 0)
                    ImGui::TextDisabled("Heat %d%% | Owned stat %d", state.heat[slot], state.owned[slot]);
                else
                    ImGui::TextDisabled("Heat -- | Owned stat %d", state.owned[slot]);
            }

            ImGui::SeparatorText("M25 Live Flow");
            ImGui::Text("Current mission: %d", state.currentMission);
            ImGui::Text("General bits: 0x%08X", state.generalBits);
            ImGui::Text("Flags: 0x%08X", state.flags);

            ImGui::SeparatorText("Runtime");
            ImGui::Text("Session: %s", state.sessionStarted ? "Online" : "Offline");
            ImGui::Text("Native backend: %s", state.nativeReady ? "Ready" : "Unavailable");
            ImGui::Text("Script globals: %s", state.globalsReady ? "Ready" : "Unavailable");
            ImGui::Text("Character slot: %d", state.characterIndex);

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
