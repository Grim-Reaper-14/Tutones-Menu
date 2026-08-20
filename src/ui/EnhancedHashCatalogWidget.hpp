#pragma once

#include "../features/network/EnhancedTransactionLists.hpp"

#include <imgui.h>

#include <cstdio>
#include <string_view>

namespace Tutones::UI
{
    inline void RenderNamedHashList(
        const char* childId,
        const char* filterLabel,
        std::span<const std::string_view> names,
        ImGuiTextFilter& filter) noexcept
    {
        using Game::NetworkFeatures::TransactionLists::Hash;

        filter.Draw(filterLabel, -1.0f);
        if (ImGui::BeginChild(childId, ImVec2(0.0f, 205.0f), true, ImGuiWindowFlags_HorizontalScrollbar))
        {
            for (std::size_t index = 0; index < names.size(); ++index)
            {
                const auto name = names[index];
                if (filter.IsActive() && !filter.PassFilter(name.data()))
                    continue;

                ImGui::PushID(static_cast<int>(index));
                const std::uint32_t hash = Hash(name);
                ImGui::Text("%s  0x%08X", name.data(), hash);
                if (ImGui::SmallButton("Copy hash"))
                {
                    char buffer[16]{};
                    std::snprintf(buffer, sizeof(buffer), "0x%08X", hash);
                    ImGui::SetClipboardText(buffer);
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Copy name"))
                    ImGui::SetClipboardText(name.data());
                ImGui::PopID();
                ImGui::Separator();
            }
        }
        ImGui::EndChild();
    }

    inline void RenderAdditionalTransactionLists() noexcept
    {
        using namespace Game::NetworkFeatures::TransactionLists;

        ImGui::SeparatorText("Additional Enhanced transaction catalogs");
        ImGui::TextDisabled("Pinned decompile helper data only. These panels never start a NETSHOP transaction.");

        if (ImGui::BeginTabBar("##extra_transaction_hash_tabs"))
        {
            if (ImGui::BeginTabItem("Spend"))
            {
                static ImGuiTextFilter filter;
                ImGui::TextDisabled("109 SERVICE_SPEND_* identifiers");
                RenderNamedHashList("##spend_hash_list", "Filter spend", SpendNames, filter);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Awards"))
            {
                static ImGuiTextFilter filter;
                ImGui::TextDisabled("63 award / bonus SERVICE_EARN_* identifiers");
                RenderNamedHashList("##award_hash_list", "Filter awards", AwardNames, filter);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Refunds"))
            {
                static ImGuiTextFilter filter;
                ImGui::TextDisabled("17 named refund identifiers");
                RenderNamedHashList("##refund_hash_list", "Filter refunds", RefundNames, filter);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("NETSHOP"))
            {
                static ImGuiTextFilter filter;
                ImGui::TextDisabled("Transaction type, action, category and invalid/reset identifiers");
                RenderNamedHashList("##netshop_hash_list", "Filter NETSHOP", NetshopNames, filter);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Raw"))
            {
                ImGui::TextDisabled("23 numeric cases preserved exactly; symbolic names are intentionally not guessed.");
                if (ImGui::BeginChild("##raw_hash_list", ImVec2(0.0f, 205.0f), true, ImGuiWindowFlags_HorizontalScrollbar))
                {
                    for (std::size_t index = 0; index < RawValues.size(); ++index)
                    {
                        const std::int32_t value = RawValues[index];
                        const std::uint32_t hash = RawHash(value);
                        ImGui::PushID(static_cast<int>(5000 + index));
                        ImGui::Text("%d  ->  0x%08X", value, hash);
                        if (value == 1445302971)
                            ImGui::TextDisabled("Observed as the earn action hash in the shared transaction helper.");
                        else
                            ImGui::TextDisabled("Unlabeled numeric transaction case from the pinned helper.");
                        if (ImGui::SmallButton("Copy hash"))
                        {
                            char buffer[16]{};
                            std::snprintf(buffer, sizeof(buffer), "0x%08X", hash);
                            ImGui::SetClipboardText(buffer);
                        }
                        ImGui::PopID();
                        ImGui::Separator();
                    }
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
}
