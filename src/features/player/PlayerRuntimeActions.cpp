#include "PlayerRuntime.hpp"

#include "../../game/native/NativeInvoker.hpp"

#include <algorithm>
#include <cstdint>

namespace Tutones::Game::PlayerFeatures
{
    namespace
    {
        constexpr int MinComponent = 0;
        constexpr int MaxComponent = 11;

        [[nodiscard]] constexpr bool ValidComponent(int componentId) noexcept
        {
            return componentId >= MinComponent && componentId <= MaxComponent;
        }
    }

    void PlayerRuntime::SetInvincible(bool enabled)
    {
        m_Invincible.store(enabled, std::memory_order_release);
        static_cast<void>(QueuePlayerOperation(PlayerAction::ApplyPersistent, [this](Player player, Ped ped) {
            return ApplyPersistentState(player, ped);
        }));
    }

    void PlayerRuntime::SetInvisible(bool enabled)
    {
        m_Invisible.store(enabled, std::memory_order_release);
        static_cast<void>(QueuePlayerOperation(PlayerAction::ApplyPersistent, [this](Player player, Ped ped) {
            return ApplyPersistentState(player, ped);
        }));
    }

    void PlayerRuntime::SetNoRagdoll(bool enabled)
    {
        m_NoRagdoll.store(enabled, std::memory_order_release);
        static_cast<void>(QueuePlayerOperation(PlayerAction::ApplyPersistent, [this](Player player, Ped ped) {
            return ApplyPersistentState(player, ped);
        }));
    }

    void PlayerRuntime::SetSuperJump(bool enabled) noexcept
    {
        m_SuperJump.store(enabled, std::memory_order_release);
    }

    void PlayerRuntime::SetInfiniteStamina(bool enabled) noexcept
    {
        m_InfiniteStamina.store(enabled, std::memory_order_release);
    }

    void PlayerRuntime::SetKeepPlayerClean(bool enabled) noexcept
    {
        m_KeepPlayerClean.store(enabled, std::memory_order_release);
    }

    void PlayerRuntime::SetDisableCriticalHits(bool enabled)
    {
        m_DisableCriticalHits.store(enabled, std::memory_order_release);
        static_cast<void>(QueuePlayerOperation(PlayerAction::ApplyPersistent, [this](Player player, Ped ped) {
            return ApplyPersistentState(player, ped);
        }));
    }

    void PlayerRuntime::SetStandOnVehicles(bool enabled) noexcept
    {
        m_StandOnVehicles.store(enabled, std::memory_order_release);
    }

    void PlayerRuntime::SetDisableActionMode(bool enabled) noexcept
    {
        m_DisableActionMode.store(enabled, std::memory_order_release);
    }

    void PlayerRuntime::SetInfiniteParachutes(bool enabled) noexcept
    {
        m_InfiniteParachutes.store(enabled, std::memory_order_release);
    }

    void PlayerRuntime::SetMobileRadio(bool enabled)
    {
        m_MobileRadio.store(enabled, std::memory_order_release);
        if (!enabled)
        {
            static_cast<void>(QueuePlayerOperation(PlayerAction::ApplyPersistent, [](Player, Ped) {
                bool success = Native::NativeInvoker::InvokeVoid(
                    Native::NativeId::SetMobilePhoneRadioState, std::int32_t{0});
                success = Native::NativeInvoker::InvokeVoid(
                    Native::NativeId::SetMobileRadioEnabledDuringGameplay, std::int32_t{0}) && success;
                return success;
            }));
        }
    }

    void PlayerRuntime::SetNeverWanted(bool enabled)
    {
        m_NeverWanted.store(enabled, std::memory_order_release);
        if (enabled)
            static_cast<void>(QueueClearWanted());
    }

    void PlayerRuntime::SetPoliceIgnore(bool enabled)
    {
        m_PoliceIgnore.store(enabled, std::memory_order_release);
        static_cast<void>(QueuePlayerOperation(PlayerAction::ApplyPersistent, [this](Player player, Ped ped) {
            return ApplyPersistentState(player, ped);
        }));
    }

    void PlayerRuntime::SetEveryoneIgnore(bool enabled)
    {
        m_EveryoneIgnore.store(enabled, std::memory_order_release);
        static_cast<void>(QueuePlayerOperation(PlayerAction::ApplyPersistent, [this](Player player, Ped ped) {
            return ApplyPersistentState(player, ped);
        }));
    }

    void PlayerRuntime::SetRunMultiplier(float multiplier)
    {
        m_RunMultiplier.store(std::clamp(multiplier, 1.0f, 1.49f), std::memory_order_release);
        static_cast<void>(QueuePlayerOperation(PlayerAction::ApplyPersistent, [this](Player player, Ped ped) {
            return ApplyPersistentState(player, ped);
        }));
    }

    void PlayerRuntime::SetSwimMultiplier(float multiplier)
    {
        m_SwimMultiplier.store(std::clamp(multiplier, 1.0f, 1.49f), std::memory_order_release);
        static_cast<void>(QueuePlayerOperation(PlayerAction::ApplyPersistent, [this](Player player, Ped ped) {
            return ApplyPersistentState(player, ped);
        }));
    }

    bool PlayerRuntime::QueueSetHealth(int health)
    {
        const int requested = std::max(0, health);
        return QueuePlayerOperation(PlayerAction::SetHealth, [requested](Player, Ped ped) {
            return PlayerNatives::SetEntityHealth(ped, requested, 0, 0);
        });
    }

    bool PlayerRuntime::QueueHeal()
    {
        return QueuePlayerOperation(PlayerAction::Heal, [](Player, Ped ped) {
            const auto maxHealth = PlayerNatives::GetEntityMaxHealth(ped);
            return maxHealth && PlayerNatives::SetEntityHealth(ped, *maxHealth, 0, 0);
        });
    }

    bool PlayerRuntime::QueueSuicide()
    {
        // Match Yim's death transition without erasing the user's persisted God Mode preference.
        return QueuePlayerOperation(PlayerAction::Suicide, [](Player, Ped ped) {
            bool success = PlayerNatives::SetEntityInvincible(ped, false, false);
            success = PlayerNatives::SetEntityHealth(ped, 0, 0, 0) && success;
            return success;
        });
    }

    bool PlayerRuntime::QueueClearDamage()
    {
        return QueuePlayerOperation(PlayerAction::ClearDamage, [](Player, Ped ped) {
            bool success = true;
            success = Native::NativeInvoker::InvokeVoid(Native::NativeId::ClearPedBloodDamage, ped) && success;
            success = Native::NativeInvoker::InvokeVoid(Native::NativeId::ClearPedWetness, ped) && success;
            success = Native::NativeInvoker::InvokeVoid(Native::NativeId::ClearPedEnvDirt, ped) && success;
            success = Native::NativeInvoker::InvokeVoid(Native::NativeId::ResetPedVisibleDamage, ped) && success;
            return success;
        });
    }

    bool PlayerRuntime::QueueSetArmor(int armor)
    {
        const int requested = std::clamp(armor, 0, 100);
        return QueuePlayerOperation(PlayerAction::SetArmor, [requested](Player, Ped ped) {
            return PlayerNatives::SetPedArmour(ped, requested);
        });
    }

    bool PlayerRuntime::QueueSetWantedLevel(int wantedLevel)
    {
        const int requested = std::clamp(wantedLevel, 0, 5);
        return QueuePlayerOperation(PlayerAction::SetWanted, [requested](Player player, Ped) {
            if (!PlayerNatives::SetPlayerWantedLevel(player, requested, false))
                return false;
            return PlayerNatives::SetPlayerWantedLevelNow(player, false);
        });
    }

    bool PlayerRuntime::QueueClearWanted()
    {
        return QueuePlayerOperation(PlayerAction::ClearWanted, [](Player player, Ped) {
            return PlayerNatives::ClearPlayerWantedLevel(player);
        });
    }

    bool PlayerRuntime::QueueModelByName(std::string modelName)
    {
        if (modelName.empty())
            return false;

        const Hash model = Joaat(modelName);
        return QueuePlayerOperation(PlayerAction::ModelRequest, [this, model](Player, Ped) {
            const auto inCdImage = PlayerNatives::IsModelInCdimage(model);
            const auto valid = PlayerNatives::IsModelValid(model);
            const auto pedModel = PlayerNatives::IsModelAPed(model);
            if (!inCdImage || !valid || !pedModel || !*inCdImage || !*valid || !*pedModel)
                return false;

            if (m_PendingModel != 0)
                static_cast<void>(PlayerNatives::SetModelAsNoLongerNeeded(m_PendingModel));

            if (!PlayerNatives::RequestModel(model))
                return false;

            m_PendingModel = model;
            m_ModelDeadline = Clock::now() + ModelLoadTimeout;
            return true;
        });
    }

    bool PlayerRuntime::QueueSetComponent(int componentId, int drawableId, int textureId, int paletteId)
    {
        if (!ValidComponent(componentId) || drawableId < 0 || textureId < 0 || paletteId < 0 || paletteId > 3)
            return false;

        return QueuePlayerOperation(PlayerAction::SetComponent, [componentId, drawableId, textureId, paletteId](Player, Ped ped) {
            const auto drawableCount = PlayerNatives::GetNumberOfPedDrawableVariations(ped, componentId);
            if (!drawableCount || drawableId >= *drawableCount)
                return false;
            const auto textureCount = PlayerNatives::GetNumberOfPedTextureVariations(ped, componentId, drawableId);
            if (!textureCount || textureId >= *textureCount)
                return false;
            return PlayerNatives::SetPedComponentVariation(ped, componentId, drawableId, textureId, paletteId);
        });
    }

    bool PlayerRuntime::QueueDefaultComponents()
    {
        return QueuePlayerOperation(PlayerAction::DefaultComponents, [](Player, Ped ped) {
            return PlayerNatives::SetPedDefaultComponentVariation(ped);
        });
    }

    bool PlayerRuntime::QueueRandomComponents()
    {
        return QueuePlayerOperation(PlayerAction::RandomComponents, [](Player, Ped ped) {
            return PlayerNatives::SetPedRandomComponentVariation(ped, 0);
        });
    }

}
