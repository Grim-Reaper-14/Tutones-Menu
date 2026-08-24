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

namespace Tutones::Game::PersonalVehicles
{
    struct TeleportPersonalVehicleSnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        std::string message{"Ready"};
    };

    class TeleportPersonalVehicleRuntime final
    {
    public:
        static constexpr std::size_t TeleportGlobal = 2640101;
        static constexpr std::size_t TeleportOffset = 8;

        static TeleportPersonalVehicleRuntime& Get() noexcept
        {
            static TeleportPersonalVehicleRuntime instance;
            return instance;
        }

        bool QueueTeleport()
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            SetPending("Teleport into personal vehicle queued");
            if (Runtime::GameRuntime::Get().Enqueue([this] { ApplyOnGameThread(); }))
                return true;

            Finish(false, "Game-thread queue unavailable");
            return false;
        }

        [[nodiscard]] TeleportPersonalVehicleSnapshot Snapshot() const
        {
            TeleportPersonalVehicleSnapshot snapshot;
            snapshot.pending = m_Pending.load(std::memory_order_acquire);

            std::scoped_lock lock(m_Mutex);
            snapshot.haveResult = m_HaveResult;
            snapshot.lastSucceeded = m_LastSucceeded;
            snapshot.message = m_Message;
            return snapshot;
        }

    private:
        TeleportPersonalVehicleRuntime() = default;

        void ApplyOnGameThread()
        {
            bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            if (!sessionStarted || !*sessionStarted)
                return Finish(false, "Join GTA Online before teleporting into your personal vehicle");

            auto* pages = GamePointers::Get().ScriptGlobals();
            if (!pages)
                return Finish(false, "Enhanced script globals are unavailable");

            int* trigger = Script::ScriptGlobal(TeleportGlobal).At(TeleportOffset).As<int>(pages);
            if (!trigger)
                return Finish(false, "Personal vehicle teleport global is unavailable");

            // Enhanced 1.73 ~ b1158.13:
            // Global_2640101.f_8 = 1 triggers teleport into the active personal vehicle.
            *trigger = 1;

            TUTONES_LOG_INFO(
                "vehicle.personal.teleport",
                "Triggered personal vehicle teleport: Global_2640101.f_8=1");

            Finish(true, "Personal vehicle teleport triggered");
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
