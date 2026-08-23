#pragma once

#include "GameSessionTypes.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
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
        bool noIdleEnabled{};
        bool noIdleReady{};
        bool servicePending{};
        GameServiceAction lastServiceAction{GameServiceAction::None};
        bool lastServiceSucceeded{};
        JoinType lastRequested{JoinType::JoinPublic};
        bool lastActionSucceeded{};
    };

    class GameSessionRuntime final
    {
    public:
        static GameSessionRuntime& Get() noexcept;

        [[nodiscard]] bool QueueJoin(JoinType type);
        [[nodiscard]] bool QueueLeaveOnline();
        [[nodiscard]] bool QueueSkipCutscene();
        [[nodiscard]] bool QueueSkipConversation();
        [[nodiscard]] bool QueueAirstrikeAhead(int damage);
        [[nodiscard]] bool QueueAmmoDrop();
        [[nodiscard]] bool QueueMinigunDrop();
        void SetNoIdle(bool enabled);
        void Shutdown() noexcept;
        [[nodiscard]] GameSessionSnapshot Snapshot() const noexcept;
        [[nodiscard]] bool CreatorSupported() const noexcept { return false; }

    private:
        using Clock = std::chrono::steady_clock;

        GameSessionRuntime() = default;
        ~GameSessionRuntime() = default;
        GameSessionRuntime(const GameSessionRuntime&) = delete;
        GameSessionRuntime& operator=(const GameSessionRuntime&) = delete;

        void ExecuteOnGameThread(JoinType type) noexcept;
        void RecordResult(JoinType type, bool shopControllerReady, bool success) noexcept;
        void RecordServiceResult(GameServiceAction action, bool success) noexcept;
        [[nodiscard]] bool QueuePickupDrop(GameServiceAction action);
        [[nodiscard]] bool BeginPickupDropOnGameThread(GameServiceAction action) noexcept;
        void ProcessPendingServiceOnGameThread() noexcept;
        [[nodiscard]] bool ExecuteAirstrikeOnGameThread(int damage) noexcept;
        [[nodiscard]] bool EnsureUtilityTick();
        void UtilityTickOnGameThread() noexcept;
        [[nodiscard]] bool ResolveNoIdleTunablesOnGameThread() noexcept;
        [[nodiscard]] bool ApplyNoIdleOnGameThread() noexcept;
        [[nodiscard]] bool RestoreNoIdleOnGameThread() noexcept;

        std::atomic<bool> m_Pending{false};
        std::atomic<bool> m_NoIdle{false};
        std::atomic<bool> m_ServicePending{false};
        std::atomic<bool> m_UtilityTickQueued{false};
        std::atomic<bool> m_NoIdleResolved{false};
        std::array<std::size_t, 8> m_NoIdleGlobals{};
        std::array<int, 8> m_NoIdleOriginals{};
        Clock::time_point m_NextNoIdleResolve{};
        GameServiceAction m_PendingServiceAction{GameServiceAction::None};
        std::uint32_t m_PendingPickupModel{};
        Clock::time_point m_ServiceDeadline{};
        mutable std::mutex m_Mutex;
        GameSessionSnapshot m_State{};
    };
}
