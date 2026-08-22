#pragma once

#include "../game/GamePointers.hpp"

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>

namespace Tutones::Runtime
{
    class GameRuntime final
    {
    public:
        static GameRuntime& Get() noexcept;

        bool Initialize();
        void Shutdown() noexcept;

        [[nodiscard]] bool IsInitialized() const noexcept;
        [[nodiscard]] bool IsOnGameThread() const noexcept;
        [[nodiscard]] std::uint32_t GameThreadId() const noexcept;
        bool Enqueue(std::function<void()> task);

        void SetReleaseDeadTargetEnabled(bool enabled) noexcept;
        [[nodiscard]] bool ReleaseDeadTargetEnabled() const noexcept;
        [[nodiscard]] bool ReleaseDeadTargetSupported() const noexcept;

    private:
        using AssistedAimShouldReleaseEntityFn = bool(*)(std::int64_t context);

        GameRuntime() = default;
        ~GameRuntime() = default;
        GameRuntime(const GameRuntime&) = delete;
        GameRuntime& operator=(const GameRuntime&) = delete;

        static bool RunScriptThreadsDetour(int operationsToExecute) noexcept;
        static bool AssistedAimShouldReleaseEntityDetour(std::int64_t context) noexcept;

        void Tick() noexcept;
        void DrainTasks() noexcept;
        [[nodiscard]] Game::Types::ScriptThread* FindExecutionThread() const noexcept;

        std::atomic<bool> m_Initialized{false};
        std::atomic<bool> m_ShuttingDown{false};
        std::atomic<std::uint32_t> m_ActiveCallbacks{0};
        std::atomic<std::uint32_t> m_GameThreadId{0};
        std::atomic<bool> m_ReleaseDeadTargetEnabled{true};
        std::atomic<bool> m_ReleaseDeadTargetSupported{false};

        Game::RunScriptThreadsFn m_OriginalRunScriptThreads{};
        void* m_RunScriptThreadsTarget{};
        AssistedAimShouldReleaseEntityFn m_OriginalAssistedAimShouldReleaseEntity{};
        void* m_AssistedAimShouldReleaseEntityTarget{};

        mutable std::mutex m_TaskMutex;
        std::deque<std::function<void()>> m_Tasks;

        bool m_NativeInitAttempted{};
        bool m_NativeCanaryPassed{};
        bool m_GameStateCanaryPassed{};
        bool m_LoggedNoScriptThread{};
        bool m_LoggedNoTls{};
        std::uint64_t m_TaskSequence{};
    };
}
