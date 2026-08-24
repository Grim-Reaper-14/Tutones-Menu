#pragma once

#include "../../game/Stats.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>
#include <string>

namespace Tutones::Game::PlayerFeatures
{
    struct PlayerStatsSnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool readable{};
        bool moneyWritable{};
        int characterIndex{-1};
        int rank{};
        int rp{};
        int kills{};
        int deaths{};
        float kdRatio{};
        std::string message{"Ready"};
    };

    class PlayerStatsRuntime final
    {
    public:
        static PlayerStatsRuntime& Get() noexcept
        {
            static PlayerStatsRuntime instance;
            return instance;
        }

        [[nodiscard]] PlayerStatsSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            return m_State;
        }

        [[nodiscard]] static std::optional<int> RpForRank(int rank) noexcept
        {
            if (rank <= 1)
                return 0;
            if (rank < 98 || rank > 8000)
                return std::nullopt;

            // Rockstar's RP curve from rank 98 upward. This also reaches the
            // documented rank-8000 total of 1,787,576,850 without overflowing int32.
            const std::int64_t r = rank;
            const std::int64_t value = (25 * r * r) + (23575 * r) - 1023150;
            if (value < 0 || value > std::numeric_limits<int>::max())
                return std::nullopt;
            return static_cast<int>(value);
        }

        bool QueueRefresh()
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            if (!Runtime::GameRuntime::Get().IsInitialized()
                || !Runtime::GameRuntime::Get().Enqueue([this] { RefreshOnGameThread("Stats refreshed"); }))
            {
                m_Pending.store(false, std::memory_order_release);
                PublishFailure("Game runtime is unavailable");
                return false;
            }
            return true;
        }

        bool QueueApply(int rank, int rp, int kills, int deaths)
        {
            rank = std::clamp(rank, 1, 8000);
            rp = std::max(0, rp);
            kills = std::max(0, kills);
            deaths = std::max(0, deaths);

            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            if (!Runtime::GameRuntime::Get().IsInitialized()
                || !Runtime::GameRuntime::Get().Enqueue([this, rank, rp, kills, deaths] {
                    const auto character = Stats::GetCharIndex();
                    if (!character || *character < 0 || *character > 1)
                    {
                        m_Pending.store(false, std::memory_order_release);
                        PublishFailure("Could not resolve the active GTA Online character");
                        return;
                    }

                    bool success = true;
                    success = Stats::SetInt("MPPLY_GLOBALXP", rp) && success;
                    success = Stats::SetInt("MPX_CHAR_XP_FM", rp, *character) && success;
                    success = Stats::SetInt("MPX_CHAR_SET_RP_GIFT_ADMIN", rp, *character) && success;
                    success = Stats::SetInt("MPX_CHAR_RANK_FM", rank, *character) && success;
                    success = Stats::SetInt("MPPLY_KILLS_PLAYERS", kills) && success;
                    success = Stats::SetInt("MPPLY_DEATHS_PLAYER", deaths) && success;

                    m_Pending.store(false, std::memory_order_release);
                    RefreshOnGameThread(success
                        ? "Rank / RP / kills / deaths applied with read-back"
                        : "One or more stat writes were rejected");
                }))
            {
                m_Pending.store(false, std::memory_order_release);
                PublishFailure("Could not queue stat writes on the GTA script thread");
                return false;
            }
            return true;
        }

    private:
        PlayerStatsRuntime() = default;

        void RefreshOnGameThread(const char* message)
        {
            PlayerStatsSnapshot state{};
            const auto character = Stats::GetCharIndex();
            if (!character || *character < 0 || *character > 1)
            {
                m_Pending.store(false, std::memory_order_release);
                PublishFailure("GTA Online character stats are unavailable");
                return;
            }

            state.characterIndex = *character;
            const auto rank = Stats::GetInt("MPX_CHAR_RANK_FM", *character);
            const auto rp = Stats::GetInt("MPX_CHAR_XP_FM", *character);
            const auto kills = Stats::GetInt("MPPLY_KILLS_PLAYERS");
            const auto deaths = Stats::GetInt("MPPLY_DEATHS_PLAYER");

            state.readable = rank && rp && kills && deaths;
            state.rank = rank.value_or(0);
            state.rp = rp.value_or(0);
            state.kills = kills.value_or(0);
            state.deaths = deaths.value_or(0);
            state.kdRatio = state.deaths > 0
                ? static_cast<float>(state.kills) / static_cast<float>(state.deaths)
                : static_cast<float>(state.kills);
            state.moneyWritable = false;
            state.pending = false;
            state.haveResult = true;
            state.lastSucceeded = state.readable;
            state.message = state.readable ? message : "One or more Online stats could not be read";

            m_Pending.store(false, std::memory_order_release);
            std::scoped_lock lock(m_Mutex);
            m_State = std::move(state);
        }

        void PublishFailure(const char* message)
        {
            std::scoped_lock lock(m_Mutex);
            m_State.pending = false;
            m_State.haveResult = true;
            m_State.lastSucceeded = false;
            m_State.message = message ? message : "Stat operation failed";
        }

        std::atomic<bool> m_Pending{false};
        mutable std::mutex m_Mutex;
        PlayerStatsSnapshot m_State{};
    };
}
