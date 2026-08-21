#pragma once

#include "../features/network/EnhancedTransactionLists.hpp"
#include "../features/network/NetworkRuntime.hpp"

#include <imgui.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace Tutones::UI
{
    [[nodiscard]] inline const char* ServiceTransactionResultName(
        Game::NetworkFeatures::ServiceTransactionResult result) noexcept
    {
        using Game::NetworkFeatures::ServiceTransactionResult;
        switch (result)
        {
        case ServiceTransactionResult::Queued: return "queued";
        case ServiceTransactionResult::Dispatched: return "dispatched";
        case ServiceTransactionResult::SessionUnavailable: return "Online session unavailable";
        case ServiceTransactionResult::ServerTransactionsUnavailable: return "server transactions unavailable";
        case ServiceTransactionResult::CatalogItemInvalid: return "catalog item invalid";
        case ServiceTransactionResult::PriceUnavailable: return "price unavailable";
        case ServiceTransactionResult::ShopControllerUnavailable: return "shop_controller helper unavailable";
        case ServiceTransactionResult::DispatchFailed: return "game-thread dispatch failed";
        default: return "idle";
        }
    }

    inline void RenderNamedHashList(
        const char* childId,
        const char* filterLabel,
        std::span<const std::string_view> names,
        ImGuiTextFilter& filter,
        bool executable) noexcept
    {
        using Game::NetworkFeatures::NetworkRuntime;
        using Game::NetworkFeatures::TransactionLists::Hash;

        auto& runtime = NetworkRuntime::Get();
        const auto snapshot = runtime.Snapshot();

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

                if (executable)
                {
                    const bool blocked = !snapshot.running
                        || !snapshot.sessionStarted
                        || snapshot.transactionPending;
                    ImGui::BeginDisabled(blocked);
                    if (ImGui::SmallButton("Execute"))
                        static_cast<void>(runtime.QueueServiceTransaction(hash));
                    ImGui::EndDisabled();

                    if (!snapshot.sessionStarted)
                    {
                        ImGui::SameLine();
                        ImGui::TextDisabled("Join GTA Online");
                    }
                    else if (snapshot.transactionPending)
                    {
                        ImGui::SameLine();
                        ImGui::TextDisabled("Transaction pending");
                    }
                }
                else
                {
                    ImGui::TextDisabled("Reference-only transaction metadata; not an executable service item.");
                }

                ImGui::PopID();
                ImGui::Separator();
            }
        }
        ImGui::EndChild();
    }

    inline void RenderAdditionalTransactionLists() noexcept
    {
        using namespace Game::NetworkFeatures;
        using namespace Game::NetworkFeatures::TransactionLists;

        auto& runtime = NetworkRuntime::Get();
        const auto snapshot = runtime.Snapshot();

        ImGui::SeparatorText("Business Transactions");
        ImGui::TextWrapped(
            "Named service items execute through GTA Online's server transaction catalog. "
            "Each request checks the active Online session, server-transaction support, catalog validity, live service price and shop_controller before dispatch.");

        if (snapshot.lastTransactionHash != 0)
        {
            ImGui::Text(
                "Last: 0x%08X - %s",
                snapshot.lastTransactionHash,
                ServiceTransactionResultName(snapshot.lastTransactionResult));
            if (snapshot.lastTransactionPrice >= 0)
                ImGui::TextDisabled("Catalog price/value: $%d", snapshot.lastTransactionPrice);
            if (snapshot.lastTransactionIndex >= 0)
                ImGui::TextDisabled("Transaction index: %d", snapshot.lastTransactionIndex);
        }
        if (snapshot.transactionPending)
            ImGui::TextDisabled("Waiting for the queued service transaction to run on the GTA script thread...");

        ImGui::TextDisabled(
            "Dispatched means the shop_controller transaction helper ran; the Rockstar service may still accept, reject or cap the transaction asynchronously.");

        if (ImGui::BeginTabBar("##extra_transaction_hash_tabs"))
        {
            if (ImGui::BeginTabItem("Spend"))
            {
                static ImGuiTextFilter filter;
                ImGui::TextDisabled("109 executable SERVICE_SPEND_* service items. These can deduct the live catalog price.");
                RenderNamedHashList("##spend_hash_list", "Filter spend", SpendNames, filter, true);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Awards"))
            {
                static ImGuiTextFilter filter;
                ImGui::TextDisabled("63 executable award / bonus SERVICE_EARN_* service items.");
                RenderNamedHashList("##award_hash_list", "Filter awards", AwardNames, filter, true);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Refunds"))
            {
                static ImGuiTextFilter filter;
                ImGui::TextDisabled("17 executable named refund service items.");
                RenderNamedHashList("##refund_hash_list", "Filter refunds", RefundNames, filter, true);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("NETSHOP"))
            {
                static ImGuiTextFilter filter;
                ImGui::TextDisabled("Transaction type, action, category and invalid/reset identifiers.");
                RenderNamedHashList("##netshop_hash_list", "Filter NETSHOP", NetshopNames, filter, false);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Raw"))
            {
                ImGui::TextDisabled("23 numeric cases preserved exactly; symbolic service-item meaning is not guessed or executed.");
                if (ImGui::BeginChild("##raw_hash_list", ImVec2(0.0f, 205.0f), true, ImGuiWindowFlags_HorizontalScrollbar))
                {
                    for (std::size_t index = 0; index < RawValues.size(); ++index)
                    {
                        const std::int32_t value = RawValues[index];
                        const std::uint32_t hash = RawHash(value);
                        ImGui::PushID(static_cast<int>(5000 + index));
                        ImGui::Text("%d  ->  0x%08X", value, hash);
                        if (value == 1445302971)
                            ImGui::TextDisabled("Observed as the earn action hash in the shared transaction helper; metadata only.");
                        else
                            ImGui::TextDisabled("Unlabeled numeric transaction case; reference only.");
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
