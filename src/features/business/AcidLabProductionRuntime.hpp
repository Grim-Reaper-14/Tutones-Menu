#pragma once

#include "AcidLabProductionLogic.hpp"
#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/PlayerNatives.hpp"
#include "../../game/Stats.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::Business
{
    struct AcidLabProductionSnapshot final
    {
        bool actionPending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool persistentStockVerified{};
        bool liveCacheUpdated{};
        int stockUnits{-1};
        std::string message{"Ready"};
    };

    class AcidLabProductionRuntime final
    {
    public:
        static AcidLabProductionRuntime& Get() noexcept
        {
            static AcidLabProductionRuntime instance;
            return instance;
        }

        [[nodiscard]] AcidLabProductionSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            AcidLabProductionSnapshot snapshot = m_Snapshot;
            snapshot.actionPending = m_Pending.load(std::memory_order_acquire);
            return snapshot;
        }

        [[nodiscard]] bool QueueInstantFinish() noexcept
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot.haveResult = false;
                m_Snapshot.lastSucceeded = false;
                m_Snapshot.persistentStockVerified = false;
                m_Snapshot.liveCacheUpdated = false;
                m_Snapshot.stockUnits = -1;
                m_Snapshot.message = "Instant Acid Lab stock fill queued";
            }

            TUTONES_LOG_INFO(
                "business.acid",
                "Queued direct Acid Lab stock fill using the factory-6 persistent stat");

            if (Runtime::GameRuntime::Get().Enqueue([this] { FinishProductionOnGameThread(); }))
                return true;

            Finish(false, false, false, -1, "Could not queue Acid Lab stock fill on the GTA script thread");
            return false;
        }

    private:
        AcidLabProductionRuntime() = default;
        AcidLabProductionRuntime(const AcidLabProductionRuntime&) = delete;
        AcidLabProductionRuntime& operator=(const AcidLabProductionRuntime&) = delete;

        [[nodiscard]] bool MirrorLiveStock(std::int64_t** pages, int player, int stockUnits) noexcept
        {
            if (!pages || player < 0 || player >= AcidLabProductionDetail::MaximumPlayers)
                return false;

            const auto factoryArray = Script::ScriptGlobal(
                AcidLabProductionDetail::FactoryArrayIndex(static_cast<std::size_t>(player)));
            const auto factoryEntry = Script::ScriptGlobal(
                AcidLabProductionDetail::AcidFactoryEntryIndex(static_cast<std::size_t>(player)));
            int* factoryCount = factoryArray.As<int>(pages);
            int* factoryType = factoryEntry.As<int>(pages);
            int* liveStock = factoryEntry.At(AcidLabProductionDetail::ProductOffset).As<int>(pages);
            if (!factoryCount || !factoryType || !liveStock
                || !AcidLabProductionDetail::CanMirrorLiveStock(
                    *factoryCount,
                    *factoryType,
                    *liveStock))
            {
                return false;
            }

            *liveStock = stockUnits;
            return *liveStock == stockUnits;
        }

        void FinishProductionOnGameThread() noexcept
        {
            if (!m_Pending.load(std::memory_order_acquire))
                return;

            auto& pointers = GamePointers::Get();
            bool* sessionStarted = pointers.IsSessionStarted();
            if (!sessionStarted || !*sessionStarted)
            {
                Finish(false, false, false, -1, "Join GTA Online before using Instant Acid Lab production fill");
                return;
            }

            const auto characterIndex = Stats::GetCharIndex();
            if (!characterIndex)
            {
                Finish(false, false, false, -1, "The active GTA Online character index is unavailable");
                return;
            }

            const auto setup = Stats::GetInt(
                AcidLabProductionDetail::SetupStat,
                *characterIndex);
            if (!setup)
            {
                Finish(false, false, false, -1, "Could not read the active character Acid Lab setup state");
                return;
            }
            if (*setup <= 0)
            {
                Finish(false, false, false, -1, "Complete the Acid Lab setup before filling production stock");
                return;
            }

            const auto before = Stats::GetInt(
                AcidLabProductionDetail::StockStat,
                *characterIndex);
            if (!before || !AcidLabProductionDetail::IsValidStock(*before))
            {
                Finish(false, false, false, before.value_or(-1),
                    "The active character Acid Lab stock stat is unavailable or outside its 0-160 range");
                return;
            }

            if (*before < AcidLabProductionDetail::MaximumStockUnits
                && !Stats::SetInt(
                    AcidLabProductionDetail::StockStat,
                    AcidLabProductionDetail::MaximumStockUnits,
                    *characterIndex))
            {
                Finish(false, false, false, *before, "GTA rejected the Acid Lab persistent stock write");
                return;
            }

            const auto after = Stats::GetInt(
                AcidLabProductionDetail::StockStat,
                *characterIndex);
            if (!after || *after != AcidLabProductionDetail::MaximumStockUnits)
            {
                Finish(false, false, false, after.value_or(-1),
                    "Acid Lab stock failed persistent read-back verification");
                return;
            }

            bool liveCacheUpdated = false;
            const auto player = PlayerNatives::PlayerId();
            auto& scripts = Script::ScriptRuntime::Get();
            if (player && scripts.IsReady()
                && scripts.FindThread(AcidLabProductionDetail::FreemodeHash))
            {
                liveCacheUpdated = MirrorLiveStock(
                    pointers.ScriptGlobals(),
                    *player,
                    *after);
            }

            TUTONES_LOG_INFO(
                "business.acid",
                std::string("Filled Acid Lab production stock: character=")
                    + std::to_string(*characterIndex)
                    + " before=" + std::to_string(*before)
                    + " after=" + std::to_string(*after)
                    + " persistent=true liveCache=" + (liveCacheUpdated ? "updated" : "deferred"));

            std::string message = "Acid Lab production is full at 160 / 160; persistent stock verified";
            if (liveCacheUpdated)
                message += " and live business cache updated";
            else
                message += "; re-enter the Acid Lab if its interior display needs a refresh";
            Finish(true, true, liveCacheUpdated, *after, std::move(message));
        }

        void Publish(
            bool success,
            bool persistentStockVerified,
            bool liveCacheUpdated,
            int stockUnits,
            std::string message) noexcept
        {
            std::scoped_lock lock(m_Mutex);
            m_Snapshot.haveResult = true;
            m_Snapshot.lastSucceeded = success;
            m_Snapshot.persistentStockVerified = persistentStockVerified;
            m_Snapshot.liveCacheUpdated = liveCacheUpdated;
            m_Snapshot.stockUnits = stockUnits;
            m_Snapshot.message = std::move(message);
        }

        void Finish(
            bool success,
            bool persistentStockVerified,
            bool liveCacheUpdated,
            int stockUnits,
            std::string message) noexcept
        {
            if (!success)
                TUTONES_LOG_WARN("business.acid", message);
            Publish(
                success,
                persistentStockVerified,
                liveCacheUpdated,
                stockUnits,
                std::move(message));
            m_Pending.store(false, std::memory_order_release);
        }

        std::atomic<bool> m_Pending{false};
        mutable std::mutex m_Mutex;
        AcidLabProductionSnapshot m_Snapshot{};
    };
}
