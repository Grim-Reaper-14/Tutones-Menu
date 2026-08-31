#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/game/DailyActivityRuntime.hpp"

#include <imgui.h>

#include <cstddef>

namespace Tutones::UI
{
    inline void RenderDailyActivityPanel() noexcept
    {
        auto& runtime = Game::DailyActivity::Runtime::Get();
        const auto snapshot = runtime.GetSnapshot();

        ImGui::TextWrapped(
            "Today's verified Enhanced activity state. Only decompile-proven completion/reset data is shown here; unverified daily offsets stay out of the menu.");
        ImGui::Spacing();

        ImGui::BeginDisabled(snapshot.pending);
        if (ImGui::Button(snapshot.pending ? "Refreshing Today..." : "Refresh Today", ImVec2(-1.0f, 28.0f)))
            static_cast<void>(runtime.QueueRefresh());
        ImGui::EndDisabled();
        DescribeLastV11Item("Read current Enhanced daily state and Street Dealer completion packed stats.");

        if (!snapshot.haveResult)
        {
            ImGui::TextDisabled("Refresh after joining GTA Online.");
            SetV11Description("Daily Activity Center - verified Enhanced daily state.");
            return;
        }

        if (!snapshot.lastSucceeded)
        {
            ImGui::TextWrapped("%s", snapshot.message.c_str());
            SetV11Description("Daily Activity Center - Enhanced daily state unavailable.");
            return;
        }

        ImGui::TextColored(V11Theme::Accent, "Street Dealers");
        ImGui::Text("Active location: %d", snapshot.activeStreetDealerLocation);
        ImGui::SameLine();
        ImGui::TextDisabled("| Active record: %d", snapshot.activeStreetDealerRecord);

        if (ImGui::BeginTable(
                "##daily_street_dealers",
                3,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Dealer", ImGuiTableColumnFlags_WidthStretch, 1.2f);
            ImGui::TableSetupColumn("Packed flag", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Today", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableHeadersRow();

            for (std::size_t index = 0; index < snapshot.streetDealerCompleted.size(); ++index)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Dealer %zu", index + 1);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextDisabled("%d", Game::StreetDealer::Enhanced173::CompletionPackedStats[index]);
                ImGui::TableSetColumnIndex(2);

                if (!snapshot.streetDealerCompletionReadable[index])
                    ImGui::TextDisabled("UNKNOWN");
                else if (snapshot.streetDealerCompleted[index])
                    ImGui::TextColored(V11Theme::Accent, "COMPLETED");
                else
                    ImGui::TextDisabled("AVAILABLE");
            }

            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::TextDisabled("More activities will only be added as their Enhanced reset/completion state is verified.");
        SetV11Description("Daily Activity Center - Street Dealer active state and verified daily completion flags.");
    }
}
