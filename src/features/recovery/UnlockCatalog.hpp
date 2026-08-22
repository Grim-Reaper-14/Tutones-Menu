#pragma once

#include <array>
#include <cstdint>

namespace Tutones::Game::Recovery
{
    enum class UnlockCategory : std::uint8_t
    {
        Activities,
        Weapons,
        WeaponUpgrades,
        VehiclePaints,
    };

    struct RankUnlockEntry final
    {
        const char* label{};
        int minimumRank{};
        UnlockCategory category{UnlockCategory::Activities};
    };

    // Verified from GTA V Enhanced 1.73 mp_unlocks.c. Keep this catalog intentionally
    // small and explicit: entries are added only after their current Enhanced condition
    // has been checked, instead of importing legacy unlock tables blindly.
    inline constexpr std::array<RankUnlockEntry, 18> EnhancedRankUnlocks{{
        {"Stunt Jumps", 2, UnlockCategory::Activities},
        {"One On One Deathmatch", 3, UnlockCategory::Activities},
        {"Shooting Range", 3, UnlockCategory::Activities},
        {"Nightstick", 3, UnlockCategory::Weapons},
        {"Pistol Extended Clip", 3, UnlockCategory::WeaponUpgrades},
        {"Pistol Flashlight", 4, UnlockCategory::WeaponUpgrades},
        {"Movies", 5, UnlockCategory::Activities},
        {"Micro SMG", 5, UnlockCategory::Weapons},
        {"Pistol Suppressor", 5, UnlockCategory::WeaponUpgrades},
        {"Arm Wrestling", 6, UnlockCategory::Activities},
        {"Darts", 6, UnlockCategory::Activities},
        {"Golf", 6, UnlockCategory::Activities},
        {"San Andreas Flight School", 6, UnlockCategory::Activities},
        {"Strip Club", 6, UnlockCategory::Activities},
        {"Tennis", 6, UnlockCategory::Activities},
        {"Micro SMG Extended Clip", 6, UnlockCategory::WeaponUpgrades},
        {"Micro SMG Flashlight", 7, UnlockCategory::WeaponUpgrades},
        {"Anthracite Black Paint", 7, UnlockCategory::VehiclePaints},
    }};

    inline constexpr int HighestMappedRank = 7;
}
