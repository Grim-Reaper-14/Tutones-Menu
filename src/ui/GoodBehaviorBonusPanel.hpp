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

        ImGui::SetCursorPos(ImVec2(226.0f, 365.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));

        if (ImGui::BeginChild("##good_behavior_bonus", ImVec2(490.0f, 81.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Good Behavior Bonus");
            ImGui::SameLine();
            ImGui::TextDisabled("$2,000");

            ImGui::BeginDisabled(state.pending);
            if (ImGui::Button(state.pending ? "Applying..." : "Trigger Good Behavior Bonus", ImVec2(-1.0f, 0.0f)))
                runtime.QueueTrigger();
            ImGui::EndDisabled();
            DescribeLastV11Item("Enhanced 1.73 b1158.13: write Global_2697091 = 2000 first, then Global_2697090 = 1 to trigger the one-shot Good Behavior Bonus.");

            if (state.pending)
                ImGui::TextDisabled("Queued on the GTA script thread...");
            else if (state.haveResult)
                ImGui::TextDisabled("%s", state.message.c_str());
            else
                ImGui::TextDisabled("One-shot Enhanced reward trigger.");
        }

        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }
}
