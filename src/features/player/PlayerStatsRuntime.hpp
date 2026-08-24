#pragma once

#include "../../game/Stats.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

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
            static constexpr std::array<int, 97> LevelToRp1To97{{
                0, 800, 2100, 3800, 6100, 9500, 12500, 16000, 19800, 24000,
                28500, 33400, 38700, 44200, 50200, 56400, 63000, 69900, 77100, 84700,
                92500, 100700, 109200, 118000, 127100, 136500, 146200, 156200, 166500, 177100,
                188000, 199200, 210700, 222400, 234500, 246800, 259400, 272300, 285500, 299000,
                312700, 326800, 341000, 355600, 370500, 385600, 401000, 416600, 432600, 448800,
                465200, 482000, 499000, 516300, 533800, 551600, 569600, 588000, 606500, 625400,
                644500, 663800, 683400, 703300, 723400, 743800, 764500, 785400, 806500, 827900,
                849600, 871500, 893600, 916000, 938700, 961600, 984700, 1008100, 1031800, 1055700,
                1079800, 1104200, 1128800, 1153700, 1178800, 1204200, 1229800, 1255600, 1281700, 1308100,
                1334600, 1361400, 1388500, 1415800, 1443300, 1471100, 1499100,
            }};

            rank = std::clamp(rank, 1, 8000);
            if (rank <= 97)
                return LevelToRp1To97[static_cast<std::size_t>(rank - 1)];

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

            PublishPending("Reading current GTA Online stats...");
            auto& runtime = Runtime::GameRuntime::Get();
            if (!runtime.IsInitialized()
                || !runtime.Enqueue([this] { RefreshOnGameThread("Stats refreshed", true); }))
            {
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

            PublishPending("Applying Rank / RP / Kills / Deaths...");
            auto& runtime = Runtime::GameRuntime::Get();
            if (!runtime.IsInitialized()
                || !runtime.Enqueue([this, rank, rp, kills, deaths] {
                    const auto character = Stats::GetCharIndex();
                    if (!character || *character < 0 || *character > 1)
                    {
                        PublishFailure("Could not resolve the active GTA Online character");
                        return;
                    }

                    bool writesSucceeded = true;
                    writesSucceeded = Stats::SetInt("MPPLY_GLOBALXP", rp) && writesSucceeded;
                    writesSucceeded = Stats::SetInt("MPX_CHAR_XP_FM", rp, *character) && writesSucceeded;
                    writesSucceeded = Stats::SetInt("MPX_CHAR_SET_RP_GIFT_ADMIN", rp, *character) && writesSucceeded;
                    writesSucceeded = Stats::SetInt("MPX_CHAR_RANK_FM", rank, *character) && writesSucceeded;
                    writesSucceeded = Stats::SetInt("MPPLY_KILLS_PLAYERS", kills) && writesSucceeded;
                    writesSucceeded = Stats::SetInt("MPPLY_DEATHS_PLAYER", deaths) && writesSucceeded;

                    const auto readRank = Stats::GetInt("MPX_CHAR_RANK_FM", *character);
                    const auto readRp = Stats::GetInt("MPX_CHAR_XP_FM", *character);
                    const auto readKills = Stats::GetInt("MPPLY_KILLS_PLAYERS");
                    const auto readDeaths = Stats::GetInt("MPPLY_DEATHS_PLAYER");
                    const bool readBackMatches = readRank && readRp && readKills && readDeaths
                        && *readRank == rank
                        && *readRp == rp
                        && *readKills == kills
                        && *readDeaths == deaths;

                    RefreshOnGameThread(
                        writesSucceeded && readBackMatches
                            ? "Rank / RP / kills / deaths applied and verified"
                            : writesSucceeded
                                ? "Stat writes completed, but read-back did not fully match"
                                : "One or more stat writes were rejected",
                        writesSucceeded && readBackMatches);
                }))
            {
                PublishFailure("Could not queue stat writes on the GTA script thread");
                return false;
            }
            return true;
        }

    private:
        PlayerStatsRuntime() = default;

        void RefreshOnGameThread(const char* message, bool operationSucceeded)
        {
            PlayerStatsSnapshot state{};
            const auto character = Stats::GetCharIndex();
            if (!character || *character < 0 || *character > 1)
            {
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
            state.lastSucceeded = state.readable && operationSucceeded;
            state.message = !state.readable
                ? "One or more Online stats could not be read"
                : message ? message : "Stats refreshed";

            m_Pending.store(false, std::memory_order_release);
            std::scoped_lock lock(m_Mutex);
            m_State = std::move(state);
        }

        void PublishPending(const char* message)
        {
            std::scoped_lock lock(m_Mutex);
            m_State.pending = true;
            m_State.message = message ? message : "Working...";
        }

        void PublishFailure(const char* message)
        {
            m_Pending.store(false, std::memory_order_release);
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
