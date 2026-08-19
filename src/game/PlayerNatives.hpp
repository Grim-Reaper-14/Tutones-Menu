#pragma once

#include "Natives.hpp"

#include <cstdint>
#include <optional>

namespace Tutones::Game
{
    using Player = std::int32_t;

    namespace PlayerNatives
    {
        [[nodiscard]] inline std::optional<Player> PlayerId() noexcept
        {
            return Native::NativeInvoker::Invoke<Player>(Native::NativeId::PlayerId);
        }

        [[nodiscard]] inline std::optional<Ped> PlayerPedId() noexcept
        {
            return Natives::PlayerPedId();
        }

        [[nodiscard]] inline std::optional<Hash> GetEntityModel(Entity entity) noexcept
        {
            return Natives::GetEntityModel(entity);
        }

        [[nodiscard]] inline std::optional<int> GetEntityHealth(Entity entity) noexcept
        {
            return Native::NativeInvoker::Invoke<int>(Native::NativeId::GetEntityHealth, entity);
        }

        [[nodiscard]] inline std::optional<int> GetEntityMaxHealth(Entity entity) noexcept
        {
            return Native::NativeInvoker::Invoke<int>(Native::NativeId::GetEntityMaxHealth, entity);
        }

        [[nodiscard]] inline std::optional<bool> IsEntityDead(Entity entity, bool p1 = true) noexcept
        {
            const auto result = Native::NativeInvoker::Invoke<std::int32_t>(
                Native::NativeId::IsEntityDead,
                entity,
                static_cast<std::int32_t>(p1));
            return result ? std::optional<bool>(*result != 0) : std::nullopt;
        }

        inline bool SetEntityHealth(Entity entity, int health, Entity instigator, Hash weaponType) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::SetEntityHealth, entity, health, instigator, weaponType);
        }

        inline bool SetEntityInvincible(Entity entity, bool enabled, bool dontResetOnCleanup = false) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetEntityInvincible,
                entity,
                static_cast<std::int32_t>(enabled),
                static_cast<std::int32_t>(dontResetOnCleanup));
        }

        inline bool SetEntityVisible(Entity entity, bool visible, bool p2 = false) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetEntityVisible,
                entity,
                static_cast<std::int32_t>(visible),
                static_cast<std::int32_t>(p2));
        }

        [[nodiscard]] inline std::optional<int> GetPedArmour(Ped ped) noexcept
        {
            return Native::NativeInvoker::Invoke<int>(Native::NativeId::GetPedArmour, ped);
        }

        inline bool SetPedArmour(Ped ped, int amount) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::SetPedArmour, ped, amount);
        }

        inline bool SetPedCanRagdoll(Ped ped, bool enabled) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::SetPedCanRagdoll, ped, static_cast<std::int32_t>(enabled));
        }

        [[nodiscard]] inline std::optional<int> GetPlayerWantedLevel(Player player) noexcept
        {
            return Native::NativeInvoker::Invoke<int>(Native::NativeId::GetPlayerWantedLevel, player);
        }

        inline bool SetPlayerWantedLevel(Player player, int wantedLevel, bool disableNoMission) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetPlayerWantedLevel,
                player,
                wantedLevel,
                static_cast<std::int32_t>(disableNoMission));
        }

        inline bool SetPlayerWantedLevelNow(Player player, bool p1 = false) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::SetPlayerWantedLevelNow, player, static_cast<std::int32_t>(p1));
        }

        inline bool ClearPlayerWantedLevel(Player player) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::ClearPlayerWantedLevel, player);
        }

        inline bool SetPoliceIgnorePlayer(Player player, bool enabled) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::SetPoliceIgnorePlayer, player, static_cast<std::int32_t>(enabled));
        }

        inline bool SetEveryoneIgnorePlayer(Player player, bool enabled) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::SetEveryoneIgnorePlayer, player, static_cast<std::int32_t>(enabled));
        }

        inline bool SetSuperJumpThisFrame(Player player) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::SetSuperJumpThisFrame, player);
        }

        inline bool SetRunSprintMultiplierForPlayer(Player player, float multiplier) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::SetRunSprintMultiplierForPlayer, player, multiplier);
        }

        inline bool SetSwimMultiplierForPlayer(Player player, float multiplier) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::SetSwimMultiplierForPlayer, player, multiplier);
        }

        inline bool RestorePlayerStamina(Player player, float amount) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::RestorePlayerStamina, player, amount);
        }

        [[nodiscard]] inline std::optional<bool> IsModelInCdimage(Hash model) noexcept
        {
            const auto result = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::IsModelInCdimage, model);
            return result ? std::optional<bool>(*result != 0) : std::nullopt;
        }

        [[nodiscard]] inline std::optional<bool> IsModelValid(Hash model) noexcept
        {
            const auto result = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::IsModelValid, model);
            return result ? std::optional<bool>(*result != 0) : std::nullopt;
        }

        [[nodiscard]] inline std::optional<bool> IsModelAPed(Hash model) noexcept
        {
            const auto result = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::IsModelAPed, model);
            return result ? std::optional<bool>(*result != 0) : std::nullopt;
        }

        inline bool RequestModel(Hash model) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::RequestModel, model);
        }

        [[nodiscard]] inline std::optional<bool> HasModelLoaded(Hash model) noexcept
        {
            const auto result = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::HasModelLoaded, model);
            return result ? std::optional<bool>(*result != 0) : std::nullopt;
        }

        inline bool SetModelAsNoLongerNeeded(Hash model) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::SetModelAsNoLongerNeeded, model);
        }

        inline bool SetPlayerModel(Player player, Hash model) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::SetPlayerModel, player, model);
        }

        [[nodiscard]] inline std::optional<int> GetPedDrawableVariation(Ped ped, int componentId) noexcept
        {
            return Native::NativeInvoker::Invoke<int>(Native::NativeId::GetPedDrawableVariation, ped, componentId);
        }

        [[nodiscard]] inline std::optional<int> GetNumberOfPedDrawableVariations(Ped ped, int componentId) noexcept
        {
            return Native::NativeInvoker::Invoke<int>(Native::NativeId::GetNumberOfPedDrawableVariations, ped, componentId);
        }

        [[nodiscard]] inline std::optional<int> GetPedTextureVariation(Ped ped, int componentId) noexcept
        {
            return Native::NativeInvoker::Invoke<int>(Native::NativeId::GetPedTextureVariation, ped, componentId);
        }

        [[nodiscard]] inline std::optional<int> GetNumberOfPedTextureVariations(Ped ped, int componentId, int drawableId) noexcept
        {
            return Native::NativeInvoker::Invoke<int>(Native::NativeId::GetNumberOfPedTextureVariations, ped, componentId, drawableId);
        }

        [[nodiscard]] inline std::optional<int> GetPedPaletteVariation(Ped ped, int componentId) noexcept
        {
            return Native::NativeInvoker::Invoke<int>(Native::NativeId::GetPedPaletteVariation, ped, componentId);
        }

        inline bool SetPedComponentVariation(Ped ped, int componentId, int drawableId, int textureId, int paletteId) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetPedComponentVariation,
                ped,
                componentId,
                drawableId,
                textureId,
                paletteId);
        }

        inline bool SetPedRandomComponentVariation(Ped ped, int p1 = 0) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::SetPedRandomComponentVariation, ped, p1);
        }

        inline bool SetPedDefaultComponentVariation(Ped ped) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::SetPedDefaultComponentVariation, ped);
        }
    }
}
