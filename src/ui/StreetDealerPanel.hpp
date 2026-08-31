#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/game/StreetDealerRuntime.hpp"

#include <imgui.h>

#include <cstddef>

namespace Tutones::UI
{
    inline void RenderStreetDealerPanel() noexcept
    {
        auto& runtime = Game::StreetDealer::Runtime::Get();
        const auto snapshot = runtime.GetSnapshot();

        ImGui::TextWrapped(
            "Enhanced Street Dealer state decoded from fm_street_dealer.c. Payouts are read-only and validated against the current Enhanced layout.");
        ImGui::Spacing();

        ImGui::BeginDisabled(snapshot.pending);
        if (ImGui::Button(snapshot.pending ? "Refreshing Dealer..." : "Refresh Dealer State", ImVec2(-1.0f, 28.0f)))
            static_cast<void>(runtime.QueueRefresh());
        ImGui::EndDisabled();
        DescribeLastV11Item("Read current Enhanced 1.73 / b1158.13 Street Dealer state on the GTA game thread.");

        if (!snapshot.haveResult)
        {
            ImGui::TextDisabled("Refresh after joining GTA Online.");
            SetV11Description("Street Dealer Manager - validated Enhanced dealer state and payout records.");
            return;
        }

        if (!snapshot.lastSucceeded || !snapshot.layoutValid)
        {
            ImGui::TextWrapped("%s", snapshot.message.c_str());
            ImGui::TextDisabled("Writes remain disabled because the current layout did not validate.");
            SetV11Description("Street Dealer Manager - Enhanced layout validation failed or is unavailable.");
            return;
        }

        ImGui::Text("Active location: %d", snapshot.activeLocation);
        ImGui::SameLine();
        ImGui::TextDisabled("| Active dealer record: %d", snapshot.activeDealer);
        ImGui::TextDisabled("Layout: Enhanced 1.73 / b1158.13 VERIFIED READ PATH");
        ImGui::Separator();

        if (ImGui::BeginTable(
                "##street_dealer_records",
                7,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollX))
        {
            ImGui::TableSetupColumn("Dealer", ImGuiTableColumnFlags_WidthFixed, 72.0f);
            ImGui::TableSetupColumn("Premium", ImGuiTableColumnFlags_WidthFixed, 75.0f);
            ImGui::TableSetupColumn("Coke", ImGuiTableColumnFlags_WidthFixed, 65.0f);
            ImGui::TableSetupColumn("Meth", ImGuiTableColumnFlags_WidthFixed, 65.0f);
            ImGui::TableSetupColumn("Weed", ImGuiTableColumnFlags_WidthFixed, 65.0f);
            ImGui::TableSetupColumn("Acid", ImGuiTableColumnFlags_WidthFixed, 65.0f);
            ImGui::TableSetupColumn("Done", ImGuiTableColumnFlags_WidthFixed, 52.0f);
            ImGui::TableHeadersRow();

            for (std::size_t index = 0; index < snapshot.dealers.size(); ++index)
            {
                const auto& dealer = snapshot.dealers[index];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (snapshot.activeDealer == static_cast<int>(index))
                    ImGui::TextColored(V11Theme::Accent, "Dealer %zu", index + 1);
                else
                    ImGui::Text("Dealer %zu", index + 1);

                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(Game::StreetDealer::ProductName(dealer.premiumProduct));

                ImGui::TableSetColumnIndex(2); ImGui::Text("$%d", dealer.cocainePayout);
                ImGui::TableSetColumnIndex(3); ImGui::Text("$%d", dealer.methPayout);
                ImGui::TableSetColumnIndex(4); ImGui::Text("$%d", dealer.weedPayout);
                ImGui::TableSetColumnIndex(5); ImGui::Text("$%d", dealer.acidPayout);
                ImGui::TableSetColumnIndex(6);
                if (dealer.completed)
                    ImGui::TextColored(V11Theme::Accent, "YES");
                else
                    ImGui::TextDisabled("NO");
            }

            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::TextWrapped(
            "Premium is the product ID Rockstar stores for that dealer's high-value product. Coke/Meth/Weed/Acid are the exact per-unit payout fields consumed by the Street Dealer menu and transaction payload.");
        SetV11Description("Street Dealer Manager - active location, premium product and decoded per-unit payouts from the Enhanced decompile.");
    }
}
