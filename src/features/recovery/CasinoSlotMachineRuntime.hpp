#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/script/ScriptLocal.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

namespace Tutones::Game::Recovery
{
    namespace CasinoSlotMachineDetail
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

        [[nodiscard]] constexpr bool IsBlacklistedResultIndex(int index) noexcept
        {
            return index == 9 || index == 21 || index == 22 || index == 87 || index == 152;
        }

        [[nodiscard]] constexpr bool IsSafeSpinState(int state) noexcept
        {
            return state == 8 || state == 14;
        }
    }

    struct CasinoSlotMachineSnapshot final
    {
        bool enabled{};
        bool taskQueued{};
        bool restorePending{};
        bool scriptActive{};
        bool safeSpinState{};
        bool haveResult{};
        bool lastSucceeded{};
        int spinState{-1};
        std::size_t lastWriteCount{};
        std::string message{"Ready"};
    };

    class CasinoSlotMachineRuntime final
    {
    public:
        static constexpr std::uint32_t SlotsScriptHash = CasinoSlotMachineDetail::Joaat("casino_slots");
        static constexpr std::size_t RandomResultsTable = 1357;
        static constexpr int RandomResultsFirst = 3;
        static constexpr int RandomResultsLast = 196;
        static constexpr std::size_t SpinStateLocal = 1675;
        static constexpr int ForcedWinResult = 6;

        static CasinoSlotMachineRuntime& Get() noexcept
        {
            static CasinoSlotMachineRuntime instance;
            return instance;
        }

        void SetEnabled(bool enabled)
        {
            const bool previous = m_Enabled.exchange(enabled, std::memory_order_acq_rel);
            if (previous == enabled)
                return;

            m_RestoreRequested.store(!enabled, std::memory_order_release);

            std::scoped_lock lock(m_Mutex);
            m_HaveResult = false;
            m_LastSucceeded = false;
            m_LastWriteCount = 0;
            m_Message = enabled
                ? "Slot rig armed; waiting for casino_slots and a safe spin state"
                : "Slot rig disabled; safe result-table reset requested";
        }

        [[nodiscard]] bool Enabled() const noexcept
        {
            return m_Enabled.load(std::memory_order_acquire);
        }

        void Tick()
        {
            if (!m_Enabled.load(std::memory_order_acquire)
                && !m_RestoreRequested.load(std::memory_order_acquire))
            {
                return;
            }

            bool expected = false;
            if (!m_TaskQueued.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return;

            if (Runtime::GameRuntime::Get().Enqueue([this] {
                    ProcessTick();
                    m_TaskQueued.store(false, std::memory_order_release);
                }))
            {
                return;
            }

            m_TaskQueued.store(false, std::memory_order_release);
            UpdateState(false, false, false, -1, 0, false, "Game-thread queue unavailable");
        }

        [[nodiscard]] CasinoSlotMachineSnapshot Snapshot() const
        {
            CasinoSlotMachineSnapshot snapshot;
            snapshot.enabled = m_Enabled.load(std::memory_order_acquire);
            snapshot.taskQueued = m_TaskQueued.load(std::memory_order_acquire);
            snapshot.restorePending = m_RestoreRequested.load(std::memory_order_acquire);

            std::scoped_lock lock(m_Mutex);
            snapshot.scriptActive = m_ScriptActive;
            snapshot.safeSpinState = m_SafeSpinState;
            snapshot.haveResult = m_HaveResult;
            snapshot.lastSucceeded = m_LastSucceeded;
            snapshot.spinState = m_SpinState;
            snapshot.lastWriteCount = m_LastWriteCount;
            snapshot.message = m_Message;
            return snapshot;
        }

    private:
        CasinoSlotMachineRuntime() = default;

        [[nodiscard]] static bool LocalAvailable(const Types::ScriptThread* thread, std::size_t index) noexcept
        {
            if (!thread || !thread->stack)
                return false;

            const std::size_t slotCount = static_cast<std::size_t>(thread->context.stackSize) / sizeof(std::uint64_t);
            return index < slotCount;
        }

        void ProcessTick()
        {
            bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            if (!sessionStarted || !*sessionStarted)
            {
                m_RestoreRequested.store(false, std::memory_order_release);
                UpdateState(false, false, false, -1, 0, false, "Join GTA Online before using Rig Slot Machines");
                return;
            }

            auto& scripts = Script::ScriptRuntime::Get();
            if (!scripts.IsReady())
            {
                UpdateState(false, false, false, -1, 0, false, "Shared Enhanced script runtime is unavailable");
                return;
            }

            auto* thread = scripts.FindThread(SlotsScriptHash);
            if (!thread || !thread->stack)
            {
                // A destroyed casino_slots stack cannot retain our local writes, so
                // there is nothing to restore when the script is no longer running.
                if (!m_Enabled.load(std::memory_order_acquire))
                    m_RestoreRequested.store(false, std::memory_order_release);

                UpdateState(false, false, false, -1, 0, false, "casino_slots is not active; enter the casino and use a slot machine");
                return;
            }

            const std::size_t lastResultLocal = RandomResultsTable + static_cast<std::size_t>(RandomResultsLast);
            if (!LocalAvailable(thread, SpinStateLocal) || !LocalAvailable(thread, lastResultLocal))
            {
                UpdateState(true, false, false, -1, 0, false, "casino_slots locals are outside the active script stack");
                return;
            }

            int* spinStateValue = Script::ScriptLocal(thread, SpinStateLocal).As<int>();
            if (!spinStateValue)
            {
                UpdateState(true, false, false, -1, 0, false, "casino_slots spin-state local is unavailable");
                return;
            }

            const int spinState = *spinStateValue;
            const bool safeSpinState = CasinoSlotMachineDetail::IsSafeSpinState(spinState);

            if (m_Enabled.load(std::memory_order_acquire))
            {
                if (!safeSpinState)
                {
                    UpdateState(true, false, true, spinState, 0, false, "Slot rig armed; waiting for safe spin state 8 or 14");
                    return;
                }

                std::size_t writes{};
                bool verified = true;
                for (int resultIndex = RandomResultsFirst; resultIndex <= RandomResultsLast; ++resultIndex)
                {
                    if (CasinoSlotMachineDetail::IsBlacklistedResultIndex(resultIndex))
                        continue;

                    int* result = Script::ScriptLocal(thread, RandomResultsTable + static_cast<std::size_t>(resultIndex)).As<int>();
                    if (!result)
                    {
                        verified = false;
                        break;
                    }

                    if (*result != ForcedWinResult)
                    {
                        *result = ForcedWinResult;
                        ++writes;
                    }

                    if (*result != ForcedWinResult)
                    {
                        verified = false;
                        break;
                    }
                }

                if (verified)
                {
                    if (writes != 0)
                        TUTONES_LOG_DEBUG("recovery.casino", std::string("Rig Slot Machines forced ") + std::to_string(writes) + " casino_slots results to 6");
                    UpdateState(true, true, true, spinState, writes, true, writes != 0 ? "Rig Slot Machines applied" : "Rig Slot Machines active");
                }
                else
                {
                    UpdateState(true, true, true, spinState, writes, false, "Rig Slot Machines failed result read-back verification");
                }
                return;
            }

            if (!m_RestoreRequested.load(std::memory_order_acquire))
            {
                UpdateState(true, safeSpinState, false, spinState, 0, true, "Rig Slot Machines disabled");
                return;
            }

            if (!safeSpinState)
            {
                UpdateState(true, false, true, spinState, 0, false, "Rig disabled; waiting for safe spin state before resetting results");
                return;
            }

            std::uint32_t seed = static_cast<std::uint32_t>(
                std::chrono::steady_clock::now().time_since_epoch().count());
            const auto nextResetResult = [&seed]() noexcept
            {
                seed = seed * 1664525u + 1013904223u;
                return 3 + static_cast<int>(seed % 7u);
            };

            std::size_t writes{};
            for (int resultIndex = RandomResultsFirst; resultIndex <= RandomResultsLast; ++resultIndex)
            {
                if (CasinoSlotMachineDetail::IsBlacklistedResultIndex(resultIndex))
                    continue;

                int* result = Script::ScriptLocal(thread, RandomResultsTable + static_cast<std::size_t>(resultIndex)).As<int>();
                if (!result)
                {
                    UpdateState(true, true, true, spinState, writes, false, "Slot result reset stopped because a local became unavailable");
                    return;
                }

                *result = nextResetResult();
                ++writes;
            }

            m_RestoreRequested.store(false, std::memory_order_release);
            TUTONES_LOG_DEBUG("recovery.casino", std::string("Rig Slot Machines reset ") + std::to_string(writes) + " casino_slots result locals");
            UpdateState(true, true, true, spinState, writes, true, "Rig Slot Machines disabled and result table reset");
        }

        void UpdateState(
            bool scriptActive,
            bool safeSpinState,
            bool haveResult,
            int spinState,
            std::size_t writeCount,
            bool success,
            std::string message)
        {
            std::scoped_lock lock(m_Mutex);
            m_ScriptActive = scriptActive;
            m_SafeSpinState = safeSpinState;
            m_HaveResult = haveResult;
            m_LastSucceeded = success;
            m_SpinState = spinState;
            m_LastWriteCount = writeCount;
            m_Message = std::move(message);
        }

        std::atomic<bool> m_Enabled{false};
        std::atomic<bool> m_TaskQueued{false};
        std::atomic<bool> m_RestoreRequested{false};

        mutable std::mutex m_Mutex;
        bool m_ScriptActive{};
        bool m_SafeSpinState{};
        bool m_HaveResult{};
        bool m_LastSucceeded{};
        int m_SpinState{-1};
        std::size_t m_LastWriteCount{};
        std::string m_Message{"Ready"};
    };
}
