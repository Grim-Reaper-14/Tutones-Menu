#pragma once

#include "../../game/GamePointers.hpp"
#include "../../game/PlayerNatives.hpp"
#include "../../game/Stats.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

namespace Tutones::Game::Business
{
    namespace MoneyFrontsEnhanced173
    {
        inline constexpr std::size_t FlowGlobal = 1985024;
        inline constexpr std::size_t PlayerEntrySize = 149;
        inline constexpr std::size_t MoneyFrontsFlowOffset = 145;
        inline constexpr std::size_t GeneralBitsOffset = 0;
        inline constexpr std::size_t FlagsOffset = 1;
        inline constexpr std::size_t CurrentMissionOffset = 2;

        inline constexpr int FrontCount = 3;
        inline constexpr std::array<const char*, FrontCount> FrontNames{
            "Hands On Car Wash",
            "Smoke on the Water",
            "Higgins Helitours",
        };
        inline constexpr std::array<int, FrontCount> HeatPackedStats{
            24924,
            24925,
            24926,
        };
        inline constexpr std::array<const char*, FrontCount> OwnedStats{
            "MPX_SB_CAR_WASH_OWNED",
            "MPX_SB_WEED_SHOP_OWNED",
            "MPX_SB_HELI_TOURS_OWNED",
        };

        inline constexpr const char* CarWashSafeCashStat = "MPX_CWASH_SAFE_CASH_VALUE";
        inline constexpr std::size_t CarWashSafeCollectGlobal = 2708890;
        inline constexpr int MinimumHeat = 0;
        inline constexpr int MaximumHeat = 100;

        [[nodiscard]] inline const char* FrontName(int index) noexcept
        {
            if (index < 0 || index >= FrontCount)
                return "Unknown Front";
            return FrontNames[static_cast<std::size_t>(index)];
        }
    }

    struct MoneyFrontsSnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool sessionStarted{};
        bool nativeReady{};
        bool globalsReady{};
        int playerId{-1};
        int characterIndex{-1};
        std::uint32_t generalBits{};
        std::uint32_t flags{};
        int currentMission{-1};
        std::array<int, MoneyFrontsEnhanced173::FrontCount> heat{{-1, -1, -1}};
        std::array<int, MoneyFrontsEnhanced173::FrontCount> owned{{0, 0, 0}};
        int carWashSafeCash{};
        std::string message{"Press Refresh Money Fronts"};
    };

    class MoneyFrontsRuntime final
    {
    public:
        static MoneyFrontsRuntime& Get() noexcept
        {
            static MoneyFrontsRuntime instance;
            return instance;
        }

        [[nodiscard]] bool QueueRefresh()
        {
            return Queue("Reading current Money Fronts state", [this] {
                MoneyFrontsSnapshot state;
                const bool success = CaptureState(state);
                Finish(
                    success,
                    std::move(state),
                    success ? "Money Fronts state refreshed" : "Unable to read Money Fronts state");
            });
        }

        [[nodiscard]] bool QueueSetHeat(int frontIndex, int heat)
        {
            using namespace MoneyFrontsEnhanced173;
            if (frontIndex < 0 || frontIndex >= FrontCount || heat < MinimumHeat || heat > MaximumHeat)
                return false;

            return Queue("Updating Money Fronts heat", [this, frontIndex, heat] {
                MoneyFrontsSnapshot before;
                if (!CaptureState(before) || before.characterIndex < 0)
                {
                    Finish(false, std::move(before), "Money Fronts packed stats are unavailable");
                    return;
                }

                const int packedIndex = HeatPackedStats[static_cast<std::size_t>(frontIndex)];
                const auto original = Stats::GetPackedInt(packedIndex, before.characterIndex);
                if (!original)
                {
                    Finish(false, std::move(before), "Unable to read current front heat");
                    return;
                }

                if (!Stats::SetPackedInt(packedIndex, heat, before.characterIndex))
                {
                    Finish(false, std::move(before), "Front heat write was rejected");
                    return;
                }

                const auto verified = Stats::GetPackedInt(packedIndex, before.characterIndex);
                if (!verified || *verified != heat)
                {
                    static_cast<void>(Stats::SetPackedInt(packedIndex, *original, before.characterIndex));
                    MoneyFrontsSnapshot rolledBack;
                    static_cast<void>(CaptureState(rolledBack));
                    Finish(false, std::move(rolledBack), "Front heat verification failed and was rolled back");
                    return;
                }

                MoneyFrontsSnapshot after;
                const bool captured = CaptureState(after);
                Finish(
                    captured,
                    std::move(after),
                    captured ? std::string(FrontName(frontIndex)) + " heat updated" : "Heat changed but state refresh failed");
            });
        }

        [[nodiscard]] bool QueueSetAllHeat(int heat)
        {
            using namespace MoneyFrontsEnhanced173;
            if (heat < MinimumHeat || heat > MaximumHeat)
                return false;

            return Queue("Updating all Money Fronts heat", [this, heat] {
                MoneyFrontsSnapshot before;
                if (!CaptureState(before) || before.characterIndex < 0)
                {
                    Finish(false, std::move(before), "Money Fronts packed stats are unavailable");
                    return;
                }

                std::array<int, FrontCount> originals{};
                for (int index = 0; index < FrontCount; ++index)
                {
                    const auto value = Stats::GetPackedInt(HeatPackedStats[static_cast<std::size_t>(index)], before.characterIndex);
                    if (!value)
                    {
                        Finish(false, std::move(before), "Unable to capture all front heat values");
                        return;
                    }
                    originals[static_cast<std::size_t>(index)] = *value;
                }

                bool success = true;
                for (int index = 0; index < FrontCount; ++index)
                {
                    const int packedIndex = HeatPackedStats[static_cast<std::size_t>(index)];
                    if (!Stats::SetPackedInt(packedIndex, heat, before.characterIndex))
                    {
                        success = false;
                        break;
                    }
                    const auto verified = Stats::GetPackedInt(packedIndex, before.characterIndex);
                    if (!verified || *verified != heat)
                    {
                        success = false;
                        break;
                    }
                }

                if (!success)
                {
                    for (int index = 0; index < FrontCount; ++index)
                    {
                        static_cast<void>(Stats::SetPackedInt(
                            HeatPackedStats[static_cast<std::size_t>(index)],
                            originals[static_cast<std::size_t>(index)],
                            before.characterIndex));
                    }
                    MoneyFrontsSnapshot rolledBack;
                    static_cast<void>(CaptureState(rolledBack));
                    Finish(false, std::move(rolledBack), "All-front heat update failed and was rolled back");
                    return;
                }

                MoneyFrontsSnapshot after;
                const bool captured = CaptureState(after);
                Finish(captured, std::move(after), captured ? "All Money Fronts heat updated" : "Heat changed but state refresh failed");
            });
        }

        [[nodiscard]] bool QueueCollectCarWashSafe()
        {
            using namespace MoneyFrontsEnhanced173;
            return Queue("Requesting Hands On Car Wash safe collection", [this] {
                MoneyFrontsSnapshot before;
                if (!CaptureState(before))
                {
                    Finish(false, std::move(before), "Money Fronts state is unavailable");
                    return;
                }

                if (before.carWashSafeCash <= 0)
                {
                    Finish(false, std::move(before), "Hands On Car Wash safe is empty");
                    return;
                }

                auto** globals = Script::ScriptRuntime::Get().Globals();
                if (!globals)
                {
                    Finish(false, std::move(before), "Script globals are unavailable");
                    return;
                }

                auto* collect = Script::ScriptGlobal(CarWashSafeCollectGlobal).As<std::int32_t>(globals);
                if (!collect)
                {
                    Finish(false, std::move(before), "Car Wash safe-collect global is unavailable");
                    return;
                }

                const std::int32_t original = *collect;
                *collect = 1;
                if (*collect != 1)
                {
                    *collect = original;
                    Finish(false, std::move(before), "Car Wash safe-collect request failed");
                    return;
                }

                MoneyFrontsSnapshot after;
                static_cast<void>(CaptureState(after));
                Finish(true, std::move(after), "Hands On Car Wash safe collection requested");
            });
        }

        [[nodiscard]] MoneyFrontsSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            MoneyFrontsSnapshot state = m_Snapshot;
            state.pending = m_Pending.load(std::memory_order_acquire);
            return state;
        }

    private:
        MoneyFrontsRuntime() = default;
        MoneyFrontsRuntime(const MoneyFrontsRuntime&) = delete;
        MoneyFrontsRuntime& operator=(const MoneyFrontsRuntime&) = delete;

        template<typename Callback>
        [[nodiscard]] bool Queue(std::string pendingMessage, Callback&& callback)
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot.haveResult = false;
                m_Snapshot.lastSucceeded = false;
                m_Snapshot.message = std::move(pendingMessage);
            }

            if (Runtime::GameRuntime::Get().Enqueue(std::forward<Callback>(callback)))
                return true;

            MoneyFrontsSnapshot state;
            Finish(false, std::move(state), "GTA script-thread queue is unavailable");
            return false;
        }

        [[nodiscard]] bool CaptureState(MoneyFrontsSnapshot& state) const noexcept
        {
            using namespace MoneyFrontsEnhanced173;

            const bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            state.sessionStarted = sessionStarted && *sessionStarted;
            state.nativeReady = Native::NativeRegistry::Get().CanInvokeOnCurrentThread();

            auto& scripts = Script::ScriptRuntime::Get();
            auto** globals = scripts.Globals();
            state.globalsReady = globals != nullptr;
            if (!state.sessionStarted || !state.nativeReady || !globals)
                return false;

            const auto player = PlayerNatives::PlayerId();
            if (!player || *player < 0 || *player >= 32)
                return false;
            state.playerId = *player;

            const auto characterIndex = Stats::GetCharIndex();
            if (characterIndex)
                state.characterIndex = *characterIndex;

            const auto flow = Script::ScriptGlobal(FlowGlobal)
                .At(static_cast<std::size_t>(*player), PlayerEntrySize)
                .At(MoneyFrontsFlowOffset);
            const auto* general = flow.At(GeneralBitsOffset).As<std::int32_t>(globals);
            const auto* flags = flow.At(FlagsOffset).As<std::int32_t>(globals);
            const auto* mission = flow.At(CurrentMissionOffset).As<std::int32_t>(globals);
            if (!general || !flags || !mission)
                return false;

            state.generalBits = static_cast<std::uint32_t>(*general);
            state.flags = static_cast<std::uint32_t>(*flags);
            state.currentMission = *mission;

            if (state.characterIndex >= 0)
            {
                for (int index = 0; index < FrontCount; ++index)
                {
                    const auto heat = Stats::GetPackedInt(HeatPackedStats[static_cast<std::size_t>(index)], state.characterIndex);
                    if (heat)
                        state.heat[static_cast<std::size_t>(index)] = *heat;

                    const auto owned = Stats::GetInt(OwnedStats[static_cast<std::size_t>(index)]);
                    if (owned)
                        state.owned[static_cast<std::size_t>(index)] = *owned;
                }
            }

            if (const auto safeCash = Stats::GetInt(CarWashSafeCashStat))
                state.carWashSafeCash = *safeCash;

            return true;
        }

        void Finish(bool success, MoneyFrontsSnapshot state, std::string message) noexcept
        {
            state.pending = false;
            state.haveResult = true;
            state.lastSucceeded = success;
            state.message = std::move(message);
            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot = std::move(state);
            }
            m_Pending.store(false, std::memory_order_release);
        }

        std::atomic<bool> m_Pending{false};
        mutable std::mutex m_Mutex;
        MoneyFrontsSnapshot m_Snapshot{};
    };
}
