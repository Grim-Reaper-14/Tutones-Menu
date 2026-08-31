#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/game/ServicesDiagnosticsRuntime.hpp"

#include <imgui.h>

#include <cstddef>

namespace Tutones::UI
{
    inline void RenderServicesDiagnosticsPanel() noexcept
    {
        auto& runtime = Game::ServicesDiagnostics::Runtime::Get();
        const auto snapshot = runtime.GetSnapshot();

        ImGui::TextWrapped(
            "Read-only state from am_contact_requests.c. Existing Tutones service buttons stay where they are; this page shows what Rockstar's controller currently sees.");
        ImGui::Spacing();

        ImGui::BeginDisabled(snapshot.pending);
        if (ImGui::Button(snapshot.pending ? "Refreshing Services..." : "Refresh Service State", ImVec2(-1.0f, 28.0f)))
            static_cast<void>(runtime.QueueRefresh());
        ImGui::EndDisabled();
        DescribeLastV11Item("Read the current Enhanced contact-request latches without firing a service request.");

        if (!snapshot.haveResult)
        {
            ImGui::TextDisabled("Refresh after joining GTA Online.");
            SetV11Description("Services diagnostics - read-only Enhanced contact-request controller state.");
            return;
        }

        if (!snapshot.lastSucceeded)
        {
            ImGui::TextWrapped("%s", snapshot.message.c_str());
            SetV11Description("Services diagnostics - Enhanced contact-request globals unavailable.");
            return;
        }

        if (snapshot.contactRequestBusy)
            ImGui::TextColored(V11Theme::Accent, "Contact controller: BUSY");
        else
            ImGui::TextDisabled("Contact controller: IDLE");
        ImGui::SameLine();
        ImGui::TextDisabled("| grouped flags: 0x%X", snapshot.groupedRequestFlags);
        ImGui::Separator();

        if (ImGui::BeginTable(
                "##enhanced_service_latches",
                3,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY,
                ImVec2(0.0f, 150.0f)))
        {
            ImGui::TableSetupColumn("Service", ImGuiTableColumnFlags_WidthStretch, 1.8f);
            ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthStretch, 0.8f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.7f);
            ImGui::TableHeadersRow();

            for (std::size_t index = 0; index < Game::NetworkFeatures::RequestServiceCatalog.size(); ++index)
            {
                const auto& definition = Game::NetworkFeatures::RequestServiceCatalog[index];
                const int value = snapshot.requestValues[index];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(definition.label);
                ImGui::TableSetColumnIndex(1);
                if (value != 0)
                    ImGui::TextColored(V11Theme::Accent, "ACTIVE");
                else
                    ImGui::TextDisabled("IDLE");
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%d", value);
            }

            ImGui::EndTable();
        }

        ImGui::TextWrapped(
            "ACTIVE means the request latch is nonzero. It does not automatically mean a service is available; ownership, session, cooldown and contact-specific gates will be decoded separately before Tutones labels them available/unavailable.");
        SetV11Description("Services diagnostics - live Enhanced request latches and contact-controller busy state.");
    }
}
