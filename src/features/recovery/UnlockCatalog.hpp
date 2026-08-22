#pragma once

#include <array>
#include <cstddef>
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

    enum class PackedUnlockPack : std::uint8_t
    {
        CasinoHeistTattoos,
        LosSantosTunersTattoos,
    };

    struct PackedBoolUnlockEntry final
    {
        const char* label{};
        int code{-1};
        PackedUnlockPack pack{PackedUnlockPack::CasinoHeistTattoos};
    };

    // Direct item-to-packed-code mappings verified against GTA V Enhanced tattoo_shop.c.
    // These are intentionally individual codes rather than broad legacy ranges. Several
    // tattoos have normal gameplay requirements as well; setting the mapped bool is the
    // explicit fallback condition used by the current Enhanced tattoo shop script.
    inline constexpr std::array<PackedBoolUnlockEntry, 34> EnhancedPackedBoolUnlocks{{
        {"Casino Heist Tee 001", 28199, PackedUnlockPack::CasinoHeistTattoos},
        {"Casino Heist Tee 000", 28200, PackedUnlockPack::CasinoHeistTattoos},
        {"Casino Heist Tee 007 / 008 / 009", 28204, PackedUnlockPack::CasinoHeistTattoos},
        {"Casino Heist Tee 004", 28206, PackedUnlockPack::CasinoHeistTattoos},
        {"Casino Heist Tee 005", 28207, PackedUnlockPack::CasinoHeistTattoos},
        {"Casino Heist Tee 006", 28212, PackedUnlockPack::CasinoHeistTattoos},
        {"Casino Heist Tee 002", 28249, PackedUnlockPack::CasinoHeistTattoos},
        {"Casino Heist Tee 013", 28183, PackedUnlockPack::CasinoHeistTattoos},
        {"Casino Heist Tee 014", 28182, PackedUnlockPack::CasinoHeistTattoos},
        {"Casino Heist Tee 015", 28184, PackedUnlockPack::CasinoHeistTattoos},
        {"Casino Heist Tee 016", 28181, PackedUnlockPack::CasinoHeistTattoos},
        {"Casino Heist Tee 017", 28178, PackedUnlockPack::CasinoHeistTattoos},
        {"Casino Heist Tee 018", 28177, PackedUnlockPack::CasinoHeistTattoos},
        {"Casino Heist Tee 019", 28176, PackedUnlockPack::CasinoHeistTattoos},
        {"Casino Heist Tee 020", 28180, PackedUnlockPack::CasinoHeistTattoos},
        {"Casino Heist Tee 021", 28179, PackedUnlockPack::CasinoHeistTattoos},
        {"Casino Heist Tee 022", 28221, PackedUnlockPack::CasinoHeistTattoos},
        {"Casino Heist Tee 023", 28191, PackedUnlockPack::CasinoHeistTattoos},
        {"Casino Heist Tee 011", 28190, PackedUnlockPack::CasinoHeistTattoos},
        {"Casino Heist Tee 012", 28189, PackedUnlockPack::CasinoHeistTattoos},
        {"Casino Heist Tee 010", 28222, PackedUnlockPack::CasinoHeistTattoos},

        {"Tuners Tee 000", 31760, PackedUnlockPack::LosSantosTunersTattoos},
        {"Tuners Tee 002", 31761, PackedUnlockPack::LosSantosTunersTattoos},
        {"Tuners Tee 003", 31762, PackedUnlockPack::LosSantosTunersTattoos},
        {"Tuners Tee 005", 31763, PackedUnlockPack::LosSantosTunersTattoos},
        {"Tuners Tee 006", 31764, PackedUnlockPack::LosSantosTunersTattoos},
        {"Tuners Tee 008", 31768, PackedUnlockPack::LosSantosTunersTattoos},
        {"Tuners Tee 010", 31769, PackedUnlockPack::LosSantosTunersTattoos},
        {"Tuners Tee 011", 31770, PackedUnlockPack::LosSantosTunersTattoos},
        {"Tuners Tee 012", 31771, PackedUnlockPack::LosSantosTunersTattoos},
        {"Tuners Tee 013", 31772, PackedUnlockPack::LosSantosTunersTattoos},
        {"Tuners Tee 014", 31773, PackedUnlockPack::LosSantosTunersTattoos},
        {"Tuners Tee 015", 31774, PackedUnlockPack::LosSantosTunersTattoos},
        {"Tuners Tee 016", 31775, PackedUnlockPack::LosSantosTunersTattoos},
    }};

    inline constexpr std::size_t EnhancedPackedBoolUnlockCount = EnhancedPackedBoolUnlocks.size();
}
