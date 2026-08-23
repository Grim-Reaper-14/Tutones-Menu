#pragma once

#include "../../game/GamePointers.hpp"
#include "../../game/NetshoppingNatives.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>

namespace Tutones::Game::NetworkFeatures
{
    enum class DirectServiceTransactionAction : std::uint8_t
    {
        Earn,
        Spend,
    };

    enum class DirectServiceTransactionResult : std::uint8_t
    {
        None,
        Queued,
        CheckoutStarted,
        SessionUnavailable,
        ServerTransactionsUnavailable,
        CatalogItemInvalid,
        PriceUnavailable,
        ShopControllerUnavailable,
        BeginServiceFailed,
        CheckoutFailed,
        QueueFailed,
    };

    struct DirectServiceTransactionSnapshot final
    {
        bool pending{};
        std::uint32_t serviceHash{};
        std::uint32_t actionHash{};
        DirectServiceTransactionAction action{DirectServiceTransactionAction::Earn};
        int price{};
        int transactionId{-1};
        DirectServiceTransactionResult result{DirectServiceTransactionResult::None};
    };

    class DirectServiceTransactionRuntime final
    {
    public:
        static DirectServiceTransactionRuntime& Get() noexcept
        {
            static DirectServiceTransactionRuntime instance;
            return instance;
        }

        [[nodiscard]] bool Queue(
            std::uint32_t serviceHash,
            DirectServiceTransactionAction action = DirectServiceTransactionAction::Earn)
        {
            if (serviceHash == 0)
                return false;

            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            Store(serviceHash, action, ActionHash(action), 0, -1, DirectServiceTransactionResult::Queued);
            if (Runtime::GameRuntime::Get().Enqueue([this, serviceHash, action] {
                    ExecuteOnGameThread(serviceHash, action);
                }))
            {
                return true;
            }

            Finish(serviceHash, action, 0, -1, DirectServiceTransactionResult::QueueFailed);
            return false;
        }

        [[nodiscard]] DirectServiceTransactionSnapshot Snapshot() const noexcept
        {
            std::scoped_lock lock(m_Mutex);
            auto snapshot = m_Snapshot;
            snapshot.pending = m_Pending.load(std::memory_order_acquire);
            return snapshot;
        }

    private:
        // Precomputed JOAAT values. Keeping these as literals avoids MSVC C2131
        // when a constexpr member function is used to initialize static data
        // before the class definition is complete.
        static constexpr std::uint32_t ShopControllerHash = 0x39DA738Bu;
        static constexpr std::uint32_t ServiceThresholdCategory = 0x57DE404Eu;
        static constexpr std::uint32_t EarnAction = 0x562592BBu;  // NET_SHOP_ACTION_EARN
        static constexpr std::uint32_t SpendAction = 0x2005D9A9u; // NET_SHOP_ACTION_SPEND

        [[nodiscard]] static constexpr std::uint32_t ActionHash(
            DirectServiceTransactionAction action) noexcept
        {
            return action == DirectServiceTransactionAction::Spend ? SpendAction : EarnAction;
        }

        class ScriptTlsScope final
        {
        public:
            ScriptTlsScope(Types::TlsContext* tls, Types::ScriptThread* thread) noexcept
                : m_Tls(tls)
            {
                if (!m_Tls || !thread)
                    return;
                m_OriginalThread = m_Tls->currentScriptThread;
                m_OriginalActive = m_Tls->scriptThreadActive;
                m_Tls->currentScriptThread = thread;
                m_Tls->scriptThreadActive = true;
                m_Active = true;
            }

            ~ScriptTlsScope()
            {
                if (!m_Active)
                    return;
                m_Tls->scriptThreadActive = m_OriginalActive;
                m_Tls->currentScriptThread = m_OriginalThread;
            }

            [[nodiscard]] bool Active() const noexcept { return m_Active; }

        private:
            Types::TlsContext* m_Tls{};
            Types::ScriptThread* m_OriginalThread{};
            bool m_OriginalActive{};
            bool m_Active{};
        };

        void ExecuteOnGameThread(
            std::uint32_t serviceHash,
            DirectServiceTransactionAction action) noexcept
        {
            bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            if (!sessionStarted || !*sessionStarted)
                return Finish(serviceHash, action, 0, -1, DirectServiceTransactionResult::SessionUnavailable);

            const auto useServerTransactions = NetshoppingNatives::UseServerTransactions();
            if (!useServerTransactions || !*useServerTransactions)
                return Finish(serviceHash, action, 0, -1, DirectServiceTransactionResult::ServerTransactionsUnavailable);

            const auto valid = NetshoppingNatives::CatalogItemKeyIsValid(serviceHash);
            if (!valid || !*valid)
                return Finish(serviceHash, action, 0, -1, DirectServiceTransactionResult::CatalogItemInvalid);

            const auto price = NetshoppingNatives::GetPrice(serviceHash, ServiceThresholdCategory, true);
            if (!price || *price < 0)
                return Finish(serviceHash, action, 0, -1, DirectServiceTransactionResult::PriceUnavailable);

            auto& scripts = Script::ScriptRuntime::Get();
            auto* shopController = scripts.FindThread(ShopControllerHash);
            if (!scripts.IsReady() || !shopController || !shopController->stack)
                return Finish(serviceHash, action, *price, -1, DirectServiceTransactionResult::ShopControllerUnavailable);

            auto* tls = Types::TlsContext::Get();
            ScriptTlsScope scope(tls, shopController);
            if (!scope.Active())
                return Finish(serviceHash, action, *price, -1, DirectServiceTransactionResult::ShopControllerUnavailable);

            int transactionId{-1};
            const auto began = NetshoppingNatives::BeginService(
                &transactionId,
                ServiceThresholdCategory,
                serviceHash,
                ActionHash(action),
                *price,
                4);
            if (!began || !*began || transactionId < 0)
                return Finish(serviceHash, action, *price, transactionId, DirectServiceTransactionResult::BeginServiceFailed);

            const auto checkout = NetshoppingNatives::CheckoutStart(transactionId);
            if (!checkout || !*checkout)
                return Finish(serviceHash, action, *price, transactionId, DirectServiceTransactionResult::CheckoutFailed);

            Finish(serviceHash, action, *price, transactionId, DirectServiceTransactionResult::CheckoutStarted);
        }

        void Store(
            std::uint32_t hash,
            DirectServiceTransactionAction action,
            std::uint32_t actionHash,
            int price,
            int transactionId,
            DirectServiceTransactionResult result) noexcept
        {
            std::scoped_lock lock(m_Mutex);
            m_Snapshot.serviceHash = hash;
            m_Snapshot.action = action;
            m_Snapshot.actionHash = actionHash;
            m_Snapshot.price = price;
            m_Snapshot.transactionId = transactionId;
            m_Snapshot.result = result;
            m_Snapshot.pending = m_Pending.load(std::memory_order_acquire);
        }

        void Finish(
            std::uint32_t hash,
            DirectServiceTransactionAction action,
            int price,
            int transactionId,
            DirectServiceTransactionResult result) noexcept
        {
            m_Pending.store(false, std::memory_order_release);
            Store(hash, action, ActionHash(action), price, transactionId, result);
        }

        std::atomic<bool> m_Pending{false};
        mutable std::mutex m_Mutex;
        DirectServiceTransactionSnapshot m_Snapshot{};
    };
}
