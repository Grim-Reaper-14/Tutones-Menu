#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/Stats.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../runtime/GameRuntime.hpp"
#include "StreetDealerRuntime.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::DailyActivity
{
    struct Snapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool sessionStarted{};
        bool globalsReady{};
        int activeStreetDealerLocation{-1};
        int activeStreetDealerRecord{-1};
        std::array<bool, StreetDealer::Enhanced173::DealerCount> streetDealerCompleted{};
        std::array<bool, StreetDealer::Enhanced173::DealerCount> streetDealerCompletionReadable{};
        std::string message{"Ready"};
    };

    class Runtime final
    {
    public:
        static Runtime& Get() noexcept
        {
            static Runtime instance;
            return instance;
        }

        bool QueueRefresh()
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            SetPending("Reading Enhanced daily activity state");
            if (Tutones::Runtime::GameRuntime::Get().Enqueue([this] {
                Snapshot state;
                if (bool* sessionStarted = GamePointers::Get().IsSessionStarted())
                    state.sessionStarted = *sessionStarted;

                if (!state.sessionStarted)
                    return Finish(false, std::move(state), "Join GTA Online before reading daily activity state");

                auto* pages = GamePointers::Get().ScriptGlobals();
                state.globalsReady = pages != nullptr;
                if (!state.globalsReady)
                    return Finish(false, std::move(state), "Enhanced script globals are unavailable");

                const Script::ScriptGlobal root(StreetDealer::Enhanced173::FreemodeGlobal);
                const int* location = root.At(StreetDealer::Enhanced173::ActiveLocationOffset).As<int>(pages);
                const int* dealer = root.At(StreetDealer::Enhanced173::ActiveDealerOffset).As<int>(pages);
                if (!location || !dealer)
                    return Finish(false, std::move(state), "Street Dealer daily globals are unavailable");

                state.activeStreetDealerLocation = *location;
                state.activeStreetDealerRecord = *dealer;
                if (state.activeStreetDealerLocation < -1 || state.activeStreetDealerLocation > 49 ||
                    state.activeStreetDealerRecord < -1 ||
                    state.activeStreetDealerRecord >= static_cast<int>(StreetDealer::Enhanced173::DealerCount))
                {
                    return Finish(false, std::move(state), "Enhanced daily Street Dealer layout validation failed");
                }

                for (std::size_t index = 0; index < StreetDealer::Enhanced173::DealerCount; ++index)
                {
                    const auto value = Stats::GetPackedBool(
                        StreetDealer::Enhanced173::CompletionPackedStats[index],
                        -1);
                    if (!value)
                        continue;

                    state.streetDealerCompletionReadable[index] = true;
                    state.streetDealerCompleted[index] = *value;
                }

                TUTONES_LOG_DEBUG("daily.activity", "Enhanced daily activity state refreshed");
                Finish(true, std::move(state), "Enhanced daily activity state refreshed");
            }))
            {
                return true;
            }

            Finish(false, {}, "Game-thread queue unavailable");
            return false;
        }

        [[nodiscard]] Snapshot GetSnapshot() const
        {
            Snapshot snapshot;
            snapshot.pending = m_Pending.load(std::memory_order_acquire);
            std::scoped_lock lock(m_Mutex);
            snapshot.haveResult = m_State.haveResult;
            snapshot.lastSucceeded = m_State.lastSucceeded;
            snapshot.sessionStarted = m_State.sessionStarted;
            snapshot.globalsReady = m_State.globalsReady;
            snapshot.activeStreetDealerLocation = m_State.activeStreetDealerLocation;
            snapshot.activeStreetDealerRecord = m_State.activeStreetDealerRecord;
            snapshot.streetDealerCompleted = m_State.streetDealerCompleted;
            snapshot.streetDealerCompletionReadable = m_State.streetDealerCompletionReadable;
            snapshot.message = m_State.message;
            return snapshot;
        }

    private:
        Runtime() = default;

        void SetPending(std::string message)
        {
            std::scoped_lock lock(m_Mutex);
            m_State.haveResult = false;
            m_State.lastSucceeded = false;
            m_State.message = std::move(message);
        }

        void Finish(bool success, Snapshot state, std::string message)
        {
            {
                std::scoped_lock lock(m_Mutex);
                state.pending = false;
                state.haveResult = true;
                state.lastSucceeded = success;
                state.message = std::move(message);
                m_State = std::move(state);
            }
            m_Pending.store(false, std::memory_order_release);
        }

        std::atomic<bool> m_Pending{false};
        mutable std::mutex m_Mutex;
        Snapshot m_State{};
    };
}
