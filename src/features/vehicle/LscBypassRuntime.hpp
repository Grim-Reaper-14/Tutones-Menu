#pragma once

#include "../../game/script/ScriptPatchRuntime.hpp"

#include <atomic>
#include <mutex>

namespace Tutones::Game::Mods
{
    struct LscBypassSnapshot final
    {
        bool running{};
        bool enabled{};
        bool hookActive{};
        bool programLoaded{};
        bool canUseVehicleSupported{};
        bool blockMenuOptionSupported{};
        bool applied{};
    };

    class LscBypassRuntime final
    {
    public:
        static LscBypassRuntime& Get() noexcept;

        bool Start();
        void Stop() noexcept;

        void SetEnabled(bool enabled) noexcept;
        [[nodiscard]] bool Enabled() const noexcept;
        [[nodiscard]] bool IsRunning() const noexcept;
        [[nodiscard]] LscBypassSnapshot Snapshot() const noexcept;

    private:
        LscBypassRuntime() = default;
        ~LscBypassRuntime() = default;
        LscBypassRuntime(const LscBypassRuntime&) = delete;
        LscBypassRuntime& operator=(const LscBypassRuntime&) = delete;

        bool QueueNextTick();
        void TickOnGameThread() noexcept;
        void PublishSnapshot() noexcept;

        std::atomic<bool> m_Running{false};
        std::atomic<bool> m_Enabled{false};
        Script::ScriptPatchHandle m_CanUseVehiclePatch{};
        Script::ScriptPatchHandle m_BlockMenuOptionPatch{};

        mutable std::mutex m_Mutex;
        LscBypassSnapshot m_Snapshot;
    };
}
