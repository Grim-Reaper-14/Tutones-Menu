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
        bool fastProductionEnabled{};
        bool tickQueued{};
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
            snapshot.fastProductionEnabled = m_Enabled.load(std::memory_order_acquire);
            snapshot.tickQueued = m_TickQueued.load(std::memory_order_acquire);
            return snapshot;
        }

        [[nodiscard]] bool SetFastProduction(bool enabled) noexcept
        {
            const bool wasEnabled = m_Enabled.exchange(enabled, std::memory_order_acq_rel);
            if (wasEnabled == enabled)
                return true;

            if (!enabled)
            {
                Publish(true, "Fast Acid Lab production disabled");
                TUTONES_LOG_INFO("business.acid", "Fast Acid Lab production disabled");
                return true;
            }

            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot.haveResult = false;
                m_Snapshot.lastSucceeded = false;
                m_Snapshot.message = "Fast Acid Lab production enabled; waiting for the production controller";
            }

            if (QueueNextTick())
            {
                TUTONES_LOG_INFO(
                    "business.acid",
                    "Fast Acid Lab production enabled using Enhanced production globals 2708938/2708939");
                return true;
            }

            m_Enabled.store(false, std::memory_order_release);
            Publish(false, "Could not queue Fast Acid Lab production on the GTA script thread");
            return false;
        }

    private:
        AcidLabProductionRuntime() = default;
        AcidLabProductionRuntime(const AcidLabProductionRuntime&) = delete;
        AcidLabProductionRuntime& operator=(const AcidLabProductionRuntime&) = delete;

        [[nodiscard]] bool QueueNextTick() noexcept
        {
            if (!m_Enabled.load(std::memory_order_acquire))
                return true;

            bool expected = false;
            if (!m_TickQueued.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return true;

            if (Runtime::GameRuntime::Get().Enqueue([this] {
                    m_TickQueued.store(false, std::memory_order_release);
                    TickOnGameThread();
                }))
            {
                return true;
            }

            m_TickQueued.store(false, std::memory_order_release);
            return false;
        }

        void TickOnGameThread() noexcept
        {
            if (!m_Enabled.load(std::memory_order_acquire))
                return;

            auto& pointers = GamePointers::Get();
            bool* sessionStarted = pointers.IsSessionStarted();
            auto* pages = pointers.ScriptGlobals();
            auto& scripts = Script::ScriptRuntime::Get();

            if (!sessionStarted || !*sessionStarted || !pages)
            {
                Publish(false, "Join GTA Online before using Fast Acid Lab production");
                Requeue();
                return;
            }

            if (!scripts.IsReady() || !scripts.FindThread(AcidLabProductionDetail::FreemodeHash))
            {
                Publish(false, "Freemode is not ready for Acid Lab production ticks");
                Requeue();
                return;
            }

            const auto stock = Stats::GetInt(AcidLabProductionDetail::StockStat);
            if (stock)
            {
                {
                    std::scoped_lock lock(m_Mutex);
                    m_Snapshot.stockUnits = *stock;
                }

                if (*stock >= AcidLabProductionDetail::MaximumStockUnits)
                {
                    m_Enabled.store(false, std::memory_order_release);
                    Publish(true, "Acid Lab stock is full; Fast Production stopped automatically");
                    TUTONES_LOG_INFO("business.acid", "Fast Acid Lab production stopped because stock reached 160 units");
                    return;
                }
            }

            int* timer = Script::ScriptGlobal(AcidLabProductionDetail::ProductionTimerGlobal).As<int>(pages);
            int* trigger = Script::ScriptGlobal(AcidLabProductionDetail::ProductionTriggerGlobal).As<int>(pages);
            if (!timer || !trigger)
            {
                Publish(false, "Enhanced Acid Lab production globals are unavailable");
                Requeue();
                return;
            }

            *timer = 0;
            *trigger = 1;
            const bool success = *timer == 0 && *trigger == 1;
            Publish(
                success,
                success
                    ? "Fast Acid Lab production is running"
                    : "Acid Lab production tick failed read-back verification");

            Requeue();
        }

        void Requeue() noexcept
        {
            if (!m_Enabled.load(std::memory_order_acquire))
                return;

            if (!QueueNextTick())
            {
                m_Enabled.store(false, std::memory_order_release);
                Publish(false, "Fast Acid Lab production stopped because the GTA script-thread queue is unavailable");
                TUTONES_LOG_ERROR("business.acid", "Fast Acid Lab production lost its GTA script-thread scheduling slot");
            }
        }

        void Publish(bool success, std::string message) noexcept
        {
            std::scoped_lock lock(m_Mutex);
            m_Snapshot.haveResult = true;
            m_Snapshot.lastSucceeded = success;
            m_Snapshot.message = std::move(message);
        }

        std::atomic<bool> m_Enabled{false};
        std::atomic<bool> m_TickQueued{false};
        mutable std::mutex m_Mutex;
        AcidLabProductionSnapshot m_Snapshot{};
    };
}
