#pragma once

#include "../../game/GamePointers.hpp"
#include "../../game/Stats.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <array>
#include <atomic>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::Heist
{
    struct SalvageProcessingSnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool sessionStarted{};
        bool nativeReady{};
        bool staffUpgrade{};
        bool wallSafeUpgrade{};
        int incomeThreshold{-1};
        std::array<int, 2> liftModels{{0, 0}};
        std::array<int, 2> liftValues{{0, 0}};
        std::array<int, 2> liftPosix{{0, 0}};
        std::string message{"Press Refresh Salvage Processing"};
    };

    class SalvageProcessingRuntime final
    {
    public:
        static SalvageProcessingRuntime& Get() noexcept
        {
            static SalvageProcessingRuntime instance;
            return instance;
        }

        [[nodiscard]] bool QueueRefresh()
        {
            return Queue("Reading Salvage Yard lifts and processing state", [this] {
                SalvageProcessingSnapshot state;
                const bool success = CaptureState(state);
                Finish(success, std::move(state), success
                    ? "Salvage Yard lift state refreshed"
                    : "Unable to read Salvage Yard lift state");
            });
        }

        [[nodiscard]] bool QueueFinishLift(int slot)
        {
            if (slot < 1 || slot > 2)
                return false;

            return Queue("Advancing Salvage Yard lift processing", [this, slot] {
                SalvageProcessingSnapshot state;
                if (!CaptureState(state))
                {
                    Finish(false, std::move(state), "Salvage Yard stats are unavailable");
                    return;
                }

                const auto index = static_cast<std::size_t>(slot - 1);
                if (state.liftModels[index] == 0)
                {
                    Finish(false, std::move(state), "That Salvage Yard lift is empty");
                    return;
                }

                const int reductionSeconds = state.staffUpgrade ? 2880 : 5760;
                const std::string stat = slot == 1
                    ? "MPX_SALVAGING_POSIX_LIFT1"
                    : "MPX_SALVAGING_POSIX_LIFT2";
                const int original = state.liftPosix[index];
                const int wanted = original - reductionSeconds;
                if (!Stats::SetInt(stat, wanted))
                {
                    Finish(false, std::move(state), "Salvage processing timestamp write failed");
                    return;
                }

                const auto verified = Stats::GetInt(stat);
                if (!verified || *verified != wanted)
                {
                    static_cast<void>(Stats::SetInt(stat, original));
                    CaptureState(state);
                    Finish(false, std::move(state), "Salvage processing write failed verification and was rolled back");
                    return;
                }

                CaptureState(state);
                Finish(true, std::move(state), "Salvage processing timestamp advanced using the current staff-upgrade duration");
            });
        }

        [[nodiscard]] bool QueueMaxIncome()
        {
            return Queue("Maximizing Salvage Yard income threshold", [this] {
                SalvageProcessingSnapshot state;
                if (!CaptureState(state))
                {
                    Finish(false, std::move(state), "Salvage Yard stats are unavailable");
                    return;
                }

                if (!Stats::SetPackedInt(51051, 100))
                {
                    Finish(false, std::move(state), "Packed stat 51051 write failed");
                    return;
                }

                CaptureState(state);
                const bool verified = state.incomeThreshold >= 100;
                Finish(verified, std::move(state), verified
                    ? "Salvage Yard income threshold set to 100"
                    : "Salvage Yard income threshold write did not verify");
            });
        }

        [[nodiscard]] SalvageProcessingSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            auto state = m_State;
            state.pending = m_Pending.load(std::memory_order_acquire);
            return state;
        }

    private:
        SalvageProcessingRuntime() = default;

        template<typename Callback>
        [[nodiscard]] bool Queue(std::string pendingMessage, Callback&& callback)
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;
            {
                std::scoped_lock lock(m_Mutex);
                m_State.haveResult = false;
                m_State.lastSucceeded = false;
                m_State.message = std::move(pendingMessage);
            }
            if (Runtime::GameRuntime::Get().Enqueue(std::forward<Callback>(callback)))
                return true;
            Finish(false, {}, "GTA script-thread queue is unavailable");
            return false;
        }

        [[nodiscard]] bool CaptureState(SalvageProcessingSnapshot& state) const noexcept
        {
            const bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            state.sessionStarted = sessionStarted && *sessionStarted;
            state.nativeReady = Native::NativeRegistry::Get().CanInvokeOnCurrentThread();
            if (!state.sessionStarted || !state.nativeReady)
                return false;

            const auto staff = Stats::GetInt("MPX_SALVAGE_YARD_STAFF");
            const auto wallSafe = Stats::GetInt("MPX_SALVAGE_YARD_WALL_SAFE");
            const auto income = Stats::GetPackedInt(51051);
            if (!staff || !wallSafe || !income)
                return false;

            state.staffUpgrade = *staff == 1;
            state.wallSafeUpgrade = *wallSafe == 1;
            state.incomeThreshold = *income;

            constexpr std::array<const char*, 2> ModelStats{
                "MPX_MPSV_MODEL_SALVAGE_LIFT1",
                "MPX_MPSV_MODEL_SALVAGE_LIFT2",
            };
            constexpr std::array<const char*, 2> ValueStats{
                "MPX_MPSV_VALUE_SALVAGE_LIFT1",
                "MPX_MPSV_VALUE_SALVAGE_LIFT2",
            };
            constexpr std::array<const char*, 2> PosixStats{
                "MPX_SALVAGING_POSIX_LIFT1",
                "MPX_SALVAGING_POSIX_LIFT2",
            };

            for (std::size_t index = 0; index < 2; ++index)
            {
                const auto model = Stats::GetInt(ModelStats[index]);
                const auto value = Stats::GetInt(ValueStats[index]);
                const auto posix = Stats::GetInt(PosixStats[index]);
                if (!model || !value || !posix)
                    return false;
                state.liftModels[index] = *model;
                state.liftValues[index] = *value;
                state.liftPosix[index] = *posix;
            }
            return true;
        }

        void Finish(bool success, SalvageProcessingSnapshot state, std::string message) noexcept
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
        SalvageProcessingSnapshot m_State{};
    };
}
