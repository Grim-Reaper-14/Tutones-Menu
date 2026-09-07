#pragma once

#include "CayoPericoRuntime.hpp"
#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/Stats.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../game/script/ScriptLocal.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::Heist
{
    namespace CayoLootEnhanced173
    {
        inline constexpr std::size_t CutsGlobal = 1980570;
        inline constexpr std::size_t CutsStateOffset = 831;
        inline constexpr std::size_t CutsArrayOffset = 56;
        inline constexpr std::size_t CutCount = 4;

        inline constexpr int CashIslandMask = 16711680;
        inline constexpr int CokeIslandMask = 255;
        inline constexpr int GoldCompoundMask = 255;
        inline constexpr int WeedIslandMask = 65280;
        inline constexpr int PaintingMask = 127;

        inline constexpr int DefaultCashValue = 83250;
        inline constexpr int DefaultCokeValue = 202500;
        inline constexpr int DefaultGoldValue = 333333;
        inline constexpr int DefaultWeedValue = 135000;
        inline constexpr int DefaultPaintingValue = 180000;
        inline constexpr int MaximumLootValue = 10000000;

        using CutArray = std::array<int, CutCount>;

        [[nodiscard]] inline bool ValidLootValue(int value) noexcept
        {
            return value >= 0 && value <= MaximumLootValue;
        }

        [[nodiscard]] inline bool ValidCuts(const CutArray& cuts) noexcept
        {
            for (const int cut : cuts)
            {
                if (cut < 0 || cut > 100)
                    return false;
            }
            return true;
        }
    }

    struct CayoLootConfig final
    {
        bool cash{true};
        bool coke{true};
        bool gold{true};
        bool weed{true};
        bool paintings{true};
        int cashValue{CayoLootEnhanced173::DefaultCashValue};
        int cokeValue{CayoLootEnhanced173::DefaultCokeValue};
        int goldValue{CayoLootEnhanced173::DefaultGoldValue};
        int weedValue{CayoLootEnhanced173::DefaultWeedValue};
        int paintingValue{CayoLootEnhanced173::DefaultPaintingValue};
    };

    struct CayoLootSnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool sessionStarted{};
        bool nativeReady{};
        bool globalsReady{};
        bool planningRunning{};
        bool planningReloaded{};
        bool lootReady{};
        bool cutsReady{};
        bool cash{};
        bool coke{};
        bool gold{};
        bool weed{};
        bool paintings{};
        int cashValue{};
        int cokeValue{};
        int goldValue{};
        int weedValue{};
        int paintingValue{};
        CayoLootEnhanced173::CutArray cuts{};
        std::string message{"Press Refresh Loot / Cuts"};
    };

    class CayoLootRuntime final
    {
    public:
        static CayoLootRuntime& Get() noexcept
        {
            static CayoLootRuntime instance;
            return instance;
        }

        [[nodiscard]] bool QueueRefresh()
        {
            return Queue("Reading Cayo loot and cut state", [this] {
                CayoLootSnapshot state;
                const bool success = CaptureState(state);
                Finish(
                    success,
                    std::move(state),
                    success
                        ? "Cayo loot and cuts refreshed"
                        : "Unable to read Cayo loot and cut state");
            });
        }

        [[nodiscard]] bool QueueApplyLoot(CayoLootConfig config)
        {
            if (!CayoLootEnhanced173::ValidLootValue(config.cashValue)
                || !CayoLootEnhanced173::ValidLootValue(config.cokeValue)
                || !CayoLootEnhanced173::ValidLootValue(config.goldValue)
                || !CayoLootEnhanced173::ValidLootValue(config.weedValue)
                || !CayoLootEnhanced173::ValidLootValue(config.paintingValue))
            {
                return false;
            }

            return Queue("Applying Cayo secondary loot", [this, config] {
                CayoLootSnapshot state;
                if (!RequireOnlineAndNatives(state))
                {
                    Finish(false, std::move(state), "Join GTA Online and wait for the native backend before changing Cayo loot");
                    return;
                }

                const auto characterIndex = Stats::GetCharIndex();
                if (!characterIndex)
                {
                    CaptureState(state);
                    Finish(false, std::move(state), "Unable to resolve the active GTA Online character slot");
                    return;
                }

                LootWriteArray writes{{
                    {"MPX_H4LOOT_CASH_I", config.cash ? CayoLootEnhanced173::CashIslandMask : 0, 0},
                    {"MPX_H4LOOT_CASH_I_SCOPED", config.cash ? CayoLootEnhanced173::CashIslandMask : 0, 0},
                    {"MPX_H4LOOT_CASH_C", 0, 0},
                    {"MPX_H4LOOT_CASH_C_SCOPED", 0, 0},
                    {"MPX_H4LOOT_COKE_I", config.coke ? CayoLootEnhanced173::CokeIslandMask : 0, 0},
                    {"MPX_H4LOOT_COKE_I_SCOPED", config.coke ? CayoLootEnhanced173::CokeIslandMask : 0, 0},
                    {"MPX_H4LOOT_COKE_C", 0, 0},
                    {"MPX_H4LOOT_COKE_C_SCOPED", 0, 0},
                    {"MPX_H4LOOT_GOLD_I", 0, 0},
                    {"MPX_H4LOOT_GOLD_I_SCOPED", 0, 0},
                    {"MPX_H4LOOT_GOLD_C", config.gold ? CayoLootEnhanced173::GoldCompoundMask : 0, 0},
                    {"MPX_H4LOOT_GOLD_C_SCOPED", config.gold ? CayoLootEnhanced173::GoldCompoundMask : 0, 0},
                    {"MPX_H4LOOT_WEED_I", config.weed ? CayoLootEnhanced173::WeedIslandMask : 0, 0},
                    {"MPX_H4LOOT_WEED_I_SCOPED", config.weed ? CayoLootEnhanced173::WeedIslandMask : 0, 0},
                    {"MPX_H4LOOT_WEED_C", 0, 0},
                    {"MPX_H4LOOT_WEED_C_SCOPED", 0, 0},
                    {"MPX_H4LOOT_PAINT", config.paintings ? CayoLootEnhanced173::PaintingMask : 0, 0},
                    {"MPX_H4LOOT_PAINT_SCOPED", config.paintings ? CayoLootEnhanced173::PaintingMask : 0, 0},
                    {"MPX_H4LOOT_CASH_V", config.cashValue, 0},
                    {"MPX_H4LOOT_COKE_V", config.cokeValue, 0},
                    {"MPX_H4LOOT_GOLD_V", config.goldValue, 0},
                    {"MPX_H4LOOT_WEED_V", config.weedValue, 0},
                    {"MPX_H4LOOT_PAINT_V", config.paintingValue, 0},
                }};

                const char* failedStat = nullptr;
                if (!CaptureOriginalStats(writes, *characterIndex, failedStat))
                {
                    CaptureState(state);
                    Finish(false, std::move(state), std::string("Unable to capture original Cayo loot stat: ") + (failedStat ? failedStat : "unknown"));
                    return;
                }

                if (!ApplyVerifiedStats(writes, *characterIndex, failedStat))
                {
                    const bool restored = RestoreStats(writes, *characterIndex);
                    CaptureState(state);
                    Finish(
                        false,
                        std::move(state),
                        restored
                            ? std::string("Cayo loot write failed verification at ") + (failedStat ? failedStat : "unknown") + "; original loot restored"
                            : std::string("Cayo loot write failed at ") + (failedStat ? failedStat : "unknown") + "; rollback could not be fully verified");
                    return;
                }

                bool planningRunning = false;
                const bool planningReloaded = ReloadPlanningBoard(planningRunning);
                CaptureState(state);
                state.planningRunning = planningRunning;
                state.planningReloaded = planningReloaded;

                TUTONES_LOG_INFO(
                    "heist.cayo",
                    std::string("Applied Cayo loot layout cash=") + (config.cash ? "on" : "off")
                        + " coke=" + (config.coke ? "on" : "off")
                        + " gold=" + (config.gold ? "on" : "off")
                        + " weed=" + (config.weed ? "on" : "off")
                        + " paintings=" + (config.paintings ? "on" : "off"));

                Finish(
                    true,
                    std::move(state),
                    planningReloaded
                        ? "Cayo loot applied and the planning board was reloaded"
                        : "Cayo loot applied; reopen the Kosatka planning board if the display is stale");
            });
        }

        [[nodiscard]] bool QueueApplyCuts(const CayoLootEnhanced173::CutArray& cuts)
        {
            if (!CayoLootEnhanced173::ValidCuts(cuts))
                return false;

            return Queue("Applying Cayo player cuts", [this, cuts] {
                CayoLootSnapshot state;
                if (!RequireOnlineAndNatives(state))
                {
                    Finish(false, std::move(state), "Join GTA Online and wait for the native backend before changing Cayo cuts");
                    return;
                }

                auto** globals = Script::ScriptRuntime::Get().Globals();
                state.globalsReady = globals != nullptr;
                if (!globals)
                {
                    Finish(false, std::move(state), "Enhanced script globals are unavailable");
                    return;
                }

                auto base = Script::ScriptGlobal(CayoLootEnhanced173::CutsGlobal)
                    .At(CayoLootEnhanced173::CutsStateOffset)
                    .At(CayoLootEnhanced173::CutsArrayOffset);

                std::array<std::int32_t*, CayoLootEnhanced173::CutCount> targets{};
                CayoLootEnhanced173::CutArray originals{};
                for (std::size_t index = 0; index < targets.size(); ++index)
                {
                    targets[index] = base.At(index, 1).As<std::int32_t>(globals);
                    if (!targets[index])
                    {
                        CaptureState(state);
                        Finish(false, std::move(state), "Cayo cut globals are unavailable");
                        return;
                    }
                    originals[index] = *targets[index];
                }

                bool verified = true;
                for (std::size_t index = 0; index < targets.size(); ++index)
                {
                    *targets[index] = cuts[index];
                    verified = *targets[index] == cuts[index] && verified;
                }

                if (!verified)
                {
                    bool restored = true;
                    for (std::size_t index = 0; index < targets.size(); ++index)
                    {
                        *targets[index] = originals[index];
                        restored = *targets[index] == originals[index] && restored;
                    }
                    CaptureState(state);
                    Finish(
                        false,
                        std::move(state),
                        restored
                            ? "Cayo cut write failed verification; original cuts restored"
                            : "Cayo cut write failed and rollback could not be fully verified");
                    return;
                }

                CaptureState(state);
                state.cuts = cuts;
                state.cutsReady = true;

                const int total = cuts[0] + cuts[1] + cuts[2] + cuts[3];
                TUTONES_LOG_INFO(
                    "heist.cayo",
                    std::string("Applied Cayo cuts p1=") + std::to_string(cuts[0])
                        + " p2=" + std::to_string(cuts[1])
                        + " p3=" + std::to_string(cuts[2])
                        + " p4=" + std::to_string(cuts[3])
                        + " total=" + std::to_string(total));

                Finish(true, std::move(state), "Cayo player cuts applied to the live planning state");
            });
        }

        [[nodiscard]] CayoLootSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            CayoLootSnapshot state = m_Snapshot;
            state.pending = m_Pending.load(std::memory_order_acquire);
            return state;
        }

    private:
        struct StatWrite final
        {
            const char* name{};
            int value{};
            int original{};
        };

        using LootWriteArray = std::array<StatWrite, 23>;

        CayoLootRuntime() = default;
        CayoLootRuntime(const CayoLootRuntime&) = delete;
        CayoLootRuntime& operator=(const CayoLootRuntime&) = delete;

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

            CayoLootSnapshot state;
            Finish(false, std::move(state), "GTA script-thread queue is unavailable");
            return false;
        }

        [[nodiscard]] bool RequireOnlineAndNatives(CayoLootSnapshot& state) const noexcept
        {
            const bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            state.sessionStarted = sessionStarted && *sessionStarted;
            state.nativeReady = Native::NativeRegistry::Get().CanInvokeOnCurrentThread();
            return state.sessionStarted && state.nativeReady;
        }

        [[nodiscard]] bool CaptureState(CayoLootSnapshot& state) const noexcept
        {
            const bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            state.sessionStarted = sessionStarted && *sessionStarted;
            state.nativeReady = Native::NativeRegistry::Get().CanInvokeOnCurrentThread();

            auto& scripts = Script::ScriptRuntime::Get();
            auto** globals = scripts.Globals();
            state.globalsReady = globals != nullptr;
            if (scripts.IsReady())
            {
                if (const auto* thread = scripts.FindThread(CayoPericoEnhanced173::PlanningHash))
                {
                    state.planningRunning = thread->context.threadId != 0
                        && thread->context.state != Types::ScriptThreadState::Killed;
                }
            }

            if (!state.sessionStarted || !state.nativeReady || !globals)
                return false;

            const auto characterIndex = Stats::GetCharIndex();
            if (!characterIndex)
                return false;

            const auto cashI = Stats::GetInt("MPX_H4LOOT_CASH_I", *characterIndex);
            const auto cashC = Stats::GetInt("MPX_H4LOOT_CASH_C", *characterIndex);
            const auto cokeI = Stats::GetInt("MPX_H4LOOT_COKE_I", *characterIndex);
            const auto cokeC = Stats::GetInt("MPX_H4LOOT_COKE_C", *characterIndex);
            const auto goldI = Stats::GetInt("MPX_H4LOOT_GOLD_I", *characterIndex);
            const auto goldC = Stats::GetInt("MPX_H4LOOT_GOLD_C", *characterIndex);
            const auto weedI = Stats::GetInt("MPX_H4LOOT_WEED_I", *characterIndex);
            const auto weedC = Stats::GetInt("MPX_H4LOOT_WEED_C", *characterIndex);
            const auto paintings = Stats::GetInt("MPX_H4LOOT_PAINT", *characterIndex);
            const auto cashValue = Stats::GetInt("MPX_H4LOOT_CASH_V", *characterIndex);
            const auto cokeValue = Stats::GetInt("MPX_H4LOOT_COKE_V", *characterIndex);
            const auto goldValue = Stats::GetInt("MPX_H4LOOT_GOLD_V", *characterIndex);
            const auto weedValue = Stats::GetInt("MPX_H4LOOT_WEED_V", *characterIndex);
            const auto paintingValue = Stats::GetInt("MPX_H4LOOT_PAINT_V", *characterIndex);

            state.lootReady = cashI && cashC && cokeI && cokeC && goldI && goldC
                && weedI && weedC && paintings
                && cashValue && cokeValue && goldValue && weedValue && paintingValue;
            if (state.lootReady)
            {
                state.cash = *cashI != 0 || *cashC != 0;
                state.coke = *cokeI != 0 || *cokeC != 0;
                state.gold = *goldI != 0 || *goldC != 0;
                state.weed = *weedI != 0 || *weedC != 0;
                state.paintings = *paintings != 0;
                state.cashValue = *cashValue;
                state.cokeValue = *cokeValue;
                state.goldValue = *goldValue;
                state.weedValue = *weedValue;
                state.paintingValue = *paintingValue;
            }

            auto cutsBase = Script::ScriptGlobal(CayoLootEnhanced173::CutsGlobal)
                .At(CayoLootEnhanced173::CutsStateOffset)
                .At(CayoLootEnhanced173::CutsArrayOffset);
            state.cutsReady = true;
            for (std::size_t index = 0; index < state.cuts.size(); ++index)
            {
                const auto* cut = cutsBase.At(index, 1).As<std::int32_t>(globals);
                if (!cut)
                {
                    state.cutsReady = false;
                    break;
                }
                state.cuts[index] = *cut;
            }

            return state.lootReady || state.cutsReady;
        }

        template<std::size_t N>
        [[nodiscard]] static bool CaptureOriginalStats(
            std::array<StatWrite, N>& writes,
            int characterIndex,
            const char*& failedStat) noexcept
        {
            failedStat = nullptr;
            for (auto& write : writes)
            {
                const auto original = Stats::GetInt(write.name, characterIndex);
                if (!original)
                {
                    failedStat = write.name;
                    return false;
                }
                write.original = *original;
            }
            return true;
        }

        template<std::size_t N>
        [[nodiscard]] static bool ApplyVerifiedStats(
            const std::array<StatWrite, N>& writes,
            int characterIndex,
            const char*& failedStat) noexcept
        {
            failedStat = nullptr;
            for (const auto& write : writes)
            {
                if (!Stats::SetInt(write.name, write.value, characterIndex))
                {
                    failedStat = write.name;
                    return false;
                }
                const auto readback = Stats::GetInt(write.name, characterIndex);
                if (!readback || *readback != write.value)
                {
                    failedStat = write.name;
                    return false;
                }
            }
            return true;
        }

        template<std::size_t N>
        [[nodiscard]] static bool RestoreStats(const std::array<StatWrite, N>& writes, int characterIndex) noexcept
        {
            bool restored = true;
            for (const auto& write : writes)
                restored = Stats::SetInt(write.name, write.original, characterIndex) && restored;

            for (const auto& write : writes)
            {
                const auto readback = Stats::GetInt(write.name, characterIndex);
                restored = readback && *readback == write.original && restored;
            }
            return restored;
        }

        [[nodiscard]] static bool ReloadPlanningBoard(bool& planningRunning) noexcept
        {
            planningRunning = false;
            auto& scripts = Script::ScriptRuntime::Get();
            if (!scripts.IsReady())
                return false;

            auto* thread = scripts.FindThread(CayoPericoEnhanced173::PlanningHash);
            if (!thread
                || thread->context.threadId == 0
                || thread->context.state == Types::ScriptThreadState::Killed)
            {
                return false;
            }

            planningRunning = true;
            int* reload = Script::ScriptLocal(thread, CayoPericoEnhanced173::PlanningReloadLocal).As<int>();
            if (!reload)
                return false;

            const int original = *reload;
            *reload = CayoPericoEnhanced173::PlanningReloadValue;
            const bool verified = *reload == CayoPericoEnhanced173::PlanningReloadValue;
            if (!verified)
                *reload = original;
            return verified;
        }

        void Finish(bool success, CayoLootSnapshot state, std::string message) noexcept
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
        CayoLootSnapshot m_Snapshot{};
    };
}
