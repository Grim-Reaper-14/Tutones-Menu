#pragma once

#include "../features/network/DirectServiceTransactionRuntime.hpp"
#include "../features/network/EnhancedTransactionLists.hpp"
#include "../features/network/NetworkRuntime.hpp"

#include <imgui.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace Tutones::UI
{
    [[nodiscard]] inline const char* DirectServiceTransactionResultName(
        Game::NetworkFeatures::DirectServiceTransactionResult result) noexcept
    {
        using Game::NetworkFeatures::DirectServiceTransactionResult;
        switch (result)
        {
        case DirectServiceTransactionResult::Queued: return "queued";
        case DirectServiceTransactionResult::CheckoutStarted: return "checkout started";
        case DirectServiceTransactionResult::SessionUnavailable: return "Online session unavailable";
        case DirectServiceTransactionResult::ServerTransactionsUnavailable: return "server transactions unavailable";
        case DirectServiceTransactionResult::CatalogItemInvalid: return "catalog item invalid";
        case DirectServiceTransactionResult::PriceUnavailable: return "price unavailable";
        case DirectServiceTransactionResult::ShopControllerUnavailable: return "shop_controller unavailable";
        case DirectServiceTransactionResult::BeginServiceFailed: return "begin service failed";
        case DirectServiceTransactionResult::CheckoutFailed: return "checkout failed";
        case DirectServiceTransactionResult::QueueFailed: return "game-thread queue failed";
        default: return "idle";
        }
    }

    [[nodiscard]] inline const char* DirectServiceTransactionActionName(
        Game::NetworkFeatures::DirectServiceTransactionAction action) noexcept
    {
        using Game::NetworkFeatures::DirectServiceTransactionAction;
        return action == DirectServiceTransactionAction::Spend ? "Spend" : "Earn";
    }

    inline void RenderNamedHashList(
        const char* childId,
        const char* filterLabel,
        std::span<const std::string_view> names,
        ImGuiTextFilter& filter,
        bool executable,
        Game::NetworkFeatures::DirectServiceTransactionAction action) noexcept
    {
        using Game::NetworkFeatures::DirectServiceTransactionRuntime;
        using Game::NetworkFeatures::NetworkRuntime;
        using Game::NetworkFeatures::TransactionLists::Hash;

        const auto network = NetworkRuntime::Get().Snapshot();
        auto& direct = DirectServiceTransactionRuntime::Get();
        const auto transaction = direct.Snapshot();

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
                    const bool blocked = !network.running
                        || !network.sessionStarted
                        || transaction.pending;
                    ImGui::BeginDisabled(blocked);
                    if (ImGui::SmallButton("Execute"))
                        static_cast<void>(direct.Queue(hash, action));
                    ImGui::EndDisabled();

                    if (!network.sessionStarted)
                    {
                        ImGui::SameLine();
                        ImGui::TextDisabled("Join GTA Online");
                    }
                    else if (transaction.pending)
                    {
                        ImGui::SameLine();
                        ImGui::TextDisabled("Transaction pending");
                    }
                }
                else
                {
                    ImGui::TextDisabled("Reference only");
                }

                ImGui::PopID();
                ImGui::Separator();
            }
        }
        ImGui::EndChild();
    }

    inline void RenderAdditionalTransactionLists(bool executable = false) noexcept
    {
        using namespace Game::NetworkFeatures;
        using namespace Game::NetworkFeatures::TransactionLists;

        const auto network = NetworkRuntime::Get().Snapshot();
        const auto transaction = DirectServiceTransactionRuntime::Get().Snapshot();

        ImGui::SeparatorText("Business Transactions");
        if (executable)
        {
            ImGui::TextWrapped(
                "Named service items use the direct NETSHOP service path: validate the catalog item, read its live price, switch into shop_controller TLS, call NET_GAMESERVER_BEGIN_SERVICE with the correct Earn/Spend action, then NET_GAMESERVER_CHECKOUT_START.");

            if (transaction.serviceHash != 0)
            {
                ImGui::Text(
                    "Last: 0x%08X - %s",
                    transaction.serviceHash,
                    DirectServiceTransactionResultName(transaction.result));
                ImGui::TextDisabled(
                    "Action: %s (0x%08X)",
                    DirectServiceTransactionActionName(transaction.action),
                    transaction.actionHash);
                if (transaction.price >= 0)
                    ImGui::TextDisabled("Catalog price/value: $%d", transaction.price);
                if (transaction.transactionId >= 0)
                    ImGui::TextDisabled("Transaction ID: %d", transaction.transactionId);
            }
            if (transaction.pending)
                ImGui::TextDisabled("Waiting for the queued NETSHOP service transaction to run on the GTA script thread...");

            ImGui::TextDisabled(
                "Checkout started means GTA accepted creation/start of the service checkout. Service/refund settlement remains server-controlled; the service path is fire-and-forget.");
        }
        else
        {
            ImGui::TextDisabled("Read-only transaction catalog inspector. Execute actions are available under Recovery > Businesses.");
        }

        if (ImGui::BeginTabBar("##extra_transaction_hash_tabs"))
        {
            if (ImGui::BeginTabItem("Spend"))
            {
                static ImGuiTextFilter filter;
                ImGui::TextDisabled(executable
                    ? "109 executable SERVICE_SPEND_* service items. These use NET_SHOP_ACTION_SPEND and can deduct the live catalog price."
                    : "109 SERVICE_SPEND_* identifiers");
                RenderNamedHashList(
                    "##spend_hash_list",
                    "Filter spend",
                    SpendNames,
                    filter,
                    executable,
                    DirectServiceTransactionAction::Spend);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Awards"))
            {
                static ImGuiTextFilter filter;
                ImGui::TextDisabled(executable
                    ? "63 executable award / bonus SERVICE_EARN_* service items using NET_SHOP_ACTION_EARN."
                    : "63 award / bonus SERVICE_EARN_* identifiers");
                RenderNamedHashList(
                    "##award_hash_list",
                    "Filter awards",
                    AwardNames,
                    filter,
                    executable,
                    DirectServiceTransactionAction::Earn);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Refunds"))
            {
                static ImGuiTextFilter filter;
                ImGui::TextDisabled(executable
                    ? "17 executable SERVICE_EARN_REFUND_* service items using NET_SHOP_ACTION_EARN."
                    : "17 named refund identifiers");
                RenderNamedHashList(
                    "##refund_hash_list",
                    "Filter refunds",
                    RefundNames,
                    filter,
                    executable,
                    DirectServiceTransactionAction::Earn);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("NETSHOP"))
            {
                static ImGuiTextFilter filter;
                ImGui::TextDisabled("Transaction type, action, category and invalid/reset identifiers.");
                RenderNamedHashList(
                    "##netshop_hash_list",
                    "Filter NETSHOP",
                    NetshopNames,
                    filter,
                    false,
                    DirectServiceTransactionAction::Earn);
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
                            ImGui::TextDisabled("NET_SHOP_ACTION_EARN metadata value.");
                        else if (value == 537254313)
                            ImGui::TextDisabled("NET_SHOP_ACTION_SPEND metadata value.");
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

        if (executable && !network.sessionStarted)
            ImGui::TextDisabled("Join GTA Online before running service transactions.");
    }
}
