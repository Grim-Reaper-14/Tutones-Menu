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

        [[nodiscard]] bool Queue(std::uint32_t serviceHash)
        {
            if (serviceHash == 0)
                return false;

            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            Store(serviceHash, 0, -1, DirectServiceTransactionResult::Queued);
            if (Runtime::GameRuntime::Get().Enqueue([this, serviceHash] {
                    ExecuteOnGameThread(serviceHash);
                }))
            {
                return true;
            }

            Finish(serviceHash, 0, -1, DirectServiceTransactionResult::QueueFailed);
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
        static constexpr std::uint32_t Joaat(const char* text) noexcept
        {
            std::uint32_t hash{};
            while (text && *text)
            {
                char c = *text++;
                if (c >= 'A' && c <= 'Z')
                    c = static_cast<char>(c - 'A' + 'a');
                hash += static_cast<std::uint8_t>(c);
                hash += hash << 10;
                hash ^= hash >> 6;
            }
            hash += hash << 3;
            hash ^= hash >> 11;
            hash += hash << 15;
            return hash;
        }

        static constexpr std::uint32_t ShopControllerHash = Joaat("shop_controller");
        static constexpr std::uint32_t ServiceThresholdCategory = Joaat("CATEGORY_SERVICE_WITH_THRESHOLD");
        static constexpr std::uint32_t EarnAction = Joaat("NET_SHOP_ACTION_EARN");

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

        void ExecuteOnGameThread(std::uint32_t serviceHash) noexcept
        {
            bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            if (!sessionStarted || !*sessionStarted)
                return Finish(serviceHash, 0, -1, DirectServiceTransactionResult::SessionUnavailable);

            const auto useServerTransactions = NetshoppingNatives::UseServerTransactions();
            if (!useServerTransactions || !*useServerTransactions)
                return Finish(serviceHash, 0, -1, DirectServiceTransactionResult::ServerTransactionsUnavailable);

            const auto valid = NetshoppingNatives::CatalogItemKeyIsValid(serviceHash);
            if (!valid || !*valid)
                return Finish(serviceHash, 0, -1, DirectServiceTransactionResult::CatalogItemInvalid);

            const auto price = NetshoppingNatives::GetPrice(serviceHash, ServiceThresholdCategory, true);
            if (!price || *price < 0)
                return Finish(serviceHash, 0, -1, DirectServiceTransactionResult::PriceUnavailable);

            auto& scripts = Script::ScriptRuntime::Get();
            auto* shopController = scripts.FindThread(ShopControllerHash);
            if (!scripts.IsReady() || !shopController || !shopController->stack)
                return Finish(serviceHash, *price, -1, DirectServiceTransactionResult::ShopControllerUnavailable);

            auto* tls = Types::TlsContext::Get();
            ScriptTlsScope scope(tls, shopController);
            if (!scope.Active())
                return Finish(serviceHash, *price, -1, DirectServiceTransactionResult::ShopControllerUnavailable);

            int transactionId{-1};
            const auto began = NetshoppingNatives::BeginService(
                &transactionId,
                ServiceThresholdCategory,
                serviceHash,
                EarnAction,
                *price,
                4);
            if (!began || !*began || transactionId < 0)
                return Finish(serviceHash, *price, transactionId, DirectServiceTransactionResult::BeginServiceFailed);

            const auto checkout = NetshoppingNatives::CheckoutStart(transactionId);
            if (!checkout || !*checkout)
                return Finish(serviceHash, *price, transactionId, DirectServiceTransactionResult::CheckoutFailed);

            Finish(serviceHash, *price, transactionId, DirectServiceTransactionResult::CheckoutStarted);
        }

        void Store(
            std::uint32_t hash,
            int price,
            int transactionId,
            DirectServiceTransactionResult result) noexcept
        {
            std::scoped_lock lock(m_Mutex);
            m_Snapshot.serviceHash = hash;
            m_Snapshot.price = price;
            m_Snapshot.transactionId = transactionId;
            m_Snapshot.result = result;
            m_Snapshot.pending = m_Pending.load(std::memory_order_acquire);
        }

        void Finish(
            std::uint32_t hash,
            int price,
            int transactionId,
            DirectServiceTransactionResult result) noexcept
        {
            m_Pending.store(false, std::memory_order_release);
            Store(hash, price, transactionId, result);
        }

        std::atomic<bool> m_Pending{false};
        mutable std::mutex m_Mutex;
        DirectServiceTransactionSnapshot m_Snapshot{};
    };
}
