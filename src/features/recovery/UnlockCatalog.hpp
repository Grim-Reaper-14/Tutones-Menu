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

    // Verified from GTA V Enhanced 1.73 mp_unlocks.c. Entries are kept explicit so the
    // Recovery page reflects current Enhanced rank gates rather than importing stale
    // Legacy unlock tables. Vehicle entries below are limited to paint unlocks so the
    // existing Recovery category labels remain accurate.
    inline constexpr auto EnhancedRankUnlocks = std::to_array<RankUnlockEntry>({
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
        {"Black Steel Paint", 7, UnlockCategory::VehiclePaints},
        {"Carbon Black Paint", 7, UnlockCategory::VehiclePaints},
        {"Graphite Paint", 7, UnlockCategory::VehiclePaints},
        {"Simeon's Export Requests", 8, UnlockCategory::Activities},
        {"Combat Pistol", 8, UnlockCategory::Weapons},
        {"Combat Pistol Extended Clip", 9, UnlockCategory::WeaponUpgrades},
        {"Micro SMG Scope", 9, UnlockCategory::WeaponUpgrades},
        {"Bluish Silver Paint", 9, UnlockCategory::VehiclePaints},
        {"Midnight Silver Paint", 9, UnlockCategory::VehiclePaints},
        {"Rolled Steel Paint", 9, UnlockCategory::VehiclePaints},
        {"Shadow Silver Paint", 9, UnlockCategory::VehiclePaints},
        {"Bounties", 10, UnlockCategory::Activities},
        {"Parachuting", 11, UnlockCategory::Activities},
        {"SMG", 11, UnlockCategory::Weapons},
        {"Combat Pistol Flashlight", 11, UnlockCategory::WeaponUpgrades},
        {"Blaze Red Paint", 11, UnlockCategory::VehiclePaints},
        {"Formula Red Paint", 11, UnlockCategory::VehiclePaints},
        {"Grace Red Paint", 11, UnlockCategory::VehiclePaints},
        {"Torino Red Paint", 11, UnlockCategory::VehiclePaints},
        {"Ammo Drop", 12, UnlockCategory::Activities},
        {"Cops Turn a Blind Eye", 12, UnlockCategory::Activities},
        {"Combat Pistol Suppressor", 12, UnlockCategory::WeaponUpgrades},
        {"SMG Extended Clip", 12, UnlockCategory::WeaponUpgrades},
        {"SMG Flashlight", 13, UnlockCategory::WeaponUpgrades},
        {"Bright Orange Paint", 13, UnlockCategory::VehiclePaints},
        {"Pfister Pink Paint", 13, UnlockCategory::VehiclePaints},
        {"Salmon Pink Paint", 13, UnlockCategory::VehiclePaints},
        {"Sunset Red Paint", 13, UnlockCategory::VehiclePaints},
        {"SMG Scope", 14, UnlockCategory::WeaponUpgrades},
        {"Locate a Car", 15, UnlockCategory::Activities},
        {"SMG Suppressor", 15, UnlockCategory::WeaponUpgrades},
        {"Bronze Paint", 15, UnlockCategory::VehiclePaints},
        {"Dark Green Paint", 15, UnlockCategory::VehiclePaints},
        {"Sea Green Paint", 15, UnlockCategory::VehiclePaints},
        {"Yellow Paint", 15, UnlockCategory::VehiclePaints},
        {"Bull Shark Testosterone", 17, UnlockCategory::Activities},
        {"Pump Shotgun", 17, UnlockCategory::Weapons},
        {"Dark Blue Paint", 17, UnlockCategory::VehiclePaints},
        {"Galaxy Blue Paint", 17, UnlockCategory::VehiclePaints},
        {"Mariner Blue Paint", 17, UnlockCategory::VehiclePaints},
        {"Saxon Blue Paint", 17, UnlockCategory::VehiclePaints},
        {"Gang Attacks", 18, UnlockCategory::Activities},
        {"Pump Shotgun Flashlight", 18, UnlockCategory::WeaponUpgrades},
        {"Pump Shotgun Suppressor", 19, UnlockCategory::WeaponUpgrades},
        {"Diamond Blue Paint", 19, UnlockCategory::VehiclePaints},
        {"Nautical Blue Paint", 19, UnlockCategory::VehiclePaints},
        {"Racing Blue Paint", 19, UnlockCategory::VehiclePaints},
        {"Surf Blue Paint", 19, UnlockCategory::VehiclePaints},
        {"Maximum Health Increased", 20, UnlockCategory::Activities},
        {"Health Regeneration Increased", 20, UnlockCategory::Activities},
        {"Backup Buzzard Attack Chopper", 20, UnlockCategory::Activities},
        {"Jerry Can", 20, UnlockCategory::Weapons},
        {"Remove Wanted Level", 21, UnlockCategory::Activities},
        {"Sniper Rifle", 21, UnlockCategory::Weapons},
        {"Chocolate Brown Paint", 21, UnlockCategory::VehiclePaints},
        {"Feltzer Brown Paint", 21, UnlockCategory::VehiclePaints},
        {"Maple Brown Paint", 21, UnlockCategory::VehiclePaints},
        {"Sienna Brown Paint", 21, UnlockCategory::VehiclePaints},
        {"Sniper Rifle Suppressor", 22, UnlockCategory::WeaponUpgrades},
        {"Sniper Rifle Advanced Scope", 23, UnlockCategory::WeaponUpgrades},
        {"Moss Brown Paint", 23, UnlockCategory::VehiclePaints},
        {"Sandy Brown Paint", 23, UnlockCategory::VehiclePaints},
        {"Straw Brown Paint", 23, UnlockCategory::VehiclePaints},
        {"Woodbeech Brown Paint", 23, UnlockCategory::VehiclePaints},
        {"Assault Rifle", 24, UnlockCategory::Weapons},
        {"Boat Pick-Up", 25, UnlockCategory::Activities},
        {"Locate a Boat", 25, UnlockCategory::Activities},
        {"Assault Rifle Extended Clip", 25, UnlockCategory::WeaponUpgrades},
        {"Cream Paint", 25, UnlockCategory::VehiclePaints},
        {"Frost White Paint", 25, UnlockCategory::VehiclePaints},
        {"Midnight Purple Paint", 25, UnlockCategory::VehiclePaints},
        {"Schafter Purple Paint", 25, UnlockCategory::VehiclePaints},
        {"Assault Rifle Grip", 26, UnlockCategory::WeaponUpgrades},
        {"Assault Rifle Flashlight", 27, UnlockCategory::WeaponUpgrades},
        {"Assault Rifle Scope", 28, UnlockCategory::WeaponUpgrades},
        {"Assault SMG", 29, UnlockCategory::Weapons},
        {"Assault Rifle Suppressor", 29, UnlockCategory::WeaponUpgrades},
        {"Helicopter Pickup", 30, UnlockCategory::Activities},
        {"Assault SMG Extended Clip", 30, UnlockCategory::WeaponUpgrades},
        {"Assault SMG Flashlight", 31, UnlockCategory::WeaponUpgrades},
        {"Assault SMG Scope", 32, UnlockCategory::WeaponUpgrades},
        {"AP Pistol", 33, UnlockCategory::Weapons},
        {"Assault SMG Suppressor", 33, UnlockCategory::WeaponUpgrades},
        {"AP Pistol Extended Clip", 34, UnlockCategory::WeaponUpgrades},
        {"Locate a Helicopter", 35, UnlockCategory::Activities},
        {"Send Mercenaries", 35, UnlockCategory::Activities},
        {"AP Pistol Flashlight", 35, UnlockCategory::WeaponUpgrades},
        {"AP Pistol Suppressor", 36, UnlockCategory::WeaponUpgrades},
        {"Assault Shotgun", 37, UnlockCategory::Weapons},
        {"Assault Shotgun Extended Clip", 38, UnlockCategory::WeaponUpgrades},
        {"Assault Shotgun Grip", 39, UnlockCategory::WeaponUpgrades},
        {"Off the Radar", 40, UnlockCategory::Activities},
        {"Reveal Players", 40, UnlockCategory::Activities},
        {"Assault Shotgun Flashlight", 40, UnlockCategory::WeaponUpgrades},
        {"Assault Shotgun Suppressor", 41, UnlockCategory::WeaponUpgrades},
        {"Carbine Rifle", 42, UnlockCategory::Weapons},
        {"Carbine Rifle Extended Clip", 43, UnlockCategory::WeaponUpgrades},
        {"Carbine Rifle Grip", 44, UnlockCategory::WeaponUpgrades},
        {"Locate a Plane", 45, UnlockCategory::Activities},
        {"Carbine Rifle Flashlight", 45, UnlockCategory::WeaponUpgrades},
        {"Carbine Rifle Scope", 46, UnlockCategory::WeaponUpgrades},
        {"Carbine Rifle Suppressor", 47, UnlockCategory::WeaponUpgrades},
        {"Airstrike", 50, UnlockCategory::Activities},
        {"Mugger", 50, UnlockCategory::Activities},
        {"MG", 50, UnlockCategory::Weapons},
    });

    inline constexpr int HighestMappedRank = 50;

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
