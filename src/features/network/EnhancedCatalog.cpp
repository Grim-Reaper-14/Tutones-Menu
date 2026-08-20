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

#define TUTONES_REWARD(label, service, scriptName, kindValue) \
        RewardDefinition{label, service, scriptName, kindValue, Joaat(service), 0, 0, 0, true}

        constexpr std::array<RewardDefinition, RewardCatalogSize> Rewards{{
            {"Bunker Excess Weapon Parts", "SERVICE_EARN_AMBIENT_JOB_AMMUNATION_DELIVERY", "shared NETSHOP catalog", RewardKind::Earn,
                Joaat("SERVICE_EARN_AMBIENT_JOB_AMMUNATION_DELIVERY"), TunablesGlobal, 32173, 50000, true},
            TUTONES_REWARD("Crate Drop", "SERVICE_EARN_CRATE_DROP", "shared NETSHOP catalog", RewardKind::Earn),
            TUTONES_REWARD("Bounty Collected", "SERVICE_EARN_BOUNTY_COLLECTED", "shared NETSHOP catalog", RewardKind::Earn),
            TUTONES_REWARD("Armored Trucks", "SERVICE_EARN_ARMORED_TRUCKS", "shared NETSHOP catalog", RewardKind::Earn),
            TUTONES_REWARD("Daily Objectives", "SERVICE_EARN_DAILY_OBJECTIVES", "shared NETSHOP catalog", RewardKind::Earn),
            TUTONES_REWARD("Distract Cops", "SERVICE_EARN_AMBIENT_JOB_DISTRACT_COPS", "shared NETSHOP catalog", RewardKind::Earn),
            TUTONES_REWARD("Destroy Vehicle", "SERVICE_EARN_AMBIENT_JOB_DESTROY_VEH", "shared NETSHOP catalog", RewardKind::Earn),
            TUTONES_REWARD("Checkpoint Collection", "SERVICE_EARN_AMBIENT_JOB_CHECKPOINT_COLLECTION", "shared NETSHOP catalog", RewardKind::Earn),
            TUTONES_REWARD("Dead Drop", "SERVICE_EARN_AMBIENT_JOB_DEAD_DROP", "shared NETSHOP catalog", RewardKind::Earn),
            TUTONES_REWARD("Vehicle Export", "SERVICE_EARN_FROM_VEHICLE_EXPORT", "shared NETSHOP catalog", RewardKind::Earn),
            TUTONES_REWARD("Bounty Hunter Reward", "SERVICE_EARN_BOUNTY_HUNTER_REWARD", "shared NETSHOP catalog", RewardKind::Earn),
            TUTONES_REWARD("Business Battle", "SERVICE_EARN_FROM_BUSINESS_BATTLE", "shared NETSHOP catalog", RewardKind::Earn),
            TUTONES_REWARD("Business Hub Sell", "SERVICE_EARN_FROM_BUSINESS_HUB_SELL", "shared NETSHOP catalog", RewardKind::Earn),
            TUTONES_REWARD("Casino Mission", "SERVICE_EARN_CASINO_MISSION_REWARD", "shared NETSHOP catalog", RewardKind::Earn),
            TUTONES_REWARD("Casino Heist Finale", "SERVICE_EARN_CASINO_HEIST_FINALE", "shared NETSHOP catalog", RewardKind::Earn),
            TUTONES_REWARD("Cayo Perico Finale", "SERVICE_EARN_ISLAND_HEIST_FINALE", "shared NETSHOP catalog", RewardKind::Earn),
            TUTONES_REWARD("Tuner Robbery Finale", "SERVICE_EARN_TUNER_ROBBERY_FINALE", "shared NETSHOP catalog", RewardKind::Earn),
            TUTONES_REWARD("Auto Shop Delivery", "SERVICE_EARN_AUTO_SHOP_DELIVERY_AWARD", "shared NETSHOP catalog", RewardKind::Earn),
            TUTONES_REWARD("Agency Security Contract", "SERVICE_EARN_AGENCY_SECURITY_CONTRACT", "shared NETSHOP catalog", RewardKind::Earn),
            TUTONES_REWARD("Agency Payphone Hit", "SERVICE_EARN_AGENCY_PAYPHONE_HIT", "shared NETSHOP catalog", RewardKind::Earn),
            TUTONES_REWARD("Agency Story Finale", "SERVICE_EARN_AGENCY_STORY_FINALE", "shared NETSHOP catalog", RewardKind::Earn),
            TUTONES_REWARD("Taxi Job", "SERVICE_EARN_TAXI_JOB", "shared NETSHOP catalog", RewardKind::Earn),
            TUTONES_REWARD("Stash House Complete", "SERVICE_EARN_DAILY_STASH_HOUSE_COMPLETED", "shared NETSHOP catalog", RewardKind::Earn),
            TUTONES_REWARD("Acid Lab Sell", "SERVICE_EARN_ACID_LAB_SELL_PARTICIPATION", "shared NETSHOP catalog", RewardKind::Earn),
            TUTONES_REWARD("Armored Truck Event", "SERVICE_EARN_AMBIENT_JOB_ARMORED_TRUCK", "shared NETSHOP catalog", RewardKind::Earn),
            TUTONES_REWARD("Cluckin Bell Finale", "SERVICE_EARN_CHICKEN_FACTORY_RAID_FINALE", "shared NETSHOP catalog", RewardKind::Earn),
            TUTONES_REWARD("Salvage Yard Robbery Finale", "SERVICE_EARN_SALVAGE_YARD_ROBBERY_FINALE", "shared NETSHOP catalog", RewardKind::Earn),
            TUTONES_REWARD("Salvage Vehicle", "SERVICE_EARN_SALVAGE_VEHICLE", "shared NETSHOP catalog", RewardKind::Earn),
            TUTONES_REWARD("Tow Truck Work", "SERVICE_EARN_AMBIENT_JOB_TOW_TRUCK_WORK", "shared NETSHOP catalog", RewardKind::Earn),
            TUTONES_REWARD("Salvage Yard Vehicle Sale", "SERVICE_EARN_SALVAGE_YARD_SELL_VEH", "shared NETSHOP catalog", RewardKind::Earn),
            TUTONES_REWARD("Weekly Objective", "SERVICE_EARN_WEEKLY_OBJECTIVE_COMPLETED", "shared NETSHOP catalog", RewardKind::Earn),
            TUTONES_REWARD("Ammo Drop Refund", "SERVICE_EARN_REFUNDAMMODROP", "am_ammo_drop.c", RewardKind::Refund),
            TUTONES_REWARD("Helicopter Pickup Refund", "SERVICE_EARN_REFUND_HELI_PICKUP", "am_ammo_drop.c shared transaction helper", RewardKind::Refund),
            TUTONES_REWARD("Boat Pickup Refund", "SERVICE_EARN_REFUND_BOAT_PICKUP", "am_ammo_drop.c shared transaction helper", RewardKind::Refund),
            TUTONES_REWARD("Clear Wanted Refund", "SERVICE_EARN_REFUND_CLEAR_WANTED", "am_ammo_drop.c shared transaction helper", RewardKind::Refund),
        }};

#undef TUTONES_REWARD

        template<std::size_t N, typename Definition>
        [[nodiscard]] std::array<CatalogObservation, N> SampleTunables(
            const std::array<Definition, N>& definitions,
            std::int64_t** globals) noexcept
        {
            std::array<CatalogObservation, N> observations{};
            if (!globals)
                return observations;

            for (std::size_t index = 0; index < N; ++index)
            {
                const auto& definition = definitions[index];
                if (definition.tunableBase == 0)
                    continue;

                const auto* value = Script::ScriptGlobal(definition.tunableBase)
                    .At(definition.tunableOffset)
                    .template As<int>(globals);
                if (!value)
                    continue;

                observations[index].readable = true;
                observations[index].value = static_cast<std::int64_t>(*value);
            }
            return observations;
        }
    }

    std::span<const CooldownDefinition, CooldownCatalogSize> CooldownCatalog() noexcept
    {
        return Cooldowns;
    }

    std::span<const RewardDefinition, RewardCatalogSize> RewardCatalog() noexcept
    {
        return Rewards;
    }

    CooldownObservations SampleCooldownTunables(std::int64_t** globals) noexcept
    {
        CooldownObservations observations{};
        if (!globals)
            return observations;

        for (std::size_t index = 0; index < Cooldowns.size(); ++index)
        {
            const auto& definition = Cooldowns[index];
            if (definition.globalBase == 0)
                continue;

            const auto* value = Script::ScriptGlobal(definition.globalBase)
                .At(definition.globalOffset)
                .As<int>(globals);
            if (!value)
                continue;

            observations[index].readable = true;
            observations[index].value = static_cast<std::int64_t>(*value);
        }
        return observations;
    }

    RewardObservations SampleRewardTunables(std::int64_t** globals) noexcept
    {
        return SampleTunables(Rewards, globals);
    }

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

    const char* RewardKindName(RewardKind kind) noexcept
    {
        return kind == RewardKind::Refund ? "Refund" : "Earn";
    }

    std::string FormatDuration(std::int64_t milliseconds)
    {
        milliseconds = std::max<std::int64_t>(milliseconds, 0);
        const auto totalSeconds = milliseconds / 1000;
        const auto days = totalSeconds / 86400;
        const auto hours = (totalSeconds / 3600) % 24;
        const auto minutes = (totalSeconds / 60) % 60;
        const auto seconds = totalSeconds % 60;

        char buffer[64]{};
        if (days > 0)
            std::snprintf(buffer, sizeof(buffer), "%lldd %02lld:%02lld:%02lld",
                static_cast<long long>(days), static_cast<long long>(hours),
                static_cast<long long>(minutes), static_cast<long long>(seconds));
        else if (hours > 0)
            std::snprintf(buffer, sizeof(buffer), "%02lld:%02lld:%02lld",
                static_cast<long long>(hours), static_cast<long long>(minutes), static_cast<long long>(seconds));
        else
            std::snprintf(buffer, sizeof(buffer), "%02lld:%02lld",
                static_cast<long long>(minutes), static_cast<long long>(seconds));
        return buffer;
    }
}
