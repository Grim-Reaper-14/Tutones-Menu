#pragma once

namespace Tutones::Game::WeaponFeatures
{
    struct WeaponSettings final
    {
        bool infiniteAmmo{};
        bool infiniteClip{};
        bool aimbot{};
        bool aimForHead{true};
        bool targetDrivers{true};
        bool releaseDeadPed{true};
        bool explosiveAmmo{};
        int explosionType{18};
        float explosionDamage{1.0f};
        float explosionCameraShake{0.1f};
    };
}
