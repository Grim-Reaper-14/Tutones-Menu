#pragma once

#include "BusinessScriptMonitorRuntime.hpp"
#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/native/NativeCallContext.hpp"
#include "../../game/native/NativeInvoker.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <Windows.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::Business
{
    struct VehicleCargoAutoSourceSnapshot final
    {
        bool enabled{};
        bool pending{};
        bool sessionReady{};
        bool launcherReady{};
        bool vehicleCargoRunning{};
        bool waitingForStart{};
        bool lastSucceeded{};
        int launcherState{-1};
        int launcherIndex{-1};
        std::string message{"Auto Source is off"};
    };

    class VehicleCargoAutoSourceRuntime final
    {
    public:
        // GTA5 Enhanced b1158.13 decompiled flow:
        // apphackertruck -> activity 178 -> TU event 1613825825 -> freemode host
        // -> launcher 73 -> GB_VEHICLE_EXPORT. Activity 188 / launcher 74 is sell.
        static constexpr int VehicleCargoSourceMissionId = 178;
        static constexpr int VehicleCargoSellMissionId = 188;
        static constexpr int VehicleCargoSourceLauncherIndex = 73;
        static constexpr std::uint32_t VehicleCargoLaunchEvent = 1613825825u;

        // apphackertruck func_425/426/427 forwards these three Import/Export
        // setup slots with the launch event.
        static constexpr std::size_t FreemodeBusinessGlobal = 2733326;
        static constexpr std::size_t ImportExportSetupRootOffset = 3989;
        static constexpr std::size_t ImportExportSetupAOffset = 348;
        static constexpr std::size_t ImportExportSetupBOffset = 349;
        static constexpr std::size_t ImportExportSetupCOffset = 350;

        // GPBD_FM_3: Global_1893070[player /*615*/].f_10 is BOSS_GOON.
        // apphackertruck func_431 writes f_33 = activity, while func_435 writes
        // VEHICLE_EXPORT at f_188 before it sends the TU launch event.
        static constexpr std::size_t PlayerOrganizationGlobal = 1893070;
        static constexpr std::size_t PlayerOrganizationEntrySize = 615;
        static constexpr std::size_t PlayerOrganizationMissionOffset = 10 + 33;
        static constexpr std::size_t PlayerOrganizationVehicleExportOffset = 10 + 188;
        static constexpr std::size_t VehicleExportArraySize = 4;
        static constexpr std::size_t VehicleExportPadOffset = 5;

        // GPBD_FM: Global_1845347[player /*884*/].f_260 is PROPERTY_DATA.
        // PROPERTY_DATA::IEWarehouseData starts at f_156 and is:
        // Index, NumVehicles, SCR_ARRAY<40> Vehicles, PAD, variation.
        static constexpr std::size_t PlayerFreemodeGlobal = 1845347;
        static constexpr std::size_t PlayerFreemodeEntrySize = 884;
        static constexpr std::size_t PropertyDataOffset = 260;
        static constexpr std::size_t IEWarehouseDataOffset = 156;
        static constexpr std::size_t IEWarehouseIndexOffset = 0;
        static constexpr std::size_t IEWarehouseVehicleCountOffset = 1;
        static constexpr std::size_t IEWarehouseVehiclesOffset = 2;
        static constexpr int IEWarehouseVehicleSlots = 40;
        static constexpr int MaxPlayers = 32;

        static VehicleCargoAutoSourceRuntime& Get() noexcept
        {
            static VehicleCargoAutoSourceRuntime instance;
            return instance;
        }

        void SetEnabled(bool enabled) noexcept
        {
            const bool previous = m_Enabled.exchange(enabled, std::memory_order_acq_rel);
            if (previous == enabled)
                return;

            m_NextPollMs.store(0, std::memory_order_release);
            if (enabled)
                m_ResetRequested.store(true, std::memory_order_release);

            std::scoped_lock lock(m_Mutex);
            m_Message = enabled
                ? "Auto Source armed; waiting for the Enhanced freemode Vehicle Cargo route"
                : (m_WaitingForStartSnapshot
                    ? "Auto Source is off; finishing the pending Enhanced source request"
                    : "Auto Source is off");
        }

        [[nodiscard]] bool Enabled() const noexcept
        {
            return m_Enabled.load(std::memory_order_acquire);
        }

        bool QueueSourceNow()
        {
            return QueuePoll(true);
        }

        void Tick() noexcept
        {
            const bool enabled = m_Enabled.load(std::memory_order_acquire);
            bool followPending = false;
            if (!enabled)
            {
                std::scoped_lock lock(m_Mutex);
                followPending = m_WaitingForStartSnapshot;
            }

            if (!enabled && !followPending)
                return;

            const auto now = NowMs();
            auto next = m_NextPollMs.load(std::memory_order_acquire);
            if (now < next)
                return;
            if (!m_NextPollMs.compare_exchange_strong(next, now + PollIntervalMs, std::memory_order_acq_rel))
                return;

            static_cast<void>(QueuePoll(!enabled && followPending));
        }

        [[nodiscard]] VehicleCargoAutoSourceSnapshot Snapshot() const
        {
            VehicleCargoAutoSourceSnapshot out;
            out.enabled = m_Enabled.load(std::memory_order_acquire);
            out.pending = m_Pending.load(std::memory_order_acquire);

            std::scoped_lock lock(m_Mutex);
            out.sessionReady = m_SessionReady;
            out.launcherReady = m_LauncherReady;
            out.vehicleCargoRunning = m_VehicleCargoRunning;
            out.waitingForStart = m_WaitingForStartSnapshot;
            out.lastSucceeded = m_LastSucceeded;
            out.launcherState = m_LauncherStateSnapshot;
            out.launcherIndex = m_LauncherIndexSnapshot;
            out.message = m_Message;
            return out;
        }

    private:
        struct NativeProgram final
        {
            std::byte pad00[0x2C]{};
            std::uint32_t nativeCount{};
            std::byte pad30[0x10]{};
            Native::NativeHandler* nativeEntrypoints{};
            std::byte pad48[0x38]{};
        };

        static_assert(offsetof(NativeProgram, nativeCount) == 0x2C);
        static_assert(offsetof(NativeProgram, nativeEntrypoints) == 0x40);
        static_assert(sizeof(NativeProgram) == 0x80);

        // Current GTA5 Enhanced targets.
        static constexpr std::uint64_t NetworkGetHostOfScriptHash = 0xF1A4B8228C5E44B7ull;
        static constexpr std::uint64_t SendTuScriptEventNewHash = 0x71A6F836422FDD2Bull;

        static constexpr std::int64_t PollIntervalMs = 500;
        static constexpr std::int64_t MissionStartTimeoutMs = 15000;
        static constexpr std::int64_t RetryBackoffMs = 5000;
        static constexpr std::int64_t MissionEndSettleMs = 1500;

        VehicleCargoAutoSourceRuntime() = default;

        [[nodiscard]] static std::int64_t NowMs() noexcept
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        }

        [[nodiscard]] static bool IsExecutable(std::uintptr_t address) noexcept
        {
            if (!address)
                return false;

            MEMORY_BASIC_INFORMATION memory{};
            if (::VirtualQuery(reinterpret_cast<const void*>(address), &memory, sizeof(memory)) != sizeof(memory))
                return false;
            if (memory.State != MEM_COMMIT || (memory.Protect & PAGE_GUARD) || memory.Protect == PAGE_NOACCESS)
                return false;

            switch (memory.Protect & 0xFF)
            {
            case PAGE_EXECUTE:
            case PAGE_EXECUTE_READ:
            case PAGE_EXECUTE_READWRITE:
            case PAGE_EXECUTE_WRITECOPY:
                return true;
            default:
                return false;
            }
        }

        bool ResolveLaunchNatives() noexcept
        {
            if (m_NetworkGetHostOfScript && m_SendTuScriptEventNew)
                return true;
            if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread())
                return false;

            const auto init = GamePointers::Get().InitNativeTables();
            if (!init)
                return false;

            std::array<std::uint64_t, 2> slots{{
                NetworkGetHostOfScriptHash,
                SendTuScriptEventNewHash,
            }};

            NativeProgram program{};
            program.nativeCount = static_cast<std::uint32_t>(slots.size());
            program.nativeEntrypoints = reinterpret_cast<Native::NativeHandler*>(slots.data());
            init(&program);

            if (!IsExecutable(static_cast<std::uintptr_t>(slots[0]))
                || !IsExecutable(static_cast<std::uintptr_t>(slots[1])))
            {
                m_NetworkGetHostOfScript = nullptr;
                m_SendTuScriptEventNew = nullptr;
                return false;
            }

            m_NetworkGetHostOfScript = reinterpret_cast<Native::NativeHandler>(slots[0]);
            m_SendTuScriptEventNew = reinterpret_cast<Native::NativeHandler>(slots[1]);
            return true;
        }

        bool ScriptHost(const char* scriptName, int& outHost) noexcept
        {
            outHost = -1;
            if (!scriptName || !ResolveLaunchNatives())
                return false;

            Native::CallContext context;
            if (!context.PushArg(scriptName)
                || !context.PushArg(std::int32_t{-1})
                || !context.PushArg(std::int32_t{0}))
            {
                return false;
            }

            m_NetworkGetHostOfScript(&context);
            context.FixVectors();
            outHost = context.GetReturnValue<std::int32_t>();
            return outHost >= 0 && outHost < MaxPlayers;
        }

        bool QueuePoll(bool manual)
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            if (Runtime::GameRuntime::Get().Enqueue([this, manual] { Evaluate(manual); }))
                return true;

            Finish(false, false, false, false, false, -1, -1, "Game-thread queue unavailable");
            return false;
        }

        void ClearCapturedSourceState() noexcept
        {
            m_PreviousMissionState = -1;
            m_PreviousVehicleExportState.fill(0);
            m_CapturedPlayerId = -1;
            m_RequestedSourceVariation = 0;
            m_HavePreviousSourceState = false;
        }

        void RestoreSourceLaunchState() noexcept
        {
            if (!m_HavePreviousSourceState)
                return;

            auto* pages = GamePointers::Get().ScriptGlobals();
            if (pages && m_CapturedPlayerId >= 0 && m_CapturedPlayerId < MaxPlayers)
            {
                const auto orgEntry = Script::ScriptGlobal(PlayerOrganizationGlobal)
                    .At(static_cast<std::size_t>(m_CapturedPlayerId), PlayerOrganizationEntrySize);
                int* mission = orgEntry.At(PlayerOrganizationMissionOffset).As<int>(pages);
                const auto exportArray = orgEntry.At(PlayerOrganizationVehicleExportOffset);
                int* exportCount = exportArray.As<int>(pages);
                std::array<int*, VehicleExportArraySize> slots{};
                for (std::size_t i = 0; i < slots.size(); ++i)
                    slots[i] = exportArray.At(i, 1).As<int>(pages);
                int* exportPad = orgEntry.At(PlayerOrganizationVehicleExportOffset + VehicleExportPadOffset).As<int>(pages);

                if (mission && *mission == VehicleCargoSourceMissionId)
                    *mission = m_PreviousMissionState;

                bool canRestoreExport = exportCount && *exportCount == static_cast<int>(VehicleExportArraySize) && exportPad;
                for (const auto* slot : slots)
                    canRestoreExport = canRestoreExport && slot != nullptr;

                if (canRestoreExport
                    && *slots[0] == m_RequestedSourceVariation
                    && *slots[1] == 0
                    && *slots[2] == 0
                    && *slots[3] == 0
                    && *exportPad == 0)
                {
                    for (std::size_t i = 0; i < slots.size(); ++i)
                        *slots[i] = m_PreviousVehicleExportState[i];
                    *exportPad = m_PreviousVehicleExportState[VehicleExportArraySize];
                }
            }

            ClearCapturedSourceState();
        }

        void ResetCycleState() noexcept
        {
            RestoreSourceLaunchState();
            m_WaitingForStart = false;
            m_WaitingSinceMs = 0;
            m_SawMissionRunning = false;
            m_NotBeforeMs = 0;
        }

        [[nodiscard]] static bool StoredCodeMatches(int stored, int candidate) noexcept
        {
            return stored == candidate || stored == 1000 + candidate;
        }

        bool SelectSourceVariation(std::int64_t** pages, int playerId, int& outVariation,
            int& outWarehouseVehicleCount, std::string& outFailure) noexcept
        {
            outVariation = 0;
            outWarehouseVehicleCount = 0;

            if (!pages || playerId < 0 || playerId >= MaxPlayers)
            {
                outFailure = "Enhanced Vehicle Warehouse globals unavailable";
                return false;
            }

            const auto playerEntry = Script::ScriptGlobal(PlayerFreemodeGlobal)
                .At(static_cast<std::size_t>(playerId), PlayerFreemodeEntrySize);
            const auto warehouseBase = playerEntry.At(PropertyDataOffset + IEWarehouseDataOffset);

            int* warehouseIndex = warehouseBase.At(IEWarehouseIndexOffset).As<int>(pages);
            int* vehicleCount = warehouseBase.At(IEWarehouseVehicleCountOffset).As<int>(pages);
            const auto vehicles = warehouseBase.At(IEWarehouseVehiclesOffset);
            int* vehiclesCountHeader = vehicles.As<int>(pages);

            if (!warehouseIndex || !vehicleCount || !vehiclesCountHeader || *vehiclesCountHeader != IEWarehouseVehicleSlots)
            {
                outFailure = "Enhanced Vehicle Warehouse layout validation failed";
                return false;
            }

            if (*warehouseIndex == 0)
            {
                outFailure = "Purchase a Vehicle Warehouse before using Auto Source";
                return false;
            }

            int count = *vehicleCount;
            if (count < 0)
                count = 0;
            if (count > IEWarehouseVehicleSlots)
                count = IEWarehouseVehicleSlots;
            outWarehouseVehicleCount = count;

            std::array<int, IEWarehouseVehicleSlots> stored{};
            for (int i = 0; i < IEWarehouseVehicleSlots; ++i)
            {
                int* value = vehicles.At(static_cast<std::size_t>(i), 1).As<int>(pages);
                if (!value)
                {
                    outFailure = "Enhanced Vehicle Warehouse inventory is unavailable";
                    return false;
                }
                stored[static_cast<std::size_t>(i)] = *value;
            }

            const auto isPresent = [&stored](int candidate) noexcept {
                for (const int value : stored)
                    if (VehicleCargoAutoSourceRuntime::StoredCodeMatches(value, candidate))
                        return true;
                return false;
            };

            // apphackertruck::func_447 encodes 32 source groups x three tiers
            // as 1..96. func_446 rejects an entire group while stock < 32;
            // at 32+ vehicles func_445 rejects only the exact occupied tier.
            const std::uint64_t seed = static_cast<std::uint64_t>(NowMs())
                + (static_cast<std::uint64_t>(m_SourceVariationNonce++) * 29ull);
            const int start = static_cast<int>(seed % 96ull);

            for (int attempt = 0; attempt < 96; ++attempt)
            {
                const int candidate = ((start + (attempt * 17)) % 96) + 1;
                bool blocked = false;

                if (count < 32)
                {
                    const int groupStart = (((candidate - 1) / 3) * 3) + 1;
                    blocked = isPresent(groupStart) || isPresent(groupStart + 1) || isPresent(groupStart + 2);
                }
                else
                {
                    blocked = isPresent(candidate);
                }

                if (!blocked)
                {
                    outVariation = candidate;
                    return true;
                }
            }

            outFailure = "No valid Enhanced Vehicle Cargo source variation is available";
            return false;
        }

        bool ApplySourceLaunchState(std::int64_t** pages, int playerId, int sourceVariation,
            std::string& outFailure) noexcept
        {
            if (!pages || playerId < 0 || playerId >= MaxPlayers || sourceVariation < 1 || sourceVariation > 96)
            {
                outFailure = "Enhanced Vehicle Cargo source state is invalid";
                return false;
            }
            if (m_HavePreviousSourceState)
            {
                outFailure = "A Vehicle Cargo source request is already pending";
                return false;
            }

            const auto orgEntry = Script::ScriptGlobal(PlayerOrganizationGlobal)
                .At(static_cast<std::size_t>(playerId), PlayerOrganizationEntrySize);
            int* mission = orgEntry.At(PlayerOrganizationMissionOffset).As<int>(pages);
            const auto exportArray = orgEntry.At(PlayerOrganizationVehicleExportOffset);
            int* exportCount = exportArray.As<int>(pages);
            std::array<int*, VehicleExportArraySize> slots{};
            for (std::size_t i = 0; i < slots.size(); ++i)
                slots[i] = exportArray.At(i, 1).As<int>(pages);
            int* exportPad = orgEntry.At(PlayerOrganizationVehicleExportOffset + VehicleExportPadOffset).As<int>(pages);

            bool valid = mission && exportCount && *exportCount == static_cast<int>(VehicleExportArraySize) && exportPad;
            for (const auto* slot : slots)
                valid = valid && slot != nullptr;
            if (!valid)
            {
                outFailure = "Enhanced VehicleExport layout validation failed";
                return false;
            }

            m_PreviousMissionState = *mission;
            for (std::size_t i = 0; i < slots.size(); ++i)
                m_PreviousVehicleExportState[i] = *slots[i];
            m_PreviousVehicleExportState[VehicleExportArraySize] = *exportPad;
            m_CapturedPlayerId = playerId;
            m_RequestedSourceVariation = sourceVariation;
            m_HavePreviousSourceState = true;

            // apphackertruck::func_435(variation, 0, 0, 0, 0), then func_431(178).
            *slots[0] = sourceVariation;
            *slots[1] = 0;
            *slots[2] = 0;
            *slots[3] = 0;
            *exportPad = 0;
            *mission = VehicleCargoSourceMissionId;
            return true;
        }

        bool SendFreemodeSourceRequest(int playerId, int& outFreemodeHost, int& outSetupA, int& outSetupB,
            int& outSetupC, int& outSourceVariation, int& outWarehouseVehicleCount, std::string& outFailure)
        {
            outFreemodeHost = -1;
            outSetupA = 0;
            outSetupB = 0;
            outSetupC = 0;
            outSourceVariation = 0;
            outWarehouseVehicleCount = 0;
            outFailure.clear();

            if (!ResolveLaunchNatives() || !ScriptHost("freemode", outFreemodeHost))
            {
                outFailure = "Enhanced freemode host is unavailable";
                return false;
            }

            auto* pages = GamePointers::Get().ScriptGlobals();
            if (!pages)
            {
                outFailure = "Enhanced script globals are unavailable";
                return false;
            }

            int* setupA = Script::ScriptGlobal(FreemodeBusinessGlobal)
                .At(ImportExportSetupRootOffset + ImportExportSetupAOffset)
                .As<int>(pages);
            int* setupB = Script::ScriptGlobal(FreemodeBusinessGlobal)
                .At(ImportExportSetupRootOffset + ImportExportSetupBOffset)
                .As<int>(pages);
            int* setupC = Script::ScriptGlobal(FreemodeBusinessGlobal)
                .At(ImportExportSetupRootOffset + ImportExportSetupCOffset)
                .As<int>(pages);

            if (!setupA || !setupB || !setupC)
            {
                outFailure = "Enhanced Import/Export setup globals are unavailable";
                return false;
            }

            outSetupA = *setupA;
            outSetupB = *setupB;
            outSetupC = *setupC;

            if (!SelectSourceVariation(pages, playerId, outSourceVariation, outWarehouseVehicleCount, outFailure))
                return false;
            if (!ApplySourceLaunchState(pages, playerId, outSourceVariation, outFailure))
                return false;

            std::array<std::int64_t, 8> eventData{};
            eventData[0] = static_cast<std::int64_t>(VehicleCargoLaunchEvent);
            eventData[1] = playerId;
            eventData[2] = 0;
            eventData[3] = VehicleCargoSourceMissionId;
            eventData[4] = outSetupA;
            eventData[5] = outSetupB;
            eventData[6] = outSetupC;
            eventData[7] = -1;

            const std::uint32_t recipientMask = std::uint32_t{1} << static_cast<std::uint32_t>(outFreemodeHost);

            Native::CallContext context;
            if (!context.PushArg(std::int32_t{1})
                || !context.PushArg(eventData.data())
                || !context.PushArg(std::int32_t{static_cast<int>(eventData.size())})
                || !context.PushArg(static_cast<std::int32_t>(recipientMask))
                || !context.PushArg(VehicleCargoLaunchEvent))
            {
                outFailure = "Unable to build the Enhanced Vehicle Cargo TU event";
                RestoreSourceLaunchState();
                return false;
            }

            m_SendTuScriptEventNew(&context);
            context.FixVectors();
            return true;
        }

        void CancelWaitingRequest() noexcept
        {
            RestoreSourceLaunchState();
            m_WaitingForStart = false;
            m_WaitingSinceMs = 0;
        }

        void Evaluate(bool manual)
        {
            if (m_ResetRequested.exchange(false, std::memory_order_acq_rel))
                ResetCycleState();

            if (!manual && !m_Enabled.load(std::memory_order_acquire))
                return Finish(true, false, false, false, false, -1, -1, "Auto Source is off");

            bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            if (!sessionStarted || !*sessionStarted)
            {
                if (m_WaitingForStart)
                    CancelWaitingRequest();
                return Finish(false, false, false, false, false, -1, -1, "Join GTA Online before using Auto Source");
            }

            auto& scripts = Script::ScriptRuntime::Get();
            if (!scripts.IsReady())
            {
                if (m_WaitingForStart)
                    CancelWaitingRequest();
                return Finish(false, true, false, false, false, -1, -1, "Enhanced script runtime unavailable");
            }

            const auto* cargo = scripts.FindThread(BusinessScriptMonitorRuntime::VehicleCargoScriptHash);
            const bool cargoRunning = cargo && cargo->stack;
            const auto now = NowMs();

            if (cargoRunning)
            {
                m_SawMissionRunning = true;
                m_WaitingForStart = false;
                m_WaitingSinceMs = 0;
                ClearCapturedSourceState();
                return Finish(true, true, true, true, false, -1, VehicleCargoSourceLauncherIndex,
                    "gb_vehicle_export is running; Enhanced source request accepted");
            }

            if (m_SawMissionRunning)
            {
                m_SawMissionRunning = false;
                m_NotBeforeMs = now + MissionEndSettleMs;
                return Finish(true, true, true, false, false, -1, VehicleCargoSourceLauncherIndex,
                    "Vehicle Cargo ended; allowing freemode to settle");
            }

            constexpr std::uint32_t FreemodeScriptHash = BusinessScriptMonitorDetail::Joaat("freemode");
            const auto* freemode = scripts.FindThread(FreemodeScriptHash);
            if (!freemode || !freemode->stack)
            {
                if (m_WaitingForStart)
                    CancelWaitingRequest();
                return Finish(false, true, false, false, false, -1, -1, "Enhanced freemode thread unavailable");
            }

            const auto playerId = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::PlayerId);
            if (!playerId || *playerId < 0 || *playerId >= MaxPlayers)
            {
                if (m_WaitingForStart)
                    CancelWaitingRequest();
                return Finish(false, true, false, false, false, -1, -1, "PLAYER_ID unavailable");
            }

            if (m_WaitingForStart)
            {
                int host = -1;
                static_cast<void>(ScriptHost("freemode", host));

                if ((now - m_WaitingSinceMs) < MissionStartTimeoutMs)
                {
                    return Finish(true, true, true, false, true, host, VehicleCargoSourceLauncherIndex,
                        std::string("Freemode accepted source 178 variation ")
                            + std::to_string(m_RequestedSourceVariation)
                            + "; waiting for GB_VEHICLE_EXPORT (host "
                            + std::to_string(host) + ")");
                }

                CancelWaitingRequest();
                m_NotBeforeMs = now + RetryBackoffMs;
                return Finish(false, true, true, false, false, host, VehicleCargoSourceLauncherIndex,
                    "Enhanced freemode did not start Vehicle Cargo; source globals restored before retry");
            }

            if (!manual && now < m_NotBeforeMs)
                return Finish(true, true, true, false, false, -1, VehicleCargoSourceLauncherIndex,
                    "Auto Source waiting before next Enhanced freemode request");

            int freemodeHost = -1;
            int setupA = 0;
            int setupB = 0;
            int setupC = 0;
            int sourceVariation = 0;
            int warehouseVehicleCount = 0;
            std::string failure;
            if (!SendFreemodeSourceRequest(*playerId, freemodeHost, setupA, setupB, setupC,
                    sourceVariation, warehouseVehicleCount, failure))
            {
                RestoreSourceLaunchState();
                m_NotBeforeMs = now + RetryBackoffMs;
                return Finish(false, true, false, false, false, freemodeHost, VehicleCargoSourceLauncherIndex,
                    failure.empty() ? "Unable to submit the Enhanced Vehicle Cargo freemode event" : std::move(failure));
            }

            m_WaitingForStart = true;
            m_WaitingSinceMs = now;

            TUTONES_LOG_INFO("business.vehicle_cargo",
                std::string("Enhanced Vehicle Cargo source submitted: mission=178 launcher=73 variation=")
                    + std::to_string(sourceVariation)
                    + " warehouseStock=" + std::to_string(warehouseVehicleCount)
                    + " freemodeHost=" + std::to_string(freemodeHost)
                    + " setup=" + std::to_string(setupA)
                    + "," + std::to_string(setupB)
                    + "," + std::to_string(setupC));

            Finish(true, true, true, false, true, freemodeHost, VehicleCargoSourceLauncherIndex,
                std::string("Enhanced source 178 variation ") + std::to_string(sourceVariation)
                    + " sent to freemode host " + std::to_string(freemodeHost));
        }

        void Finish(bool success, bool sessionReady, bool launcherReady, bool vehicleCargoRunning,
            bool waitingForStart, int launcherState, int launcherIndex, std::string message)
        {
            {
                std::scoped_lock lock(m_Mutex);
                m_SessionReady = sessionReady;
                m_LauncherReady = launcherReady;
                m_VehicleCargoRunning = vehicleCargoRunning;
                m_WaitingForStartSnapshot = waitingForStart;
                m_LastSucceeded = success;
                m_LauncherStateSnapshot = launcherState;
                m_LauncherIndexSnapshot = launcherIndex;
                m_Message = std::move(message);
            }
            m_Pending.store(false, std::memory_order_release);
        }

        std::atomic<bool> m_Enabled{false};
        std::atomic<bool> m_Pending{false};
        std::atomic<bool> m_ResetRequested{false};
        std::atomic<std::int64_t> m_NextPollMs{0};

        bool m_WaitingForStart{};
        std::int64_t m_WaitingSinceMs{};
        bool m_SawMissionRunning{};
        std::int64_t m_NotBeforeMs{};

        int m_PreviousMissionState{-1};
        std::array<int, VehicleExportArraySize + 1> m_PreviousVehicleExportState{};
        int m_CapturedPlayerId{-1};
        int m_RequestedSourceVariation{};
        bool m_HavePreviousSourceState{};
        std::uint32_t m_SourceVariationNonce{};

        Native::NativeHandler m_NetworkGetHostOfScript{};
        Native::NativeHandler m_SendTuScriptEventNew{};

        mutable std::mutex m_Mutex;
        bool m_SessionReady{};
        bool m_LauncherReady{};
        bool m_VehicleCargoRunning{};
        bool m_WaitingForStartSnapshot{};
        bool m_LastSucceeded{};
        int m_LauncherStateSnapshot{-1};
        int m_LauncherIndexSnapshot{-1};
        std::string m_Message{"Auto Source is off"};
    };
}
