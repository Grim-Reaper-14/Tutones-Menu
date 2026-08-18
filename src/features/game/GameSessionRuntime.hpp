#pragma once

#include "GameSessionTypes.hpp"

#include <atomic>
#include <mutex>

namespace Tutones::Game::SessionFeatures
{
    struct GameSessionSnapshot final
    {
        bool scriptRuntimeReady{};
        bool capabilityProbed{};
        bool shopControllerReady{};
        bool actionPending{};
        bool hasLastAction{};
        JoinType lastRequested{JoinType::JoinPublic};
        bool lastActionSucceeded{};
    };

    class GameSessionRuntime final
    {
    public:
        static GameSessionRuntime& Get() noexcept;

        [[nodiscard]] bool QueueJoin(JoinType type);
        [[nodiscard]] bool QueueLeaveOnline();
        [[nodiscard]] GameSessionSnapshot Snapshot() const noexcept;
        [[nodiscard]] bool CreatorSupported() const noexcept { return false; }

    private:
        GameSessionRuntime() = default;
        ~GameSessionRuntime() = default;
        GameSessionRuntime(const GameSessionRuntime&) = delete;
        GameSessionRuntime& operator=(const GameSessionRuntime&) = delete;

        void ExecuteOnGameThread(JoinType type) noexcept;
        void RecordResult(JoinType type, bool shopControllerReady, bool success) noexcept;

        std::atomic<bool> m_Pending{false};
        mutable std::mutex m_Mutex;
        GameSessionSnapshot m_State{};
    };
}
