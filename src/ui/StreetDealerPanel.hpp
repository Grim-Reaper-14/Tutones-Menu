#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/game/StreetDealerRuntime.hpp"

#include <imgui.h>

#include <cstddef>
#include <cstdio>

namespace Tutones::UI
{
    inline void RenderStreetDealerLocationTeleports(
        Game::StreetDealer::Runtime& runtime,
        const Game::StreetDealer::Snapshot& snapshot) noexcept
    {
        const auto teleport = runtime.GetTeleportSnapshot();
        static int selectedLocation{};

        if (selectedLocation < 0
            || selectedLocation >= static_cast<int>(Game::StreetDealer::Enhanced173::Locations.size()))
        {
            selectedLocation = 0;
        }

        ImGui::SeparatorText("Street Dealer Locations");
        ImGui::TextWrapped(
            "All 50 Rockstar Street Dealer spawn locations from Enhanced 1.73 / b1158.13 are available here. Teleporting moves your current vehicle with you when applicable.");

        char preview[48]{};
        std::snprintf(preview, sizeof(preview), "Location %02d", selectedLocation + 1);
        if (snapshot.layoutValid && snapshot.activeLocation == selectedLocation)
            std::snprintf(preview, sizeof(preview), "Location %02d  [ACTIVE]", selectedLocation + 1);

        if (ImGui::BeginCombo("Dealer Location", preview))
        {
            for (std::size_t index = 0; index < Game::StreetDealer::Enhanced173::Locations.size(); ++index)
            {
                char label[48]{};
                const bool active = snapshot.layoutValid && snapshot.activeLocation == static_cast<int>(index);
                std::snprintf(
                    label,
                    sizeof(label),
                    active ? "Location %02zu  [ACTIVE]" : "Location %02zu",
                    index + 1);

                const bool selected = selectedLocation == static_cast<int>(index);
                if (ImGui::Selectable(label, selected))
                    selectedLocation = static_cast<int>(index);
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        DescribeLastV11Item("Select any of the 50 exact Street Dealer spawn points decoded from fm_street_dealer.c::func_359.");

        const auto& location = Game::StreetDealer::Enhanced173::Locations[static_cast<std::size_t>(selectedLocation)];
        ImGui::TextDisabled("X %.3f | Y %.3f | Z %.3f", location.x, location.y, location.z);

        ImGui::BeginDisabled(!teleport.nativeReady || teleport.pending);
        if (ImGui::Button(
                teleport.pending ? "Teleporting..." : "Teleport to Selected Location",
                ImVec2(-1.0f, 28.0f)))
        {
            static_cast<void>(runtime.QueueTeleport(static_cast<std::size_t>(selectedLocation)));
        }
        ImGui::EndDisabled();
        DescribeLastV11Item("Teleport to the selected Rockstar Street Dealer coordinate on the GTA game thread.");

        const bool activeLocationValid = snapshot.layoutValid
            && snapshot.activeLocation >= 0
            && snapshot.activeLocation < static_cast<int>(Game::StreetDealer::Enhanced173::Locations.size());
        ImGui::BeginDisabled(!teleport.nativeReady || teleport.pending || !activeLocationValid);
        if (ImGui::Button("Teleport to Active Dealer", ImVec2(-1.0f, 28.0f)) && activeLocationValid)
        {
            selectedLocation = snapshot.activeLocation;
            static_cast<void>(runtime.QueueTeleport(static_cast<std::size_t>(snapshot.activeLocation)));
        }
        ImGui::EndDisabled();
        DescribeLastV11Item("Jump directly to the dealer location Rockstar currently has active in fm_street_dealer.");

        if (!teleport.nativeReady)
            ImGui::TextDisabled("Teleport natives are still initializing.");
        else if (teleport.haveResult)
        {
            if (teleport.lastSucceeded)
                ImGui::TextColored(V11Theme::Accent, "%s", teleport.message.c_str());
            else
                ImGui::TextWrapped("%s", teleport.message.c_str());
        }

        ImGui::Spacing();
    }

    inline void RenderStreetDealerPanel() noexcept
    {
        auto& runtime = Game::StreetDealer::Runtime::Get();
        const auto snapshot = runtime.GetSnapshot();

        ImGui::TextWrapped(
            "Enhanced Street Dealer state decoded from fm_street_dealer.c. All 50 dealer locations are actionable below; payout fields are displayed without altering Rockstar's dealer economy state.");
        ImGui::Spacing();

        ImGui::BeginDisabled(snapshot.pending);
        if (ImGui::Button(snapshot.pending ? "Refreshing Dealer..." : "Refresh Dealer State", ImVec2(-1.0f, 28.0f)))
            static_cast<void>(runtime.QueueRefresh());
        ImGui::EndDisabled();
        DescribeLastV11Item("Read current Enhanced 1.73 / b1158.13 Street Dealer state on the GTA game thread.");

        RenderStreetDealerLocationTeleports(runtime, snapshot);

        if (!snapshot.haveResult)
        {
            ImGui::TextDisabled("Refresh after joining GTA Online to identify today's active dealer location.");
            SetV11Description("Street Dealer Manager - all 50 Enhanced locations with teleport plus validated dealer state and payouts.");
            return;
        }

        if (!snapshot.lastSucceeded || !snapshot.layoutValid)
        {
            ImGui::TextWrapped("%s", snapshot.message.c_str());
            ImGui::TextDisabled("Dealer state is unavailable, but the 50 location teleports remain usable when natives are ready.");
            SetV11Description("Street Dealer Manager - location teleports remain available while dealer-state validation is unavailable.");
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
        SetV11Description("Street Dealer Manager - all 50 location teleports, active dealer location, premium product and decoded per-unit payouts.");
    }
}
