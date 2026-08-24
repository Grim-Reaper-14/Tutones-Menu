#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <atomic>
#include <cstddef>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::Recovery
{
    struct GoodBehaviorBonusSnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        std::string message{"Ready"};
    };

    class GoodBehaviorBonusRuntime final
    {
    public:
        static constexpr std::size_t TriggerGlobal = 2697090;
        static constexpr std::size_t RewardGlobal = 2697091;
        static constexpr int RewardAmount = 2000;

        static GoodBehaviorBonusRuntime& Get() noexcept
        {
            static GoodBehaviorBonusRuntime instance;
            return instance;
        }

        bool QueueTrigger()
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            SetPending("Good Behavior Bonus queued");
            if (Runtime::GameRuntime::Get().Enqueue([this] { ApplyOnGameThread(); }))
                return true;

            Finish(false, "Game-thread queue unavailable");
            return false;
        }

        [[nodiscard]] GoodBehaviorBonusSnapshot Snapshot() const
        {
            GoodBehaviorBonusSnapshot snapshot;
            snapshot.pending = m_Pending.load(std::memory_order_acquire);

            std::scoped_lock lock(m_Mutex);
            snapshot.haveResult = m_HaveResult;
            snapshot.lastSucceeded = m_LastSucceeded;
            snapshot.message = m_Message;
            return snapshot;
        }

    private:
        GoodBehaviorBonusRuntime() = default;

        void ApplyOnGameThread()
        {
            bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            if (!sessionStarted || !*sessionStarted)
                return Finish(false, "Join GTA Online before triggering the Good Behavior Bonus");

            auto* pages = GamePointers::Get().ScriptGlobals();
            if (!pages)
                return Finish(false, "Enhanced script globals are unavailable");

            int* reward = Script::ScriptGlobal(RewardGlobal).As<int>(pages);
            int* trigger = Script::ScriptGlobal(TriggerGlobal).As<int>(pages);
            if (!reward || !trigger)
                return Finish(false, "Good Behavior Bonus globals are unavailable");

            // Enhanced 1.73 ~ b1158.13 contract supplied for Tutones:
            // write the reward amount first, then raise the one-shot trigger.
            *reward = RewardAmount;
            std::atomic_thread_fence(std::memory_order_seq_cst);
            *trigger = 1;

            const bool success = *reward == RewardAmount && *trigger == 1;
            if (success)
            {
                TUTONES_LOG_INFO(
                    "recovery.good_behavior",
                    "Good Behavior Bonus triggered: Global_2697091=2000 then Global_2697090=1");
                Finish(true, "Good Behavior Bonus triggered for $2,000");
            }
            else
            {
                TUTONES_LOG_ERROR(
                    "recovery.good_behavior",
                    "Good Behavior Bonus globals failed immediate write verification");
                Finish(false, "Good Behavior Bonus write verification failed");
            }
        }

        void SetPending(std::string message)
        {
            std::scoped_lock lock(m_Mutex);
            m_HaveResult = false;
            m_LastSucceeded = false;
            m_Message = std::move(message);
        }

        void Finish(bool success, std::string message)
        {
            {
                std::scoped_lock lock(m_Mutex);
                m_HaveResult = true;
                m_LastSucceeded = success;
                m_Message = std::move(message);
            }
            m_Pending.store(false, std::memory_order_release);
        }

        std::atomic<bool> m_Pending{false};
        mutable std::mutex m_Mutex;
        bool m_HaveResult{};
        bool m_LastSucceeded{};
        std::string m_Message{"Ready"};
    };
}
