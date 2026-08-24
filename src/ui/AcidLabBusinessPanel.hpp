#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/business/InstantResupplyRuntime.hpp"

#include <imgui.h>

namespace Tutones::UI
{
    inline void RenderAcidLabBusinessPanel() noexcept
    {
        using Game::Business::InstantResupplyRuntime;
        using Game::Business::InstantResupplyTarget;

        auto& runtime = InstantResupplyRuntime::Get();
        const auto state = runtime.Snapshot();

        ImGui::SetCursorPos(ImVec2(226.0f, 52.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##acid_lab_business_panel", ImVec2(490.0f, 394.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Acid Lab");
            ImGui::SameLine();
            ImGui::TextDisabled("Enhanced 1.73 / b1158.13");
            ImGui::Separator();

            ImGui::TextWrapped("Dedicated Acid Lab business page. The first verified action uses the supplied Instant Resupply mapping instead of borrowing controls from Motorcycle Club or Bunker.");
            ImGui::Spacing();

            ImGui::SeparatorText("Supplies");
            ImGui::BeginDisabled(state.pending);
            if (ImGui::Button("Instant Resupply Acid Lab", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueRequest(InstantResupplyTarget::AcidLab));
            ImGui::EndDisabled();
            DescribeLastV11Item("Write 1 to the supplied Global_1673820 + 7 Instant Resupply flag on the GTA script thread and verify the read-back.");

            ImGui::SeparatorText("Status");
            if (state.pending)
                ImGui::TextDisabled("%s", state.message.c_str());
            else if (state.haveResult)
                ImGui::TextDisabled("%s: %s", state.lastSucceeded ? "Success" : "Failed", state.message.c_str());
            else
                ImGui::TextDisabled("Ready. More Acid Lab decompile-backed controls can be added here as their Enhanced locals/tunables are validated.");
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        SetV11Description("Acid Lab has its own business page with the supplied Enhanced Instant Resupply flag and room for validated Acid Lab script controls.");
    }
}
