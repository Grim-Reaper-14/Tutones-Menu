#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/business/AcidLabProductionRuntime.hpp"
#include "../features/business/InstantResupplyRuntime.hpp"

#include <imgui.h>

namespace Tutones::UI
{
    inline void RenderAcidLabBusinessPanel() noexcept
    {
        using Game::Business::AcidLabProductionRuntime;
        using Game::Business::InstantResupplyRuntime;
        using Game::Business::InstantResupplyTarget;

        auto& resupplyRuntime = InstantResupplyRuntime::Get();
        const auto resupplyState = resupplyRuntime.Snapshot();
        auto& productionRuntime = AcidLabProductionRuntime::Get();
        const auto productionState = productionRuntime.Snapshot();

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

            ImGui::TextWrapped("Dedicated Acid Lab business page using the verified Enhanced resupply and production mappings.");
            ImGui::Spacing();

            ImGui::SeparatorText("Supplies");
            ImGui::BeginDisabled(resupplyState.pending);
            if (ImGui::Button("Instant Resupply Acid Lab", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(resupplyRuntime.QueueRequest(InstantResupplyTarget::AcidLab));
            ImGui::EndDisabled();
            DescribeLastV11Item("Write 1 to the supplied Global_1673820 + 7 Instant Resupply flag on the GTA script thread and verify the read-back.");

            ImGui::SeparatorText("Production");
            ImGui::BeginDisabled(productionState.actionPending);
            if (ImGui::Button("Instant Fill Production", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(productionRuntime.QueueInstantFinish());
            ImGui::EndDisabled();
            DescribeLastV11Item("Set the active character's verified factory-6 Acid stock to its 160-unit cap, verify the persistent stat, and refresh the live freemode business cache when it is available.");

            if (productionState.stockUnits >= 0)
                ImGui::TextDisabled("Observed Acid stock: %d / 160", productionState.stockUnits);
            else
                ImGui::TextDisabled("Observed Acid stock: press Instant Fill Production to refresh");

            if (productionState.haveResult && productionState.lastSucceeded)
            {
                ImGui::TextDisabled(
                    "Persistent stock: %s  |  Live cache: %s",
                    productionState.persistentStockVerified ? "VERIFIED" : "FAILED",
                    productionState.liveCacheUpdated ? "UPDATED" : "DEFERRED");
            }

            ImGui::SeparatorText("Status");
            if (productionState.actionPending)
                ImGui::TextDisabled("Production: %s", productionState.message.c_str());
            else if (productionState.haveResult)
                ImGui::TextDisabled("Production: %s: %s", productionState.lastSucceeded ? "Success" : "Failed", productionState.message.c_str());
            else
                ImGui::TextDisabled("Production: Ready");

            if (resupplyState.pending)
                ImGui::TextDisabled("Supplies: %s", resupplyState.message.c_str());
            else if (resupplyState.haveResult)
                ImGui::TextDisabled("Supplies: %s: %s", resupplyState.lastSucceeded ? "Success" : "Failed", resupplyState.message.c_str());
            else
                ImGui::TextDisabled("Supplies: Ready");
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        SetV11Description("Acid Lab has dedicated Instant Resupply and verified persistent/live-cache Instant Fill Production controls for Enhanced 1.73.");
    }
}
