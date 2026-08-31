#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/business/NightclubRuntime.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstddef>

namespace Tutones::UI
{
    namespace NightclubPanelDetail
    {
        inline Game::Business::NightclubProfile g_Profile = Game::Business::NightclubData::DefaultProfile();
        inline int g_SelectedGood{};
        inline int g_SelectedPopularityTier{};
        inline int g_Popularity = Game::Business::NightclubData::MaximumPopularity;

        inline void ResetDefaults() noexcept
        {
            g_Profile = Game::Business::NightclubData::DefaultProfile();
            g_Popularity = Game::Business::NightclubData::MaximumPopularity;
        }

        inline void RenderGoodEditor() noexcept
        {
            using namespace Game::Business;

            g_SelectedGood = std::clamp(g_SelectedGood, 0, static_cast<int>(NightclubData::GoodNames.size()) - 1);
            ImGui::Combo(
                "Good",
                &g_SelectedGood,
                NightclubData::GoodNames.data(),
                static_cast<int>(NightclubData::GoodNames.size()));

            const auto index = static_cast<std::size_t>(g_SelectedGood);
            ImGui::InputInt("Stock value", &g_Profile.stockValues[index], 100, 1000);
            DescribeLastV11Item("Global_262145 stock-sale value for the selected Nightclub warehouse good.");

            ImGui::InputInt("Special order stock value", &g_Profile.specialOrderStockValues[index], 100, 1000);
            DescribeLastV11Item("Global_262145 special-order value for the selected Nightclub warehouse good.");

            ImGui::InputInt("Max units", &g_Profile.maxUnits[index], 1, 10);
            DescribeLastV11Item("Maximum warehouse units for the selected Nightclub good.");

            ImGui::InputInt("Production time (ms)", &g_Profile.productionTimes[index], 1000, 60000);
            DescribeLastV11Item("Production interval in milliseconds for the selected Nightclub good.");

            g_Profile.stockValues[index] = std::max(g_Profile.stockValues[index], 0);
            g_Profile.specialOrderStockValues[index] = std::max(g_Profile.specialOrderStockValues[index], 0);
            g_Profile.maxUnits[index] = std::max(g_Profile.maxUnits[index], 0);
            g_Profile.productionTimes[index] = std::max(g_Profile.productionTimes[index], 0);

            auto& runtime = NightclubRuntime::Get();
            const auto snapshot = runtime.Snapshot();
            ImGui::BeginDisabled(snapshot.actionPending);
            if (ImGui::Button("Apply selected good", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueApplyGood(index, g_Profile));
            ImGui::EndDisabled();
        }

        inline void RenderCooldownEditor() noexcept
        {
            using namespace Game::Business;

            ImGui::SeparatorText("Mission Cooldowns");
            for (std::size_t index = 0; index < NightclubData::CooldownNames.size(); ++index)
            {
                ImGui::PushID(static_cast<int>(index));
                ImGui::InputInt(NightclubData::CooldownNames[index], &g_Profile.cooldowns[index], 1000, 10000);
                g_Profile.cooldowns[index] = std::max(g_Profile.cooldowns[index], 0);
                ImGui::PopID();
            }

            auto& runtime = NightclubRuntime::Get();
            const auto snapshot = runtime.Snapshot();
            ImGui::BeginDisabled(snapshot.actionPending);
            if (ImGui::Button("Apply cooldowns", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueApplyCooldowns(g_Profile.cooldowns));
            ImGui::EndDisabled();
            DescribeLastV11Item("Apply the Management, Sell, and Special Order Sell mission cooldown globals together.");

            if (ImGui::Button("Set cooldowns to 0", ImVec2(-1.0f, 0.0f)))
                g_Profile.cooldowns = {0, 0, 0};
            DescribeLastV11Item("Stage zero-millisecond Nightclub mission cooldowns. Use Apply cooldowns to write them.");
        }

        inline void RenderProductionEditor() noexcept
        {
            using namespace Game::Business;

            ImGui::SeparatorText("Production Upgrade");
            ImGui::InputFloat("Equipment multiplier", &g_Profile.equipmentUpgradeMultiplier, 0.05f, 0.10f, "%.2f");
            g_Profile.equipmentUpgradeMultiplier = std::clamp(g_Profile.equipmentUpgradeMultiplier, 0.0f, 100.0f);

            auto& runtime = NightclubRuntime::Get();
            const auto snapshot = runtime.Snapshot();
            ImGui::BeginDisabled(snapshot.actionPending);
            if (ImGui::Button("Apply equipment multiplier", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueApplyUpgradeMultiplier(g_Profile.equipmentUpgradeMultiplier));
            ImGui::EndDisabled();
            DescribeLastV11Item("Apply Global_262145.f_24047, the Nightclub equipment-upgrade production-time multiplier.");
        }

        inline void RenderPopularityEditor() noexcept
        {
            using namespace Game::Business;

            ImGui::SeparatorText("Club Popularity");
            ImGui::SliderInt(
                "Popularity",
                &g_Popularity,
                0,
                NightclubData::MaximumPopularity,
                "%d");
            g_Popularity = std::clamp(g_Popularity, 0, NightclubData::MaximumPopularity);
            ImGui::TextDisabled(
                "%d / %d (%d%%)",
                g_Popularity,
                NightclubData::MaximumPopularity,
                g_Popularity / 10);

            auto& runtime = NightclubRuntime::Get();
            const auto snapshot = runtime.Snapshot();
            ImGui::BeginDisabled(snapshot.actionPending);
            if (ImGui::Button("Apply club popularity", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueSetPopularity(g_Popularity));
            ImGui::EndDisabled();
            DescribeLastV11Item("Write MPX_CLUB_POPULARITY for the active Online character. Rockstar stores 100% popularity as 1000.");

            ImGui::SeparatorText("Popularity Income Tunables");
            g_SelectedPopularityTier = std::clamp(
                g_SelectedPopularityTier,
                0,
                static_cast<int>(NightclubData::PopularityTierNames.size()) - 1);

            ImGui::Combo(
                "Popularity tier",
                &g_SelectedPopularityTier,
                NightclubData::PopularityTierNames.data(),
                static_cast<int>(NightclubData::PopularityTierNames.size()));

            const auto index = static_cast<std::size_t>(g_SelectedPopularityTier);
            ImGui::InputInt("Income", &g_Profile.popularityIncome[index], 100, 1000);
            g_Profile.popularityIncome[index] = std::max(g_Profile.popularityIncome[index], 0);

            ImGui::BeginDisabled(snapshot.actionPending);
            if (ImGui::Button("Apply popularity income", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueApplyPopularityIncome(index, g_Profile.popularityIncome[index]));
            ImGui::EndDisabled();
            DescribeLastV11Item("Apply the Nightclub safe-income global for the selected 5-point popularity band.");
        }
    }

    inline void RenderNightclubPanel() noexcept
    {
        using namespace NightclubPanelDetail;
        using namespace Game::Business;

        auto& runtime = NightclubRuntime::Get();
        const auto snapshot = runtime.Snapshot();

        ImGui::SetCursorPos(ImVec2(226.0f, 52.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##nightclub_panel", ImVec2(490.0f, 394.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Nightclub Globals");
            ImGui::SameLine();
            ImGui::TextDisabled("Enhanced 1.73 / b1158.13");
            ImGui::Separator();

            if (ImGui::BeginTabBar("##nightclub_tabs"))
            {
                if (ImGui::BeginTabItem("Goods"))
                {
                    RenderGoodEditor();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Cooldowns"))
                {
                    RenderCooldownEditor();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Production"))
                {
                    RenderProductionEditor();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Popularity"))
                {
                    RenderPopularityEditor();
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }

            ImGui::Separator();
            ImGui::BeginDisabled(snapshot.actionPending);
            if (ImGui::Button("Apply full Nightclub profile", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueApplyProfile(g_Profile));
            ImGui::EndDisabled();
            DescribeLastV11Item("Write all supplied Enhanced 1.73 Nightclub stock, special-order, cooldown, capacity, production, equipment and popularity-income globals in one pass.");

            if (ImGui::Button("Reset editor to supplied defaults", ImVec2(-1.0f, 0.0f)))
                ResetDefaults();
            DescribeLastV11Item("Reset only the editor values to the supplied b1158.13 defaults. This does not write globals until Apply is used.");

            if (snapshot.actionPending)
                ImGui::TextDisabled("%s", snapshot.message.c_str());
            else if (snapshot.haveResult)
                ImGui::TextDisabled("%s: %s", snapshot.lastSucceeded ? "Success" : "Failed", snapshot.message.c_str());
            else
                ImGui::TextDisabled("Ready - values are written only while an Online session and script globals are available.");
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
