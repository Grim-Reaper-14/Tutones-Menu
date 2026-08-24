#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::Business
{
    struct VehicleCargoTuningProfile final
    {
        int stealCooldownMs{180000};
        int sellCooldown1Ms{1200000};
        int sellCooldown2Ms{1680000};
        int sellCooldown3Ms{2340000};
        int sellCooldown4Ms{2880000};
        int topRangeSellPrice{40000};
        int midRangeSellPrice{25000};
        int standardRangeSellPrice{15000};
    };

    struct VehicleCargoTuningSnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool readable{};
        VehicleCargoTuningProfile values{};
        std::string message{"Ready"};
    };

    class VehicleCargoTuningRuntime final
    {
    public:
        static constexpr std::size_t TunablesGlobal = 262145;

        static constexpr std::size_t StealCooldownOffset = 19170;

        static constexpr std::size_t SellCooldown1Offset = 19525;
        static constexpr std::size_t SellCooldown2Offset = 19526;
        static constexpr std::size_t SellCooldown3Offset = 19527;
        static constexpr std::size_t SellCooldown4Offset = 19528;

        static constexpr std::size_t TopRangeSellPriceOffset = 19263;
        static constexpr std::size_t MidRangeSellPriceOffset = 19264;
        static constexpr std::size_t StandardRangeSellPriceOffset = 19265;

        static VehicleCargoTuningRuntime& Get() noexcept
        {
            static VehicleCargoTuningRuntime instance;
            return instance;
        }

        bool QueueRefresh()
        {
            return Queue("Vehicle Cargo globals refresh queued", [this] {
                auto* pages = RequireGlobals();
                if (!pages)
                    return;

                VehicleCargoTuningProfile values{};
                if (!ReadProfile(pages, values))
                    return Finish(false, false, {}, "One or more Vehicle Cargo globals are unavailable");

                Finish(true, true, values, "Vehicle Cargo globals refreshed");
            });
        }

        bool QueueApplyProfile(VehicleCargoTuningProfile profile)
        {
            if (!ValidateProfile(profile))
                return false;

            return Queue("Vehicle Cargo globals apply queued", [this, profile] {
                auto* pages = RequireGlobals();
                if (!pages)
                    return;

                int* stealCooldown = Script::ScriptGlobal(TunablesGlobal).At(StealCooldownOffset).As<int>(pages);
                int* sellCooldown1 = Script::ScriptGlobal(TunablesGlobal).At(SellCooldown1Offset).As<int>(pages);
                int* sellCooldown2 = Script::ScriptGlobal(TunablesGlobal).At(SellCooldown2Offset).As<int>(pages);
                int* sellCooldown3 = Script::ScriptGlobal(TunablesGlobal).At(SellCooldown3Offset).As<int>(pages);
                int* sellCooldown4 = Script::ScriptGlobal(TunablesGlobal).At(SellCooldown4Offset).As<int>(pages);
                int* topRange = Script::ScriptGlobal(TunablesGlobal).At(TopRangeSellPriceOffset).As<int>(pages);
                int* midRange = Script::ScriptGlobal(TunablesGlobal).At(MidRangeSellPriceOffset).As<int>(pages);
                int* standardRange = Script::ScriptGlobal(TunablesGlobal).At(StandardRangeSellPriceOffset).As<int>(pages);

                if (!stealCooldown || !sellCooldown1 || !sellCooldown2 || !sellCooldown3 || !sellCooldown4
                    || !topRange || !midRange || !standardRange)
                {
                    return Finish(false, false, {}, "One or more Vehicle Cargo globals are unavailable");
                }

                *stealCooldown = profile.stealCooldownMs;
                *sellCooldown1 = profile.sellCooldown1Ms;
                *sellCooldown2 = profile.sellCooldown2Ms;
                *sellCooldown3 = profile.sellCooldown3Ms;
                *sellCooldown4 = profile.sellCooldown4Ms;
                *topRange = profile.topRangeSellPrice;
                *midRange = profile.midRangeSellPrice;
                *standardRange = profile.standardRangeSellPrice;

                VehicleCargoTuningProfile readBack{};
                const bool readable = ReadProfile(pages, readBack);
                const bool success = readable
                    && readBack.stealCooldownMs == profile.stealCooldownMs
                    && readBack.sellCooldown1Ms == profile.sellCooldown1Ms
                    && readBack.sellCooldown2Ms == profile.sellCooldown2Ms
                    && readBack.sellCooldown3Ms == profile.sellCooldown3Ms
                    && readBack.sellCooldown4Ms == profile.sellCooldown4Ms
                    && readBack.topRangeSellPrice == profile.topRangeSellPrice
                    && readBack.midRangeSellPrice == profile.midRangeSellPrice
                    && readBack.standardRangeSellPrice == profile.standardRangeSellPrice;

                if (success)
                {
                    TUTONES_LOG_INFO(
                        "business.vehicle_cargo",
                        std::string("Applied Vehicle Cargo tuning: stealMs=") + std::to_string(profile.stealCooldownMs)
                            + " sell1Ms=" + std::to_string(profile.sellCooldown1Ms)
                            + " sell2Ms=" + std::to_string(profile.sellCooldown2Ms)
                            + " sell3Ms=" + std::to_string(profile.sellCooldown3Ms)
                            + " sell4Ms=" + std::to_string(profile.sellCooldown4Ms)
                            + " top=" + std::to_string(profile.topRangeSellPrice)
                            + " mid=" + std::to_string(profile.midRangeSellPrice)
                            + " standard=" + std::to_string(profile.standardRangeSellPrice));
                }

                Finish(
                    success,
                    readable,
                    readBack,
                    success ? "Vehicle Cargo globals applied" : "Vehicle Cargo globals failed read-back verification");
            });
        }

        [[nodiscard]] VehicleCargoTuningSnapshot Snapshot() const
        {
            VehicleCargoTuningSnapshot snapshot;
            snapshot.pending = m_Pending.load(std::memory_order_acquire);

            std::scoped_lock lock(m_Mutex);
            snapshot.haveResult = m_HaveResult;
            snapshot.lastSucceeded = m_LastSucceeded;
            snapshot.readable = m_Readable;
            snapshot.values = m_Values;
            snapshot.message = m_Message;
            return snapshot;
        }

    private:
        VehicleCargoTuningRuntime() = default;

        [[nodiscard]] static bool ValidateProfile(const VehicleCargoTuningProfile& profile) noexcept
        {
            return profile.stealCooldownMs >= 0
                && profile.sellCooldown1Ms >= 0
                && profile.sellCooldown2Ms >= 0
                && profile.sellCooldown3Ms >= 0
                && profile.sellCooldown4Ms >= 0
                && profile.topRangeSellPrice >= 0
                && profile.midRangeSellPrice >= 0
                && profile.standardRangeSellPrice >= 0;
        }

        [[nodiscard]] static bool ReadProfile(std::int64_t** pages, VehicleCargoTuningProfile& out) noexcept
        {
            if (!pages)
                return false;

            int* stealCooldown = Script::ScriptGlobal(TunablesGlobal).At(StealCooldownOffset).As<int>(pages);
            int* sellCooldown1 = Script::ScriptGlobal(TunablesGlobal).At(SellCooldown1Offset).As<int>(pages);
            int* sellCooldown2 = Script::ScriptGlobal(TunablesGlobal).At(SellCooldown2Offset).As<int>(pages);
            int* sellCooldown3 = Script::ScriptGlobal(TunablesGlobal).At(SellCooldown3Offset).As<int>(pages);
            int* sellCooldown4 = Script::ScriptGlobal(TunablesGlobal).At(SellCooldown4Offset).As<int>(pages);
            int* topRange = Script::ScriptGlobal(TunablesGlobal).At(TopRangeSellPriceOffset).As<int>(pages);
            int* midRange = Script::ScriptGlobal(TunablesGlobal).At(MidRangeSellPriceOffset).As<int>(pages);
            int* standardRange = Script::ScriptGlobal(TunablesGlobal).At(StandardRangeSellPriceOffset).As<int>(pages);

            if (!stealCooldown || !sellCooldown1 || !sellCooldown2 || !sellCooldown3 || !sellCooldown4
                || !topRange || !midRange || !standardRange)
            {
                return false;
            }

            out.stealCooldownMs = *stealCooldown;
            out.sellCooldown1Ms = *sellCooldown1;
            out.sellCooldown2Ms = *sellCooldown2;
            out.sellCooldown3Ms = *sellCooldown3;
            out.sellCooldown4Ms = *sellCooldown4;
            out.topRangeSellPrice = *topRange;
            out.midRangeSellPrice = *midRange;
            out.standardRangeSellPrice = *standardRange;
            return true;
        }

        template<typename Callback>
        bool Queue(std::string pendingMessage, Callback&& callback)
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            SetPending(std::move(pendingMessage));
            if (Runtime::GameRuntime::Get().Enqueue(std::forward<Callback>(callback)))
                return true;

            Finish(false, false, {}, "Game-thread queue unavailable");
            return false;
        }

        [[nodiscard]] std::int64_t** RequireGlobals()
        {
            bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            if (!sessionStarted || !*sessionStarted)
            {
                Finish(false, false, {}, "Join GTA Online before using Vehicle Cargo tuning");
                return nullptr;
            }

            auto* pages = GamePointers::Get().ScriptGlobals();
            if (!pages)
            {
                Finish(false, false, {}, "Enhanced script globals are unavailable");
                return nullptr;
            }

            return pages;
        }

        void SetPending(std::string message)
        {
            std::scoped_lock lock(m_Mutex);
            m_HaveResult = false;
            m_LastSucceeded = false;
            m_Message = std::move(message);
        }

        void Finish(bool success, bool readable, VehicleCargoTuningProfile values, std::string message)
        {
            {
                std::scoped_lock lock(m_Mutex);
                m_HaveResult = true;
                m_LastSucceeded = success;
                m_Readable = readable;
                m_Values = values;
                m_Message = std::move(message);
            }
            m_Pending.store(false, std::memory_order_release);
        }

        std::atomic<bool> m_Pending{false};
        mutable std::mutex m_Mutex;
        bool m_HaveResult{};
        bool m_LastSucceeded{};
        bool m_Readable{};
        VehicleCargoTuningProfile m_Values{};
        std::string m_Message{"Ready"};
    };
}
