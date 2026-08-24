#pragma once

#include "../../game/GamePointers.hpp"
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
    struct MotorcycleClubProfile final
    {
        std::array<int, 6> stockValues{};
        std::array<int, 6> maxCapacities{};
        float nearSaleMultiplier{1.0f};
        float farSaleMultiplier{1.5f};
    };

    namespace MotorcycleClubData
    {
        inline constexpr std::size_t TunablesGlobal = 262145;

        inline constexpr std::array<const char*, 6> BusinessNames{{
            "Documents",
            "Cash",
            "Cocaine",
            "Meth",
            "Weed",
            "Acid",
        }};

        inline constexpr std::array<std::size_t, 6> StockOffsets{{
            17412, // Documents
            17413, // Cash
            17414, // Cocaine
            17415, // Meth
            17416, // Weed
            17417, // Acid
        }};

        // Ordered to match BusinessNames above. The supplied capacity globals are
        // grouped differently in the decompile, so keep the explicit mapping here.
        inline constexpr std::array<std::size_t, 6> CapacityOffsets{{
            18837, // Documents
            18845, // Cash
            18829, // Cocaine
            18821, // Meth
            18813, // Weed
            18853, // Acid
        }};

        inline constexpr std::size_t NearSaleMultiplierOffset = 18967;
        inline constexpr std::size_t FarSaleMultiplierOffset = 18968;

        [[nodiscard]] constexpr MotorcycleClubProfile DefaultProfile() noexcept
        {
            return MotorcycleClubProfile{
                {1350, 4725, 27000, 11475, 2025, 1485},
                {60, 40, 10, 20, 80, 160},
                1.0f,
                1.5f,
            };
        }
    }

    struct MotorcycleClubSnapshot final
    {
        bool actionPending{};
        bool haveResult{};
        bool lastSucceeded{};
        std::uint64_t revision{};
        std::string message{"Ready"};
    };

    class MotorcycleClubRuntime final
    {
    public:
        static MotorcycleClubRuntime& Get() noexcept
        {
            static MotorcycleClubRuntime instance;
            return instance;
        }

        [[nodiscard]] MotorcycleClubSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            MotorcycleClubSnapshot snapshot = m_Snapshot;
            snapshot.actionPending = m_Pending.load(std::memory_order_acquire);
            return snapshot;
        }

        [[nodiscard]] bool QueueApplyBusiness(std::size_t index, const MotorcycleClubProfile& profile)
        {
            if (index >= MotorcycleClubData::BusinessNames.size()
                || profile.stockValues[index] < 0
                || profile.maxCapacities[index] < 0)
            {
                return false;
            }

            return QueueAction("MC business values", [index, profile] {
                bool ok = true;
                ok = WriteInt(MotorcycleClubData::StockOffsets[index], profile.stockValues[index]) && ok;
                ok = WriteInt(MotorcycleClubData::CapacityOffsets[index], profile.maxCapacities[index]) && ok;
                return ok;
            });
        }

        [[nodiscard]] bool QueueApplySaleMultipliers(float nearMultiplier, float farMultiplier)
        {
            if (!ValidFloat(nearMultiplier) || !ValidFloat(farMultiplier))
                return false;

            return QueueAction("MC sale multipliers", [nearMultiplier, farMultiplier] {
                bool ok = true;
                ok = WriteFloat(MotorcycleClubData::NearSaleMultiplierOffset, nearMultiplier) && ok;
                ok = WriteFloat(MotorcycleClubData::FarSaleMultiplierOffset, farMultiplier) && ok;
                return ok;
            });
        }

        [[nodiscard]] bool QueueApplyProfile(const MotorcycleClubProfile& profile)
        {
            if (!ValidateProfile(profile))
                return false;

            return QueueAction("MC globals profile", [profile] {
                bool ok = true;
                for (std::size_t index = 0; index < MotorcycleClubData::BusinessNames.size(); ++index)
                {
                    ok = WriteInt(MotorcycleClubData::StockOffsets[index], profile.stockValues[index]) && ok;
                    ok = WriteInt(MotorcycleClubData::CapacityOffsets[index], profile.maxCapacities[index]) && ok;
                }

                ok = WriteFloat(MotorcycleClubData::NearSaleMultiplierOffset, profile.nearSaleMultiplier) && ok;
                ok = WriteFloat(MotorcycleClubData::FarSaleMultiplierOffset, profile.farSaleMultiplier) && ok;
                return ok;
            });
        }

    private:
        MotorcycleClubRuntime() = default;
        MotorcycleClubRuntime(const MotorcycleClubRuntime&) = delete;
        MotorcycleClubRuntime& operator=(const MotorcycleClubRuntime&) = delete;

        [[nodiscard]] static bool SessionReady() noexcept
        {
            bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            return sessionStarted && *sessionStarted && GamePointers::Get().ScriptGlobals() != nullptr;
        }

        [[nodiscard]] static bool ValidFloat(float value) noexcept
        {
            return std::isfinite(value) && value >= 0.0f;
        }

        [[nodiscard]] static bool WriteInt(std::size_t offset, int value) noexcept
        {
            if (value < 0 || !SessionReady())
                return false;

            auto* pages = GamePointers::Get().ScriptGlobals();
            int* target = Script::ScriptGlobal(MotorcycleClubData::TunablesGlobal).At(offset).As<int>(pages);
            if (!target)
                return false;

            *target = value;
            return *target == value;
        }

        [[nodiscard]] static bool WriteFloat(std::size_t offset, float value) noexcept
        {
            if (!ValidFloat(value) || !SessionReady())
                return false;

            auto* pages = GamePointers::Get().ScriptGlobals();
            float* target = Script::ScriptGlobal(MotorcycleClubData::TunablesGlobal).At(offset).As<float>(pages);
            if (!target)
                return false;

            *target = value;
            return std::fabs(*target - value) <= 0.0001f;
        }

        [[nodiscard]] static bool ValidateProfile(const MotorcycleClubProfile& profile) noexcept
        {
            for (const int value : profile.stockValues)
                if (value < 0)
                    return false;
            for (const int value : profile.maxCapacities)
                if (value < 0)
                    return false;

            return ValidFloat(profile.nearSaleMultiplier)
                && ValidFloat(profile.farSaleMultiplier);
        }

        template<typename Fn>
        [[nodiscard]] bool QueueAction(const char* label, Fn&& action)
        {
            if (m_Pending.exchange(true, std::memory_order_acq_rel))
                return false;

            const std::string actionLabel = label ? label : "MC action";
            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot.haveResult = false;
                m_Snapshot.lastSucceeded = false;
                m_Snapshot.message = actionLabel + " queued";
            }

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
        MotorcycleClubSnapshot m_Snapshot{};
    };
}
