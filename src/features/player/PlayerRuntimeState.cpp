#include "PlayerRuntime.hpp"

#include "../../game/native/NativeInvoker.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace Tutones::Game::PlayerFeatures
{
    namespace
    {
        bool SetEntityBulletproof(Ped ped, bool enabled) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetEntityProofs,
                ped,
                static_cast<std::int32_t>(enabled),
                0,
                0,
                0,
                0,
                0,
                0,
                0);
        }
    }

    bool PlayerRuntime::Refresh(Player player, Ped ped) noexcept
    {
        const auto model = PlayerNatives::GetEntityModel(ped);
        const auto health = PlayerNatives::GetEntityHealth(ped);
        const auto maxHealth = PlayerNatives::GetEntityMaxHealth(ped);
        const auto armor = PlayerNatives::GetPedArmour(ped);
        const auto wanted = PlayerNatives::GetPlayerWantedLevel(player);
        const int component = m_ObservedComponent.load(std::memory_order_acquire);
        const auto drawableCount = PlayerNatives::GetNumberOfPedDrawableVariations(ped, component);
        const auto currentDrawable = PlayerNatives::GetPedDrawableVariation(ped, component);
        const auto currentTexture = PlayerNatives::GetPedTextureVariation(ped, component);
        const auto currentPalette = PlayerNatives::GetPedPaletteVariation(ped, component);

        if (!model || !health || !maxHealth || !armor || !wanted || !drawableCount
            || !currentDrawable || !currentTexture || !currentPalette)
        {
            ClearSnapshot();
            return false;
        }

        const int requestedDrawable = m_ObservedDrawable.load(std::memory_order_acquire);
        const int textureQueryDrawable = requestedDrawable >= 0 && requestedDrawable < *drawableCount
            ? requestedDrawable
            : *currentDrawable;
        const auto textureCount = PlayerNatives::GetNumberOfPedTextureVariations(ped, component, textureQueryDrawable);
        if (!textureCount)
        {
            ClearSnapshot();
            return false;
        }

        std::scoped_lock lock(m_Mutex);
        const PlayerAction lastAction = m_Snapshot.lastAction;
        const bool lastActionSucceeded = m_Snapshot.lastActionSucceeded;
        m_Snapshot = {};
        m_Snapshot.player = player;
        m_Snapshot.ped = ped;
        m_Snapshot.model = *model;
        m_Snapshot.health = *health;
        m_Snapshot.maxHealth = *maxHealth;
        m_Snapshot.armor = *armor;
        m_Snapshot.wantedLevel = std::clamp(*wanted, 0, 5);
        m_Snapshot.observedComponent = component;
        m_Snapshot.drawableCount = std::max(0, *drawableCount);
        m_Snapshot.textureCount = std::max(0, *textureCount);
        m_Snapshot.textureQueryDrawable = textureQueryDrawable;
        m_Snapshot.currentDrawable = *currentDrawable;
        m_Snapshot.currentTexture = *currentTexture;
        m_Snapshot.currentPalette = *currentPalette;
        m_Snapshot.invincible = m_Invincible.load(std::memory_order_acquire);
        m_Snapshot.bulletproof = m_Bulletproof.load(std::memory_order_acquire);
        m_Snapshot.invisible = m_Invisible.load(std::memory_order_acquire);
        m_Snapshot.noRagdoll = m_NoRagdoll.load(std::memory_order_acquire);
        m_Snapshot.superJump = m_SuperJump.load(std::memory_order_acquire);
        m_Snapshot.infiniteStamina = m_InfiniteStamina.load(std::memory_order_acquire);
        m_Snapshot.neverWanted = m_NeverWanted.load(std::memory_order_acquire);
        m_Snapshot.policeIgnore = m_PoliceIgnore.load(std::memory_order_acquire);
        m_Snapshot.everyoneIgnore = m_EveryoneIgnore.load(std::memory_order_acquire);
        m_Snapshot.runMultiplier = m_RunMultiplier.load(std::memory_order_acquire);
        m_Snapshot.swimMultiplier = m_SwimMultiplier.load(std::memory_order_acquire);
        m_Snapshot.modelLoadPending = m_PendingModel != 0;
        m_Snapshot.pendingModel = m_PendingModel;
        m_Snapshot.lastAction = lastAction;
        m_Snapshot.lastActionSucceeded = lastActionSucceeded;
        m_Snapshot.valid = true;
        return true;
    }

    void PlayerRuntime::SetBulletproof(bool enabled)
    {
        m_Bulletproof.store(enabled, std::memory_order_release);
        static_cast<void>(QueuePlayerOperation(PlayerAction::ApplyPersistent, [this](Player player, Ped ped) {
            return ApplyPersistentState(player, ped);
        }));
    }

    bool PlayerRuntime::ApplyPersistentState(Player player, Ped ped) noexcept
    {
        const auto dead = PlayerNatives::IsEntityDead(ped, true);
        const bool requestedInvincible = m_Invincible.load(std::memory_order_acquire);
        const bool applyInvincible = requestedInvincible && (!dead || !*dead);

        bool success = true;
        success = PlayerNatives::SetEntityInvincible(ped, applyInvincible, false) && success;
        success = SetEntityBulletproof(ped, m_Bulletproof.load(std::memory_order_acquire)) && success;
        success = PlayerNatives::SetEntityVisible(ped, !m_Invisible.load(std::memory_order_acquire), false) && success;
        success = PlayerNatives::SetPedCanRagdoll(ped, !m_NoRagdoll.load(std::memory_order_acquire)) && success;
        success = PlayerNatives::SetPoliceIgnorePlayer(player, m_PoliceIgnore.load(std::memory_order_acquire)) && success;
        success = PlayerNatives::SetEveryoneIgnorePlayer(player, m_EveryoneIgnore.load(std::memory_order_acquire)) && success;
        success = PlayerNatives::SetRunSprintMultiplierForPlayer(player, m_RunMultiplier.load(std::memory_order_acquire)) && success;
        success = PlayerNatives::SetSwimMultiplierForPlayer(player, m_SwimMultiplier.load(std::memory_order_acquire)) && success;
        return success;
    }

    bool PlayerRuntime::QueuePlayerOperation(PlayerAction action, std::function<bool(Player, Ped)> apply)
    {
        if (!apply)
            return false;

        return Runtime::GameRuntime::Get().Enqueue([this, action, apply = std::move(apply)]() mutable {
            const auto player = PlayerNatives::PlayerId();
            const auto ped = PlayerNatives::PlayerPedId();
            if (!player || !ped || *ped == 0)
            {
                RecordAction(action, false);
                return;
            }

            const bool success = apply(*player, *ped);
            RecordAction(action, success);
            if (success)
                static_cast<void>(Refresh(*player, *ped));
        });
    }

    void PlayerRuntime::ProcessPendingModel(Player player) noexcept
    {
        if (m_PendingModel == 0)
            return;

        const Hash model = m_PendingModel;
        const auto loaded = PlayerNatives::HasModelLoaded(model);
        if (!loaded)
        {
            if (Clock::now() >= m_ModelDeadline)
            {
                static_cast<void>(PlayerNatives::SetModelAsNoLongerNeeded(model));
                m_PendingModel = 0;
                RecordAction(PlayerAction::ModelSwap, false);
            }
            return;
        }

        if (*loaded)
        {
            const bool swapped = PlayerNatives::SetPlayerModel(player, model);
            static_cast<void>(PlayerNatives::SetModelAsNoLongerNeeded(model));
            m_PendingModel = 0;

            if (swapped)
            {
                const auto newPed = PlayerNatives::PlayerPedId();
                if (newPed && *newPed != 0)
                {
                    static_cast<void>(PlayerNatives::SetPedDefaultComponentVariation(*newPed));
                    static_cast<void>(ApplyPersistentState(player, *newPed));
                    m_LastPed = 0;
                    m_NextRefresh = {};
                }
            }
            RecordAction(PlayerAction::ModelSwap, swapped);
        }
        else if (Clock::now() >= m_ModelDeadline)
        {
            static_cast<void>(PlayerNatives::SetModelAsNoLongerNeeded(model));
            m_PendingModel = 0;
            RecordAction(PlayerAction::ModelSwap, false);
        }
    }

    void PlayerRuntime::RecordAction(PlayerAction action, bool success) noexcept
    {
        std::scoped_lock lock(m_Mutex);
        m_Snapshot.lastAction = action;
        m_Snapshot.lastActionSucceeded = success;
        m_Snapshot.modelLoadPending = m_PendingModel != 0;
        m_Snapshot.pendingModel = m_PendingModel;
    }

    void PlayerRuntime::ClearSnapshot() noexcept
    {
        std::scoped_lock lock(m_Mutex);
        const PlayerAction lastAction = m_Snapshot.lastAction;
        const bool lastSuccess = m_Snapshot.lastActionSucceeded;
        m_Snapshot = {};
        m_Snapshot.lastAction = lastAction;
        m_Snapshot.lastActionSucceeded = lastSuccess;
        m_Snapshot.invincible = m_Invincible.load(std::memory_order_acquire);
        m_Snapshot.bulletproof = m_Bulletproof.load(std::memory_order_acquire);
        m_Snapshot.invisible = m_Invisible.load(std::memory_order_acquire);
        m_Snapshot.noRagdoll = m_NoRagdoll.load(std::memory_order_acquire);
        m_Snapshot.superJump = m_SuperJump.load(std::memory_order_acquire);
        m_Snapshot.infiniteStamina = m_InfiniteStamina.load(std::memory_order_acquire);
        m_Snapshot.neverWanted = m_NeverWanted.load(std::memory_order_acquire);
        m_Snapshot.policeIgnore = m_PoliceIgnore.load(std::memory_order_acquire);
        m_Snapshot.everyoneIgnore = m_EveryoneIgnore.load(std::memory_order_acquire);
        m_Snapshot.runMultiplier = m_RunMultiplier.load(std::memory_order_acquire);
        m_Snapshot.swimMultiplier = m_SwimMultiplier.load(std::memory_order_acquire);
        m_Snapshot.modelLoadPending = m_PendingModel != 0;
        m_Snapshot.pendingModel = m_PendingModel;
    }
}
