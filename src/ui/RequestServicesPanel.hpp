#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/network/RequestServicesRuntime.hpp"

#include <imgui.h>

#include <cstddef>

namespace Tutones::UI
{
    inline void RenderRequestServicesPanel() noexcept
    {
        using Game::NetworkFeatures::RequestService;
        using Game::NetworkFeatures::RequestServiceCatalog;
        using Game::NetworkFeatures::RequestServicesRuntime;

        auto& runtime = RequestServicesRuntime::Get();
        const auto state = runtime.Snapshot();

        if (!ImGui::CollapsingHeader("Request Services", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        ImGui::TextDisabled("Enhanced 1.73 / b1158.13 | Global_2733326 request flags");
        ImGui::BeginDisabled(state.pending);

        for (std::size_t index = 0; index < RequestServiceCatalog.size(); ++index)
        {
            const auto& definition = RequestServiceCatalog[index];
            ImGui::PushID(static_cast<int>(index));
            if (ImGui::Button(definition.label, ImVec2(215.0f, 0.0f)))
                runtime.QueueRequest(static_cast<RequestService>(index));
            ImGui::PopID();

            if ((index % 2u) == 0u && index + 1u < RequestServiceCatalog.size())
                ImGui::SameLine();
        }

        ImGui::SeparatorText("Special request sequences");
        if (ImGui::Button("Helicopter Pickup (SuperVolito)", ImVec2(-1.0f, 0.0f)))
            runtime.QueueSuperVolito();
        DescribeLastV11Item("Set Global_2733326.f_547 = 1 first, then Global_2733326.f_540 = 1 for the supplied SuperVolito pickup sequence.");

        if (ImGui::Button("Ballistic Equipment - Instant Equip", ImVec2(215.0f, 0.0f)))
            runtime.QueueBallisticInstantEquip();
        ImGui::SameLine();
        if (ImGui::Button("Instant Remove", ImVec2(-1.0f, 0.0f)))
            runtime.QueueBallisticInstantRemove();
        DescribeLastV11Item("Use the supplied Ballistic Equipment follow-up flags: f_549 for Instant Equip and f_550 for Instant Remove.");

        ImGui::EndDisabled();

        if (state.pending)
            ImGui::TextDisabled("Request queued on the GTA script thread...");
        else if (state.haveResult)
            ImGui::TextDisabled("%s: %s", state.lastSucceeded ? "Success" : "Failed", state.message.c_str());
        else
            ImGui::TextDisabled("Ready. Each button writes its supplied one-shot request flag to 1.");
    }
}
