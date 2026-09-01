#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
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
    namespace AcidLabProductionDetail
    {
        [[nodiscard]] constexpr std::uint32_t Joaat(const char* text) noexcept
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

        inline constexpr std::uint32_t FreemodeHash = Joaat("freemode");
        inline constexpr std::size_t ProductionTimerGlobal = 2708938;
        inline constexpr std::size_t ProductionTriggerGlobal = 2708939;
        inline constexpr int MaximumStockUnits = 160;
        inline constexpr const char* StockStat = "MPX_PRODTOTALFORFACTORY6";
    }

    struct AcidLabProductionSnapshot final
    {
        bool actionPending{};
        bool haveResult{};
        bool lastSucceeded{};
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
                m_Snapshot.message = "Instant Acid Lab production queued";
            }

            if (Runtime::GameRuntime::Get().Enqueue([this] { FinishProductionOnGameThread(); }))
                return true;

            m_Pending.store(false, std::memory_order_release);
            Publish(false, "Could not queue Instant Acid Lab production on the GTA script thread");
            return false;
        }

    private:
        AcidLabProductionRuntime() = default;
        AcidLabProductionRuntime(const AcidLabProductionRuntime&) = delete;
        AcidLabProductionRuntime& operator=(const AcidLabProductionRuntime&) = delete;

        void FinishProductionOnGameThread() noexcept
        {
            auto& pointers = GamePointers::Get();
            bool* sessionStarted = pointers.IsSessionStarted();
            if (!sessionStarted || !*sessionStarted)
            {
                Finish(false, "Join GTA Online before using Instant Acid Lab production");
                return;
            }

            const auto before = Stats::GetInt(AcidLabProductionDetail::StockStat);
            if (before && *before >= AcidLabProductionDetail::MaximumStockUnits)
            {
                SetObservedStock(*before);
                Finish(true, "Acid Lab stock is already full at 160 / 160");
                return;
            }

            const bool wroteStock = Stats::SetInt(
                AcidLabProductionDetail::StockStat,
                AcidLabProductionDetail::MaximumStockUnits);

            bool controllerKicked = false;
            auto* pages = pointers.ScriptGlobals();
            auto& scripts = Script::ScriptRuntime::Get();
            if (pages && scripts.IsReady() && scripts.FindThread(AcidLabProductionDetail::FreemodeHash))
            {
                int* timer = Script::ScriptGlobal(AcidLabProductionDetail::ProductionTimerGlobal).As<int>(pages);
                int* trigger = Script::ScriptGlobal(AcidLabProductionDetail::ProductionTriggerGlobal).As<int>(pages);
                if (timer && trigger)
                {
                    *timer = 0;
                    *trigger = 1;
                    controllerKicked = *timer == 0 && *trigger == 1;
                }
            }

            const auto after = Stats::GetInt(AcidLabProductionDetail::StockStat);
            if (after)
                SetObservedStock(*after);

            const bool success = wroteStock
                && after
                && *after >= AcidLabProductionDetail::MaximumStockUnits;

            if (success)
            {
                TUTONES_LOG_INFO(
                    "business.acid",
                    std::string("Instantly completed Acid Lab production at 160 units; controllerKick=")
                        + (controllerKicked ? "true" : "false"));
                Finish(true, "Acid Lab production instantly completed at 160 / 160");
                return;
            }

            TUTONES_LOG_WARN(
                "business.acid",
                std::string("Instant Acid Lab production failed; statWrite=")
                    + (wroteStock ? "true" : "false")
                    + " controllerKick=" + (controllerKicked ? "true" : "false")
                    + " observed=" + (after ? std::to_string(*after) : std::string("unavailable")));
            Finish(false, "Instant Acid Lab production failed stock read-back verification");
        }

        void SetObservedStock(int stockUnits) noexcept
        {
            std::scoped_lock lock(m_Mutex);
            m_Snapshot.stockUnits = stockUnits;
        }

        void Publish(bool success, std::string message) noexcept
        {
            std::scoped_lock lock(m_Mutex);
            m_Snapshot.haveResult = true;
            m_Snapshot.lastSucceeded = success;
            m_Snapshot.message = std::move(message);
        }

        void Finish(bool success, std::string message) noexcept
        {
            Publish(success, std::move(message));
            m_Pending.store(false, std::memory_order_release);
        }

        std::atomic<bool> m_Pending{false};
        mutable std::mutex m_Mutex;
        AcidLabProductionSnapshot m_Snapshot{};
    };
}
