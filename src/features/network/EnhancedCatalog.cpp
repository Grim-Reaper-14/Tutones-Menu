#include "EnhancedCatalog.hpp"

#include "../../game/script/ScriptGlobal.hpp"

#include <algorithm>
#include <array>
#include <cstdio>

namespace Tutones::Game::NetworkFeatures
{
    namespace
    {
        constexpr std::size_t TunablesGlobal = 262145;

        constexpr std::array<CooldownDefinition, CooldownCatalogSize> Cooldowns{{
            {"Bunker Excess Weapon Parts time limit", "Business", "freemode / Ammu-Nation delivery", CooldownSource::Tunable,
                TunablesGlobal, 32178, 600000, true},
            {"Backup Helicopter host watchdog", "Merryweather", "am_backup_heli.c", CooldownSource::ScriptStopwatch,
                0, 0, 20000, true},
            {"Ammo/Minigun model-load deadline", "Tutones service runtime", "GameSessionRuntime.cpp", CooldownSource::Internal,
                0, 0, 5000, true},
            {"Street Dealer reset", "Daily", "fm_street_dealer.c", CooldownSource::DailyReset,
                0, 0, 86400000, false},
            {"Stash House reset", "Daily", "freemode", CooldownSource::DailyReset,
                0, 0, 86400000, false},
            {"Lucky Wheel reset", "Casino", "casino / freemode", CooldownSource::DailyReset,
                0, 0, 86400000, false},
            {"Daily Objectives reset", "Daily", "freemode", CooldownSource::DailyReset,
                0, 0, 86400000, false},
            {"Weekly Objective reset", "Weekly", "freemode", CooldownSource::WeeklyReset,
                0, 0, 604800000, false},
        }};

        constexpr std::array<std::string_view, NamedRewardCatalogSize> NamedRewardServices{{
"SERVICE_EARN_PICKUP","SERVICE_EARN_JOBS","SERVICE_EARN_BETTING","SERVICE_EARN_LOTTERY","SERVICE_EARN_CHALLENGE_WIN","SERVICE_EARN_PROPERTY_SALES","SERVICE_EARN_VEHICLE_SALES","SERVICE_EARN_LESTER_TARGET_KILL","SERVICE_EARN_BOUNTY_COLLECTED","SERVICE_EARN_CRATE_DROP","SERVICE_EARN_HOLDUPS","SERVICE_EARN_IMPORT_EXPORT","SERVICE_EARN_ARMORED_TRUCKS","SERVICE_EARN_JOBSHARE_CASH","SERVICE_EARN_NOT_BADSPORT","SERVICE_EARN_BANK_INTEREST","SERVICE_EARN_DEBUG","SERVICE_EARN_CNCW","SERVICE_EARN_CNCB","SERVICE_EARN_JOB_BONUS","SERVICE_EARN_BEND_JOB","SERVICE_EARN_PERSONAL_VEHICLE","SERVICE_EARN_DAILY_OBJECTIVES","SERVICE_EARN_AMBIENT_JOB_PLANE_TAKEDOWN","SERVICE_EARN_AMBIENT_JOB_DISTRACT_COPS","SERVICE_EARN_AMBIENT_JOB_DESTROY_VEH","SERVICE_EARN_REFUND_BACKUP_VAGOS","SERVICE_EARN_REFUND_BACKUP_LOST","SERVICE_EARN_REFUND_BACKUP_FAMILIES","SERVICE_EARN_REFUND_HIRE_MUGGER","SERVICE_EARN_REFUND_HIRE_MERCENARY","SERVICE_EARN_REFUND_BUY_CARDROPOFF","SERVICE_EARN_REFUND_HELI_PICKUP","SERVICE_EARN_REFUND_BOAT_PICKUP","SERVICE_EARN_REFUND_CLEAR_WANTED","SERVICE_EARN_REFUND_HEAD_2_HEAD","SERVICE_EARN_REFUND_CHALLENGE","SERVICE_EARN_REFUND_SHARE_LAST_JOB","SERVICE_EARN_REFUND_LOTTERY","SERVICE_EARN_REFUNDAPPEARANCE","SERVICE_EARN_GANGATTACK_PICKUP","SERVICE_EARN_AMBIENT_JOB_HOT_TARGET_DELIVER","SERVICE_EARN_AMBIENT_JOB_HOT_TARGET_KILL","SERVICE_EARN_AMBIENT_JOB_URBAN_WARFARE","SERVICE_EARN_AMBIENT_JOB_CHECKPOINT_COLLECTION","SERVICE_EARN_AMBIENT_JOB_TIME_TRIAL","SERVICE_EARN_AMBIENT_JOB_CHALLENGES","SERVICE_EARN_AMBIENT_JOB_HELI_HOT_TARGET","SERVICE_EARN_AMBIENT_JOB_DEAD_DROP","SERVICE_EARN_AMBIENT_JOB_PENNED_IN","SERVICE_EARN_AMBIENT_JOB_PASS_PARCEL","SERVICE_EARN_AMBIENT_JOB_BLAST","SERVICE_EARN_AMBIENT_JOB_HOT_PROPERTY","SERVICE_EARN_AMBIENT_JOB_KING","SERVICE_EARN_AMBIENT_JOB_BEAST","SERVICE_EARN_BOSS","SERVICE_EARN_GOON","SERVICE_EARN_BOSS_AGENCY","SERVICE_EARN_FROM_DESTROYING_CONTRABAND","SERVICE_EARN_PREMIUM_JOB","SERVICE_EARN_FROM_VEHICLE_EXPORT","SERVICE_EARN_SMUGGLER_AGENCY","SERVICE_EARN_WAGE_PAYMENT_BONUS","SERVICE_EARN_WAGE_PAYMENT","SERVICE_EARN_REFUNDAMMODROP","SERVICE_EARN_SALVAGE_CHECKPOINT_COLLECTION","SERVICE_EARN_AMBIENT_MUGGING","SERVICE_EARN_AMBIENT_PICKUP","SERVICE_EARN_DEATHMATCH_BOUNTY","SERVICE_EARN_CASHING_OUT","SERVICE_EARN_JOB_BONUS_CRIMINAL_MASTERMIND","SERVICE_EARN_JOB_BONUS_HEIST_AWARD","SERVICE_EARN_JOB_BONUS_FIRST_TIME_BONUS","SERVICE_EARN_REFUND_ORBITAL_MANUAL","SERVICE_EARN_REFUND_ORBITAL_AUTO","SERVICE_EARN_GANGOPS_WAGES","SERVICE_EARN_GANGOPS_WAGES_BONUS","SERVICE_EARN_GANGOPS_PREP_PARTICIPATION","SERVICE_EARN_GANGOPS_SETUP","SERVICE_EARN_GANGOPS_SETUP_FAIL","SERVICE_EARN_GANGOPS_FINALE","SERVICE_EARN_GANGOPS_AWARD_MASTERMIND_2","SERVICE_EARN_GANGOPS_AWARD_MASTERMIND_3","SERVICE_EARN_GANGOPS_AWARD_MASTERMIND_4","SERVICE_EARN_GANGOPS_AWARD_LOYALTY_AWARD_2","SERVICE_EARN_GANGOPS_AWARD_LOYALTY_AWARD_3","SERVICE_EARN_GANGOPS_AWARD_LOYALTY_AWARD_4","SERVICE_EARN_GANGOPS_AWARD_FIRST_TIME_XM_BASE","SERVICE_EARN_GANGOPS_AWARD_FIRST_TIME_XM_SUBMARINE","SERVICE_EARN_GANGOPS_AWARD_FIRST_TIME_XM_SILO","SERVICE_EARN_GANGOPS_AWARD_SUPPORTING","SERVICE_EARN_GANGOPS_AWARD_ORDER","SERVICE_EARN_GANGOPS_ELITE_XM_BASE","SERVICE_EARN_GANGOPS_ELITE_XM_SUBMARINE","SERVICE_EARN_GANGOPS_ELITE_XM_SILO","SERVICE_EARN_GANGOPS_RIVAL_DELIVERY","SERVICE_EARN_DOOMSDAY_FINALE_BONUS","SERVICE_EARN_BOUNTY_HUNTER_REWARD","SERVICE_EARN_FROM_BUSINESS_BATTLE","SERVICE_EARN_FROM_CLUB_MANAGEMENT_PARTICIPATION","SERVICE_EARN_FROM_FMBB_PHONECALL_MISSION","SERVICE_EARN_FROM_BUSINESS_HUB_SELL","SERVICE_EARN_FROM_FMBB_BOSS_WORK","SERVICE_EARN_FMBB_WAGE_BONUS","SERVICE_EARN_BB_EVENT_BONUS","SERVICE_EARN_ARENA_SKILL_LVL_AWARD","SERVICE_EARN_ARENA_CAREER_TIER_PROGRESSION_1","SERVICE_EARN_ARENA_CAREER_TIER_PROGRESSION_2","SERVICE_EARN_ARENA_CAREER_TIER_PROGRESSION_3","SERVICE_EARN_ARENA_CAREER_TIER_PROGRESSION_4","SERVICE_EARN_SPIN_THE_WHEEL_CASH","SERVICE_EARN_ASSASSINATE_TARGET_KILLED","SERVICE_EARN_ARENA_WAR","SERVICE_EARN_REFUND_ARENA_SPEC_BOX_ENTRY","SERVICE_EARN_AMBIENT_JOB_RC_TIME_TRIAL","SERVICE_EARN_DAILY_OBJECTIVE_EVENT","SERVICE_EARN_COLLECTABLES_ACTION_FIGURES","SERVICE_EARN_CASINO_MISSION_REWARD","SERVICE_EARN_CASINO_AWARD_MISSION_ONE_FIRST_TIME","SERVICE_EARN_CASINO_AWARD_MISSION_TWO_FIRST_TIME","SERVICE_EARN_CASINO_AWARD_MISSION_THREE_FIRST_TIME","SERVICE_EARN_CASINO_AWARD_MISSION_FOUR_FIRST_TIME","SERVICE_EARN_CASINO_AWARD_MISSION_FIVE_FIRST_TIME","SERVICE_EARN_CASINO_AWARD_MISSION_SIX_FIRST_TIME","SERVICE_EARN_CASINO_AWARD_STRAIGHT_FLUSH","SERVICE_EARN_CASINO_AWARD_TOP_PAIR","SERVICE_EARN_CASINO_AWARD_FULL_HOUSE","SERVICE_EARN_CASINO_AWARD_LUCKY_LUCKY","SERVICE_EARN_CASINO_AWARD_HIGH_ROLLER_BRONZE","SERVICE_EARN_CASINO_AWARD_HIGH_ROLLER_SILVER","SERVICE_EARN_CASINO_AWARD_HIGH_ROLLER_GOLD","SERVICE_EARN_CASINO_AWARD_HIGH_ROLLER_PLATINUM","SERVICE_EARN_CASINO_STORY_MISSION_REWARD","SERVICE_EARN_CASINO_HEIST_SETUP_MISSION","SERVICE_EARN_CASINO_HEIST_PREP_MISSION","SERVICE_EARN_CASINO_HEIST_FINALE","SERVICE_EARN_CASINO_HEIST_AWARD_SMASH_N_GRAB","SERVICE_EARN_CASINO_HEIST_AWARD_IN_PLAIN_SIGHT","SERVICE_EARN_CASINO_HEIST_AWARD_UNDETECTED","SERVICE_EARN_CASINO_HEIST_AWARD_ALL_ROUNDER","SERVICE_EARN_CASINO_HEIST_AWARD_ELITE_THIEF","SERVICE_EARN_CASINO_HEIST_AWARD_PROFESSIONAL","SERVICE_EARN_CASINO_HEIST_ELITE_STEALTH","SERVICE_EARN_CASINO_HEIST_ELITE_SUBTERFUGE","SERVICE_EARN_CASINO_HEIST_ELITE_DIRECT","SERVICE_EARN_COLLECTABLE_ITEM","SERVICE_EARN_COLLECTABLE_COMPLETED_COLLECTION","SERVICE_EARN_COLLECTABLES_SIGNAL_JAMMERS","SERVICE_EARN_COLLECTABLES_SIGNAL_JAMMERS_COMPLETE","SERVICE_EARN_ISLAND_HEIST_FINALE","SERVICE_EARN_ISLAND_HEIST_ELITE_CHALLENGE","SERVICE_EARN_ISLAND_HEIST_AWARD_PROFESSIONAL","SERVICE_EARN_ISLAND_HEIST_AWARD_ELITE_THIEF","SERVICE_EARN_ISLAND_HEIST_AWARD_THE_ISLAND_HEIST","SERVICE_EARN_ISLAND_HEIST_AWARD_GOING_ALONE","SERVICE_EARN_ISLAND_HEIST_AWARD_TEAM_WORK","SERVICE_EARN_ISLAND_HEIST_AWARD_CAT_BURGLAR","SERVICE_EARN_ISLAND_HEIST_AWARD_PRO_THIEF","SERVICE_EARN_ISLAND_HEIST_AWARD_MIXING_IT_UP","SERVICE_EARN_ISLAND_HEIST_PREP","SERVICE_EARN_ISLAND_HEIST_DJ_MISSION","SERVICE_EARN_TUNER_ROBBERY_PREP","SERVICE_EARN_TUNER_ROBBERY_FINALE","SERVICE_EARN_TUNER_CAR_CLUB_MEMBERSHIP","SERVICE_EARN_TUNER_DAILY_VEHICLE","SERVICE_EARN_TUNER_DAILY_VEHICLE_BONUS","SERVICE_EARN_TUNER_AWARD_UNION_DEPOSITORY","SERVICE_EARN_TUNER_AWARD_MILITARY_CONVOY","SERVICE_EARN_TUNER_AWARD_FLEECA_BANK","SERVICE_EARN_TUNER_AWARD_FREIGHT_TRAIN","SERVICE_EARN_TUNER_AWARD_BOLINGBROKE_ASS","SERVICE_EARN_TUNER_AWARD_IAA_RAID","SERVICE_EARN_TUNER_AWARD_METH_JOB","SERVICE_EARN_TUNER_AWARD_BUNKER_RAID","SERVICE_EARN_AUTO_SHOP_DELIVERY_AWARD","SERVICE_EARN_AGENCY_SECURITY_CONTRACT","SERVICE_EARN_AGENCY_PAYPHONE_HIT","SERVICE_EARN_AGENCY_STORY_PREP","SERVICE_EARN_AGENCY_STORY_FINALE","SERVICE_EARN_FIXER_AWARD_SEC_CON","SERVICE_EARN_FIXER_AWARD_PHONE_HIT","SERVICE_EARN_FIXER_AWARD_AGENCY_STORY","SERVICE_EARN_FIXER_AWARD_SHORT_TRIP","SERVICE_EARN_FIXER_RIVAL_DELIVERY","SERVICE_EARN_MUSIC_STUDIO_SHORT_TRIP","SERVICE_EARN_FROM_CONTRABAND","SERVICE_EARN_NCLUB_TROUBLEMAKER","SERVICE_EARN_SIGHTSEEING_REWARD","SERVICE_EARN_AMBIENT_JOB_CLUBHOUSE_CONTRACT","SERVICE_EARN_AMBIENT_JOB_UNDERWATER_CARGO","SERVICE_EARN_AMBIENT_JOB_CRIME_SCENE","SERVICE_EARN_AMBIENT_JOB_METAL_DETECTOR","SERVICE_EARN_AMBIENT_JOB_SMUGGLER_PLANE","SERVICE_EARN_AMBIENT_JOB_SMUGGLER_TRAIL","SERVICE_EARN_AMBIENT_JOB_GOLDEN_GUN","SERVICE_EARN_AMBIENT_JOB_AMMUNATION_DELIVERY","SERVICE_EARN_AMBIENT_JOB_SOURCE_RESEARCH","SERVICE_EARN_YOHAN_SOURCE_GOODS","SERVICE_EARN_TAXI_JOB","SERVICE_EARN_DAILY_STASH_HOUSE_PARTICIPATION","SERVICE_EARN_DAILY_STASH_HOUSE_COMPLETED","SERVICE_EARN_AMBIENT_JOB_GANG_CONVOY","SERVICE_EARN_AMBIENT_JOB_SHOP_ROBBERY","SERVICE_EARN_AMBIENT_JOB_XMAS_MUGGER","SERVICE_EARN_AMBIENT_JOB_MAZE_BANK","SERVICE_EARN_JUGGALO_STORY_MISSION","SERVICE_EARN_JUGGALO_PHONE_MISSION","SERVICE_EARN_WINTER_22_AWARD_JUGGALO_STORY","SERVICE_EARN_WINTER_22_AWARD_ACID_LAB","SERVICE_EARN_WINTER_22_AWARD_DAILY_STASH","SERVICE_EARN_WINTER_22_AWARD_DEAD_DROP","SERVICE_EARN_WINTER_22_AWARD_RANDOM_EVENT","SERVICE_EARN_WINTER_22_AWARD_TAXI","SERVICE_EARN_ACID_LAB_SETUP_PARTICIPATION","SERVICE_EARN_ACID_LAB_SOURCE_PARTICIPATION","SERVICE_EARN_ACID_LAB_SELL_PARTICIPATION","SERVICE_EARN_SMUGGLER_OPS","SERVICE_EARN_AMBIENT_JOB_ARMORED_TRUCK","SERVICE_EARN_AMBIENT_JOB_BICYCLE_TIME_TRIAL","SERVICE_EARN_CAYO_ATTRITION_BONUS_OBJECTIVE","SERVICE_EARN_AVENGER_OPERATIONS","SERVICE_EARN_AVENGER_OPS_BONUS","SERVICE_EARN_AMBIENT_JOB_DRUG_VEHICLE","SERVICE_EARN_CHICKEN_FACTORY_RAID_PREP","SERVICE_EARN_CHICKEN_FACTORY_RAID_FINALE","SERVICE_EARN_WINTER_23_AWARD_CHICKEN_FACTORY_RAID","SERVICE_EARN_WINTER_23_AWARD_SALVAGE_YARD","SERVICE_EARN_SALVAGE_YARD_ROBBERY_PREP","SERVICE_EARN_SALVAGE_YARD_ROBBERY_FINALE","SERVICE_EARN_SALVAGE_VEHICLE","SERVICE_EARN_WEEKLY_OBJECTIVE_COMPLETED","SERVICE_EARN_AMBIENT_JOB_XMAS_TRUCK","SERVICE_EARN_AMBIENT_JOB_TOW_TRUCK_WORK","SERVICE_EARN_SALVAGE_YARD_SELL_VEH","SERVICE_EARN_BAIL_OFFICE_PRISONER","SERVICE_EARN_BAIL_OFFICE_HIGH_VALUE_PRISONER","SERVICE_EARN_BOUNTY_STANDARD_TARGET_BOSS","SERVICE_EARN_BOUNTY_STANDARD_TARGET_GOON","SERVICE_EARN_BOUNTY_HIGH_VALUE_TARGET_BOSS","SERVICE_EARN_BOUNTY_HIGH_VALUE_TARGET_GOON","SERVICE_EARN_BOUNTY24_DISPATCH_WORK","SERVICE_EARN_BOUNTY24_PIZZA_DELIVERY","SERVICE_EARN_BOUNTY24_UFO_ABDUCTION","SERVICE_EARN_BOUNTY24_AWARD","SERVICE_EARN_ARMS_TRAFFICKING","SERVICE_EARN_OSCAR_GUZMAN_MISSION","SERVICE_EARN_HACKER_ROBBERY_FINALE","SERVICE_EARN_HACKER_ROBBERY_PREP","SERVICE_EARN_MCKENZIE_AWARD","SERVICE_EARN_HACKER_DEN_AWARD"
        }};

        constexpr std::array<std::uint32_t, RawResolvedRewardCatalogSize> RawResolvedRewards{{
            1080388086u,
            616397339u,
            175159049u,
        }};

        constexpr bool Contains(std::string_view text, std::string_view token) noexcept
        {
            return text.find(token) != std::string_view::npos;
        }

        constexpr RewardKind KindFor(std::string_view service) noexcept
        {
            return Contains(service, "REFUND") ? RewardKind::Refund : RewardKind::Earn;
        }

        constexpr RewardGroup GroupFor(std::string_view service) noexcept
        {
            if (Contains(service, "REFUND")) return RewardGroup::Refund;
            if (Contains(service, "_AWARD") || Contains(service, "JOB_BONUS") || Contains(service, "CAREER_TIER")) return RewardGroup::Award;
            if (Contains(service, "HEIST") || Contains(service, "GANGOPS") || Contains(service, "TUNER_ROBBERY") || Contains(service, "CHICKEN_FACTORY_RAID")) return RewardGroup::Heist;
            if (Contains(service, "BUSINESS") || Contains(service, "SALVAGE") || Contains(service, "AGENCY") || Contains(service, "ACID_LAB") || Contains(service, "SMUGGLER") || Contains(service, "BOUNTY") || Contains(service, "BAIL_OFFICE") || Contains(service, "ARMS_TRAFFICKING") || Contains(service, "HACKER") || Contains(service, "MCKENZIE") || Contains(service, "VEHICLE_EXPORT") || Contains(service, "CONTRABAND") || Contains(service, "CLUB")) return RewardGroup::Business;
            if (Contains(service, "AMBIENT") || Contains(service, "DAILY") || Contains(service, "WEEKLY") || Contains(service, "COLLECTABLE") || Contains(service, "TAXI") || Contains(service, "FREEMODE")) return RewardGroup::Freemode;
            return RewardGroup::General;
        }

        constexpr RewardDefinition NamedReward(std::string_view service) noexcept
        {
            const bool ammunation = service == "SERVICE_EARN_AMBIENT_JOB_AMMUNATION_DELIVERY";
            return RewardDefinition{service, service, "YimMenu FMTransactionTriggerer / Enhanced NETSHOP catalog", KindFor(service), GroupFor(service), RewardResolution::NamedService, Joaat(service), ammunation ? TunablesGlobal : 0, ammunation ? 32173u : 0u, ammunation ? 50000 : 0, true};
        }

        constexpr RewardDefinition RawReward(std::uint32_t hash) noexcept
        {
            return RewardDefinition{"Enhanced DLC raw reward", {}, "Enhanced 1.73 decompile reward classifier", RewardKind::Earn, RewardGroup::RawResolvedDlc, RewardResolution::RawResolved, hash, 0, 0, 0, true};
        }

        constexpr auto BuildRewards() noexcept
        {
            std::array<RewardDefinition, RewardCatalogSize> rewards{};
            for (std::size_t index = 0; index < NamedRewardServices.size(); ++index) rewards[index] = NamedReward(NamedRewardServices[index]);
            for (std::size_t index = 0; index < RawResolvedRewards.size(); ++index) rewards[NamedRewardServices.size() + index] = RawReward(RawResolvedRewards[index]);
            return rewards;
        }

        constexpr auto Rewards = BuildRewards();

        constexpr bool RewardHashesUnique() noexcept
        {
            for (std::size_t left = 0; left < Rewards.size(); ++left)
                for (std::size_t right = left + 1; right < Rewards.size(); ++right)
                    if (Rewards[left].hash == Rewards[right].hash) return false;
            return true;
        }

        static_assert(NamedRewardServices.size() == NamedRewardCatalogSize);
        static_assert(Rewards.size() == 253);
        static_assert(RewardHashesUnique(), "Reward catalog contains duplicate hashes");

        template<std::size_t N, typename Definition>
        [[nodiscard]] std::array<CatalogObservation, N> SampleTunables(const std::array<Definition, N>& definitions, std::int64_t** globals) noexcept
        {
            std::array<CatalogObservation, N> observations{};
            if (!globals) return observations;
            for (std::size_t index = 0; index < N; ++index)
            {
                const auto& definition = definitions[index];
                if (definition.tunableBase == 0) continue;
                const auto* value = Script::ScriptGlobal(definition.tunableBase).At(definition.tunableOffset).template As<int>(globals);
                if (!value) continue;
                observations[index].readable = true;
                observations[index].value = static_cast<std::int64_t>(*value);
            }
            return observations;
        }
    }

    std::span<const CooldownDefinition, CooldownCatalogSize> CooldownCatalog() noexcept { return Cooldowns; }
    std::span<const RewardDefinition, RewardCatalogSize> RewardCatalog() noexcept { return Rewards; }

    CooldownObservations SampleCooldownTunables(std::int64_t** globals) noexcept
    {
        CooldownObservations observations{};
        if (!globals) return observations;
        for (std::size_t index = 0; index < Cooldowns.size(); ++index)
        {
            const auto& definition = Cooldowns[index];
            if (definition.globalBase == 0) continue;
            const auto* value = Script::ScriptGlobal(definition.globalBase).At(definition.globalOffset).As<int>(globals);
            if (!value) continue;
            observations[index].readable = true;
            observations[index].value = static_cast<std::int64_t>(*value);
        }
        return observations;
    }

    RewardObservations SampleRewardTunables(std::int64_t** globals) noexcept { return SampleTunables(Rewards, globals); }

    const char* CooldownSourceName(CooldownSource source) noexcept
    {
        switch (source)
        {
        case CooldownSource::Tunable: return "Tunable";
        case CooldownSource::ScriptStopwatch: return "Script stopwatch";
        case CooldownSource::NetworkTimer: return "Network timer";
        case CooldownSource::GameTimer: return "Game timer";
        case CooldownSource::DailyReset: return "Daily reset";
        case CooldownSource::WeeklyReset: return "Weekly reset";
        case CooldownSource::Internal: return "Menu runtime";
        }
        return "Unknown";
    }

    const char* RewardKindName(RewardKind kind) noexcept { return kind == RewardKind::Refund ? "Refund" : "Earn"; }

    const char* RewardGroupName(RewardGroup group) noexcept
    {
        switch (group)
        {
        case RewardGroup::General: return "General";
        case RewardGroup::Freemode: return "Freemode";
        case RewardGroup::Business: return "Business";
        case RewardGroup::Heist: return "Heist";
        case RewardGroup::Award: return "Award / bonus";
        case RewardGroup::Refund: return "Refund";
        case RewardGroup::RawResolvedDlc: return "DLC raw reward";
        }
        return "Unknown";
    }

    const char* RewardResolutionName(RewardResolution resolution) noexcept { return resolution == RewardResolution::RawResolved ? "Raw-resolved" : "Named service"; }

    std::string FormatDuration(std::int64_t milliseconds)
    {
        milliseconds = std::max<std::int64_t>(milliseconds, 0);
        const auto totalSeconds = milliseconds / 1000;
        const auto days = totalSeconds / 86400;
        const auto hours = (totalSeconds / 3600) % 24;
        const auto minutes = (totalSeconds / 60) % 60;
        const auto seconds = totalSeconds % 60;
        char buffer[64]{};
        if (days > 0) std::snprintf(buffer, sizeof(buffer), "%lldd %02lld:%02lld:%02lld", static_cast<long long>(days), static_cast<long long>(hours), static_cast<long long>(minutes), static_cast<long long>(seconds));
        else if (hours > 0) std::snprintf(buffer, sizeof(buffer), "%02lld:%02lld:%02lld", static_cast<long long>(hours), static_cast<long long>(minutes), static_cast<long long>(seconds));
        else std::snprintf(buffer, sizeof(buffer), "%02lld:%02lld", static_cast<long long>(minutes), static_cast<long long>(seconds));
        return buffer;
    }
}
