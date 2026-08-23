#include "PlayerRuntime.hpp"

#include "../../game/native/NativeInvoker.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <thread>

namespace Tutones::Game::PlayerFeatures
{
    namespace
    {
        constexpr int MinComponent = 0;
        constexpr int MaxComponent = 11;
        constexpr float FullOxygenPercentage = 100.0f;
        constexpr float InfiniteUnderwaterSeconds = 2147483647.0f;

        bool SetPedMaxTimeUnderwater(Ped ped, float seconds) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::SetPedMaxTimeUnderwater, ped, seconds);
        }

        bool SetPlayerUnderwaterTimeRemaining(Player player, float percentage) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::SetPlayerUnderwaterTimeRemaining, player, percentage);
        }

        bool ClearPlayerDamage(Ped ped) noexcept
        {
            bool success = true;
            success = Native::NativeInvoker::InvokeVoid(Native::NativeId::ClearPedBloodDamage, ped) && success;
            success = Native::NativeInvoker::InvokeVoid(Native::NativeId::ClearPedWetness, ped) && success;
            success = Native::NativeInvoker::InvokeVoid(Native::NativeId::ClearPedEnvDirt, ped) && success;
            success = Native::NativeInvoker::InvokeVoid(Native::NativeId::ResetPedVisibleDamage, ped) && success;
            return success;
        }

        bool SetPedResetFlag(Ped ped, int flag) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetPedResetFlag, ped, flag, std::int32_t{1});
        }

        bool SetBlockingOfNonTemporaryEventsForAmbientPedsThisFrame(bool enabled) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetBlockingOfNonTemporaryEventsForAmbientPedsThisFrame,
                static_cast<std::int32_t>(enabled));
        }

        bool SetPlayerHasReserveParachute(Player player) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::SetPlayerHasReserveParachute, player);
        }

        bool HasPedGotWeapon(Ped ped, Hash weapon) noexcept
        {
            const auto result = Native::NativeInvoker::Invoke<std::int32_t>(
                Native::NativeId::HasPedGotWeapon, ped, weapon, std::int32_t{0});
            return result && *result != 0;
        }

        bool GiveWeaponToPed(Ped ped, Hash weapon, int ammo = 1) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(
                Native::NativeId::GiveWeaponToPed,
                ped,
                weapon,
                ammo,
                std::int32_t{0},
                std::int32_t{0});
        }

        bool SetMobileRadioState(bool enabled) noexcept
        {
            const std::int32_t state = enabled ? 1 : 0;
            bool success = Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetMobilePhoneRadioState, state);
            success = Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetMobileRadioEnabledDuringGameplay, state) && success;
            return success;
        }
    }

    PlayerRuntime& PlayerRuntime::Get() noexcept
    {
        static PlayerRuntime instance;
        return instance;
    }

    bool PlayerRuntime::Start()
    {
        bool expected = false;
        if (!m_Running.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            return true;

        if (QueueNextTick())
            return true;

        m_Running.store(false, std::memory_order_release);
        return false;
    }

    void PlayerRuntime::Stop() noexcept
    {
        if (!m_Running.exchange(false, std::memory_order_acq_rel))
            return;

        // Stop owns every reversible player state. Preferences are saved by Application
        // before Stop() is called, so resetting these atomics prevents stale state from
        // leaking into a later runtime while preserving the user's persisted settings.
        m_Invincible.store(false, std::memory_order_release);
        m_Bulletproof.store(false, std::memory_order_release);
        m_AquaLungs.store(false, std::memory_order_release);
        m_InfiniteOxygen.store(false, std::memory_order_release);
        m_Invisible.store(false, std::memory_order_release);
        m_NoRagdoll.store(false, std::memory_order_release);
        m_SuperJump.store(false, std::memory_order_release);
        m_InfiniteStamina.store(false, std::memory_order_release);
        m_KeepPlayerClean.store(false, std::memory_order_release);
        m_DisableCriticalHits.store(false, std::memory_order_release);
        m_StandOnVehicles.store(false, std::memory_order_release);
        m_DisableActionMode.store(false, std::memory_order_release);
        m_InfiniteParachutes.store(false, std::memory_order_release);
        m_MobileRadio.store(false, std::memory_order_release);
        m_NeverWanted.store(false, std::memory_order_release);
        m_PoliceIgnore.store(false, std::memory_order_release);
        m_EveryoneIgnore.store(false, std::memory_order_release);
        m_RunMultiplier.store(1.0f, std::memory_order_release);
        m_SwimMultiplier.store(1.0f, std::memory_order_release);

        const auto cleanup = [this] {
            const auto player = PlayerNatives::PlayerId();
            const auto ped = PlayerNatives::PlayerPedId();

            if (ped && *ped != 0)
            {
                static_cast<void>(PlayerNatives::SetEntityInvincible(*ped, false, false));
                static_cast<void>(Native::NativeInvoker::InvokeVoid(
                    Native::NativeId::SetEntityProofs,
                    *ped,
                    std::int32_t{0},
                    std::int32_t{0},
                    std::int32_t{0},
                    std::int32_t{0},
                    std::int32_t{0},
                    std::int32_t{0},
                    std::int32_t{0},
                    std::int32_t{0}));
                static_cast<void>(PlayerNatives::SetEntityVisible(*ped, true, false));
                static_cast<void>(PlayerNatives::SetPedCanRagdoll(*ped, true));
                static_cast<void>(Native::NativeInvoker::InvokeVoid(
                    Native::NativeId::SetPedSuffersCriticalHits, *ped, std::int32_t{1}));
                static_cast<void>(SetPedMaxTimeUnderwater(*ped, -1.0f));
            }

            static_cast<void>(SetMobileRadioState(false));

            if (player)
            {
                static_cast<void>(PlayerNatives::SetPoliceIgnorePlayer(*player, false));
                static_cast<void>(PlayerNatives::SetEveryoneIgnorePlayer(*player, false));
                static_cast<void>(PlayerNatives::SetRunSprintMultiplierForPlayer(*player, 1.0f));
                static_cast<void>(PlayerNatives::SetSwimMultiplierForPlayer(*player, 1.0f));
            }

            if (m_PendingModel != 0)
            {
                static_cast<void>(PlayerNatives::SetModelAsNoLongerNeeded(m_PendingModel));
                m_PendingModel = 0;
                m_ModelDeadline = {};
            }
        };

        auto& runtime = Runtime::GameRuntime::Get();
        if (runtime.IsOnGameThread())
        {
            cleanup();
        }
        else if (runtime.IsInitialized())
        {
            const auto cleaned = std::make_shared<std::atomic<bool>>(false);
            if (runtime.Enqueue([cleanup, cleaned] {
                    cleanup();
                    cleaned->store(true, std::memory_order_release);
                }))
            {
                const auto deadline = Clock::now() + std::chrono::milliseconds(250);
                while (!cleaned->load(std::memory_order_acquire) && Clock::now() < deadline)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        m_LastPed = 0;
        m_NextRefresh = {};
        ClearSnapshot();
    }

    bool PlayerRuntime::IsRunning() const noexcept
    {
        return m_Running.load(std::memory_order_acquire);
    }

    PlayerSnapshot PlayerRuntime::Snapshot() const noexcept
    {
        std::scoped_lock lock(m_Mutex);
        return m_Snapshot;
    }

    void PlayerRuntime::SetObservedComponent(int componentId, int drawableId) noexcept
    {
        m_ObservedComponent.store(std::clamp(componentId, MinComponent, MaxComponent), std::memory_order_release);
        m_ObservedDrawable.store(drawableId, std::memory_order_release);
    }

    void PlayerRuntime::SetAquaLungs(bool enabled) noexcept
    {
        m_AquaLungs.store(enabled, std::memory_order_release);
    }

    void PlayerRuntime::SetInfiniteOxygen(bool enabled)
    {
        const bool previous = m_InfiniteOxygen.exchange(enabled, std::memory_order_acq_rel);
        if (previous == enabled)
            return;

        if (!enabled)
        {
            static_cast<void>(QueuePlayerOperation(PlayerAction::ApplyPersistent, [this](Player player, Ped ped) {
                bool success = SetPedMaxTimeUnderwater(ped, -1.0f);
                if (m_AquaLungs.load(std::memory_order_acquire))
                    success = SetPlayerUnderwaterTimeRemaining(player, FullOxygenPercentage) && success;
                return success;
            }));
        }
    }

    Hash PlayerRuntime::Joaat(std::string_view value) noexcept
    {
        std::uint32_t hash = 0;
        for (unsigned char character : value)
        {
            if (character >= 'A' && character <= 'Z')
                character = static_cast<unsigned char>(character + ('a' - 'A'));
            hash += character;
            hash += hash << 10;
            hash ^= hash >> 6;
        }
        hash += hash << 3;
        hash ^= hash >> 11;
        hash += hash << 15;
        return hash;
    }

    bool PlayerRuntime::QueueNextTick()
    {
        if (!IsRunning())
            return false;
        return Runtime::GameRuntime::Get().Enqueue([this] { TickOnGameThread(); });
    }

    void PlayerRuntime::TickOnGameThread() noexcept
    {
        if (!IsRunning())
            return;

        const auto player = PlayerNatives::PlayerId();
        const auto ped = PlayerNatives::PlayerPedId();
        if (!player || !ped || *ped == 0)
        {
            m_LastPed = 0;
            m_NextRefresh = {};
            ClearSnapshot();
        }
        else
        {
            // Maintain God Mode continuously while the local ped is alive, but clear
            // invincibility while dead so GTA can complete death/respawn state normally.
            if (m_Invincible.load(std::memory_order_acquire))
            {
                const auto dead = PlayerNatives::IsEntityDead(*ped, true);
                if (dead)
                    static_cast<void>(PlayerNatives::SetEntityInvincible(*ped, !*dead, false));
            }

            if (m_SuperJump.load(std::memory_order_acquire))
                static_cast<void>(PlayerNatives::SetSuperJumpThisFrame(*player));
            if (m_InfiniteStamina.load(std::memory_order_acquire))
                static_cast<void>(PlayerNatives::RestorePlayerStamina(*player, 1.0f));

            const bool infiniteOxygen = m_InfiniteOxygen.load(std::memory_order_acquire);
            if (infiniteOxygen)
                static_cast<void>(SetPedMaxTimeUnderwater(*ped, InfiniteUnderwaterSeconds));
            else if (m_AquaLungs.load(std::memory_order_acquire))
                static_cast<void>(SetPlayerUnderwaterTimeRemaining(*player, FullOxygenPercentage));

            if (m_KeepPlayerClean.load(std::memory_order_acquire))
                static_cast<void>(ClearPlayerDamage(*ped));
            if (m_DisableCriticalHits.load(std::memory_order_acquire))
                static_cast<void>(Native::NativeInvoker::InvokeVoid(
                    Native::NativeId::SetPedSuffersCriticalHits, *ped, std::int32_t{0}));
            if (m_StandOnVehicles.load(std::memory_order_acquire))
                static_cast<void>(SetPedResetFlag(*ped, 274));
            if (m_DisableActionMode.load(std::memory_order_acquire))
                static_cast<void>(SetPedResetFlag(*ped, 200));
            if (m_InfiniteParachutes.load(std::memory_order_acquire))
            {
                static_cast<void>(SetPlayerHasReserveParachute(*player));
                const Hash parachute = Joaat("GADGET_PARACHUTE");
                if (!HasPedGotWeapon(*ped, parachute))
                    static_cast<void>(GiveWeaponToPed(*ped, parachute));
            }
            if (m_MobileRadio.load(std::memory_order_acquire))
                static_cast<void>(SetMobileRadioState(true));

            if (m_NeverWanted.load(std::memory_order_acquire))
                static_cast<void>(PlayerNatives::ClearPlayerWantedLevel(*player));

            // GTA can reset these relationship/dispatch states during normal world and
            // session processing, so treat them as real per-frame features. Police Ignore
            // and Ignore Everyone remain independent toggles.
            if (m_PoliceIgnore.load(std::memory_order_acquire))
                static_cast<void>(PlayerNatives::SetPoliceIgnorePlayer(*player, true));

            if (m_EveryoneIgnore.load(std::memory_order_acquire))
            {
                static_cast<void>(PlayerNatives::SetEveryoneIgnorePlayer(*player, true));
                // Match the stronger Yim-style peds-ignore behavior: stop ambient peds
                // reacting to non-temporary events this frame and keep reset flag 124 set.
                static_cast<void>(SetBlockingOfNonTemporaryEventsForAmbientPedsThisFrame(true));
                static_cast<void>(SetPedResetFlag(*ped, 124));
            }

            ProcessPendingModel(*player);

            const auto currentPed = PlayerNatives::PlayerPedId();
            if (currentPed && *currentPed != 0)
            {
                const auto now = Clock::now();
                const int observed = m_ObservedComponent.load(std::memory_order_acquire);
                const int observedDrawable = m_ObservedDrawable.load(std::memory_order_acquire);
                if (*currentPed != m_LastPed
                    || observed != m_LastObservedComponent
                    || observedDrawable != m_LastObservedDrawable
                    || m_NextRefresh == Clock::time_point{}
                    || now >= m_NextRefresh)
                {
                    m_LastPed = *currentPed;
                    m_LastObservedComponent = observed;
                    m_LastObservedDrawable = observedDrawable;
                    static_cast<void>(ApplyPersistentState(*player, *currentPed));
                    static_cast<void>(Refresh(*player, *currentPed));
                    m_NextRefresh = now + RefreshInterval;
                }
            }
        }

        if (IsRunning() && !QueueNextTick())
            Stop();
    }
}
