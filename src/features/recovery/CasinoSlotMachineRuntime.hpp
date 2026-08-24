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
#include <utility>

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
        bool tableReadable{};
        int spinState{-1};
        std::size_t lastWriteCount{};
        std::size_t tableEntryCount{};
        std::size_t forcedWinCount{};
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
                ? "Continuous slot rig enabled; win table will be rewritten while casino_slots is active"
                : "Continuous slot rig disabled; result-table reset queued";
        }

        [[nodiscard]] bool Enabled() const noexcept
        {
            return m_Enabled.load(std::memory_order_acquire);
        }

        bool QueueReadResults()
        {
            return QueueAction("Reading casino_slots result table", [this] {
                Types::ScriptThread* thread{};
                int spinState{-1};
                bool safeSpinState{};
                std::string error;
                if (!ResolveContext(thread, spinState, safeSpinState, error))
                {
                    Finish(false, false, false, spinState, 0, 0, 0, std::move(error));
                    return;
                }

                std::size_t total{};
                std::size_t forcedWins{};
                if (!ReadResultTable(thread, total, forcedWins))
                {
                    Finish(false, true, false, spinState, 0, 0, 0, "Could not read the full casino_slots result table");
                    return;
                }

                Finish(
                    true,
                    true,
                    safeSpinState,
                    spinState,
                    0,
                    total,
                    forcedWins,
                    std::string("Read casino_slots table: ") + std::to_string(forcedWins)
                        + "/" + std::to_string(total) + " entries currently equal win result 6");
            });
        }

        bool QueueWriteWinResults()
        {
            return QueueAction("Writing forced-win casino_slots result table", [this] {
                Types::ScriptThread* thread{};
                int spinState{-1};
                bool safeSpinState{};
                std::string error;
                if (!ResolveContext(thread, spinState, safeSpinState, error))
                {
                    Finish(false, false, false, spinState, 0, 0, 0, std::move(error));
                    return;
                }

                std::size_t writes{};
                if (!WriteWinTable(thread, writes))
                {
                    Finish(false, true, safeSpinState, spinState, writes, 0, 0, "Forced-win table write failed");
                    return;
                }

                std::size_t total{};
                std::size_t forcedWins{};
                const bool readable = ReadResultTable(thread, total, forcedWins);
                const bool success = readable && total != 0 && forcedWins == total;

                if (success)
                {
                    TUTONES_LOG_INFO(
                        "recovery.casino",
                        std::string("Forced casino_slots win table: writes=") + std::to_string(writes)
                            + " verified=" + std::to_string(forcedWins)
                            + "/" + std::to_string(total)
                            + " spinState=" + std::to_string(spinState));
                }

                Finish(
                    success,
                    true,
                    safeSpinState,
                    spinState,
                    writes,
                    total,
                    forcedWins,
                    success
                        ? "Forced-win table written and read-back verified"
                        : "Forced-win table did not survive read-back verification");
            });
        }

        bool QueueResetResults()
        {
            return QueueAction("Resetting casino_slots result table", [this] {
                Types::ScriptThread* thread{};
                int spinState{-1};
                bool safeSpinState{};
                std::string error;
                if (!ResolveContext(thread, spinState, safeSpinState, error))
                {
                    Finish(false, false, false, spinState, 0, 0, 0, std::move(error));
                    return;
                }

                std::size_t writes{};
                if (!ResetResultTable(thread, writes))
                {
                    Finish(false, true, safeSpinState, spinState, writes, 0, 0, "Result-table reset failed");
                    return;
                }

                m_Enabled.store(false, std::memory_order_release);
                m_RestoreRequested.store(false, std::memory_order_release);

                std::size_t total{};
                std::size_t forcedWins{};
                const bool readable = ReadResultTable(thread, total, forcedWins);

                TUTONES_LOG_INFO(
                    "recovery.casino",
                    std::string("Reset casino_slots result table: writes=") + std::to_string(writes)
                        + " remainingResult6=" + std::to_string(forcedWins)
                        + "/" + std::to_string(total));

                Finish(
                    readable,
                    true,
                    safeSpinState,
                    spinState,
                    writes,
                    total,
                    forcedWins,
                    readable ? "Slot result table reset to normal 3-9 values" : "Slot table reset completed but read-back failed");
            });
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
            Finish(false, false, false, -1, 0, 0, 0, "Game-thread queue unavailable");
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
            snapshot.tableReadable = m_TableReadable;
            snapshot.spinState = m_SpinState;
            snapshot.lastWriteCount = m_LastWriteCount;
            snapshot.tableEntryCount = m_TableEntryCount;
            snapshot.forcedWinCount = m_ForcedWinCount;
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

        [[nodiscard]] bool ResolveContext(
            Types::ScriptThread*& thread,
            int& spinState,
            bool& safeSpinState,
            std::string& error)
        {
            bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            if (!sessionStarted || !*sessionStarted)
            {
                error = "Join GTA Online before using Rig Slot Machines";
                return false;
            }

            auto& scripts = Script::ScriptRuntime::Get();
            if (!scripts.IsReady())
            {
                error = "Shared Enhanced script runtime is unavailable";
                return false;
            }

            thread = scripts.FindThread(SlotsScriptHash);
            if (!thread || !thread->stack)
            {
                error = "casino_slots is not active; sit at/use a casino slot machine first";
                return false;
            }

            const std::size_t lastResultLocal = RandomResultsTable + static_cast<std::size_t>(RandomResultsLast);
            if (!LocalAvailable(thread, SpinStateLocal) || !LocalAvailable(thread, lastResultLocal))
            {
                error = "casino_slots locals are outside the active script stack";
                return false;
            }

            int* spinStateValue = Script::ScriptLocal(thread, SpinStateLocal).As<int>();
            if (!spinStateValue)
            {
                error = "casino_slots spin-state local is unavailable";
                return false;
            }

            spinState = *spinStateValue;
            safeSpinState = CasinoSlotMachineDetail::IsSafeSpinState(spinState);
            return true;
        }

        [[nodiscard]] static bool ReadResultTable(
            Types::ScriptThread* thread,
            std::size_t& total,
            std::size_t& forcedWins) noexcept
        {
            total = 0;
            forcedWins = 0;
            if (!thread || !thread->stack)
                return false;

            for (int resultIndex = RandomResultsFirst; resultIndex <= RandomResultsLast; ++resultIndex)
            {
                if (CasinoSlotMachineDetail::IsBlacklistedResultIndex(resultIndex))
                    continue;

                int* result = Script::ScriptLocal(
                    thread,
                    RandomResultsTable + static_cast<std::size_t>(resultIndex)).As<int>();
                if (!result)
                    return false;

                ++total;
                if (*result == ForcedWinResult)
                    ++forcedWins;
            }

            return total != 0;
        }

        [[nodiscard]] static bool WriteWinTable(Types::ScriptThread* thread, std::size_t& writes) noexcept
        {
            writes = 0;
            if (!thread || !thread->stack)
                return false;

            for (int resultIndex = RandomResultsFirst; resultIndex <= RandomResultsLast; ++resultIndex)
            {
                if (CasinoSlotMachineDetail::IsBlacklistedResultIndex(resultIndex))
                    continue;

                int* result = Script::ScriptLocal(
                    thread,
                    RandomResultsTable + static_cast<std::size_t>(resultIndex)).As<int>();
                if (!result)
                    return false;

                if (*result != ForcedWinResult)
                {
                    *result = ForcedWinResult;
                    ++writes;
                }
            }

            return true;
        }

        [[nodiscard]] static bool ResetResultTable(Types::ScriptThread* thread, std::size_t& writes) noexcept
        {
            writes = 0;
            if (!thread || !thread->stack)
                return false;

            std::uint32_t seed = static_cast<std::uint32_t>(
                std::chrono::steady_clock::now().time_since_epoch().count());
            const auto nextResetResult = [&seed]() noexcept
            {
                seed = seed * 1664525u + 1013904223u;
                return 3 + static_cast<int>(seed % 7u);
            };

            for (int resultIndex = RandomResultsFirst; resultIndex <= RandomResultsLast; ++resultIndex)
            {
                if (CasinoSlotMachineDetail::IsBlacklistedResultIndex(resultIndex))
                    continue;

                int* result = Script::ScriptLocal(
                    thread,
                    RandomResultsTable + static_cast<std::size_t>(resultIndex)).As<int>();
                if (!result)
                    return false;

                *result = nextResetResult();
                ++writes;
            }

            return true;
        }

        void ProcessTick()
        {
            Types::ScriptThread* thread{};
            int spinState{-1};
            bool safeSpinState{};
            std::string error;
            if (!ResolveContext(thread, spinState, safeSpinState, error))
            {
                if (!m_Enabled.load(std::memory_order_acquire))
                    m_RestoreRequested.store(false, std::memory_order_release);
                Finish(false, false, false, spinState, 0, 0, 0, std::move(error));
                return;
            }

            if (m_Enabled.load(std::memory_order_acquire))
            {
                // Continuous mode intentionally rewrites the table whenever the game
                // changes it. The former 8/14-only gate made the option look dead
                // outside a very small state window.
                std::size_t writes{};
                if (!WriteWinTable(thread, writes))
                {
                    Finish(false, true, safeSpinState, spinState, writes, 0, 0, "Continuous slot rig write failed");
                    return;
                }

                std::size_t total{};
                std::size_t forcedWins{};
                const bool readable = ReadResultTable(thread, total, forcedWins);
                const bool success = readable && total != 0 && forcedWins == total;
                Finish(
                    success,
                    true,
                    safeSpinState,
                    spinState,
                    writes,
                    total,
                    forcedWins,
                    success ? "Continuous Rig Slot Machines active" : "Continuous slot rig failed read-back verification");
                return;
            }

            if (!m_RestoreRequested.load(std::memory_order_acquire))
                return;

            std::size_t writes{};
            if (!ResetResultTable(thread, writes))
            {
                Finish(false, true, safeSpinState, spinState, writes, 0, 0, "Automatic slot result reset failed");
                return;
            }

            m_RestoreRequested.store(false, std::memory_order_release);

            std::size_t total{};
            std::size_t forcedWins{};
            const bool readable = ReadResultTable(thread, total, forcedWins);
            Finish(
                readable,
                true,
                safeSpinState,
                spinState,
                writes,
                total,
                forcedWins,
                readable ? "Continuous rig disabled and result table reset" : "Continuous rig reset completed but read-back failed");
        }

        template<typename Callback>
        bool QueueAction(std::string pendingMessage, Callback&& callback)
        {
            bool expected = false;
            if (!m_TaskQueued.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            {
                std::scoped_lock lock(m_Mutex);
                m_HaveResult = false;
                m_LastSucceeded = false;
                m_Message = std::move(pendingMessage);
            }

            if (Runtime::GameRuntime::Get().Enqueue([this, callback = std::forward<Callback>(callback)]() mutable {
                    callback();
                    m_TaskQueued.store(false, std::memory_order_release);
                }))
            {
                return true;
            }

            m_TaskQueued.store(false, std::memory_order_release);
            Finish(false, false, false, -1, 0, 0, 0, "Game-thread queue unavailable");
            return false;
        }

        void Finish(
            bool success,
            bool scriptActive,
            bool safeSpinState,
            int spinState,
            std::size_t writeCount,
            std::size_t tableEntryCount,
            std::size_t forcedWinCount,
            std::string message)
        {
            std::scoped_lock lock(m_Mutex);
            m_ScriptActive = scriptActive;
            m_SafeSpinState = safeSpinState;
            m_HaveResult = true;
            m_LastSucceeded = success;
            m_TableReadable = tableEntryCount != 0;
            m_SpinState = spinState;
            m_LastWriteCount = writeCount;
            m_TableEntryCount = tableEntryCount;
            m_ForcedWinCount = forcedWinCount;
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
        bool m_TableReadable{};
        int m_SpinState{-1};
        std::size_t m_LastWriteCount{};
        std::size_t m_TableEntryCount{};
        std::size_t m_ForcedWinCount{};
        std::string m_Message{"Ready"};
    };
}
