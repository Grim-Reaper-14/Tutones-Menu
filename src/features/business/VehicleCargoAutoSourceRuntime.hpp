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
        // GTA5 Enhanced decompiled flow:
        //   apphackertruck -> mission 178 -> TU event 1613825825 -> freemode host
        //   -> am_launcher activity 73 -> GB_VEHICLE_EXPORT.
        // Activity 188 / launcher 74 is the sell route and is never used here.
        static constexpr int VehicleCargoSourceMissionId = 178;
        static constexpr int VehicleCargoSellMissionId = 188;
        static constexpr int VehicleCargoSourceLauncherIndex = 73;
        static constexpr std::uint32_t VehicleCargoLaunchEvent = 1613825825u;

        // apphackertruck func_425/426/427 forwards these three Import/Export
        // setup slots with the launch event. Source does not manufacture its own
        // values; it forwards the live Enhanced session values exactly as GTA does.
        static constexpr std::size_t FreemodeBusinessGlobal = 2733326;
        static constexpr std::size_t ImportExportSetupRootOffset = 3989;
        static constexpr std::size_t ImportExportSetupAOffset = 348;
        static constexpr std::size_t ImportExportSetupBOffset = 349;
        static constexpr std::size_t ImportExportSetupCOffset = 350;

        // apphackertruck func_431 sets the local boss mission request before it
        // sends the TU event. Global_1893070 is a counted player array.
        static constexpr std::size_t PlayerOrganizationGlobal = 1893070;
        static constexpr std::size_t PlayerOrganizationEntrySize = 615;
        static constexpr std::size_t PlayerOrganizationMissionOffset = 10 + 33;
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

        // Current GTA5 Enhanced target for NETWORK_GET_HOST_OF_SCRIPT.
        // Canonical 0x1D6A14F1F9A736FC -> 0xF1A4B8228C5E44B7.
        static constexpr std::uint64_t NetworkGetHostOfScriptHash = 0xF1A4B8228C5E44B7ull;

        // _SEND_TU_SCRIPT_EVENT_NEW was introduced with the newer script event
        // path and remains 0x71A6F836422FDD2B in the current Enhanced crossmap.
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

        void ResetCycleState() noexcept
        {
            m_WaitingForStart = false;
            m_WaitingSinceMs = 0;
            m_SawMissionRunning = false;
            m_NotBeforeMs = 0;
            m_PreviousMissionState = -1;
            m_HavePreviousMissionState = false;
        }

        void RestoreLocalMissionState() noexcept
        {
            if (!m_HavePreviousMissionState)
                return;

            auto* pages = GamePointers::Get().ScriptGlobals();
            const auto playerId = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::PlayerId);
            if (pages && playerId && *playerId >= 0 && *playerId < MaxPlayers)
            {
                int* mission = Script::ScriptGlobal(PlayerOrganizationGlobal)
                    .At(static_cast<std::size_t>(*playerId), PlayerOrganizationEntrySize)
                    .At(PlayerOrganizationMissionOffset)
                    .As<int>(pages);

                if (mission && *mission == VehicleCargoSourceMissionId)
                    *mission = m_PreviousMissionState;
            }

            m_PreviousMissionState = -1;
            m_HavePreviousMissionState = false;
        }

        bool SendFreemodeSourceRequest(int playerId, int& outFreemodeHost, int& outSetupA, int& outSetupB, int& outSetupC)
        {
            outFreemodeHost = -1;
            outSetupA = 0;
            outSetupB = 0;
            outSetupC = 0;

            if (!ResolveLaunchNatives() || !ScriptHost("freemode", outFreemodeHost))
                return false;

            auto* pages = GamePointers::Get().ScriptGlobals();
            if (!pages)
                return false;

            int* setupA = Script::ScriptGlobal(FreemodeBusinessGlobal)
                .At(ImportExportSetupRootOffset + ImportExportSetupAOffset)
                .As<int>(pages);
            int* setupB = Script::ScriptGlobal(FreemodeBusinessGlobal)
                .At(ImportExportSetupRootOffset + ImportExportSetupBOffset)
                .As<int>(pages);
            int* setupC = Script::ScriptGlobal(FreemodeBusinessGlobal)
                .At(ImportExportSetupRootOffset + ImportExportSetupCOffset)
                .As<int>(pages);
            int* localMission = Script::ScriptGlobal(PlayerOrganizationGlobal)
                .At(static_cast<std::size_t>(playerId), PlayerOrganizationEntrySize)
                .At(PlayerOrganizationMissionOffset)
                .As<int>(pages);

            if (!setupA || !setupB || !setupC || !localMission)
                return false;

            outSetupA = *setupA;
            outSetupB = *setupB;
            outSetupC = *setupC;

            if (!m_HavePreviousMissionState)
            {
                m_PreviousMissionState = *localMission;
                m_HavePreviousMissionState = true;
            }

            // apphackertruck::func_431 mirrors this before func_424 sends the event.
            *localMission = VehicleCargoSourceMissionId;

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
                RestoreLocalMissionState();
                return false;
            }

            m_SendTuScriptEventNew(&context);
            context.FixVectors();
            return true;
        }

        void Evaluate(bool manual)
        {
            if (m_ResetRequested.exchange(false, std::memory_order_acq_rel))
                ResetCycleState();

            if (!manual && !m_Enabled.load(std::memory_order_acquire))
                return Finish(true, false, false, false, false, -1, -1, "Auto Source is off");

            bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            if (!sessionStarted || !*sessionStarted)
                return Finish(false, false, false, false, false, -1, -1, "Join GTA Online before using Auto Source");

            auto& scripts = Script::ScriptRuntime::Get();
            if (!scripts.IsReady())
                return Finish(false, true, false, false, false, -1, -1, "Enhanced script runtime unavailable");

            const auto* cargo = scripts.FindThread(BusinessScriptMonitorRuntime::VehicleCargoScriptHash);
            const bool cargoRunning = cargo && cargo->stack;
            const auto now = NowMs();

            if (cargoRunning)
            {
                m_SawMissionRunning = true;
                m_WaitingForStart = false;
                m_WaitingSinceMs = 0;
                m_PreviousMissionState = -1;
                m_HavePreviousMissionState = false;
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
                return Finish(false, true, false, false, m_WaitingForStart, -1, -1,
                    "Enhanced freemode thread unavailable");

            const auto playerId = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::PlayerId);
            if (!playerId || *playerId < 0 || *playerId >= MaxPlayers)
                return Finish(false, true, false, false, m_WaitingForStart, -1, -1, "PLAYER_ID unavailable");

            if (m_WaitingForStart)
            {
                int host = -1;
                static_cast<void>(ScriptHost("freemode", host));

                if ((now - m_WaitingSinceMs) < MissionStartTimeoutMs)
                {
                    return Finish(true, true, true, false, true, host, VehicleCargoSourceLauncherIndex,
                        std::string("Freemode accepted source event 178; waiting for GB_VEHICLE_EXPORT (host ")
                            + std::to_string(host) + ")");
                }

                RestoreLocalMissionState();
                m_WaitingForStart = false;
                m_WaitingSinceMs = 0;
                m_NotBeforeMs = now + RetryBackoffMs;
                return Finish(false, true, true, false, false, host, VehicleCargoSourceLauncherIndex,
                    "Enhanced freemode did not start Vehicle Cargo; backed off before retry");
            }

            if (!manual && now < m_NotBeforeMs)
                return Finish(true, true, true, false, false, -1, VehicleCargoSourceLauncherIndex,
                    "Auto Source waiting before next Enhanced freemode request");

            int freemodeHost = -1;
            int setupA = 0;
            int setupB = 0;
            int setupC = 0;
            if (!SendFreemodeSourceRequest(*playerId, freemodeHost, setupA, setupB, setupC))
            {
                RestoreLocalMissionState();
                m_NotBeforeMs = now + RetryBackoffMs;
                return Finish(false, true, false, false, false, freemodeHost, VehicleCargoSourceLauncherIndex,
                    "Unable to submit the Enhanced Vehicle Cargo freemode event");
            }

            m_WaitingForStart = true;
            m_WaitingSinceMs = now;

            TUTONES_LOG_INFO("business.vehicle_cargo",
                std::string("Enhanced Vehicle Cargo source event submitted: mission=178 launcher=73 freemodeHost=")
                    + std::to_string(freemodeHost)
                    + " setup=" + std::to_string(setupA)
                    + "," + std::to_string(setupB)
                    + "," + std::to_string(setupC));

            Finish(true, true, true, false, true, freemodeHost, VehicleCargoSourceLauncherIndex,
                std::string("Enhanced Vehicle Cargo source event 178 sent to freemode host ")
                    + std::to_string(freemodeHost));
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
        bool m_HavePreviousMissionState{};

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
