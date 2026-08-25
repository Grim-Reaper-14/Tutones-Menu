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
        // GTA5 Enhanced am_launcher host data. LauncherServerData is rooted at
        // Global_2700113 and its CurrentScript occupies slots +3 through +7.
        static constexpr std::size_t LauncherServerGlobal = 2700113;
        static constexpr std::size_t LauncherFlagsOffset = 1;
        static constexpr std::size_t LauncherStateOffset = 2;
        static constexpr std::size_t CurrentScriptEventIndexOffset = 3;
        static constexpr std::size_t CurrentScriptLauncherIndexOffset = 4;
        static constexpr std::size_t CurrentScriptTerminatedOffset = 7;

        // Enhanced LauncherClientData is SCR_ARRAY<LauncherClientDataEntry, 32>
        // at local 270. The first slot stores the array count and each entry is
        // ClientState, Flags, LauncherState (three script slots).
        static constexpr std::size_t LauncherClientDataLocal = 270;
        static constexpr std::size_t LauncherClientEntrySize = 3;
        static constexpr std::size_t LauncherClientStateOffset = 2;
        static constexpr int MaxPlayers = 32;

        // Current Enhanced launcher table: 73 and 74 both resolve to
        // GB_VEHICLE_EXPORT. The first entry is the source route; 74 is left
        // untouched so Auto Source can never intentionally start a sell route.
        static constexpr int VehicleCargoSourceLauncherIndex = 73;

        static constexpr int LauncherStateEmpty = 0;
        static constexpr int LauncherStateStartScript = 6;
        static constexpr int RunImmediatelyFlag = (1 << 1);

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

            std::scoped_lock lock(m_Mutex);
            if (enabled)
            {
                m_ResetRequested.store(true, std::memory_order_release);
                m_SessionReady = false;
                m_LauncherReady = false;
                m_VehicleCargoRunning = false;
                m_WaitingForStartSnapshot = false;
                m_LastSucceeded = true;
                m_LauncherStateSnapshot = -1;
                m_LauncherIndexSnapshot = -1;
                m_Message = "Auto Source armed; waiting for an idle Enhanced Vehicle Cargo launcher";
            }
            else
            {
                m_Message = m_WaitingForStartSnapshot
                    ? "Auto Source is off; finishing the pending Enhanced source request"
                    : "Auto Source is off";
            }
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
            bool followPendingRequest = false;
            if (!enabled)
            {
                std::scoped_lock lock(m_Mutex);
                followPendingRequest = m_WaitingForStartSnapshot;
            }

            if (!enabled && !followPendingRequest)
                return;

            const std::int64_t now = NowMs();
            std::int64_t next = m_NextPollMs.load(std::memory_order_acquire);
            if (now < next)
                return;

            if (!m_NextPollMs.compare_exchange_strong(
                    next,
                    now + PollIntervalMs,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire))
            {
                return;
            }

            static_cast<void>(QueuePoll(!enabled && followPendingRequest));
        }

        [[nodiscard]] VehicleCargoAutoSourceSnapshot Snapshot() const
        {
            VehicleCargoAutoSourceSnapshot snapshot;
            snapshot.enabled = m_Enabled.load(std::memory_order_acquire);
            snapshot.pending = m_Pending.load(std::memory_order_acquire);

            std::scoped_lock lock(m_Mutex);
            snapshot.sessionReady = m_SessionReady;
            snapshot.launcherReady = m_LauncherReady;
            snapshot.vehicleCargoRunning = m_VehicleCargoRunning;
            snapshot.waitingForStart = m_WaitingForStartSnapshot;
            snapshot.lastSucceeded = m_LastSucceeded;
            snapshot.launcherState = m_LauncherStateSnapshot;
            snapshot.launcherIndex = m_LauncherIndexSnapshot;
            snapshot.message = m_Message;
            return snapshot;
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

        // NETWORK_GET_HOST_OF_SCRIPT canonical 1D6A14F1F9A736FC maps to this
        // current GTA5 Enhanced handler through the YimMenuV2 crossmap.
        static constexpr std::uint64_t NetworkGetHostOfScriptHash = 0xF1A4B8228C5E44B7ull;
        static constexpr std::int64_t PollIntervalMs = 750;
        static constexpr std::int64_t MissionStartTimeoutMs = 8000;
        static constexpr std::int64_t RetryBackoffMs = 5000;
        static constexpr std::int64_t MissionEndSettleMs = 2000;

        VehicleCargoAutoSourceRuntime() = default;

        [[nodiscard]] static std::int64_t NowMs() noexcept
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count();
        }

        [[nodiscard]] static std::size_t LauncherClientStateLocal(int playerId) noexcept
        {
            return LauncherClientDataLocal
                + 1
                + (static_cast<std::size_t>(playerId) * LauncherClientEntrySize)
                + LauncherClientStateOffset;
        }

        [[nodiscard]] static bool IsExecutable(std::uintptr_t address) noexcept
        {
            if (address == 0)
                return false;

            MEMORY_BASIC_INFORMATION memory{};
            if (::VirtualQuery(reinterpret_cast<const void*>(address), &memory, sizeof(memory)) != sizeof(memory))
                return false;
            if (memory.State != MEM_COMMIT || (memory.Protect & PAGE_GUARD) != 0 || memory.Protect == PAGE_NOACCESS)
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

        bool ResolveHostNative() noexcept
        {
            if (m_NetworkGetHostOfScript)
                return true;
            if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread())
                return false;

            const auto init = GamePointers::Get().InitNativeTables();
            if (!init)
                return false;

            std::uint64_t slot = NetworkGetHostOfScriptHash;
            NativeProgram program{};
            program.nativeCount = 1;
            program.nativeEntrypoints = reinterpret_cast<Native::NativeHandler*>(&slot);
            init(&program);

            if (!IsExecutable(static_cast<std::uintptr_t>(slot)))
                return false;

            m_NetworkGetHostOfScript = reinterpret_cast<Native::NativeHandler>(slot);
            return true;
        }

        bool LauncherHost(int& outHost) noexcept
        {
            outHost = -1;
            if (!ResolveHostNative() || !m_NetworkGetHostOfScript)
                return false;

            Native::CallContext context;
            if (!context.PushArg("am_launcher")
                || !context.PushArg(std::int32_t{-1})
                || !context.PushArg(std::int32_t{0}))
            {
                return false;
            }

            m_NetworkGetHostOfScript(&context);
            context.FixVectors();
            outHost = context.GetReturnValue<std::int32_t>();
            return true;
        }

        bool QueuePoll(bool manual)
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            if (Runtime::GameRuntime::Get().Enqueue([this, manual] {
                Evaluate(manual);
            }))
            {
                return true;
            }

            Finish(false, false, false, false, false, -1, -1, "Game-thread queue unavailable");
            return false;
        }

        void ResetCycleState() noexcept
        {
            m_WaitingForStart = false;
            m_WaitingSinceMs = 0;
            m_SawMissionRunning = false;
            m_NotBeforeMs = 0;
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
                return Finish(false, true, false, false, false, -1, -1, "Shared Enhanced script runtime is unavailable");

            const auto* vehicleCargo = scripts.FindThread(BusinessScriptMonitorRuntime::VehicleCargoScriptHash);
            const bool vehicleCargoRunning = vehicleCargo && vehicleCargo->stack;
            const std::int64_t now = NowMs();

            if (vehicleCargoRunning)
            {
                m_SawMissionRunning = true;
                m_WaitingForStart = false;
                m_WaitingSinceMs = 0;
                return Finish(
                    true,
                    true,
                    true,
                    true,
                    false,
                    -1,
                    VehicleCargoSourceLauncherIndex,
                    "gb_vehicle_export is running; Auto Source is standing by");
            }

            if (m_SawMissionRunning)
            {
                m_SawMissionRunning = false;
                m_NotBeforeMs = now + MissionEndSettleMs;
                return Finish(
                    true,
                    true,
                    true,
                    false,
                    false,
                    -1,
                    VehicleCargoSourceLauncherIndex,
                    "Vehicle Cargo mission ended; allowing the Enhanced launcher to settle");
            }

            constexpr std::uint32_t LauncherScriptHash = BusinessScriptMonitorDetail::Joaat("am_launcher");
            auto* launcher = scripts.FindThread(LauncherScriptHash);
            if (!launcher || !launcher->stack)
                return Finish(false, true, false, false, m_WaitingForStart, -1, -1, "Enhanced am_launcher thread is unavailable");

            auto playerId = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::PlayerId);
            if (!playerId || *playerId < 0 || *playerId >= MaxPlayers)
                return Finish(false, true, true, false, m_WaitingForStart, -1, -1, "PLAYER_ID is unavailable on the GTA game thread");

            // LauncherServerData is authoritative only on the am_launcher host.
            // Do not force host migration. In a solo/invite-only session the local
            // player normally owns this script; in a public session we wait until
            // ownership is naturally local instead of writing data another host
            // will immediately replicate over.
            int launcherHost = -1;
            if (!LauncherHost(launcherHost))
                return Finish(false, true, false, false, m_WaitingForStart, -1, -1, "Unable to resolve the Enhanced am_launcher host");
            if (launcherHost != *playerId)
            {
                return Finish(
                    true,
                    true,
                    true,
                    false,
                    m_WaitingForStart,
                    -1,
                    -1,
                    std::string("Auto Source waiting for local am_launcher ownership (host player ")
                        + std::to_string(launcherHost) + ")");
            }

            auto* pages = GamePointers::Get().ScriptGlobals();
            if (!pages)
                return Finish(false, true, true, false, m_WaitingForStart, -1, -1, "Enhanced script globals are unavailable");

            int* flags = Script::ScriptGlobal(LauncherServerGlobal).At(LauncherFlagsOffset).As<int>(pages);
            int* launcherState = Script::ScriptGlobal(LauncherServerGlobal).At(LauncherStateOffset).As<int>(pages);
            int* eventIndex = Script::ScriptGlobal(LauncherServerGlobal).At(CurrentScriptEventIndexOffset).As<int>(pages);
            int* launcherIndex = Script::ScriptGlobal(LauncherServerGlobal).At(CurrentScriptLauncherIndexOffset).As<int>(pages);
            int* terminated = Script::ScriptGlobal(LauncherServerGlobal).At(CurrentScriptTerminatedOffset).As<int>(pages);

            if (!flags || !launcherState || !eventIndex || !launcherIndex || !terminated)
                return Finish(false, true, true, false, m_WaitingForStart, -1, -1, "Enhanced am_launcher globals are unavailable");

            const std::size_t clientStateLocal = LauncherClientStateLocal(*playerId);
            if (clientStateLocal >= static_cast<std::size_t>(launcher->context.stackSize))
            {
                return Finish(
                    false,
                    true,
                    true,
                    false,
                    m_WaitingForStart,
                    *launcherState,
                    *launcherIndex,
                    "Enhanced am_launcher local 270 layout does not fit the live stack");
            }

            auto* launcherLocals = static_cast<std::uint64_t*>(launcher->stack);

            if (m_WaitingForStart)
            {
                if ((now - m_WaitingSinceMs) < MissionStartTimeoutMs)
                {
                    return Finish(
                        true,
                        true,
                        true,
                        false,
                        true,
                        *launcherState,
                        *launcherIndex,
                        "Enhanced Vehicle Cargo source request accepted by local launcher host; waiting for gb_vehicle_export");
                }

                if (*launcherState == LauncherStateStartScript
                    && *launcherIndex == VehicleCargoSourceLauncherIndex)
                {
                    *launcherState = LauncherStateEmpty;
                    *launcherIndex = 0;
                    *eventIndex = 0;
                    *terminated = 0;
                    *flags &= ~RunImmediatelyFlag;
                    launcherLocals[clientStateLocal] = static_cast<std::uint64_t>(LauncherStateEmpty);
                }

                m_WaitingForStart = false;
                m_WaitingSinceMs = 0;
                m_NotBeforeMs = now + RetryBackoffMs;
                return Finish(
                    false,
                    true,
                    true,
                    false,
                    false,
                    *launcherState,
                    *launcherIndex,
                    "Enhanced launcher did not start Vehicle Cargo; request cleared and retry backed off");
            }

            if (!manual && now < m_NotBeforeMs)
            {
                return Finish(
                    true,
                    true,
                    true,
                    false,
                    false,
                    *launcherState,
                    *launcherIndex,
                    "Auto Source is waiting before the next Enhanced launcher attempt");
            }

            if (*launcherState != LauncherStateEmpty)
            {
                return Finish(
                    true,
                    true,
                    true,
                    false,
                    false,
                    *launcherState,
                    *launcherIndex,
                    "am_launcher is busy; Auto Source will retry when it becomes idle");
            }

            *flags |= RunImmediatelyFlag;
            *eventIndex = 0;
            *launcherIndex = VehicleCargoSourceLauncherIndex;
            *terminated = 0;
            *launcherState = LauncherStateStartScript;
            launcherLocals[clientStateLocal] = static_cast<std::uint64_t>(LauncherStateStartScript);

            m_WaitingForStart = true;
            m_WaitingSinceMs = now;

            TUTONES_LOG_INFO(
                "business.vehicle_cargo",
                std::string("Enhanced Vehicle Cargo source requested via locally hosted am_launcher index ")
                    + std::to_string(VehicleCargoSourceLauncherIndex)
                    + " player=" + std::to_string(*playerId));

            Finish(
                true,
                true,
                true,
                false,
                true,
                *launcherState,
                *launcherIndex,
                "Enhanced Vehicle Cargo source request submitted by local am_launcher host");
        }

        void Finish(
            bool success,
            bool sessionReady,
            bool launcherReady,
            bool vehicleCargoRunning,
            bool waitingForStart,
            int launcherState,
            int launcherIndex,
            std::string message)
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
        Native::NativeHandler m_NetworkGetHostOfScript{};

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
