#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/business/InstantResupplyRuntime.hpp"
#include "../features/business/MotorcycleClubRuntime.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstddef>

namespace Tutones::UI
{
    inline void RenderMotorcycleClubPanel() noexcept
    {
        using namespace Game::Business;

        static MotorcycleClubProfile profile = MotorcycleClubData::DefaultProfile();
        static int selectedBusiness = 0;

        auto& runtime = MotorcycleClubRuntime::Get();
        const auto snapshot = runtime.Snapshot();
        auto& resupply = InstantResupplyRuntime::Get();
        const auto resupplyState = resupply.Snapshot();

        selectedBusiness = std::clamp(
            selectedBusiness,
            0,
            static_cast<int>(MotorcycleClubData::BusinessNames.size()) - 1);
        const auto businessIndex = static_cast<std::size_t>(selectedBusiness);

        ImGui::SetCursorPos(ImVec2(226.0f, 52.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##motorcycle_club_panel", ImVec2(490.0f, 394.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Motorcycle Club Globals");
            ImGui::SameLine();
            ImGui::TextDisabled("Enhanced 1.73 / b1158.13");
            ImGui::Separator();

            ImGui::TextColored(V11Theme::Accent, "Business Values");
            ImGui::SetNextItemWidth(260.0f);
            ImGui::Combo(
                "Business",
                &selectedBusiness,
                MotorcycleClubData::BusinessNames.data(),
                static_cast<int>(MotorcycleClubData::BusinessNames.size()));

            ImGui::SetNextItemWidth(210.0f);
            ImGui::InputInt("Stock value", &profile.stockValues[businessIndex], 100, 1000);
            profile.stockValues[businessIndex] = std::max(0, profile.stockValues[businessIndex]);
            DescribeLastV11Item("Global_262145.f_17412 through f_17417 using the selected supplied MC stock-value mapping.");

            ImGui::SetNextItemWidth(210.0f);
            ImGui::InputInt("Max capacity", &profile.maxCapacities[businessIndex], 1, 10);
            profile.maxCapacities[businessIndex] = std::max(0, profile.maxCapacities[businessIndex]);
            DescribeLastV11Item("Apply the supplied max-capacity global for the selected Documents, Cash, Cocaine, Meth, Weed or Acid business.");

            ImGui::BeginDisabled(snapshot.actionPending);
            if (ImGui::Button("Apply selected business", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueApplyBusiness(businessIndex, profile));
            ImGui::EndDisabled();
            DescribeLastV11Item("Write and read back the selected business stock value and max-capacity globals.");

            ImGui::SeparatorText("Instant Resupply Slots");
            ImGui::TextDisabled("Supplied Global_1673820 slots 0-4 are kept generic until their business-to-slot mapping is verified.");
            ImGui::BeginDisabled(resupplyState.pending);
            if (ImGui::Button("Slot 0", ImVec2(86.0f, 0.0f)))
                static_cast<void>(resupply.QueueRequest(InstantResupplyTarget::Slot0));
            ImGui::SameLine();
            if (ImGui::Button("Slot 1", ImVec2(86.0f, 0.0f)))
                static_cast<void>(resupply.QueueRequest(InstantResupplyTarget::Slot1));
            ImGui::SameLine();
            if (ImGui::Button("Slot 2", ImVec2(86.0f, 0.0f)))
                static_cast<void>(resupply.QueueRequest(InstantResupplyTarget::Slot2));
            if (ImGui::Button("Slot 3", ImVec2(86.0f, 0.0f)))
                static_cast<void>(resupply.QueueRequest(InstantResupplyTarget::Slot3));
            ImGui::SameLine();
            if (ImGui::Button("Slot 4", ImVec2(86.0f, 0.0f)))
                static_cast<void>(resupply.QueueRequest(InstantResupplyTarget::Slot4));
            ImGui::EndDisabled();
            DescribeLastV11Item("Write 1 to the selected supplied Global_1673820 + 1 through +5 resupply flag with read-back verification.");

            if (resupplyState.pending)
                ImGui::TextDisabled("%s", resupplyState.message.c_str());
            else if (resupplyState.haveResult)
                ImGui::TextDisabled("Resupply %s: %s", resupplyState.lastSucceeded ? "Success" : "Failed", resupplyState.message.c_str());

            ImGui::SeparatorText("Sale Multipliers");
            ImGui::SetNextItemWidth(210.0f);
            ImGui::InputFloat("Near sale multiplier", &profile.nearSaleMultiplier, 0.1f, 0.5f, "%.2fx");
            ImGui::SetNextItemWidth(210.0f);
            ImGui::InputFloat("Far sale multiplier", &profile.farSaleMultiplier, 0.1f, 0.5f, "%.2fx");
            profile.nearSaleMultiplier = std::max(0.0f, profile.nearSaleMultiplier);
            profile.farSaleMultiplier = std::max(0.0f, profile.farSaleMultiplier);
            DescribeLastV11Item("Global_262145.f_18967 and f_18968. Supplied defaults are 1.0x Near and 1.5x Far.");

            ImGui::BeginDisabled(snapshot.actionPending);
            if (ImGui::Button("Apply sale multipliers", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueApplySaleMultipliers(profile.nearSaleMultiplier, profile.farSaleMultiplier));
            ImGui::EndDisabled();

            ImGui::SeparatorText("Profile");
            ImGui::BeginDisabled(snapshot.actionPending);
            if (ImGui::Button("Apply full MC profile", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(runtime.QueueApplyProfile(profile));
            ImGui::EndDisabled();
            DescribeLastV11Item("Write all supplied MC stock values, capacities, and Near/Far sale multipliers in one GTA script-thread action.");

            if (ImGui::Button("Reset editor to supplied defaults", ImVec2(-1.0f, 0.0f)))
                profile = MotorcycleClubData::DefaultProfile();
            DescribeLastV11Item("Reset only the editor values to the supplied Enhanced 1.73 defaults. This does not write globals until Apply is used.");

            if (snapshot.actionPending)
                ImGui::TextDisabled("%s", snapshot.message.c_str());
            else if (snapshot.haveResult)
                ImGui::TextDisabled("%s: %s", snapshot.lastSucceeded ? "Success" : "Failed", snapshot.message.c_str());
            else
                ImGui::TextDisabled("Ready - values are written only while GTA Online and script globals are available.");
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
