#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/business/HangarRuntime.hpp"

#include <imgui.h>

namespace Tutones::UI
{
    inline void RenderHangarBusinessPanel() noexcept
    {
        using namespace Game::Business::HangarEnhanced173;
        using Game::Business::HangarRuntime;

        auto& runtime = HangarRuntime::Get();
        const auto state = runtime.Snapshot();

        ImGui::SetCursorPos(ImVec2(226.0f, 52.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##hangar_business_panel", ImVec2(490.0f, 394.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Hangar / Air Freight Cargo");
            ImGui::SameLine();
            ImGui::TextDisabled("Enhanced 1.73 / b1158.13");
            ImGui::Separator();

            ImGui::TextWrapped(
                "Verified GPBD stock, packed-stat sourcing, current gb_smuggler sale locals and dynamically resolved Rockstar payout tunables.");
            ImGui::Spacing();

            ImGui::BeginDisabled(state.pending);
            if (ImGui::Button(state.pending ? "Refreshing Hangar Runtime..." : "Refresh Hangar Runtime", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueRefresh());
            if (ImGui::Button("Request Source Cargo", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueSourceCargo());
            ImGui::EndDisabled();
            DescribeLastV11Item("Use Rockstar's verified packed-stat source request instead of directly forcing the mirrored stock counter.");

            ImGui::SeparatorText("Hangar Stock");
            ImGui::Text("Property ID: %d", state.propertyId);
            ImGui::Text("Setup done: %s", state.setupDone > 0 ? "Yes" : "No");
            ImGui::Text("Total cargo: %d / %d", state.totalCargo, HangarCapacity);
            ImGui::Text("Remaining capacity: %d", state.remainingCapacity);
            ImGui::Text("Source request 36828: %s", !state.sourceRequestReadable ? "UNKNOWN" : (state.sourceRequestSet ? "SET" : "CLEAR"));

            ImGui::SeparatorText("Air-Freight Runtime");
            ImGui::Text("GTA Online: %s", state.sessionStarted ? "Ready" : "Offline");
            ImGui::Text("Script runtime: %s", state.scriptRuntimeReady ? "Ready" : "Unavailable");
            ImGui::Text("gb_smuggler mission: %s", state.haveResult ? (state.smugglerRunning ? "RUNNING" : "IDLE") : "UNKNOWN");
            ImGui::Text("apphackertruck source app: %s", state.haveResult ? (state.hackerTruckRunning ? "RUNNING" : "IDLE") : "UNKNOWN");
            ImGui::Text("Business Hub: %s", state.haveResult ? (state.businessHubRunning ? "RUNNING" : "IDLE") : "UNKNOWN");

            ImGui::SeparatorText("Active Sale");
            if (state.saleLocalsReadable)
            {
                ImGui::Text("To deliver: %d", state.saleToDeliver);
                ImGui::Text("Delivered: %d", state.saleDelivered);
            }
            else
            {
                ImGui::TextDisabled("Start a gb_smuggler sale to expose the current 1998-based delivery locals.");
            }

            ImGui::SeparatorText("Payout / Bonus Tunables");
            ImGui::Text("Tunable registry: %s", state.tunableRegistryReady ? "Ready" : "Not cached");
            if (state.payoutTunablesReadable)
            {
                for (std::size_t index = 0; index < CargoNames.size(); ++index)
                    ImGui::Text("%s: $%d / crate", CargoNames[index], state.cratePrices[index]);
                ImGui::Text("Bonus thresholds L/M/H: %d / %d / %d", state.bonusThresholdLow, state.bonusThresholdMedium, state.bonusThresholdHigh);
                ImGui::Text("Bonus pct L/M/H: %.3f / %.3f / %.3f", state.bonusPercentLow, state.bonusPercentMedium, state.bonusPercentHigh);
                ImGui::Text("Ron's cut: %.3f", state.ronsCut);
                ImGui::Text("High-demand bonus: %.3f", state.highDemandBonus);
                ImGui::Text("Sell cooldown: %d ms", state.sellCooldown);
            }
            else
            {
                ImGui::TextDisabled("Payout values remain read-only until Rockstar tunables finish caching.");
            }

            ImGui::SeparatorText("Verified Contracts");
            ImGui::Text("Source mission tx: 0x%08X", SourceMissionTransactionHash);
            ImGui::Text("Mission stat hash: 0x%08X", SourceMissionStatHash);
            ImGui::TextDisabled("Direct writes to GPBD Hangar total cargo remain intentionally disabled; it is a replicated mirror, not the authoritative inventory writer.");

            if (state.pending || state.haveResult)
                ImGui::TextDisabled("%s", state.message.c_str());
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        SetV11Description("Hangar / Air-Freight: verified b1158.13 stock, sourcing, sale-state and dynamic payout diagnostics.");
    }
}
