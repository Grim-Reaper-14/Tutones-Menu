#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/recovery/BunkerToolsRuntime.hpp"

#include <imgui.h>

#include <algorithm>

namespace Tutones::UI
{
    inline void RenderBunkerToolsControl() noexcept
    {
        using Game::Recovery::BunkerTuningProfile;
        using Game::Recovery::BunkerToolsRuntime;

        auto& runtime = BunkerToolsRuntime::Get();
        const auto state = runtime.Snapshot();

        const ImVec2 hostPos = ImGui::GetWindowPos();
        ImGui::SetNextWindowPos(ImVec2(hostPos.x + 392.0f, hostPos.y + 365.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(156.0f, 42.0f), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::Accent);

        constexpr ImGuiWindowFlags launcherFlags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoNavInputs |
            ImGuiWindowFlags_NoNavFocus;

        if (ImGui::Begin("##bunker_tools_launcher", nullptr, launcherFlags))
        {
            ImGui::BeginDisabled(state.pending);
            if (ImGui::Button("Bunker Tools...", ImVec2(-1.0f, 0.0f)))
                ImGui::OpenPopup("##bunker_tools_popup");
            ImGui::EndDisabled();
            DescribeLastV11Item("Open Enhanced 1.73 Bunker value, sale multiplier, high-demand, production-time and instant-sell controls.");

            ImGui::SetNextWindowSize(ImVec2(545.0f, 520.0f), ImGuiCond_Appearing);
            if (ImGui::BeginPopupModal("##bunker_tools_popup", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
            {
                static BunkerTuningProfile profile{};

                ImGui::TextColored(V11Theme::Accent, "Bunker Tools");
                ImGui::SameLine();
                ImGui::TextDisabled("Enhanced 1.73 / b1158.13");
                ImGui::Separator();

                ImGui::SeparatorText("Product Value");
                ImGui::SetNextItemWidth(220.0f);
                ImGui::InputInt("Product value", &profile.productValue, 100, 1000);
                profile.productValue = std::max(0, profile.productValue);
                DescribeLastV11Item("Global_262145.f_21347. Supplied default: 5000.");

                ImGui::SeparatorText("Sale Multipliers");
                ImGui::SetNextItemWidth(220.0f);
                ImGui::InputFloat("Near sale multiplier", &profile.nearSaleMultiplier, 0.1f, 0.5f, "%.2fx");
                ImGui::SetNextItemWidth(220.0f);
                ImGui::InputFloat("Far sale multiplier", &profile.farSaleMultiplier, 0.1f, 0.5f, "%.2fx");
                profile.nearSaleMultiplier = std::max(0.0f, profile.nearSaleMultiplier);
                profile.farSaleMultiplier = std::max(0.0f, profile.farSaleMultiplier);
                DescribeLastV11Item("Global_262145.f_21319 and f_21320. Supplied defaults: 1.0x near, 1.5x far.");

                ImGui::SeparatorText("High Demand Bonus");
                ImGui::SetNextItemWidth(220.0f);
                ImGui::InputFloat("High demand bonus", &profile.highDemandBonus, 0.1f, 0.5f, "%.2f");
                ImGui::SetNextItemWidth(220.0f);
                ImGui::InputFloat("Max bonus", &profile.highDemandMaxBonus, 1.0f, 5.0f, "%.2f");
                profile.highDemandBonus = std::max(0.0f, profile.highDemandBonus);
                profile.highDemandMaxBonus = std::max(0.0f, profile.highDemandMaxBonus);
                DescribeLastV11Item("Global_262145.f_21232 and f_21233. Supplied defaults: 2.5 and 20.0.");

                ImGui::SeparatorText("Production Times");
                ImGui::SetNextItemWidth(220.0f);
                ImGui::InputInt("Manufacturing (ms)", &profile.manufacturingProductionMs, 1000, 10000);
                ImGui::SetNextItemWidth(220.0f);
                ImGui::InputInt("Research (ms)", &profile.researchProductionMs, 1000, 10000);
                profile.manufacturingProductionMs = std::max(0, profile.manufacturingProductionMs);
                profile.researchProductionMs = std::max(0, profile.researchProductionMs);
                DescribeLastV11Item("Global_262145.f_21342 and f_21358. Supplied defaults: 600000ms manufacturing and 300000ms research.");

                ImGui::SeparatorText("Actions");
                ImGui::BeginDisabled(state.pending);
                if (ImGui::Button("Apply Bunker globals", ImVec2(255.0f, 0.0f)))
                    runtime.QueueApplyProfile(profile);
                ImGui::SameLine();
                if (ImGui::Button("Reset editor defaults", ImVec2(-1.0f, 0.0f)))
                    profile = BunkerTuningProfile{};
                ImGui::EndDisabled();
                DescribeLastV11Item("Write the supplied Bunker tunables in one game-thread action with immediate read-back verification.");

                ImGui::BeginDisabled(state.pending);
                if (ImGui::Button("Instant Sell (active gb_gunrunning)", ImVec2(-1.0f, 0.0f)))
                    runtime.QueueInstantSell();
                ImGui::EndDisabled();
                DescribeLastV11Item("Enhanced 1.73: set gb_gunrunning local 1275 + 774 (2049) to 0. The matching mission script must be active.");

                if (state.pending)
                    ImGui::TextDisabled("Bunker action queued on the GTA script thread...");
                else if (state.haveResult)
                    ImGui::TextDisabled("%s: %s", state.lastSucceeded ? "Success" : "Failed", state.message.c_str());
                else
                    ImGui::TextDisabled("Ready. Instant Sell only applies while gb_gunrunning is active.");

                ImGui::Separator();
                if (ImGui::Button("Close", ImVec2(-1.0f, 0.0f)))
                    ImGui::CloseCurrentPopup();

                ImGui::EndPopup();
            }
        }

        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
