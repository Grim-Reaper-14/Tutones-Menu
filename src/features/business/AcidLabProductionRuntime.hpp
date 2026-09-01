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
        inline constexpr std::size_t InstantResupplyGlobal = 1673820;
        inline constexpr std::size_t AcidLabFactorySlot = 7;
        inline constexpr std::size_t ProductionTimerRootGlobal = 2708925;
        inline constexpr std::size_t ProductionTimerGlobal =
            ProductionTimerRootGlobal + 1 + ((AcidLabFactorySlot - 1) * 2);
        inline constexpr std::size_t ProductionTriggerGlobal = ProductionTimerGlobal + 1;
        inline constexpr int MaximumStockUnits = 160;
        inline constexpr int MaximumProductionTicks = 1024;
        inline constexpr int MaximumStalledTicks = 300;
        inline constexpr const char* StockStat = "MPX_PRODTOTALFORFACTORY6";

        static_assert(ProductionTimerGlobal == 2708938);
        static_assert(ProductionTriggerGlobal == 2708939);
    }

    struct AcidLabProductionSnapshot final
    {
        bool actionPending{};
        bool haveResult{};
        bool lastSucceeded{};
        int stockUnits{-1};
        int ticksApplied{};
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
                m_LastObservedStock = -1;
                m_StalledTicks = 0;
                m_TicksApplied = 0;
                m_Snapshot.haveResult = false;
                m_Snapshot.lastSucceeded = false;
                m_Snapshot.stockUnits = -1;
                m_Snapshot.ticksApplied = 0;
                m_Snapshot.message = "Instant Acid Lab production fill queued";
            }

            TUTONES_LOG_INFO(
                "business.acid",
                "Queued controller-driven Acid Lab production fill using Enhanced production globals 2708938/2708939");

            if (QueueNextTick())
                return true;

            m_Pending.store(false, std::memory_order_release);
            Publish(false, "Could not queue Acid Lab production fill on the GTA script thread");
            return false;
        }

    private:
        AcidLabProductionRuntime() = default;
        AcidLabProductionRuntime(const AcidLabProductionRuntime&) = delete;
        AcidLabProductionRuntime& operator=(const AcidLabProductionRuntime&) = delete;

        [[nodiscard]] bool QueueNextTick() noexcept
        {
            return Runtime::GameRuntime::Get().Enqueue([this] { ProductionTickOnGameThread(); });
        }

        void ProductionTickOnGameThread() noexcept
        {
            if (!m_Pending.load(std::memory_order_acquire))
                return;

            auto& pointers = GamePointers::Get();
            bool* sessionStarted = pointers.IsSessionStarted();
            if (!sessionStarted || !*sessionStarted)
            {
                Finish(false, "Join GTA Online before using Instant Acid Lab production fill");
                return;
            }

            auto* pages = pointers.ScriptGlobals();
            auto& scripts = Script::ScriptRuntime::Get();
            if (!pages || !scripts.IsReady() || !scripts.FindThread(AcidLabProductionDetail::FreemodeHash))
            {
                Finish(false, "Freemode and Enhanced script globals must be ready for Acid Lab production fill");
                return;
            }

            const auto stock = Stats::GetInt(AcidLabProductionDetail::StockStat);
            if (!stock)
            {
                Finish(false, "Could not read the active character Acid Lab stock stat");
                return;
            }

            if (*stock >= AcidLabProductionDetail::MaximumStockUnits)
            {
                SetObservedStock(*stock);
                TUTONES_LOG_INFO("business.acid", "Acid Lab production fill completed at 160 / 160");
                Finish(true, "Acid Lab production is full at 160 / 160");
                return;
            }

            int ticksApplied{};
            int stalledTicks{};
            RecordProgress(*stock, ticksApplied, stalledTicks);

            if (ticksApplied >= AcidLabProductionDetail::MaximumProductionTicks)
            {
                TUTONES_LOG_WARN(
                    "business.acid",
                    std::string("Acid Lab production fill hit the safety tick limit; observed=")
                        + std::to_string(*stock));
                Finish(false, "Acid Lab production fill stopped at the safety tick limit");
                return;
            }

            if (stalledTicks >= AcidLabProductionDetail::MaximumStalledTicks)
            {
                TUTONES_LOG_WARN(
                    "business.acid",
                    std::string("Acid Lab production controller stopped advancing stock; observed=")
                        + std::to_string(*stock));
                Finish(false, "Acid Lab production controller did not advance stock; make sure the Acid Lab is set up and active");
                return;
            }

            int* resupply = Script::ScriptGlobal(AcidLabProductionDetail::InstantResupplyGlobal)
                                .At(AcidLabProductionDetail::AcidLabFactorySlot)
                                .As<int>(pages);
            int* timer = Script::ScriptGlobal(AcidLabProductionDetail::ProductionTimerGlobal).As<int>(pages);
            int* trigger = Script::ScriptGlobal(AcidLabProductionDetail::ProductionTriggerGlobal).As<int>(pages);
            if (!resupply || !timer || !trigger)
            {
                Finish(false, "Enhanced Acid Lab resupply or production globals are unavailable");
                return;
            }

            // Keep raw materials available while asking the legitimate freemode production
            // controller to process another Acid Lab production tick. The controller owns the
            // stock stat; we only keep pulsing it until the stat reports the 160-unit cap.
            *resupply = 1;
            *timer = 0;
            *trigger = 1;

            const bool pulseApplied = *resupply == 1 && *timer == 0 && *trigger == 1;
            if (!pulseApplied)
            {
                Finish(false, "Acid Lab production controller pulse failed read-back verification");
                return;
            }

            if (!QueueNextTick())
            {
                Finish(false, "Acid Lab production fill lost its GTA script-thread scheduling slot");
                return;
            }
        }

        void RecordProgress(int stockUnits, int& ticksApplied, int& stalledTicks) noexcept
        {
            std::scoped_lock lock(m_Mutex);

            if (stockUnits > m_LastObservedStock)
            {
                m_LastObservedStock = stockUnits;
                m_StalledTicks = 0;
            }
            else
            {
                ++m_StalledTicks;
            }

            ++m_TicksApplied;
            m_Snapshot.stockUnits = stockUnits;
            m_Snapshot.ticksApplied = m_TicksApplied;
            m_Snapshot.haveResult = false;
            m_Snapshot.lastSucceeded = false;
            m_Snapshot.message =
                "Filling Acid Lab production: " + std::to_string(stockUnits) + " / 160";

            ticksApplied = m_TicksApplied;
            stalledTicks = m_StalledTicks;
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
        int m_LastObservedStock{-1};
        int m_StalledTicks{};
        int m_TicksApplied{};
        AcidLabProductionSnapshot m_Snapshot{};
    };
}
