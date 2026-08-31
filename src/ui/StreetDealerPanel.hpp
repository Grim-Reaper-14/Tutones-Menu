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
            "Enhanced Street Dealer state from fm_street_dealer.c. This page is read-only until each product field is semantically verified.");
        ImGui::Spacing();

        ImGui::BeginDisabled(snapshot.pending);
        if (ImGui::Button(snapshot.pending ? "Refreshing Dealer..." : "Refresh Dealer State", ImVec2(-1.0f, 28.0f)))
            static_cast<void>(runtime.QueueRefresh());
        ImGui::EndDisabled();
        DescribeLastV11Item("Read the current Enhanced 1.73 / b1158.13 Street Dealer globals on the GTA game thread.");

        if (!snapshot.haveResult)
        {
            ImGui::TextDisabled("Refresh after joining GTA Online.");
            SetV11Description("Street Dealer Manager - validated Enhanced dealer state and read-only product records.");
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
                ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Dealer", ImGuiTableColumnFlags_WidthStretch, 0.8f);
            ImGui::TableSetupColumn("Field 1", ImGuiTableColumnFlags_WidthStretch, 0.9f);
            ImGui::TableSetupColumn("Field 2", ImGuiTableColumnFlags_WidthStretch, 0.9f);
            ImGui::TableSetupColumn("Field 3", ImGuiTableColumnFlags_WidthStretch, 0.9f);
            ImGui::TableSetupColumn("Field 4", ImGuiTableColumnFlags_WidthStretch, 0.9f);
            ImGui::TableSetupColumn("Field 5", ImGuiTableColumnFlags_WidthStretch, 0.9f);
            ImGui::TableSetupColumn("Done", ImGuiTableColumnFlags_WidthStretch, 0.7f);
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
                ImGui::TableSetColumnIndex(1); ImGui::Text("%d", dealer.value1);
                ImGui::TableSetColumnIndex(2); ImGui::Text("%d", dealer.value2);
                ImGui::TableSetColumnIndex(3); ImGui::Text("%d", dealer.value3);
                ImGui::TableSetColumnIndex(4); ImGui::Text("%d", dealer.value4);
                ImGui::TableSetColumnIndex(5); ImGui::Text("%d", dealer.value5);
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
            "The five raw record fields are intentionally not labeled as product stock/prices yet. The decompile proves their placement, but Tutones will name them only after their exact meaning is cross-checked through the dealer state machine.");
        SetV11Description("Street Dealer Manager - validated Enhanced dealer location, active record and read-only dealer data.");
    }
}
