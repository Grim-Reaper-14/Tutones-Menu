#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/PlayerNatives.hpp"
#include "../../game/Stats.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../game/script/ScriptLocal.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../game/tunables/TunableRegistry.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace Tutones::Game::Business
{
    namespace HangarEnhanced173
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

        inline constexpr std::uint32_t SmugglerScriptHash = Joaat("gb_smuggler");
        inline constexpr std::uint32_t HackerTruckScriptHash = Joaat("apphackertruck");
        inline constexpr std::uint32_t BusinessHubScriptHash = Joaat("am_mp_business_hub");

        inline constexpr std::size_t GpbdFmGlobal = 1845347;
        inline constexpr std::size_t GpbdFmPlayerSize = 884;
        inline constexpr std::size_t PropertyDataOffset = 260;
        inline constexpr std::size_t HangarDataOffset = 304;
        inline constexpr std::size_t HangarPropertyIdOffset = 0;
        inline constexpr std::size_t HangarTotalCargoOffset = 3;
        inline constexpr std::size_t HangarSetupDoneOffset = 5;
        inline constexpr int HangarCapacity = 50;

        inline constexpr int SourceRequestPackedBool = 36828;

        // Updated in the same Enhanced offset refresh that moved other current
        // mission locals. The sale structure is epctLocal_1998 in gb_smuggler.
        inline constexpr std::size_t SmugglerSaleBaseLocal = 1998;
        inline constexpr std::ptrdiff_t SaleToDeliverOffset = 1035;
        inline constexpr std::ptrdiff_t SaleDeliveredOffset = 1078;

        inline constexpr std::uint32_t SourceMissionTransactionHash =
            Joaat("HANGAR_CONTRABAND_MISSION_0_t0_v0");
        inline constexpr std::uint32_t SourceMissionStatHash =
            Joaat("MP_STAT_HANGAR_CONTRABAND_MISSION_v0");

        inline constexpr std::array<const char*, 9> CargoNames{
            "Animal Materials",
            "Art & Antiques",
            "Chemicals",
            "Jewelry & Gems",
            "Counterfeit Goods",
            "Medical Supplies",
            "Narcotics",
            "Tobacco & Alcohol",
            "Mixed / Random",
        };

        inline constexpr std::array<std::string_view, 9> PriceTunables{
            "SMUG_SELL_PRICE_PER_CRATE_ANIMAL_MATERIALS",
            "SMUG_SELL_PRICE_PER_CRATE_ART_AND_ANTIQUES",
            "SMUG_SELL_PRICE_PER_CRATE_CHEMICALS",
            "SMUG_SELL_PRICE_PER_CRATE_JEWELRY_AND_GEMSTONES",
            "SMUG_SELL_PRICE_PER_CRATE_COUNTERFEIT_GOODS",
            "SMUG_SELL_PRICE_PER_CRATE_MEDICAL_SUPPLIES",
            "SMUG_SELL_PRICE_PER_CRATE_NARCOTICS",
            "SMUG_SELL_PRICE_PER_CRATE_TOBACCO_AND_ALCOHOL",
            "SMUG_SELL_PRICE_PER_CRATE_MIXED",
        };
    }

    struct HangarSnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool sessionStarted{};
        bool nativeReady{};
        bool scriptRuntimeReady{};
        bool globalsReady{};
        bool smugglerRunning{};
        bool hackerTruckRunning{};
        bool businessHubRunning{};
        bool sourceRequestReadable{};
        bool sourceRequestSet{};
        bool saleLocalsReadable{};
        bool tunableRegistryReady{};
        bool payoutTunablesReadable{};
        int playerId{-1};
        int propertyId{-1};
        int totalCargo{-1};
        int remainingCapacity{-1};
        int setupDone{-1};
        int saleToDeliver{-1};
        int saleDelivered{-1};
        std::array<int, 9> cratePrices{{-1, -1, -1, -1, -1, -1, -1, -1, -1}};
        int bonusThresholdLow{-1};
        int bonusThresholdMedium{-1};
        int bonusThresholdHigh{-1};
        float bonusPercentLow{-1.0f};
        float bonusPercentMedium{-1.0f};
        float bonusPercentHigh{-1.0f};
        float ronsCut{-1.0f};
        float highDemandBonus{-1.0f};
        int sellCooldown{-1};
        std::string message{"Press Refresh Hangar Runtime"};
    };

    class HangarRuntime final
    {
    public:
        static HangarRuntime& Get() noexcept
        {
            static HangarRuntime instance;
            return instance;
        }

        [[nodiscard]] bool QueueRefresh()
        {
            return Queue("Reading Enhanced Hangar / Air-Freight runtime", [this] {
                HangarSnapshot state;
                const bool success = CaptureState(state);
                Finish(
                    success,
                    std::move(state),
                    success
                        ? "Enhanced Hangar / Air-Freight runtime refreshed"
                        : "Unable to read the complete Enhanced Hangar state");
            });
        }

        [[nodiscard]] bool QueueSourceCargo()
        {
            using namespace HangarEnhanced173;
            return Queue("Requesting Hangar source cargo through the verified packed-stat trigger", [this] {
                HangarSnapshot state;
                if (!CaptureState(state))
                {
                    Finish(false, std::move(state), "Join GTA Online and wait for the script/native backends before sourcing Hangar cargo");
                    return;
                }

                if (state.totalCargo >= HangarCapacity)
                {
                    Finish(false, std::move(state), "Hangar is already at the verified 50-crate capacity");
                    return;
                }

                if (!Stats::SetPackedBool(SourceRequestPackedBool, true))
                {
                    Finish(false, std::move(state), "Packed-stat 36828 Hangar source request was rejected by the native backend");
                    return;
                }

                if (const auto requested = Stats::GetPackedBool(SourceRequestPackedBool))
                {
                    state.sourceRequestReadable = true;
                    state.sourceRequestSet = *requested;
                }

                TUTONES_LOG_INFO("business.hangar", "Queued Hangar cargo source request through packed stat 36828");
                Finish(true, std::move(state), "Hangar source request queued; Rockstar's normal sourcing flow owns the stock update");
            });
        }

        [[nodiscard]] HangarSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            HangarSnapshot state = m_State;
            state.pending = m_Pending.load(std::memory_order_acquire);
            return state;
        }

    private:
        HangarRuntime() = default;

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

        template<typename T>
        [[nodiscard]] static std::optional<T> ReadTunable(std::string_view name, std::int64_t** globals) noexcept
        {
            if (!globals)
                return std::nullopt;
            const auto resolved = Tunables::TunableRegistry::Get().Resolve(name);
            if (!resolved)
                return std::nullopt;
            const auto* value = resolved->As<T>(globals);
            if (!value)
                return std::nullopt;
            return *value;
        }

        [[nodiscard]] bool CaptureState(HangarSnapshot& state) const noexcept
        {
            using namespace HangarEnhanced173;

            const bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            state.sessionStarted = sessionStarted && *sessionStarted;
            state.nativeReady = Native::NativeRegistry::Get().CanInvokeOnCurrentThread();

            auto& scripts = Script::ScriptRuntime::Get();
            state.scriptRuntimeReady = scripts.IsReady();
            auto** globals = scripts.Globals();
            state.globalsReady = globals != nullptr;

            const auto running = [&](std::uint32_t hash) noexcept {
                const auto* thread = scripts.FindThread(hash);
                return thread && thread->stack
                    && thread->context.threadId != 0
                    && thread->context.state != Types::ScriptThreadState::Killed;
            };

            if (state.scriptRuntimeReady)
            {
                state.smugglerRunning = running(SmugglerScriptHash);
                state.hackerTruckRunning = running(HackerTruckScriptHash);
                state.businessHubRunning = running(BusinessHubScriptHash);
            }

            if (!state.sessionStarted || !state.nativeReady || !state.scriptRuntimeReady || !globals)
                return false;

            const auto player = PlayerNatives::PlayerId();
            if (!player || *player < 0 || *player >= 32)
                return false;
            state.playerId = *player;

            const auto hangar = Script::ScriptGlobal(GpbdFmGlobal)
                .At(static_cast<std::size_t>(*player), GpbdFmPlayerSize)
                .At(PropertyDataOffset)
                .At(HangarDataOffset);

            const auto* propertyId = hangar.At(HangarPropertyIdOffset).As<std::int32_t>(globals);
            const auto* totalCargo = hangar.At(HangarTotalCargoOffset).As<std::int32_t>(globals);
            const auto* setupDone = hangar.At(HangarSetupDoneOffset).As<std::int32_t>(globals);
            if (!propertyId || !totalCargo || !setupDone)
                return false;

            state.propertyId = *propertyId;
            state.totalCargo = *totalCargo;
            state.remainingCapacity = std::clamp(HangarCapacity - state.totalCargo, 0, HangarCapacity);
            state.setupDone = *setupDone;

            if (const auto request = Stats::GetPackedBool(SourceRequestPackedBool))
            {
                state.sourceRequestReadable = true;
                state.sourceRequestSet = *request;
            }

            if (state.smugglerRunning)
            {
                if (auto* thread = scripts.FindThread(SmugglerScriptHash))
                {
                    const auto* toDeliver = Script::ScriptLocal(thread, SmugglerSaleBaseLocal)
                        .At(SaleToDeliverOffset)
                        .As<std::int32_t>();
                    const auto* delivered = Script::ScriptLocal(thread, SmugglerSaleBaseLocal)
                        .At(SaleDeliveredOffset)
                        .As<std::int32_t>();
                    if (toDeliver && delivered)
                    {
                        state.saleLocalsReadable = true;
                        state.saleToDeliver = *toDeliver;
                        state.saleDelivered = *delivered;
                    }
                }
            }

            auto& tunables = Tunables::TunableRegistry::Get();
            state.tunableRegistryReady = tunables.Initialized();
            if (state.tunableRegistryReady)
            {
                bool allPrices = true;
                for (std::size_t index = 0; index < PriceTunables.size(); ++index)
                {
                    const auto value = ReadTunable<std::int32_t>(PriceTunables[index], globals);
                    if (!value)
                    {
                        allPrices = false;
                        continue;
                    }
                    state.cratePrices[index] = *value;
                }

                const auto thresholdLow = ReadTunable<std::int32_t>("SMUG_SELL_CRATE_BONUS_THRESHOLD_LOW", globals);
                const auto thresholdMedium = ReadTunable<std::int32_t>("SMUG_SELL_CRATE_BONUS_THRESHOLD_MEDIUM", globals);
                const auto thresholdHigh = ReadTunable<std::int32_t>("SMUG_SELL_CRATE_BONUS_THRESHOLD_HIGH", globals);
                const auto percentLow = ReadTunable<float>("SMUG_SELL_CRATE_BONUS_PERCENTAGE_LOW", globals);
                const auto percentMedium = ReadTunable<float>("SMUG_SELL_CRATE_BONUS_PERCENTAGE_MEDIUM", globals);
                const auto percentHigh = ReadTunable<float>("SMUG_SELL_CRATE_BONUS_PERCENTAGE_HIGH", globals);
                const auto ron = ReadTunable<float>("SMUG_SELL_RONS_CUT", globals);
                const auto highDemand = ReadTunable<float>("SMUG_SELL_HIGH_DEMAND_BONUS_PERCENTAGE", globals);
                const auto sellCd = ReadTunable<std::int32_t>("SMUG_SELL_SELL_COOLDOWN_TIMER", globals);

                if (thresholdLow) state.bonusThresholdLow = *thresholdLow;
                if (thresholdMedium) state.bonusThresholdMedium = *thresholdMedium;
                if (thresholdHigh) state.bonusThresholdHigh = *thresholdHigh;
                if (percentLow) state.bonusPercentLow = *percentLow;
                if (percentMedium) state.bonusPercentMedium = *percentMedium;
                if (percentHigh) state.bonusPercentHigh = *percentHigh;
                if (ron) state.ronsCut = *ron;
                if (highDemand) state.highDemandBonus = *highDemand;
                if (sellCd) state.sellCooldown = *sellCd;

                state.payoutTunablesReadable = allPrices
                    && thresholdLow && thresholdMedium && thresholdHigh
                    && percentLow && percentMedium && percentHigh
                    && ron && highDemand && sellCd;
            }

            TUTONES_LOG_DEBUG(
                "business.hangar",
                "Hangar stock=" + std::to_string(state.totalCargo)
                    + "/50 property=" + std::to_string(state.propertyId)
                    + " smuggler=" + (state.smugglerRunning ? std::string("running") : std::string("idle")));
            return true;
        }

        void SetPending(std::string message)
        {
            std::scoped_lock lock(m_Mutex);
            m_State.haveResult = false;
            m_State.lastSucceeded = false;
            m_State.message = std::move(message);
        }

        void Finish(bool success, HangarSnapshot state, std::string message)
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
        HangarSnapshot m_State{};
    };
}
