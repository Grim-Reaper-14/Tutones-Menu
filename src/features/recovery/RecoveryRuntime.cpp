#include "RecoveryRuntime.hpp"

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/Stats.hpp"
#include "../../game/native/NativeInvoker.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <utility>

namespace Tutones::Game::Recovery
{
    namespace
    {
        constexpr std::size_t TunablesGlobal = 262145;
        constexpr std::size_t RpMultiplierOffset = 1;
        constexpr auto RefreshInterval = std::chrono::seconds(1);

        [[nodiscard]] int WarehouseCapacity(int propertyId) noexcept
        {
            switch (propertyId)
            {
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 9:
                return 16;

            case 7:
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
            case 21:
                return 42;

            case 6:
            case 8:
            case 16:
            case 17:
            case 18:
            case 19:
            case 20:
            case 22:
                return 111;

            default:
                return 0;
            }
        }

        [[nodiscard]] std::string WarehousePropertyStat(int slot)
        {
            return "MPX_PROP_WHOUSE_SLOT" + std::to_string(slot);
        }

        [[nodiscard]] std::string WarehouseCrateStat(int slot)
        {
            return "MPX_CONTOTALFORWHOUSE" + std::to_string(slot);
        }
    }

    RecoveryRuntime& RecoveryRuntime::Get() noexcept
    {
        static RecoveryRuntime instance;
        return instance;
    }

    bool RecoveryRuntime::Start()
    {
        bool expected = false;
        if (!m_Running.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            return true;

        m_RpEnabled.store(false, std::memory_order_release);
        m_RpMultiplier.store(1.0f, std::memory_order_release);
        m_HaveOriginalRpMultiplier = false;
        m_OriginalRpMultiplier = 1.0f;
        m_NextRefresh = {};

        {
            std::scoped_lock lock(m_Mutex);
            m_QueuedAction = RecoveryAction::None;
            m_QueuedTarget = -1;
            m_QueuedValue = 0;
            m_ActionBusy = false;
            m_Snapshot = {};
            m_Snapshot.running = true;
            m_Snapshot.requestedRpMultiplier = 1.0f;
        }

        if (QueueNextTick())
        {
            TUTONES_LOG_INFO("recovery", "Recovery runtime scheduled on the GTA script thread");
            return true;
        }

        m_Running.store(false, std::memory_order_release);
        {
            std::scoped_lock lock(m_Mutex);
            m_Snapshot.running = false;
        }
        TUTONES_LOG_ERROR("recovery", "Recovery runtime failed to queue its first GTA script-thread tick");
        return false;
    }

    void RecoveryRuntime::Stop() noexcept
    {
        if (!m_Running.exchange(false, std::memory_order_acq_rel))
            return;

        m_RpEnabled.store(false, std::memory_order_release);

        const auto restore = [this] {
            auto* pages = GamePointers::Get().ScriptGlobals();
            if (pages && m_HaveOriginalRpMultiplier)
            {
                if (float* multiplier = Script::ScriptGlobal(TunablesGlobal).At(RpMultiplierOffset).As<float>(pages))
                    *multiplier = m_OriginalRpMultiplier;
            }
            m_HaveOriginalRpMultiplier = false;
            m_OriginalRpMultiplier = 1.0f;
        };

        if (Runtime::GameRuntime::Get().IsOnGameThread())
        {
            restore();
        }
        else
        {
            const auto restored = std::make_shared<std::atomic<bool>>(false);
            if (Runtime::GameRuntime::Get().Enqueue([restore, restored] {
                    restore();
                    restored->store(true, std::memory_order_release);
                }))
            {
                const auto deadline = Clock::now() + std::chrono::milliseconds(250);
                while (!restored->load(std::memory_order_acquire) && Clock::now() < deadline)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        std::scoped_lock lock(m_Mutex);
        m_QueuedAction = RecoveryAction::None;
        m_QueuedTarget = -1;
        m_QueuedValue = 0;
        m_ActionBusy = false;
        m_Snapshot.actionPending = false;
        m_Snapshot.rpMultiplierEnabled = false;
        m_Snapshot.running = false;
        TUTONES_LOG_INFO("recovery", "Recovery runtime stopped");
    }

    bool RecoveryRuntime::IsRunning() const noexcept
    {
        return m_Running.load(std::memory_order_acquire);
    }

    RecoverySnapshot RecoveryRuntime::Snapshot() const noexcept
    {
        std::scoped_lock lock(m_Mutex);
        return m_Snapshot;
    }

    void RecoveryRuntime::SetRpMultiplier(float multiplier) noexcept
    {
        m_RpMultiplier.store(std::clamp(multiplier, 0.0f, 1000.0f), std::memory_order_release);
    }

    void RecoveryRuntime::SetRpMultiplierEnabled(bool enabled) noexcept
    {
        m_RpEnabled.store(enabled, std::memory_order_release);
    }

    bool RecoveryRuntime::QueueSetWarehouseCrates(int slot, int crates)
    {
        if (slot < 0 || slot >= 5 || crates < 0 || crates > 111)
            return false;
        return QueueAction(RecoveryAction::SetWarehouseCrates, slot, crates);
    }

    bool RecoveryRuntime::QueueSetBunkerSupplies(int supplies)
    {
        if (supplies < 0 || supplies > 100)
            return false;
        return QueueAction(RecoveryAction::SetBunkerSupplies, 5, supplies);
    }

    bool RecoveryRuntime::QueueSetBunkerProduct(int product)
    {
        if (product < 0 || product > 100)
            return false;
        return QueueAction(RecoveryAction::SetBunkerProduct, 5, product);
    }

    bool RecoveryRuntime::QueueEarnFromPickup(int amount)
    {
        if (amount <= 0)
            return false;
        return QueueAction(RecoveryAction::EarnFromPickup, -1, amount);
    }

    bool RecoveryRuntime::QueueAction(RecoveryAction action, int target, int value)
    {
        if (!IsRunning() || action == RecoveryAction::None)
            return false;

        std::scoped_lock lock(m_Mutex);
        if (m_ActionBusy)
            return false;

        m_ActionBusy = true;
        m_QueuedAction = action;
        m_QueuedTarget = target;
        m_QueuedValue = value;
        m_Snapshot.actionPending = true;
        return true;
    }

    bool RecoveryRuntime::QueueNextTick()
    {
        if (!IsRunning())
            return false;
        return Runtime::GameRuntime::Get().Enqueue([this] { TickOnGameThread(); });
    }

    void RecoveryRuntime::TickOnGameThread() noexcept
    {
        if (!IsRunning())
            return;

        ApplyRpMultiplierOnGameThread();
        ProcessActionOnGameThread();

        const auto now = Clock::now();
        if (m_NextRefresh.time_since_epoch().count() == 0 || now >= m_NextRefresh)
        {
            RefreshOnGameThread();
            m_NextRefresh = now + RefreshInterval;
        }

        if (!QueueNextTick())
        {
            m_Running.store(false, std::memory_order_release);
            std::scoped_lock lock(m_Mutex);
            m_Snapshot.running = false;
        }
    }

    void RecoveryRuntime::ApplyRpMultiplierOnGameThread() noexcept
    {
        auto* pages = GamePointers::Get().ScriptGlobals();
        bool* sessionStarted = GamePointers::Get().IsSessionStarted();
        if (!pages || !sessionStarted || !*sessionStarted)
        {
            m_HaveOriginalRpMultiplier = false;
            return;
        }

        float* multiplier = Script::ScriptGlobal(TunablesGlobal).At(RpMultiplierOffset).As<float>(pages);
        if (!multiplier)
            return;

        if (m_RpEnabled.load(std::memory_order_acquire))
        {
            if (!m_HaveOriginalRpMultiplier)
            {
                m_OriginalRpMultiplier = *multiplier;
                m_HaveOriginalRpMultiplier = true;
            }
            *multiplier = std::clamp(m_RpMultiplier.load(std::memory_order_acquire), 0.0f, 1000.0f);
        }
        else if (m_HaveOriginalRpMultiplier)
        {
            *multiplier = m_OriginalRpMultiplier;
            m_HaveOriginalRpMultiplier = false;
            m_OriginalRpMultiplier = 1.0f;
        }
    }

    void RecoveryRuntime::ProcessActionOnGameThread() noexcept
    {
        RecoveryAction action{};
        int target{};
        int value{};
        {
            std::scoped_lock lock(m_Mutex);
            if (!m_ActionBusy || m_QueuedAction == RecoveryAction::None)
                return;
            action = m_QueuedAction;
            target = m_QueuedTarget;
            value = m_QueuedValue;
        }

        bool success = false;
        bool* sessionStarted = GamePointers::Get().IsSessionStarted();
        const bool nativeReady = Native::NativeRegistry::Get().IsReady();
        if (sessionStarted && *sessionStarted && nativeReady)
        {
            if (action == RecoveryAction::EarnFromPickup && value > 0)
            {
                success = Native::NativeInvoker::InvokeVoid(Native::NativeId::NetworkEarnFromPickup, value);
            }
            else
            {
                const auto characterIndex = Stats::GetCharIndex();
                if (characterIndex)
                {
                    if (action == RecoveryAction::SetWarehouseCrates && target >= 0 && target < 5)
                    {
                        const auto property = Stats::GetInt(WarehousePropertyStat(target), *characterIndex);
                        const int capacity = property ? WarehouseCapacity(*property) : 0;
                        if (property && *property > 0 && capacity > 0 && value >= 0 && value <= capacity)
                        {
                            const std::string stat = WarehouseCrateStat(target);
                            if (Stats::SetInt(stat, value, *characterIndex))
                            {
                                const auto confirmation = Stats::GetInt(stat, *characterIndex);
                                success = confirmation && *confirmation == value;
                            }
                        }
                    }
                    else if (action == RecoveryAction::SetBunkerSupplies || action == RecoveryAction::SetBunkerProduct)
                    {
                        const auto property = Stats::GetInt("MPX_PROP_FAC_SLOT5", *characterIndex);
                        const auto setup = Stats::GetInt("MPX_FACTORYSETUP5", *characterIndex);
                        if (property && *property > 0 && setup && *setup == 1 && value >= 0 && value <= 100)
                        {
                            const char* stat = action == RecoveryAction::SetBunkerSupplies
                                ? "MPX_MATTOTALFORFACTORY5"
                                : "MPX_PRODTOTALFORFACTORY5";
                            if (Stats::SetInt(stat, value, *characterIndex))
                            {
                                const auto confirmation = Stats::GetInt(stat, *characterIndex);
                                success = confirmation && *confirmation == value;
                            }
                        }
                    }
                }
            }
        }

        RecordAction(action, target, value, success);
        m_NextRefresh = {};
    }

    void RecoveryRuntime::RefreshOnGameThread() noexcept
    {
        RecoverySnapshot next{};
        next.running = IsRunning();
        next.nativeReady = Native::NativeRegistry::Get().IsReady();
        next.requestedRpMultiplier = m_RpMultiplier.load(std::memory_order_acquire);
        next.rpMultiplierEnabled = m_RpEnabled.load(std::memory_order_acquire);

        bool* sessionStarted = GamePointers::Get().IsSessionStarted();
        next.sessionStarted = sessionStarted && *sessionStarted;

        auto* pages = GamePointers::Get().ScriptGlobals();
        if (pages && next.sessionStarted)
        {
            if (const float* multiplier = Script::ScriptGlobal(TunablesGlobal).At(RpMultiplierOffset).As<float>(pages))
            {
                next.observedRpMultiplier = *multiplier;
                next.rpMultiplierReady = true;
            }
        }

        const auto characterIndex = next.nativeReady && next.sessionStarted ? Stats::GetCharIndex() : std::nullopt;
        next.statsReady = characterIndex.has_value();
        if (characterIndex)
        {
            next.characterIndex = *characterIndex;
            for (int slot = 0; slot < 5; ++slot)
            {
                auto& warehouse = next.warehouses[static_cast<std::size_t>(slot)];
                warehouse.slot = slot;
                const auto property = Stats::GetInt(WarehousePropertyStat(slot), *characterIndex);
                if (!property)
                {
                    next.statsReady = false;
                    continue;
                }

                warehouse.propertyId = *property;
                warehouse.owned = *property > 0;
                if (!warehouse.owned)
                {
                    warehouse.readable = true;
                    continue;
                }

                warehouse.capacity = WarehouseCapacity(*property);
                const auto crates = Stats::GetInt(WarehouseCrateStat(slot), *characterIndex);
                if (!crates || warehouse.capacity == 0)
                {
                    next.statsReady = false;
                    continue;
                }

                warehouse.crates = *crates;
                warehouse.readable = true;
            }

            const auto bunkerProperty = Stats::GetInt("MPX_PROP_FAC_SLOT5", *characterIndex);
            if (!bunkerProperty)
            {
                next.statsReady = false;
            }
            else
            {
                next.bunker.propertyId = *bunkerProperty;
                next.bunker.owned = *bunkerProperty > 0;
                if (!next.bunker.owned)
                {
                    next.bunker.readable = true;
                }
                else
                {
                    const auto setup = Stats::GetInt("MPX_FACTORYSETUP5", *characterIndex);
                    const auto supplies = Stats::GetInt("MPX_MATTOTALFORFACTORY5", *characterIndex);
                    const auto product = Stats::GetInt("MPX_PRODTOTALFORFACTORY5", *characterIndex);
                    if (setup && supplies && product)
                    {
                        next.bunker.setup = *setup == 1;
                        next.bunker.supplies = *supplies;
                        next.bunker.product = *product;
                        next.bunker.readable = true;
                    }
                    else
                    {
                        next.statsReady = false;
                    }
                }
            }
        }

        {
            std::scoped_lock lock(m_Mutex);
            next.actionPending = m_ActionBusy;
            next.lastAction = m_Snapshot.lastAction;
            next.lastActionTarget = m_Snapshot.lastActionTarget;
            next.lastActionValue = m_Snapshot.lastActionValue;
            next.lastActionSucceeded = m_Snapshot.lastActionSucceeded;
            next.revision = m_Snapshot.revision + 1;
            m_Snapshot = std::move(next);
        }
    }

    void RecoveryRuntime::RecordAction(RecoveryAction action, int target, int value, bool success) noexcept
    {
        std::scoped_lock lock(m_Mutex);
        m_Snapshot.lastAction = action;
        m_Snapshot.lastActionTarget = target;
        m_Snapshot.lastActionValue = value;
        m_Snapshot.lastActionSucceeded = success;
        m_Snapshot.actionPending = false;
        m_ActionBusy = false;
        m_QueuedAction = RecoveryAction::None;
        m_QueuedTarget = -1;
        m_QueuedValue = 0;
    }
}
