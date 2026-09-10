#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/Stats.hpp"
#include "../../game/native/NativeInvoker.hpp"
#include "../../game/tunables/TunableRegistry.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Tutones::Game::Recovery
{
    struct CasinoLimitsSnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool sessionStarted{};
        bool tunableRegistryReady{};
        bool chipsWonReadable{};
        bool winTimestampReadable{};
        bool maxDailyWinReadable{};
        bool cooldownReadable{};
        bool cloudTimeReadable{};
        bool winLimitReached{};
        bool cooldownActive{};
        int chipsWon{};
        int winTimestamp{};
        int maxDailyWin{};
        int cooldownSeconds{};
        int cloudTime{};
        std::int64_t elapsedSeconds{};
        std::int64_t remainingSeconds{};
        std::int64_t winRemaining{};
        std::string message{"Ready"};
    };

    class CasinoLimitsRuntime final
    {
    public:
        static constexpr std::string_view ChipsWonStat = "MPPLY_CASINO_CHIPS_WON_GD";
        static constexpr std::string_view WinTimestampStat = "MPPLY_CASINO_CHIPS_WONTIM";
        static constexpr std::string_view MaxDailyWinTunable = "VC_CASINO_CHIP_MAX_WIN_DAILY";
        static constexpr std::string_view WinLossCooldownTunable = "VC_CASINO_CHIP_MAX_WIN_LOSS_COOLDOWN";

        static CasinoLimitsRuntime& Get() noexcept
        {
            static CasinoLimitsRuntime instance;
            return instance;
        }

        bool QueueRefresh()
        {
            return Queue("Reading casino daily-limit diagnostics", [this] {
                RefreshOnGameThread();
            });
        }

        bool QueueResetDailyRestriction()
        {
            return Queue("Resetting casino daily restriction", [this] {
                ResetDailyRestrictionOnGameThread();
            });
        }

        [[nodiscard]] CasinoLimitsSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            auto snapshot = m_State;
            snapshot.pending = m_Pending.load(std::memory_order_acquire);
            return snapshot;
        }

    private:
        CasinoLimitsRuntime() = default;

        template<typename Callback>
        bool Queue(std::string pendingMessage, Callback&& callback)
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            {
                std::scoped_lock lock(m_Mutex);
                m_State.haveResult = false;
                m_State.lastSucceeded = false;
                m_State.message = std::move(pendingMessage);
            }

            if (Runtime::GameRuntime::Get().Enqueue(std::forward<Callback>(callback)))
                return true;

            CasinoLimitsSnapshot state;
            Finish(std::move(state), false, "Game-thread queue unavailable");
            return false;
        }

        [[nodiscard]] static std::optional<int> FindTunableInt(
            const std::vector<Tunables::TunableEntrySnapshot>& entries,
            std::string_view name) noexcept
        {
            const std::uint32_t hash = Tunables::Joaat(name);
            for (const auto& entry : entries)
            {
                if (entry.hash != hash || !entry.readable)
                    continue;

                if (entry.currentRawValue < static_cast<std::int64_t>(std::numeric_limits<int>::min())
                    || entry.currentRawValue > static_cast<std::int64_t>(std::numeric_limits<int>::max()))
                {
                    return std::nullopt;
                }

                return static_cast<int>(entry.currentRawValue);
            }
            return std::nullopt;
        }

        [[nodiscard]] bool PopulateState(CasinoLimitsSnapshot& state)
        {
            bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            state.sessionStarted = sessionStarted && *sessionStarted;
            if (!state.sessionStarted)
                return false;

            const auto chipsWon = Stats::GetInt(std::string{ChipsWonStat});
            const auto winTimestamp = Stats::GetInt(std::string{WinTimestampStat});
            const auto cloudTime = Native::NativeInvoker::Invoke<std::int32_t>(
                Native::NativeId::GetCloudTimeAsInt);

            state.chipsWonReadable = chipsWon.has_value();
            state.winTimestampReadable = winTimestamp.has_value();
            state.cloudTimeReadable = cloudTime.has_value();
            if (chipsWon)
                state.chipsWon = *chipsWon;
            if (winTimestamp)
                state.winTimestamp = *winTimestamp;
            if (cloudTime)
                state.cloudTime = *cloudTime;

            auto& tunables = Tunables::TunableRegistry::Get();
            state.tunableRegistryReady = tunables.Initialized();
            const auto entries = tunables.EntriesSnapshot();
            const auto maxDailyWin = FindTunableInt(entries, MaxDailyWinTunable);
            const auto cooldown = FindTunableInt(entries, WinLossCooldownTunable);

            state.maxDailyWinReadable = maxDailyWin.has_value();
            state.cooldownReadable = cooldown.has_value();
            if (maxDailyWin)
                state.maxDailyWin = *maxDailyWin;
            if (cooldown)
                state.cooldownSeconds = *cooldown;

            if (chipsWon && maxDailyWin)
            {
                state.winRemaining = static_cast<std::int64_t>(*maxDailyWin) - static_cast<std::int64_t>(*chipsWon);
                state.winLimitReached = *chipsWon >= *maxDailyWin;
            }

            if (winTimestamp && cooldown && cloudTime && *winTimestamp > 0 && *cooldown > 0)
            {
                state.elapsedSeconds = std::max<std::int64_t>(
                    0,
                    static_cast<std::int64_t>(*cloudTime) - static_cast<std::int64_t>(*winTimestamp));
                state.remainingSeconds = std::max<std::int64_t>(
                    0,
                    static_cast<std::int64_t>(*cooldown) - state.elapsedSeconds);
            }

            state.cooldownActive = state.winLimitReached && state.remainingSeconds > 0;

            return state.chipsWonReadable
                && state.winTimestampReadable
                && state.maxDailyWinReadable
                && state.cooldownReadable
                && state.cloudTimeReadable;
        }

        void RefreshOnGameThread()
        {
            CasinoLimitsSnapshot state;
            const bool complete = PopulateState(state);

            if (!state.sessionStarted)
            {
                Finish(std::move(state), false, "Join GTA Online before reading casino limits");
                return;
            }

            if (complete)
            {
                TUTONES_LOG_INFO(
                    "recovery.casino",
                    std::string("Casino limit diagnostics: won=") + std::to_string(state.chipsWon)
                        + " max=" + std::to_string(state.maxDailyWin)
                        + " wonTime=" + std::to_string(state.winTimestamp)
                        + " cloudTime=" + std::to_string(state.cloudTime)
                        + " cooldown=" + std::to_string(state.cooldownSeconds)
                        + " remaining=" + std::to_string(state.remainingSeconds));
            }

            Finish(
                std::move(state),
                complete,
                complete
                    ? "Casino daily-limit stats, named tunables and Rockstar cloud time read successfully"
                    : "Partial casino-limit read; wait for the native/tunable runtime and refresh again");
        }

        void ResetDailyRestrictionOnGameThread()
        {
            bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            if (!sessionStarted || !*sessionStarted)
            {
                CasinoLimitsSnapshot state;
                Finish(std::move(state), false, "Join GTA Online before resetting casino limits");
                return;
            }

            const auto originalChipsWon = Stats::GetInt(std::string{ChipsWonStat});
            const auto originalWinTimestamp = Stats::GetInt(std::string{WinTimestampStat});
            if (!originalChipsWon || !originalWinTimestamp)
            {
                CasinoLimitsSnapshot state;
                static_cast<void>(PopulateState(state));
                Finish(std::move(state), false, "Casino restriction stats could not be read before reset; nothing was changed");
                return;
            }

            const bool chipsWonWritten = Stats::SetInt(std::string{ChipsWonStat}, 0);
            const bool timestampWritten = Stats::SetInt(std::string{WinTimestampStat}, 0);

            const auto verifiedChipsWon = Stats::GetInt(std::string{ChipsWonStat});
            const auto verifiedWinTimestamp = Stats::GetInt(std::string{WinTimestampStat});
            const bool verified = chipsWonWritten
                && timestampWritten
                && verifiedChipsWon
                && verifiedWinTimestamp
                && *verifiedChipsWon == 0
                && *verifiedWinTimestamp == 0;

            bool rollbackAttempted = false;
            bool rollbackSucceeded = false;
            if (!verified)
            {
                rollbackAttempted = true;
                const bool chipsRollback = Stats::SetInt(std::string{ChipsWonStat}, *originalChipsWon);
                const bool timestampRollback = Stats::SetInt(std::string{WinTimestampStat}, *originalWinTimestamp);
                const auto chipsAfterRollback = Stats::GetInt(std::string{ChipsWonStat});
                const auto timestampAfterRollback = Stats::GetInt(std::string{WinTimestampStat});
                rollbackSucceeded = chipsRollback
                    && timestampRollback
                    && chipsAfterRollback
                    && timestampAfterRollback
                    && *chipsAfterRollback == *originalChipsWon
                    && *timestampAfterRollback == *originalWinTimestamp;
            }

            CasinoLimitsSnapshot state;
            static_cast<void>(PopulateState(state));

            if (verified)
            {
                TUTONES_LOG_INFO(
                    "recovery.casino",
                    std::string("Casino daily restriction reset and verified: won ")
                        + std::to_string(*originalChipsWon)
                        + " -> 0, wonTime "
                        + std::to_string(*originalWinTimestamp)
                        + " -> 0");
            }
            else
            {
                TUTONES_LOG_WARN(
                    "recovery.casino",
                    std::string("Casino daily restriction reset verification failed; rollback ")
                        + (rollbackSucceeded ? "verified" : "could not be fully verified"));
            }

            std::string message;
            if (verified)
            {
                message = "Casino daily restriction stats reset to zero and verified by read-back";
            }
            else if (rollbackAttempted && rollbackSucceeded)
            {
                message = "Casino reset did not verify; original stat values were restored successfully";
            }
            else
            {
                message = "Casino reset did not verify and the original stat pair could not be fully restored";
            }

            Finish(std::move(state), verified, std::move(message));
        }

        void Finish(CasinoLimitsSnapshot state, bool success, std::string message)
        {
            state.pending = false;
            state.haveResult = true;
            state.lastSucceeded = success;
            state.message = std::move(message);

            {
                std::scoped_lock lock(m_Mutex);
                m_State = std::move(state);
            }
            m_Pending.store(false, std::memory_order_release);
        }

        std::atomic<bool> m_Pending{false};
        mutable std::mutex m_Mutex;
        CasinoLimitsSnapshot m_State{};
    };
}
