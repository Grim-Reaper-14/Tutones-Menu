#pragma once

#include "../../game/PlayerNatives.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>

namespace Tutones::Game::PlayerFeatures
{
    enum class PlayerAction : unsigned char
    {
        None,
        SetHealth,
        Heal,
        SetArmor,
        SetWanted,
        ClearWanted,
        ApplyPersistent,
        ModelRequest,
        ModelSwap,
        SetComponent,
        DefaultComponents,
        RandomComponents,
    };

    struct PlayerSnapshot final
    {
        Player player{};
        Ped ped{};
        Hash model{};
        int health{};
        int maxHealth{};
        int armor{};
        int wantedLevel{};
        int observedComponent{};
        int drawableCount{};
        int textureCount{};
        int textureQueryDrawable{};
        int currentDrawable{};
        int currentTexture{};
        int currentPalette{};
        bool invincible{};
        bool bulletproof{};
        bool invisible{};
        bool noRagdoll{};
        bool superJump{};
        bool infiniteStamina{};
        bool neverWanted{};
        bool policeIgnore{};
        bool everyoneIgnore{};
        float runMultiplier{1.0f};
        float swimMultiplier{1.0f};
        bool modelLoadPending{};
        Hash pendingModel{};
        PlayerAction lastAction{PlayerAction::None};
        bool lastActionSucceeded{};
        bool valid{};
    };

    class PlayerRuntime final
    {
    public:
        using Clock = std::chrono::steady_clock;
        static constexpr auto RefreshInterval = std::chrono::milliseconds{250};
        static constexpr auto ModelLoadTimeout = std::chrono::seconds{5};

        static PlayerRuntime& Get() noexcept;
        bool Start();
        void Stop() noexcept;
        [[nodiscard]] bool IsRunning() const noexcept;
        [[nodiscard]] PlayerSnapshot Snapshot() const noexcept;

        void SetObservedComponent(int componentId, int drawableId = -1) noexcept;
        void SetInvincible(bool enabled);
        void SetBulletproof(bool enabled);
        void SetInvisible(bool enabled);
        void SetNoRagdoll(bool enabled);
        void SetSuperJump(bool enabled) noexcept;
        void SetInfiniteStamina(bool enabled) noexcept;
        void SetNeverWanted(bool enabled);
        void SetPoliceIgnore(bool enabled);
        void SetEveryoneIgnore(bool enabled);
        void SetRunMultiplier(float multiplier);
        void SetSwimMultiplier(float multiplier);

        [[nodiscard]] bool QueueSetHealth(int health);
        [[nodiscard]] bool QueueHeal();
        [[nodiscard]] bool QueueSetArmor(int armor);
        [[nodiscard]] bool QueueSetWantedLevel(int wantedLevel);
        [[nodiscard]] bool QueueClearWanted();
        [[nodiscard]] bool QueueModelByName(std::string modelName);
        [[nodiscard]] bool QueueSetComponent(int componentId, int drawableId, int textureId, int paletteId);
        [[nodiscard]] bool QueueDefaultComponents();
        [[nodiscard]] bool QueueRandomComponents();

    private:
        PlayerRuntime() = default;
        ~PlayerRuntime() = default;
        PlayerRuntime(const PlayerRuntime&) = delete;
        PlayerRuntime& operator=(const PlayerRuntime&) = delete;

        [[nodiscard]] static Hash Joaat(std::string_view value) noexcept;
        bool QueueNextTick();
        void TickOnGameThread() noexcept;
        bool Refresh(Player player, Ped ped) noexcept;
        bool ApplyPersistentState(Player player, Ped ped) noexcept;
        bool QueuePlayerOperation(PlayerAction action, std::function<bool(Player, Ped)> apply);
        void ProcessPendingModel(Player player) noexcept;
        void RecordAction(PlayerAction action, bool success) noexcept;
        void ClearSnapshot() noexcept;

        std::atomic<bool> m_Running{false};
        std::atomic<int> m_ObservedComponent{0};
        std::atomic<int> m_ObservedDrawable{-1};
        std::atomic<bool> m_Invincible{false};
        std::atomic<bool> m_Bulletproof{false};
        std::atomic<bool> m_Invisible{false};
        std::atomic<bool> m_NoRagdoll{false};
        std::atomic<bool> m_SuperJump{false};
        std::atomic<bool> m_InfiniteStamina{false};
        std::atomic<bool> m_NeverWanted{false};
        std::atomic<bool> m_PoliceIgnore{false};
        std::atomic<bool> m_EveryoneIgnore{false};
        std::atomic<float> m_RunMultiplier{1.0f};
        std::atomic<float> m_SwimMultiplier{1.0f};
        int m_LastObservedComponent{-1};
        int m_LastObservedDrawable{-2};
        Ped m_LastPed{};
        Clock::time_point m_NextRefresh{};
        Hash m_PendingModel{};
        Clock::time_point m_ModelDeadline{};
        mutable std::mutex m_Mutex;
        PlayerSnapshot m_Snapshot{};
    };
}
