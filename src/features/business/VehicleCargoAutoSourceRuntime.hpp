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
        static constexpr std::size_t LauncherServerGlobal = 2700113;
        static constexpr std::size_t LauncherFlagsOffset = 1;
        static constexpr std::size_t LauncherStateOffset = 2;
        static constexpr std::size_t CurrentScriptEventIndexOffset = 3;
        static constexpr std::size_t CurrentScriptLauncherIndexOffset = 4;
        static constexpr std::size_t CurrentScriptTerminatedOffset = 7;

        // GTA5 Enhanced am_launcher.c declares uLocal_270 as a raw
        // struct<3>[32]. There is no SCR_ARRAY count cell before player 0.
        // Entry fields are: client state, flags, launcher phase.
        static constexpr std::size_t LauncherClientDataLocal = 270;
        static constexpr std::size_t LauncherClientEntrySize = 3;
        static constexpr std::size_t LauncherClientMainStateOffset = 0;
        static constexpr std::size_t LauncherClientPhaseOffset = 2;
        static constexpr int MaxPlayers = 32;

        // Enhanced am_launcher func_6 maps both 73 and 74 to
        // GB_VEHICLE_EXPORT. func_453 maps 73 to the source activity (178)
        // and 74 to the sell activity (188), so Auto Source only uses 73.
        static constexpr int VehicleCargoSourceLauncherIndex = 73;

        static constexpr int LauncherStateIdle = 0;
        static constexpr int LauncherStateStartScript = 6;
        static constexpr int ClientStateRunning = 1;
        static constexpr int ClientPhaseStartScript = 6;
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
            if (enabled)
                m_ResetRequested.store(true, std::memory_order_release);

            std::scoped_lock lock(m_Mutex);
            m_Message = enabled
                ? "Auto Source armed; waiting for Enhanced am_launcher"
                : (m_WaitingForStartSnapshot
                    ? "Auto Source is off; finishing pending source request"
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

        static constexpr std::uint64_t NetworkGetHostOfScriptHash = 0xF1A4B8228C5E44B7ull;
        static constexpr std::int64_t PollIntervalMs = 500;
        static constexpr std::int64_t MissionStartTimeoutMs = 12000;
        static constexpr std::int64_t RetryBackoffMs = 4000;
        static constexpr std::int64_t MissionEndSettleMs = 1500;

        VehicleCargoAutoSourceRuntime() = default;

        [[nodiscard]] static std::int64_t NowMs() noexcept
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        }

        [[nodiscard]] static std::size_t ClientBaseLocal(int playerId) noexcept
        {
            return LauncherClientDataLocal + (static_cast<std::size_t>(playerId) * LauncherClientEntrySize);
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
            if (!ResolveHostNative())
                return false;
            Native::CallContext context;
            if (!context.PushArg("am_launcher") || !context.PushArg(std::int32_t{-1}) || !context.PushArg(std::int32_t{0}))
                return false;
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
                return Finish(true, true, true, true, false, -1, VehicleCargoSourceLauncherIndex,
                    "gb_vehicle_export is running; Auto Source standing by");
            }

            if (m_SawMissionRunning)
            {
                m_SawMissionRunning = false;
                m_NotBeforeMs = now + MissionEndSettleMs;
                return Finish(true, true, true, false, false, -1, VehicleCargoSourceLauncherIndex,
                    "Vehicle Cargo ended; waiting for launcher to settle");
            }

            constexpr std::uint32_t LauncherScriptHash = BusinessScriptMonitorDetail::Joaat("am_launcher");
            auto* launcher = scripts.FindThread(LauncherScriptHash);
            if (!launcher || !launcher->stack)
                return Finish(false, true, false, false, m_WaitingForStart, -1, -1, "Enhanced am_launcher thread unavailable");

            const auto playerId = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::PlayerId);
            if (!playerId || *playerId < 0 || *playerId >= MaxPlayers)
                return Finish(false, true, true, false, m_WaitingForStart, -1, -1, "PLAYER_ID unavailable");

            int host = -1;
            if (!LauncherHost(host))
                return Finish(false, true, false, false, m_WaitingForStart, -1, -1, "Unable to resolve am_launcher host");
            if (host != *playerId)
                return Finish(true, true, true, false, m_WaitingForStart, -1, -1,
                    std::string("Waiting for local am_launcher ownership; host player ") + std::to_string(host));

            auto* pages = GamePointers::Get().ScriptGlobals();
            if (!pages)
                return Finish(false, true, true, false, m_WaitingForStart, -1, -1, "Enhanced script globals unavailable");

            int* flags = Script::ScriptGlobal(LauncherServerGlobal).At(LauncherFlagsOffset).As<int>(pages);
            int* state = Script::ScriptGlobal(LauncherServerGlobal).At(LauncherStateOffset).As<int>(pages);
            int* eventIndex = Script::ScriptGlobal(LauncherServerGlobal).At(CurrentScriptEventIndexOffset).As<int>(pages);
            int* launcherIndex = Script::ScriptGlobal(LauncherServerGlobal).At(CurrentScriptLauncherIndexOffset).As<int>(pages);
            int* terminated = Script::ScriptGlobal(LauncherServerGlobal).At(CurrentScriptTerminatedOffset).As<int>(pages);
            if (!flags || !state || !eventIndex || !launcherIndex || !terminated)
                return Finish(false, true, true, false, m_WaitingForStart, -1, -1, "Enhanced launcher globals unavailable");

            const std::size_t clientBase = ClientBaseLocal(*playerId);
            const std::size_t clientPhase = clientBase + LauncherClientPhaseOffset;
            if (clientPhase >= static_cast<std::size_t>(launcher->context.stackSize))
                return Finish(false, true, true, false, m_WaitingForStart, *state, *launcherIndex,
                    "Enhanced am_launcher local 270 layout does not fit live stack");

            auto* locals = static_cast<std::uint64_t*>(launcher->stack);
            const int clientMainState = static_cast<int>(locals[clientBase + LauncherClientMainStateOffset]);
            const int clientPhaseState = static_cast<int>(locals[clientPhase]);

            if (clientMainState != ClientStateRunning)
                return Finish(true, true, true, false, m_WaitingForStart, *state, *launcherIndex,
                    std::string("am_launcher client not ready; state=") + std::to_string(clientMainState));

            if (m_WaitingForStart)
            {
                if ((now - m_WaitingSinceMs) < MissionStartTimeoutMs)
                    return Finish(true, true, true, false, true, *state, *launcherIndex,
                        std::string("Source request active; client phase=") + std::to_string(clientPhaseState));

                if (*state == LauncherStateStartScript && *launcherIndex == VehicleCargoSourceLauncherIndex)
                {
                    *state = LauncherStateIdle;
                    *launcherIndex = 0;
                    *eventIndex = 0;
                    *terminated = 0;
                    *flags &= ~RunImmediatelyFlag;
                    locals[clientPhase] = 0;
                }
                m_WaitingForStart = false;
                m_WaitingSinceMs = 0;
                m_NotBeforeMs = now + RetryBackoffMs;
                return Finish(false, true, true, false, false, *state, *launcherIndex,
                    "Vehicle Cargo did not start; request cleared and backed off");
            }

            if (!manual && now < m_NotBeforeMs)
                return Finish(true, true, true, false, false, *state, *launcherIndex,
                    "Auto Source waiting before next attempt");

            if (*state != LauncherStateIdle)
                return Finish(true, true, true, false, false, *state, *launcherIndex,
                    "am_launcher busy; waiting for idle state");

            // Drive the same phase am_launcher uses in func_481 case 6:
            // Global_2700113.f_3.f_1 selects script 73, Global_2700113.f_2
            // enters start-script state 6, and uLocal_270[player].f_2 tells
            // the local participant to execute START_NEW_SCRIPT_WITH_ARGS.
            *flags |= RunImmediatelyFlag;
            *eventIndex = 0;
            *launcherIndex = VehicleCargoSourceLauncherIndex;
            *terminated = 0;
            *state = LauncherStateStartScript;
            locals[clientPhase] = static_cast<std::uint64_t>(ClientPhaseStartScript);

            m_WaitingForStart = true;
            m_WaitingSinceMs = now;

            TUTONES_LOG_INFO("business.vehicle_cargo",
                std::string("Enhanced Vehicle Cargo source requested: launcher=73 clientBase=")
                    + std::to_string(clientBase)
                    + " clientPhase=" + std::to_string(clientPhase));

            Finish(true, true, true, false, true, *state, *launcherIndex,
                "Enhanced Vehicle Cargo source request submitted");
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
