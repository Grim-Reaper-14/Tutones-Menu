#pragma once

#include "WeaponSettings.hpp"

#include <atomic>

namespace Tutones::Game::WeaponFeatures
{
    struct WeaponSnapshot final
    {
        WeaponSettings settings{};
        bool nativeReady{};
        bool pointersResolved{};
        bool aimbotSupported{};
        bool aimForHeadSupported{};
        bool targetDriversSupported{};
        bool releaseDeadTargetSupported{};
        bool running{};
    };

    class WeaponRuntime final
    {
    public:
        static WeaponRuntime& Get() noexcept;

        bool Start();
        void Stop() noexcept;

        [[nodiscard]] bool IsRunning() const noexcept;
        [[nodiscard]] WeaponSnapshot Snapshot() const noexcept;

        void SetInfiniteAmmo(bool enabled) noexcept;
        void SetInfiniteClip(bool enabled) noexcept;
        void SetAimbot(bool enabled) noexcept;
        void SetAimForHead(bool enabled) noexcept;
        void SetTargetDrivers(bool enabled) noexcept;
        void SetReleaseDeadPed(bool enabled) noexcept;
        void SetExplosiveAmmo(bool enabled) noexcept;
        void SetExplosionType(int type) noexcept;
        void SetExplosionDamage(float damage) noexcept;
        void SetExplosionCameraShake(float shake) noexcept;

    private:
        WeaponRuntime() = default;
        ~WeaponRuntime() = default;
        WeaponRuntime(const WeaponRuntime&) = delete;
        WeaponRuntime& operator=(const WeaponRuntime&) = delete;

        bool QueueNextTick();
        void TickOnGameThread() noexcept;
        void ApplyPatchState() noexcept;
        void ApplyNativeState() noexcept;
        void RestorePatches() noexcept;
        void QueueNativeCleanup() noexcept;

        std::atomic<bool> m_Running{false};
        std::atomic<bool> m_InfiniteAmmo{false};
        std::atomic<bool> m_InfiniteClip{false};
        std::atomic<bool> m_Aimbot{false};
        std::atomic<bool> m_AimForHead{true};
        std::atomic<bool> m_TargetDrivers{true};
        std::atomic<bool> m_ReleaseDeadPed{true};
        std::atomic<bool> m_ExplosiveAmmo{false};
        std::atomic<int> m_ExplosionType{18};
        std::atomic<float> m_ExplosionDamage{1.0f};
        std::atomic<float> m_ExplosionCameraShake{0.1f};

        bool m_InfiniteAmmoWasApplied{};
        bool m_InfiniteClipWasApplied{};
    };
}
