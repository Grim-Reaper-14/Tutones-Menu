#pragma once

#include "OffRadarState.hpp"

#include <atomic>
#include <mutex>

namespace Tutones::Game::PlayerFeatures
{
    class OffRadarRuntime final
    {
    public:
        static OffRadarRuntime& Get() noexcept;

        bool Start();
        void Stop() noexcept;

        [[nodiscard]] bool IsRunning() const noexcept;
        [[nodiscard]] OffRadarState Snapshot() const noexcept;
        void SetEnabled(bool enabled) noexcept;

    private:
        OffRadarRuntime() = default;
        ~OffRadarRuntime() = default;
        OffRadarRuntime(const OffRadarRuntime&) = delete;
        OffRadarRuntime& operator=(const OffRadarRuntime&) = delete;

        bool QueueNextTick();
        void TickOnGameThread() noexcept;
        void ApplyOnGameThread(bool requestedEnabled) noexcept;
        void QueueDisableCleanup() noexcept;
        void PublishState(const OffRadarState& state) noexcept;

        std::atomic<bool> m_Running{false};
        std::atomic<bool> m_Enabled{false};
        std::atomic<bool> m_Applied{false};
        mutable std::mutex m_Mutex;
        OffRadarState m_State{};
    };
}
