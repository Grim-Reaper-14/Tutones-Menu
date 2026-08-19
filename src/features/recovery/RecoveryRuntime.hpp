#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>

namespace Tutones::Game::Recovery
{
    enum class RecoveryAction : std::uint8_t
    {
        None,
        SetWarehouseCrates,
        SetBunkerSupplies,
        SetBunkerProduct,
    };

    struct WarehouseSnapshot final
    {
        int slot{-1};
        int propertyId{};
        int crates{};
        int capacity{};
        bool owned{};
        bool readable{};
    };

    struct BunkerSnapshot final
    {
        int propertyId{};
        int supplies{};
        int product{};
        bool owned{};
        bool setup{};
        bool readable{};
    };

    struct RecoverySnapshot final
    {
        std::array<WarehouseSnapshot, 5> warehouses{};
        BunkerSnapshot bunker{};
        int characterIndex{-1};
        float observedRpMultiplier{1.0f};
        float requestedRpMultiplier{1.0f};
        int lastActionTarget{-1};
        int lastActionValue{};
        RecoveryAction lastAction{RecoveryAction::None};
        bool running{};
        bool nativeReady{};
        bool sessionStarted{};
        bool statsReady{};
        bool rpMultiplierEnabled{};
        bool rpMultiplierReady{};
        bool actionPending{};
        bool lastActionSucceeded{};
        std::uint64_t revision{};
    };

    class RecoveryRuntime final
    {
    public:
        static RecoveryRuntime& Get() noexcept;

        bool Start();
        void Stop() noexcept;

        [[nodiscard]] bool IsRunning() const noexcept;
        [[nodiscard]] RecoverySnapshot Snapshot() const noexcept;

        void SetRpMultiplier(float multiplier) noexcept;
        void SetRpMultiplierEnabled(bool enabled) noexcept;

        bool QueueSetWarehouseCrates(int slot, int crates);
        bool QueueSetBunkerSupplies(int supplies);
        bool QueueSetBunkerProduct(int product);

    private:
        using Clock = std::chrono::steady_clock;

        RecoveryRuntime() = default;
        ~RecoveryRuntime() = default;
        RecoveryRuntime(const RecoveryRuntime&) = delete;
        RecoveryRuntime& operator=(const RecoveryRuntime&) = delete;

        bool QueueNextTick();
        bool QueueAction(RecoveryAction action, int target, int value);
        void TickOnGameThread() noexcept;
        void RefreshOnGameThread() noexcept;
        void ProcessActionOnGameThread() noexcept;
        void ApplyRpMultiplierOnGameThread() noexcept;
        void RecordAction(RecoveryAction action, int target, int value, bool success) noexcept;

        std::atomic<bool> m_Running{false};
        std::atomic<bool> m_RpEnabled{false};
        std::atomic<float> m_RpMultiplier{1.0f};
        mutable std::mutex m_Mutex;
        RecoverySnapshot m_Snapshot{};
        RecoveryAction m_QueuedAction{RecoveryAction::None};
        int m_QueuedTarget{-1};
        int m_QueuedValue{};
        bool m_ActionBusy{};
        bool m_HaveOriginalRpMultiplier{};
        float m_OriginalRpMultiplier{1.0f};
        Clock::time_point m_NextRefresh{};
    };
}
