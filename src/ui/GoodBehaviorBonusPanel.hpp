#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/recovery/GoodBehaviorBonusRuntime.hpp"

#include <imgui.h>

namespace Tutones::UI
{
    inline void RenderGoodBehaviorBonusControl() noexcept
    {
        auto& runtime = Game::Recovery::GoodBehaviorBonusRuntime::Get();
        const auto state = runtime.Snapshot();

        // V2 gives this reward its own card below the Recovery overview instead
        // of drawing it over the bottom of the legacy Recovery panel.
        ImGui::SetCursorPos(ImVec2(226.0f, 458.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 10.0f));

        if (ImGui::BeginChild("##good_behavior_bonus_v2", ImVec2(780.0f, 92.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "GOOD BEHAVIOR BONUS");
            ImGui::SameLine();
            ImGui::TextDisabled("ONE-SHOT REWARD  /  $2,000");

            ImGui::BeginDisabled(state.pending);
            if (ImGui::Button(state.pending ? "APPLYING..." : "TRIGGER GOOD BEHAVIOR BONUS", ImVec2(300.0f, 34.0f)))
                runtime.QueueTrigger();
            ImGui::EndDisabled();
            DescribeLastV11Item("Enhanced 1.73 b1158.13: write Global_2697091 = 2000 first, then Global_2697090 = 1 to trigger the one-shot Good Behavior Bonus.");

            ImGui::SameLine();
            if (state.pending)
                ImGui::TextDisabled("Queued on GTA script thread...");
            else if (state.haveResult)
                ImGui::TextDisabled("%s", state.message.c_str());
            else
                ImGui::TextDisabled("Ready");
        }

        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
    }
}
