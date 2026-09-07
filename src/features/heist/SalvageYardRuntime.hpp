#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/PlayerNatives.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../game/script/ScriptGlobal.hpp"
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
    namespace SalvageYardEnhanced173
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

        inline constexpr std::uint32_t PlanningHash = Joaat("vehrob_planning");

        // Enhanced GPBD_Flow root. Each player entry is 149 script slots and the
        // SALV23Flow block begins at entry offset 73 (37 slots total).
        inline constexpr std::size_t FlowGlobal = 1985024;
        inline constexpr std::size_t PlayerEntrySize = 149;
        inline constexpr std::size_t SalvageFlowOffset = 73;

        inline constexpr std::size_t GeneralFlagsOffset = 0;
        inline constexpr std::size_t FreemodeProgressOffset = 1;
        inline constexpr std::size_t InstanceProgressOffset = 2;
        inline constexpr std::size_t ScopeFlagsOffset = 4;
        inline constexpr std::size_t VehicleDataOffset = 21;
        inline constexpr std::size_t RobberyStatusArrayOffset = 31;
        inline constexpr std::size_t SalvageFlagsOffset = 35;

        inline constexpr std::size_t VehicleRobberyOffset = 0;
        inline constexpr std::size_t VehicleModelOffset = 1;
        inline constexpr std::size_t VehicleModsOffset = 2;
        inline constexpr std::size_t VehicleCanKeepOffset = 3;
        inline constexpr std::size_t VehicleSaleValueOffset = 4;
        inline constexpr std::size_t VehicleSlotOffset = 6;
        inline constexpr std::size_t VehicleWeekOffset = 7;
        inline constexpr std::size_t VehiclePackedOffset = 8;
        inline constexpr std::size_t VehicleDisruptOffset = 9;

        inline constexpr int RobberyCount = 5;
        inline constexpr std::array<const char*, RobberyCount> RobberyNames{
            "Cargo Ship Robbery",
            "Gangbanger Robbery",
            "Duggan Robbery",
            "Podium Robbery",
            "McTony Robbery",
        };

        inline constexpr std::uint32_t MandatoryPrepMask =
            (1u << 1) | (1u << 2) | (1u << 3) | (1u << 4) | (1u << 5);
        inline constexpr std::uint32_t PodiumGeneralFlag = 1u << 11;

        [[nodiscard]] inline const char* RobberyName(int robbery) noexcept
        {
            if (robbery < 0 || robbery >= RobberyCount)
                return "None / Unknown";
            return RobberyNames[static_cast<std::size_t>(robbery)];
        }
    }

    struct SalvageYardSnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool sessionStarted{};
        bool nativeReady{};
        bool globalsReady{};
        bool planningRunning{};
        bool prepFlagsComplete{};
        bool scopeComplete{};
        int playerId{-1};
        std::uint32_t generalFlags{};
        std::uint32_t freemodeProgress{};
        std::uint32_t instanceProgress{};
        std::uint32_t scopeFlags{};
        int currentRobbery{-1};
        std::int32_t vehicleModel{};
        int vehicleMods{};
        int canKeep{};
        int saleValue{};
        int vehicleSlot{-1};
        int vehicleWeek{-1};
        int packedVehicleState{};
        int disruptionState{};
        std::array<int, 3> weeklyStatuses{{-1, -1, -1}};
        std::uint32_t salvageFlags{};
        std::string message{"Press Refresh Salvage State"};
    };

    class SalvageYardRuntime final
    {
    public:
        static SalvageYardRuntime& Get() noexcept
        {
            static SalvageYardRuntime instance;
            return instance;
        }

        [[nodiscard]] bool QueueRefresh()
        {
            return Queue("Reading Enhanced Salvage Yard planning state", [this] {
                SalvageYardSnapshot state;
                const bool success = CaptureState(state);
                Finish(
                    success,
                    std::move(state),
                    success
                        ? "Salvage Yard planning state refreshed"
                        : "Unable to read the Enhanced Salvage Yard planning state");
            });
        }

        [[nodiscard]] bool QueueCompleteCurrentPreps()
        {
            using namespace SalvageYardEnhanced173;
            return Queue("Completing verified Salvage Yard prep flags", [this] {
                SalvageYardSnapshot state;
                if (!CaptureState(state))
                {
                    Finish(false, std::move(state), "Open GTA Online and establish a Salvage Yard robbery before changing prep state");
                    return;
                }

                if (state.currentRobbery < 0 || state.currentRobbery >= RobberyCount)
                {
                    Finish(false, std::move(state), "No valid current Salvage Yard robbery is selected");
                    return;
                }

                auto& scripts = Script::ScriptRuntime::Get();
                auto** globals = scripts.Globals();
                if (!globals)
                {
                    Finish(false, std::move(state), "Salvage Yard script globals are unavailable");
                    return;
                }

                const auto flow = PlayerFlow(state.playerId);
                auto* generalFlags = flow.At(GeneralFlagsOffset).As<std::int32_t>(globals);
                auto* progress = flow.At(FreemodeProgressOffset).As<std::int32_t>(globals);
                auto* scope = flow.At(ScopeFlagsOffset).As<std::int32_t>(globals);
                if (!generalFlags || !progress || !scope)
                {
                    Finish(false, std::move(state), "Unable to resolve Salvage Yard prep globals");
                    return;
                }

                const std::int32_t originalGeneral = *generalFlags;
                const std::int32_t originalProgress = *progress;
                const std::int32_t originalScope = *scope;

                const auto robberyBit = static_cast<std::uint32_t>(1u << state.currentRobbery);
                std::uint32_t wantedGeneral = static_cast<std::uint32_t>(originalGeneral);
                const std::uint32_t wantedProgress =
                    static_cast<std::uint32_t>(originalProgress) | MandatoryPrepMask;
                const std::uint32_t wantedScope =
                    static_cast<std::uint32_t>(originalScope) | robberyBit;

                // vehrob_planning::func_389 requires general flag bit 11 for the
                // Podium robbery in addition to the common scope/progress checks.
                if (state.currentRobbery == 3)
                    wantedGeneral |= PodiumGeneralFlag;

                *generalFlags = static_cast<std::int32_t>(wantedGeneral);
                *progress = static_cast<std::int32_t>(wantedProgress);
                *scope = static_cast<std::int32_t>(wantedScope);

                const bool verified =
                    static_cast<std::uint32_t>(*generalFlags) == wantedGeneral
                    && static_cast<std::uint32_t>(*progress) == wantedProgress
                    && static_cast<std::uint32_t>(*scope) == wantedScope;

                if (!verified)
                {
                    *generalFlags = originalGeneral;
                    *progress = originalProgress;
                    *scope = originalScope;
                    const bool restored = *generalFlags == originalGeneral
                        && *progress == originalProgress
                        && *scope == originalScope;
                    CaptureState(state);
                    Finish(
                        false,
                        std::move(state),
                        restored
                            ? "Salvage Yard prep write failed verification; original state restored"
                            : "Salvage Yard prep write failed and rollback could not be fully verified");
                    return;
                }

                CaptureState(state);
                TUTONES_LOG_INFO(
                    "heist.salvage",
                    std::string("Completed verified prep flags for ") + RobberyName(state.currentRobbery));
                Finish(
                    true,
                    std::move(state),
                    state.planningRunning
                        ? "Salvage Yard prep flags completed; close and reopen the planning board if it does not redraw immediately"
                        : "Salvage Yard prep flags completed; open the planning board to load the updated state");
            });
        }

        [[nodiscard]] SalvageYardSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            SalvageYardSnapshot state = m_Snapshot;
            state.pending = m_Pending.load(std::memory_order_acquire);
            return state;
        }

    private:
        SalvageYardRuntime() = default;
        SalvageYardRuntime(const SalvageYardRuntime&) = delete;
        SalvageYardRuntime& operator=(const SalvageYardRuntime&) = delete;

        [[nodiscard]] static Script::ScriptGlobal PlayerFlow(int playerId) noexcept
        {
            return Script::ScriptGlobal(SalvageYardEnhanced173::FlowGlobal)
                .At(static_cast<std::size_t>(playerId), SalvageYardEnhanced173::PlayerEntrySize)
                .At(SalvageYardEnhanced173::SalvageFlowOffset);
        }

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

            SalvageYardSnapshot state;
            Finish(false, std::move(state), "GTA script-thread queue is unavailable");
            return false;
        }

        [[nodiscard]] bool CaptureState(SalvageYardSnapshot& state) const noexcept
        {
            using namespace SalvageYardEnhanced173;

            const bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            state.sessionStarted = sessionStarted && *sessionStarted;
            state.nativeReady = Native::NativeRegistry::Get().CanInvokeOnCurrentThread();

            auto& scripts = Script::ScriptRuntime::Get();
            auto** globals = scripts.Globals();
            state.globalsReady = globals != nullptr;
            if (scripts.IsReady())
            {
                if (const auto* thread = scripts.FindThread(PlanningHash))
                {
                    state.planningRunning = thread->context.threadId != 0
                        && thread->context.state != Types::ScriptThreadState::Killed;
                }
            }

            if (!state.sessionStarted || !state.nativeReady || !globals)
                return false;

            const auto player = PlayerNatives::PlayerId();
            if (!player || *player < 0 || *player >= 32)
                return false;

            state.playerId = *player;
            const auto flow = PlayerFlow(*player);
            const auto vehicle = flow.At(VehicleDataOffset);

            const auto* generalFlags = flow.At(GeneralFlagsOffset).As<std::int32_t>(globals);
            const auto* progress = flow.At(FreemodeProgressOffset).As<std::int32_t>(globals);
            const auto* instanceProgress = flow.At(InstanceProgressOffset).As<std::int32_t>(globals);
            const auto* scope = flow.At(ScopeFlagsOffset).As<std::int32_t>(globals);
            const auto* robbery = vehicle.At(VehicleRobberyOffset).As<std::int32_t>(globals);
            const auto* model = vehicle.At(VehicleModelOffset).As<std::int32_t>(globals);
            const auto* mods = vehicle.At(VehicleModsOffset).As<std::int32_t>(globals);
            const auto* canKeep = vehicle.At(VehicleCanKeepOffset).As<std::int32_t>(globals);
            const auto* saleValue = vehicle.At(VehicleSaleValueOffset).As<std::int32_t>(globals);
            const auto* slot = vehicle.At(VehicleSlotOffset).As<std::int32_t>(globals);
            const auto* week = vehicle.At(VehicleWeekOffset).As<std::int32_t>(globals);
            const auto* packed = vehicle.At(VehiclePackedOffset).As<std::int32_t>(globals);
            const auto* disrupt = vehicle.At(VehicleDisruptOffset).As<std::int32_t>(globals);
            const auto* flags = flow.At(SalvageFlagsOffset).As<std::int32_t>(globals);

            if (!generalFlags || !progress || !instanceProgress || !scope || !robbery || !model
                || !mods || !canKeep || !saleValue || !slot || !week || !packed || !disrupt || !flags)
            {
                return false;
            }

            state.generalFlags = static_cast<std::uint32_t>(*generalFlags);
            state.freemodeProgress = static_cast<std::uint32_t>(*progress);
            state.instanceProgress = static_cast<std::uint32_t>(*instanceProgress);
            state.scopeFlags = static_cast<std::uint32_t>(*scope);
            state.currentRobbery = *robbery;
            state.vehicleModel = *model;
            state.vehicleMods = *mods;
            state.canKeep = *canKeep;
            state.saleValue = *saleValue;
            state.vehicleSlot = *slot;
            state.vehicleWeek = *week;
            state.packedVehicleState = *packed;
            state.disruptionState = *disrupt;
            state.salvageFlags = static_cast<std::uint32_t>(*flags);

            const auto statuses = flow.At(RobberyStatusArrayOffset);
            for (std::size_t index = 0; index < state.weeklyStatuses.size(); ++index)
            {
                const auto* status = statuses.At(index, 1).As<std::int32_t>(globals);
                if (!status)
                    return false;
                state.weeklyStatuses[index] = *status;
            }

            if (state.currentRobbery >= 0 && state.currentRobbery < RobberyCount)
            {
                const auto robberyBit = static_cast<std::uint32_t>(1u << state.currentRobbery);
                state.scopeComplete = (state.scopeFlags & robberyBit) != 0;
                state.prepFlagsComplete =
                    (state.freemodeProgress & MandatoryPrepMask) == MandatoryPrepMask
                    && (state.currentRobbery != 3 || (state.generalFlags & PodiumGeneralFlag) != 0);
            }

            return true;
        }

        void Finish(bool success, SalvageYardSnapshot state, std::string message) noexcept
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
        SalvageYardSnapshot m_Snapshot{};
    };
}
