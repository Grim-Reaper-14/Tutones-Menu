#include "PlayerRuntime.hpp"

#include "../../game/native/NativeInvoker.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <algorithm>
#include <cstdint>

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

        const bool restoreCriticalHits = m_DisableCriticalHits.exchange(false, std::memory_order_acq_rel);
        const bool disableMobileRadio = m_MobileRadio.exchange(false, std::memory_order_acq_rel);
        if (!restoreCriticalHits && !disableMobileRadio)
            return;

        static_cast<void>(Runtime::GameRuntime::Get().Enqueue([restoreCriticalHits, disableMobileRadio] {
            const auto ped = PlayerNatives::PlayerPedId();
            if (restoreCriticalHits && ped && *ped != 0)
            {
                static_cast<void>(Native::NativeInvoker::InvokeVoid(
                    Native::NativeId::SetPedSuffersCriticalHits, *ped, std::int32_t{1}));
            }
            if (disableMobileRadio)
            {
                static_cast<void>(Native::NativeInvoker::InvokeVoid(
                    Native::NativeId::SetMobilePhoneRadioState, std::int32_t{0}));
                static_cast<void>(Native::NativeInvoker::InvokeVoid(
                    Native::NativeId::SetMobileRadioEnabledDuringGameplay, std::int32_t{0}));
            }
        }));
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
            m_Running.store(false, std::memory_order_release);
    }
}
