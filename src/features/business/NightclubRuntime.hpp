#pragma once

#include "../../game/GamePointers.hpp"
#include "../../game/Stats.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::Business
{
    struct NightclubProfile final
    {
        std::array<int, 7> stockValues{};
        std::array<int, 7> specialOrderStockValues{};
        std::array<int, 3> cooldowns{};
        std::array<int, 7> maxUnits{};
        std::array<int, 7> productionTimes{};
        float equipmentUpgradeMultiplier{0.5f};
        std::array<int, 20> popularityIncome{};
    };

    namespace NightclubData
    {
        inline constexpr std::size_t TunablesGlobal = 262145;
        inline constexpr const char* PopularityStat = "MPX_CLUB_POPULARITY";
        inline constexpr int MaximumPopularity = 1000;

        inline constexpr std::array<const char*, 7> GoodNames{{
            "Sporting Goods",
            "South American Imports",
            "Pharmaceutical Research",
            "Organic Produce",
            "Printing and Copying",
            "Cash Creation",
            "Cargo and Shipments",
        }};

        inline constexpr std::array<const char*, 3> CooldownNames{{
            "Management Mission",
            "Sell Mission",
            "Special Order Sell Mission",
        }};

        inline constexpr std::array<const char*, 20> PopularityTierNames{{
            "1 - 5",
            "6 - 10",
            "11 - 15",
            "16 - 20",
            "21 - 25",
            "26 - 30",
            "31 - 35",
            "36 - 40",
            "41 - 45",
            "46 - 50",
            "51 - 55",
            "56 - 60",
            "61 - 65",
            "66 - 70",
            "71 - 75",
            "76 - 80",
            "81 - 85",
            "86 - 90",
            "91 - 95",
            "96 - 100",
        }};

        inline constexpr std::array<std::size_t, 7> StockOffsets{{
            24055, 24056, 24057, 24058, 24059, 24060, 24061,
        }};

        inline constexpr std::array<std::size_t, 7> SpecialOrderStockOffsets{{
            24048, 24049, 24050, 24051, 24052, 24053, 24054,
        }};

        inline constexpr std::array<std::size_t, 3> CooldownOffsets{{
            24118, 24159, 24160,
        }};

        inline constexpr std::array<std::size_t, 7> MaxUnitOffsets{{
            24062, 24063, 24064, 24065, 24066, 24067, 24068,
        }};

        inline constexpr std::array<std::size_t, 7> ProductionTimeOffsets{{
            24040, 24041, 24042, 24043, 24044, 24045, 24046,
        }};

        inline constexpr std::size_t EquipmentUpgradeMultiplierOffset = 24047;

        inline constexpr std::array<std::size_t, 20> PopularityIncomeOffsets{{
            23750, 23751, 23752, 23753, 23754,
            23755, 23756, 23757, 23758, 23759,
            23760, 23761, 23762, 23763, 23764,
            23765, 23766, 23767, 23768, 23769,
        }};

        [[nodiscard]] constexpr NightclubProfile DefaultProfile() noexcept
        {
            return NightclubProfile{
                {5000, 27000, 11475, 2025, 1350, 4725, 10000},
                {5000, 27000, 11475, 2025, 1350, 4725, 10000},
                {300000, 300000, 300000},
                {100, 10, 20, 80, 60, 40, 50},
                {4800000, 14400000, 7200000, 2400000, 1800000, 3600000, 8400000},
                0.5f,
                {1500, 1600, 1800, 2000, 2200, 2500, 8000, 8500, 9000, 9500,
                 10000, 20000, 21000, 22000, 23000, 24000, 25000, 45000, 50000, 50000},
            };
        }
    }

    struct NightclubSnapshot final
    {
        bool actionPending{};
        bool haveResult{};
        bool lastSucceeded{};
        std::uint64_t revision{};
        std::string message{"Ready"};
    };

    class NightclubRuntime final
    {
    public:
        static NightclubRuntime& Get() noexcept
        {
            static NightclubRuntime instance;
            return instance;
        }

        [[nodiscard]] NightclubSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            NightclubSnapshot snapshot = m_Snapshot;
            snapshot.actionPending = m_Pending.load(std::memory_order_acquire);
            return snapshot;
        }

        [[nodiscard]] bool QueueApplyGood(std::size_t index, const NightclubProfile& profile)
        {
            if (index >= NightclubData::GoodNames.size())
                return false;

            return QueueAction("Nightclub good values", [index, profile] {
                bool ok = true;
                ok = WriteInt(NightclubData::StockOffsets[index], profile.stockValues[index]) && ok;
                ok = WriteInt(NightclubData::SpecialOrderStockOffsets[index], profile.specialOrderStockValues[index]) && ok;
                ok = WriteInt(NightclubData::MaxUnitOffsets[index], profile.maxUnits[index]) && ok;
                ok = WriteInt(NightclubData::ProductionTimeOffsets[index], profile.productionTimes[index]) && ok;
                return ok;
            });
        }

        [[nodiscard]] bool QueueApplyCooldowns(const std::array<int, 3>& values)
        {
            return QueueAction("Nightclub cooldowns", [values] {
                bool ok = true;
                for (std::size_t index = 0; index < values.size(); ++index)
                    ok = WriteInt(NightclubData::CooldownOffsets[index], values[index]) && ok;
                return ok;
            });
        }

        [[nodiscard]] bool QueueApplyUpgradeMultiplier(float multiplier)
        {
            if (!std::isfinite(multiplier) || multiplier < 0.0f || multiplier > 100.0f)
                return false;

            return QueueAction("Nightclub equipment multiplier", [multiplier] {
                return WriteFloat(NightclubData::EquipmentUpgradeMultiplierOffset, multiplier);
            });
        }

        [[nodiscard]] bool QueueSetPopularity(int popularity)
        {
            if (popularity < 0 || popularity > NightclubData::MaximumPopularity)
                return false;

            return QueueAction("Nightclub popularity", [popularity] {
                return SessionReady() && Stats::SetInt(NightclubData::PopularityStat, popularity);
            });
        }

        [[nodiscard]] bool QueueApplyPopularityIncome(std::size_t index, int value)
        {
            if (index >= NightclubData::PopularityIncomeOffsets.size() || value < 0)
                return false;

            return QueueAction("Nightclub popularity income", [index, value] {
                return WriteInt(NightclubData::PopularityIncomeOffsets[index], value);
            });
        }

        [[nodiscard]] bool QueueApplyProfile(const NightclubProfile& profile)
        {
            if (!ValidateProfile(profile))
                return false;

            return QueueAction("Nightclub profile", [profile] {
                bool ok = true;
                for (std::size_t index = 0; index < NightclubData::GoodNames.size(); ++index)
                {
                    ok = WriteInt(NightclubData::StockOffsets[index], profile.stockValues[index]) && ok;
                    ok = WriteInt(NightclubData::SpecialOrderStockOffsets[index], profile.specialOrderStockValues[index]) && ok;
                    ok = WriteInt(NightclubData::MaxUnitOffsets[index], profile.maxUnits[index]) && ok;
                    ok = WriteInt(NightclubData::ProductionTimeOffsets[index], profile.productionTimes[index]) && ok;
                }

                for (std::size_t index = 0; index < NightclubData::CooldownOffsets.size(); ++index)
                    ok = WriteInt(NightclubData::CooldownOffsets[index], profile.cooldowns[index]) && ok;

                ok = WriteFloat(
                    NightclubData::EquipmentUpgradeMultiplierOffset,
                    profile.equipmentUpgradeMultiplier) && ok;

                for (std::size_t index = 0; index < NightclubData::PopularityIncomeOffsets.size(); ++index)
                    ok = WriteInt(NightclubData::PopularityIncomeOffsets[index], profile.popularityIncome[index]) && ok;

                return ok;
            });
        }

    private:
        NightclubRuntime() = default;
        NightclubRuntime(const NightclubRuntime&) = delete;
        NightclubRuntime& operator=(const NightclubRuntime&) = delete;

        [[nodiscard]] static bool SessionReady() noexcept
        {
            bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            return sessionStarted && *sessionStarted && GamePointers::Get().ScriptGlobals() != nullptr;
        }

        [[nodiscard]] static bool WriteInt(std::size_t offset, int value) noexcept
        {
            if (value < 0 || !SessionReady())
                return false;

            auto* pages = GamePointers::Get().ScriptGlobals();
            int* target = Script::ScriptGlobal(NightclubData::TunablesGlobal).At(offset).As<int>(pages);
            if (!target)
                return false;

            *target = value;
            return *target == value;
        }

        [[nodiscard]] static bool WriteFloat(std::size_t offset, float value) noexcept
        {
            if (!std::isfinite(value) || value < 0.0f || !SessionReady())
                return false;

            auto* pages = GamePointers::Get().ScriptGlobals();
            float* target = Script::ScriptGlobal(NightclubData::TunablesGlobal).At(offset).As<float>(pages);
            if (!target)
                return false;

            *target = value;
            return std::fabs(*target - value) <= 0.0001f;
        }

        [[nodiscard]] static bool ValidateProfile(const NightclubProfile& profile) noexcept
        {
            for (const int value : profile.stockValues)
                if (value < 0)
                    return false;
            for (const int value : profile.specialOrderStockValues)
                if (value < 0)
                    return false;
            for (const int value : profile.cooldowns)
                if (value < 0)
                    return false;
            for (const int value : profile.maxUnits)
                if (value < 0)
                    return false;
            for (const int value : profile.productionTimes)
                if (value < 0)
                    return false;
            for (const int value : profile.popularityIncome)
                if (value < 0)
                    return false;

            return std::isfinite(profile.equipmentUpgradeMultiplier)
                && profile.equipmentUpgradeMultiplier >= 0.0f
                && profile.equipmentUpgradeMultiplier <= 100.0f;
        }

        template<typename Fn>
        [[nodiscard]] bool QueueAction(const char* label, Fn&& action)
        {
            if (m_Pending.exchange(true, std::memory_order_acq_rel))
                return false;

            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot.haveResult = false;
                m_Snapshot.lastSucceeded = false;
                m_Snapshot.message = std::string(label ? label : "Nightclub action") + " queued";
            }

            const std::string actionLabel = label ? label : "Nightclub action";
            if (!Runtime::GameRuntime::Get().Enqueue([this, actionLabel, fn = std::forward<Fn>(action)]() mutable {
                    const bool success = fn();
                    {
                        std::scoped_lock lock(m_Mutex);
                        m_Snapshot.haveResult = true;
                        m_Snapshot.lastSucceeded = success;
                        m_Snapshot.message = actionLabel + (success ? " applied" : " failed");
                        ++m_Snapshot.revision;
                    }
                    m_Pending.store(false, std::memory_order_release);
                }))
            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot.haveResult = true;
                m_Snapshot.lastSucceeded = false;
                m_Snapshot.message = "GTA script-thread queue unavailable";
                ++m_Snapshot.revision;
                m_Pending.store(false, std::memory_order_release);
                return false;
            }

            return true;
        }

        std::atomic<bool> m_Pending{false};
        mutable std::mutex m_Mutex;
        NightclubSnapshot m_Snapshot{};
    };
}
