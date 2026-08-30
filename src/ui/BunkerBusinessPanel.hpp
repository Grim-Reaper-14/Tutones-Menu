#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/business/InstantResupplyRuntime.hpp"
#include "../features/recovery/BunkerToolsRuntime.hpp"
#include "../features/recovery/RecoveryRuntime.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdint>

namespace Tutones::UI
{
    inline void RenderBunkerBusinessPanel() noexcept
    {
        using Game::Business::InstantResupplyRuntime;
        using Game::Business::InstantResupplyTarget;
        using Game::Recovery::BunkerTuningProfile;
        using Game::Recovery::BunkerToolsRuntime;
        using Game::Recovery::RecoveryRuntime;

        auto& runtime = RecoveryRuntime::Get();
        const auto snapshot = runtime.Snapshot();
        auto& tools = BunkerToolsRuntime::Get();
        const auto toolState = tools.Snapshot();
        auto& resupply = InstantResupplyRuntime::Get();
        const auto resupplyState = resupply.Snapshot();

        static std::uint64_t lastRevision{};
        static int supplies{};
        static int product{};
        static const char* message = "Ready";
        static BunkerTuningProfile profile{};

        if (lastRevision != snapshot.revision)
        {
            lastRevision = snapshot.revision;
            if (snapshot.bunker.readable)
            {
                supplies = std::clamp(snapshot.bunker.supplies, 0, 100);
                product = std::clamp(snapshot.bunker.product, 0, 100);
            }
        }

        ImGui::SetCursorPos(ImVec2(226.0f, 52.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##bunker_business_panel", ImVec2(490.0f, 394.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Bunker");
            ImGui::SameLine();
            ImGui::TextDisabled("Enhanced 1.73 / b1158.13");
            ImGui::Separator();

            if (!runtime.IsRunning())
            {
                ImGui::TextDisabled("Recovery business runtime is offline.");
            }
            else if (!snapshot.sessionStarted)
            {
                ImGui::TextDisabled("Join GTA Online to read Bunker state.");
            }
            else if (!snapshot.bunker.owned)
            {
                ImGui::TextDisabled("No owned Bunker was detected.");
            }
            else if (!snapshot.bunker.readable)
            {
                ImGui::TextDisabled("Bunker state is not readable yet.");
            }
            else if (!snapshot.bunker.setup)
            {
                ImGui::TextDisabled("The owned Bunker has not completed setup.");
            }
            else
            {
                ImGui::Text("Property ID: %d", snapshot.bunker.propertyId);

                supplies = std::clamp(supplies, 0, 100);
                ImGui::SetNextItemWidth(300.0f);
                ImGui::SliderInt("Supplies", &supplies, 0, 100);
                ImGui::SameLine();
                ImGui::BeginDisabled(snapshot.actionPending || !snapshot.statsReady);
                if (ImGui::Button("Apply##bunker_supplies", ImVec2(92.0f, 0.0f)))
                    message = runtime.QueueSetBunkerSupplies(supplies) ? "Bunker supplies queued" : "Bunker supplies rejected";
                ImGui::EndDisabled();
                ImGui::TextDisabled("Current supplies: %d / 100", snapshot.bunker.supplies);

                product = std::clamp(product, 0, 100);
                ImGui::SetNextItemWidth(300.0f);
                ImGui::SliderInt("Product", &product, 0, 100);
                ImGui::SameLine();
                ImGui::BeginDisabled(snapshot.actionPending || !snapshot.statsReady);
                if (ImGui::Button("Apply##bunker_product", ImVec2(92.0f, 0.0f)))
                    message = runtime.QueueSetBunkerProduct(product) ? "Bunker product queued" : "Bunker product rejected";
                ImGui::EndDisabled();
                ImGui::TextDisabled("Current product: %d / 100", snapshot.bunker.product);

                if (snapshot.actionPending)
                    ImGui::TextDisabled("Bunker action is running on the GTA script thread...");
                else
                    ImGui::TextDisabled("%s", message);
            }

            if (ImGui::CollapsingHeader("Bunker Tools", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::SeparatorText("Production Boost");
                bool fastProduction = toolState.fastProductionEnabled;
                ImGui::BeginDisabled(toolState.pending);
                if (ImGui::Checkbox("Fast Production Stock (1 second)", &fastProduction))
                    static_cast<void>(tools.QueueSetFastProduction(fastProduction));
                ImGui::EndDisabled();
                DescribeLastV11Item(
                    "Accelerates the Bunker manufacturing cycle to 1 second while enabled. It still uses the real Bunker production path, so supplies and staff assignment remain authoritative. Disabling restores the manufacturing time that was active before the boost.");
                ImGui::TextDisabled(
                    "Manufacturing boost: %s",
                    toolState.fastProductionEnabled ? "FAST (1000 ms)" : "NORMAL");

                ImGui::SeparatorText("Product Value");
                ImGui::SetNextItemWidth(220.0f);
                ImGui::InputInt("Product value", &profile.productValue, 100, 1000);
                profile.productValue = std::max(0, profile.productValue);

                ImGui::SeparatorText("Sale Multipliers");
                ImGui::SetNextItemWidth(220.0f);
                ImGui::InputFloat("Near sale multiplier", &profile.nearSaleMultiplier, 0.1f, 0.5f, "%.2fx");
                ImGui::SetNextItemWidth(220.0f);
                ImGui::InputFloat("Far sale multiplier", &profile.farSaleMultiplier, 0.1f, 0.5f, "%.2fx");
                profile.nearSaleMultiplier = std::max(0.0f, profile.nearSaleMultiplier);
                profile.farSaleMultiplier = std::max(0.0f, profile.farSaleMultiplier);

                ImGui::SeparatorText("High Demand Bonus");
                ImGui::SetNextItemWidth(220.0f);
                ImGui::InputFloat("High demand bonus", &profile.highDemandBonus, 0.1f, 0.5f, "%.2f");
                ImGui::SetNextItemWidth(220.0f);
                ImGui::InputFloat("Max bonus", &profile.highDemandMaxBonus, 1.0f, 5.0f, "%.2f");
                profile.highDemandBonus = std::max(0.0f, profile.highDemandBonus);
                profile.highDemandMaxBonus = std::max(0.0f, profile.highDemandMaxBonus);

                ImGui::SeparatorText("Production Times");
                ImGui::SetNextItemWidth(220.0f);
                ImGui::InputInt("Manufacturing (ms)", &profile.manufacturingProductionMs, 1000, 10000);
                ImGui::SetNextItemWidth(220.0f);
                ImGui::InputInt("Research (ms)", &profile.researchProductionMs, 1000, 10000);
                profile.manufacturingProductionMs = std::max(0, profile.manufacturingProductionMs);
                profile.researchProductionMs = std::max(0, profile.researchProductionMs);

                ImGui::SeparatorText("Actions");
                ImGui::BeginDisabled(toolState.pending);
                if (ImGui::Button("Apply Bunker globals", ImVec2(220.0f, 0.0f)))
                    tools.QueueApplyProfile(profile);
                ImGui::SameLine();
                if (ImGui::Button("Reset editor defaults", ImVec2(-1.0f, 0.0f)))
                    profile = BunkerTuningProfile{};
                ImGui::EndDisabled();

                ImGui::BeginDisabled(resupplyState.pending);
                if (ImGui::Button("Instant Resupply (supplied +6)", ImVec2(-1.0f, 0.0f)))
                    static_cast<void>(resupply.QueueRequest(InstantResupplyTarget::Bunker));
                ImGui::EndDisabled();
                DescribeLastV11Item("Write 1 to the supplied Global_1673820 + 6 Bunker Instant Resupply flag and verify the read-back.");

                ImGui::BeginDisabled(toolState.pending);
                if (ImGui::Button("Instant Sell (active gb_gunrunning)", ImVec2(-1.0f, 0.0f)))
                    tools.QueueInstantSell();
                ImGui::EndDisabled();

                if (resupplyState.pending)
                    ImGui::TextDisabled("%s", resupplyState.message.c_str());
                else if (resupplyState.haveResult)
                    ImGui::TextDisabled("Resupply %s: %s", resupplyState.lastSucceeded ? "Success" : "Failed", resupplyState.message.c_str());

                if (toolState.pending)
                    ImGui::TextDisabled("Bunker action queued on the GTA script thread...");
                else if (toolState.haveResult)
                    ImGui::TextDisabled("%s: %s", toolState.lastSucceeded ? "Success" : "Failed", toolState.message.c_str());
                else
                    ImGui::TextDisabled("Ready. Instant Sell only applies while gb_gunrunning is active.");
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        SetV11Description("Bunker is fully rendered inside its business tab: stock, Fast Production Stock, Instant Resupply, product value, sale multipliers, high-demand bonus, production times and Instant Sell.");
    }
}
