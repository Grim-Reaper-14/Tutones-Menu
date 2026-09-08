#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/script/ScriptLocal.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::Heist
{
    namespace TowTruckEnhanced173
    {
        [[nodiscard]] constexpr std::uint32_t Joaat(const char* text) noexcept
        {
            std::uint32_t hash{};
            while (text && *text)
            {
                char c = *text++;
                if (c >= 'A' && c <= 'Z')
                    c = static_cast<char>(c - 'A' + 'a');
                hash += static_cast<std::uint8_t>(c);
                hash += hash << 10;
                hash ^= hash >> 6;
            }
            hash += hash << 3;
            hash ^= hash >> 11;
            hash += hash << 15;
            return hash;
        }

        inline constexpr std::uint32_t TowTruckWorkScriptHash = Joaat("fm_content_tow_truck_work");
        inline constexpr std::uint32_t TowTruckWorkRewardHash =
            Joaat("SERVICE_EARN_AMBIENT_JOB_TOW_TRUCK_WORK");
        inline constexpr std::uint32_t SalvageVehicleRewardHash =
            Joaat("SERVICE_EARN_SALVAGE_VEHICLE");
        inline constexpr std::uint32_t SalvageYardSellRewardHash =
            Joaat("SERVICE_EARN_SALVAGE_YARD_SELL_VEH");

        inline constexpr std::size_t GenericBitsetBaseLocal = 1828;
        inline constexpr std::ptrdiff_t GenericBitsetOffset = 1;
        inline constexpr std::size_t EndReasonBaseLocal = 1885;
        inline constexpr std::ptrdiff_t EndReasonOffset = 93;
        inline constexpr std::uint32_t CompletionBit = 11;
        inline constexpr std::int32_t CompletedEndReason = 3;
    }

    struct TowTruckOperationsSnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool sessionStarted{};
        bool scriptRuntimeReady{};
        bool towWorkRunning{};
        bool finishLocalsReadable{};
        std::uint32_t genericBitset{};
        int endReason{-1};
        std::string message{"Press Refresh Tow Truck Runtime"};
    };

    class TowTruckOperationsRuntime final
    {
    public:
        static TowTruckOperationsRuntime& Get() noexcept
        {
            static TowTruckOperationsRuntime instance;
            return instance;
        }

        [[nodiscard]] bool QueueRefresh()
        {
            return Queue("Reading Enhanced Tow Truck runtime", [this] {
                TowTruckOperationsSnapshot state;
                const bool success = CaptureState(state);
                Finish(
                    success,
                    std::move(state),
                    success ? "Enhanced Tow Truck runtime refreshed" : "Unable to read Enhanced Tow Truck runtime");
            });
        }

        [[nodiscard]] bool QueueFinishCurrentTowJob()
        {
            using namespace TowTruckEnhanced173;
            return Queue("Finishing active Tow Truck job through guarded mission locals", [this] {
                TowTruckOperationsSnapshot state;
                if (!CaptureState(state) || !state.towWorkRunning || !state.finishLocalsReadable)
                {
                    Finish(false, std::move(state), "Start a Tow Truck Work mission before using instant finish");
                    return;
                }

                auto& scripts = Script::ScriptRuntime::Get();
                auto* thread = scripts.FindThread(TowTruckWorkScriptHash);
                if (!thread)
                {
                    Finish(false, std::move(state), "Tow Truck script thread disappeared before the write");
                    return;
                }

                auto* bitset = Script::ScriptLocal(thread, GenericBitsetBaseLocal)
                    .At(GenericBitsetOffset)
                    .As<std::int32_t>();
                auto* endReason = Script::ScriptLocal(thread, EndReasonBaseLocal)
                    .At(EndReasonOffset)
                    .As<std::int32_t>();
                if (!bitset || !endReason)
                {
                    Finish(false, std::move(state), "Current Tow Truck finish locals failed bounds validation");
                    return;
                }

                const auto originalBits = static_cast<std::uint32_t>(*bitset);
                const auto originalEndReason = *endReason;
                const auto wantedBits = originalBits | (1u << CompletionBit);

                *bitset = static_cast<std::int32_t>(wantedBits);
                *endReason = CompletedEndReason;

                const bool verified = static_cast<std::uint32_t>(*bitset) == wantedBits
                    && *endReason == CompletedEndReason;
                if (!verified)
                {
                    *bitset = static_cast<std::int32_t>(originalBits);
                    *endReason = originalEndReason;
                    CaptureState(state);
                    Finish(false, std::move(state), "Tow Truck finish write failed verification and was rolled back");
                    return;
                }

                CaptureState(state);
                TUTONES_LOG_INFO("heist.towtruck", "Set Tow Truck completion bit 11 and end reason 3 without clobbering unrelated mission flags");
                Finish(true, std::move(state), "Tow Truck completion state applied; Rockstar's mission flow now owns the payout/cleanup");
            });
        }

        [[nodiscard]] TowTruckOperationsSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            TowTruckOperationsSnapshot state = m_State;
            state.pending = m_Pending.load(std::memory_order_acquire);
            return state;
        }

    private:
        TowTruckOperationsRuntime() = default;

        template<typename Callback>
        [[nodiscard]] bool Queue(std::string pendingMessage, Callback&& callback)
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;
            SetPending(std::move(pendingMessage));
            if (Runtime::GameRuntime::Get().Enqueue(std::forward<Callback>(callback)))
                return true;
            Finish(false, {}, "Game-thread queue unavailable");
            return false;
        }

        [[nodiscard]] bool CaptureState(TowTruckOperationsSnapshot& state) const noexcept
        {
            using namespace TowTruckEnhanced173;
            const bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            state.sessionStarted = sessionStarted && *sessionStarted;

            auto& scripts = Script::ScriptRuntime::Get();
            state.scriptRuntimeReady = scripts.IsReady();
            if (!state.sessionStarted || !state.scriptRuntimeReady)
                return false;

            auto* thread = scripts.FindThread(TowTruckWorkScriptHash);
            state.towWorkRunning = thread && thread->stack
                && thread->context.threadId != 0
                && thread->context.state != Types::ScriptThreadState::Killed;
            if (!state.towWorkRunning)
                return true;

            const auto* bitset = Script::ScriptLocal(thread, GenericBitsetBaseLocal)
                .At(GenericBitsetOffset)
                .As<std::int32_t>();
            const auto* endReason = Script::ScriptLocal(thread, EndReasonBaseLocal)
                .At(EndReasonOffset)
                .As<std::int32_t>();
            if (!bitset || !endReason)
                return true;

            state.finishLocalsReadable = true;
            state.genericBitset = static_cast<std::uint32_t>(*bitset);
            state.endReason = *endReason;
            return true;
        }

        void SetPending(std::string message)
        {
            std::scoped_lock lock(m_Mutex);
            m_State.haveResult = false;
            m_State.lastSucceeded = false;
            m_State.message = std::move(message);
        }

        void Finish(bool success, TowTruckOperationsSnapshot state, std::string message)
        {
            state.pending = false;
            state.haveResult = true;
            state.lastSucceeded = success;
            state.message = std::move(message);
            {
                std::scoped_lock lock(m_Mutex);
                m_State = std::move(state);
            }
            m_Pending.store(false, std::memory_order_release);
        }

        std::atomic<bool> m_Pending{false};
        mutable std::mutex m_Mutex;
        TowTruckOperationsSnapshot m_State{};
    };
}
