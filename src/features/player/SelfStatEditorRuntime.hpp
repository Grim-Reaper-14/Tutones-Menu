#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/Stats.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <atomic>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::PlayerFeatures
{
    struct SelfStatEditorSnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool readable{};
        int characterIndex{-1};
        int rank{};
        int rp{};
        int kills{};
        int deaths{};
        float kdRatio{};
        std::string message{"Ready"};
    };

    class SelfStatEditorRuntime final
    {
    public:
        static SelfStatEditorRuntime& Get() noexcept
        {
            static SelfStatEditorRuntime instance;
            return instance;
        }

        bool QueueRefresh()
        {
            return Queue(false, 0, 0, 0, 0);
        }

        bool QueueApply(int rank, int rp, int kills, int deaths)
        {
            if (rank < 0 || rank > 8000 || rp < 0 || kills < 0 || deaths < 0)
                return false;
            return Queue(true, rank, rp, kills, deaths);
        }

        [[nodiscard]] SelfStatEditorSnapshot Snapshot() const
        {
            SelfStatEditorSnapshot result;
            result.pending = m_Pending.load(std::memory_order_acquire);
            std::scoped_lock lock(m_Mutex);
            result.haveResult = m_HaveResult;
            result.lastSucceeded = m_LastSucceeded;
            result.readable = m_Readable;
            result.characterIndex = m_CharacterIndex;
            result.rank = m_Rank;
            result.rp = m_Rp;
            result.kills = m_Kills;
            result.deaths = m_Deaths;
            result.kdRatio = m_Deaths > 0 ? static_cast<float>(m_Kills) / static_cast<float>(m_Deaths) : static_cast<float>(m_Kills);
            result.message = m_Message;
            return result;
        }

    private:
        SelfStatEditorRuntime() = default;

        bool Queue(bool apply, int rank, int rp, int kills, int deaths)
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            {
                std::scoped_lock lock(m_Mutex);
                m_HaveResult = false;
                m_LastSucceeded = false;
                m_Message = apply ? "Stat update queued" : "Stat refresh queued";
            }

            if (Runtime::GameRuntime::Get().Enqueue([this, apply, rank, rp, kills, deaths] {
                    RunOnGameThread(apply, rank, rp, kills, deaths);
                }))
                return true;

            Finish(false, false, -1, 0, 0, 0, 0, "Game-thread queue unavailable");
            return false;
        }

        void RunOnGameThread(bool apply, int rank, int rp, int kills, int deaths)
        {
            bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            if (!sessionStarted || !*sessionStarted)
                return Finish(false, false, -1, 0, 0, 0, 0, "Join GTA Online before editing stats");
            if (!Native::NativeRegistry::Get().IsReady())
                return Finish(false, false, -1, 0, 0, 0, 0, "Native stat backend is unavailable");

            const auto characterIndex = Stats::GetCharIndex();
            if (!characterIndex)
                return Finish(false, false, -1, 0, 0, 0, 0, "Online character index is unavailable");

            bool success = true;
            if (apply)
            {
                success = Stats::SetInt("MPX_CHAR_RANK_FM", rank, *characterIndex)
                    && Stats::SetInt("MPX_CHAR_XP_FM", rp, *characterIndex)
                    && Stats::SetInt("MPPLY_KILLS_PLAYERS", kills, *characterIndex)
                    && Stats::SetInt("MPPLY_DEATHS_PLAYER", deaths, *characterIndex);
            }

            const auto rankRead = Stats::GetInt("MPX_CHAR_RANK_FM", *characterIndex);
            const auto rpRead = Stats::GetInt("MPX_CHAR_XP_FM", *characterIndex);
            const auto killsRead = Stats::GetInt("MPPLY_KILLS_PLAYERS", *characterIndex);
            const auto deathsRead = Stats::GetInt("MPPLY_DEATHS_PLAYER", *characterIndex);
            const bool readable = rankRead && rpRead && killsRead && deathsRead;

            if (apply && readable)
            {
                success = success
                    && *rankRead == rank
                    && *rpRead == rp
                    && *killsRead == kills
                    && *deathsRead == deaths;
            }
            else if (!apply)
            {
                success = readable;
            }

            const int finalRank = rankRead.value_or(0);
            const int finalRp = rpRead.value_or(0);
            const int finalKills = killsRead.value_or(0);
            const int finalDeaths = deathsRead.value_or(0);

            TUTONES_LOG_INFO(
                "player.stats",
                "Self stat editor %s: character=%d rank=%d rp=%d kills=%d deaths=%d success=%s",
                apply ? "apply" : "refresh",
                *characterIndex,
                finalRank,
                finalRp,
                finalKills,
                finalDeaths,
                success ? "true" : "false");

            Finish(
                success,
                readable,
                *characterIndex,
                finalRank,
                finalRp,
                finalKills,
                finalDeaths,
                apply ? (success ? "Stats written and read-back verified" : "One or more stats failed read-back verification")
                      : (success ? "Stats refreshed" : "One or more stats could not be read"));
        }

        void Finish(bool success, bool readable, int characterIndex, int rank, int rp, int kills, int deaths, std::string message)
        {
            {
                std::scoped_lock lock(m_Mutex);
                m_HaveResult = true;
                m_LastSucceeded = success;
                m_Readable = readable;
                m_CharacterIndex = characterIndex;
                m_Rank = rank;
                m_Rp = rp;
                m_Kills = kills;
                m_Deaths = deaths;
                m_Message = std::move(message);
            }
            m_Pending.store(false, std::memory_order_release);
        }

        std::atomic<bool> m_Pending{false};
        mutable std::mutex m_Mutex;
        bool m_HaveResult{};
        bool m_LastSucceeded{};
        bool m_Readable{};
        int m_CharacterIndex{-1};
        int m_Rank{};
        int m_Rp{};
        int m_Kills{};
        int m_Deaths{};
        std::string m_Message{"Ready"};
    };
}
