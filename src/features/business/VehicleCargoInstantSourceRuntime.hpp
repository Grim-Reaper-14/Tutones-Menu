#pragma once

#include "BusinessScriptMonitorRuntime.hpp"
#include "VehicleCargoAutoSourceRuntime.hpp"
#include "VehicleCargoRuntimeShared.hpp"
#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/Natives.hpp"
#include "../../game/Stats.hpp"
#include "../../game/VehicleNatives.hpp"
#include "../../game/native/NativeCallContext.hpp"
#include "../../game/native/NativeHandlerValidation.hpp"
#include "../../game/native/NativeInvoker.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

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
    struct VehicleCargoInstantSourceSnapshot final
    {
        bool active{};
        bool pending{};
        bool sessionReady{};
        bool missionRunning{};
        bool targetResolved{};
        bool networkControl{};
        bool vehicleReady{};
        bool lastSucceeded{};
        int variation{};
        Vehicle vehicle{};
        std::string message{"Instant Source is idle"};
    };

    // Owns only the source half of Vehicle Cargo:
    // launch activity 178 -> wait for GB_VEHICLE_EXPORT -> resolve Rockstar's
    // export-entity blip -> exact model/plate validation -> network control ->
    // put the player in the sourced vehicle. It never reads warehouse positions
    // and never moves an entity to a Vehicle Warehouse.
    class VehicleCargoInstantSourceRuntime final
    {
    public:
        static VehicleCargoInstantSourceRuntime& Get() noexcept
        {
            static VehicleCargoInstantSourceRuntime instance;
            return instance;
        }

        bool QueueSourceNow()
        {
            bool expected = false;
            if (!m_Active.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            ResetCycle();
            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot = {};
                m_Snapshot.active = true;
                m_Snapshot.message = "Instant Source starting a genuine Vehicle Cargo source mission";
            }
            m_NextPollMs.store(0, std::memory_order_release);
            return QueueEvaluate(true);
        }

        void Cancel() noexcept
        {
            m_Active.store(false, std::memory_order_release);
            ResetCycle();
            std::scoped_lock lock(m_Mutex);
            m_Snapshot.active = false;
            m_Snapshot.pending = false;
            m_Snapshot.message = "Instant Source cancelled";
        }

        void ClearResult() noexcept
        {
            std::scoped_lock lock(m_Mutex);
            m_Snapshot.vehicleReady = false;
            m_Snapshot.vehicle = 0;
            m_Snapshot.variation = 0;
            if (!m_Active.load(std::memory_order_acquire))
                m_Snapshot.message = "Instant Source result handed to delivery runtime";
        }

        void Tick() noexcept
        {
            if (!m_Active.load(std::memory_order_acquire))
                return;

            const auto now = NowMs();
            auto next = m_NextPollMs.load(std::memory_order_acquire);
            if (now < next)
                return;
            if (!m_NextPollMs.compare_exchange_strong(next, now + PollIntervalMs, std::memory_order_acq_rel))
                return;

            static_cast<void>(QueueEvaluate(false));
        }

        [[nodiscard]] VehicleCargoInstantSourceSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            auto out = m_Snapshot;
            out.active = m_Active.load(std::memory_order_acquire);
            out.pending = m_Pending.load(std::memory_order_acquire);
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

        enum HandlerIndex : std::size_t
        {
            NetworkRequestControlOfEntity,
            NetworkHasControlOfEntity,
            GetBlipInfoIdIterator,
            GetFirstBlipInfoId,
            GetNextBlipInfoId,
            DoesBlipExist,
            GetBlipInfoIdEntityIndex,
            HandlerCount,
        };

        static constexpr std::array<std::uint64_t, HandlerCount> HandlerHashes{{
            0xF093E270C0B6B318ull, // NETWORK_REQUEST_CONTROL_OF_ENTITY
            0x1B1A446EFA398EB5ull, // NETWORK_HAS_CONTROL_OF_ENTITY
            0x2A3612A4B836469Eull, // _GET_BLIP_INFO_ID_ITERATOR
            0xD56419CB9E15983Full, // GET_FIRST_BLIP_INFO_ID
            0xA3F6143A8F610118ull, // GET_NEXT_BLIP_INFO_ID
            0xB5DA0E63D08D983Dull, // DOES_BLIP_EXIST
            0xA143F68B0CD079F4ull, // GET_BLIP_INFO_ID_ENTITY_INDEX
        }};

        static constexpr std::int64_t PollIntervalMs = 250;
        static constexpr std::int64_t MissionStartTimeoutMs = 20000;
        static constexpr std::int64_t AcquireRetryMs = 500;
        static constexpr std::int64_t AcquireSettleMs = 1000;
        static constexpr int MaxControlAttempts = 40;
        static constexpr int MaxBlipsPerPass = 256;

        VehicleCargoInstantSourceRuntime() = default;

        [[nodiscard]] static std::int64_t NowMs() noexcept
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        }

        [[nodiscard]] bool ResolveHandlers() noexcept
        {
            bool ready = true;
            for (const auto handler : m_Handlers)
                ready = ready && handler != nullptr;
            if (ready)
                return true;

            if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread())
                return false;

            const auto init = GamePointers::Get().InitNativeTables();
            if (!init)
                return false;

            auto slots = HandlerHashes;
            NativeProgram program{};
            program.nativeCount = static_cast<std::uint32_t>(slots.size());
            program.nativeEntrypoints = reinterpret_cast<Native::NativeHandler*>(slots.data());
            init(&program);
            return Native::AssignValidatedHandlers(slots, m_Handlers);
        }

        template<typename Ret, typename... Args>
        [[nodiscard]] bool Call(std::size_t index, Ret& out, Args... args) const noexcept
        {
            if (index >= m_Handlers.size() || !m_Handlers[index])
                return false;

            Native::CallContext context;
            if (!(context.PushArg(args) && ...))
                return false;
            m_Handlers[index](&context);
            context.FixVectors();
            out = context.GetReturnValue<Ret>();
            return true;
        }

        [[nodiscard]] Vehicle FindFromRockstarBlip(int variation) noexcept
        {
            if (variation < 1 || variation > 96 || !ResolveHandlers())
                return 0;

            std::int32_t iterator{};
            if (!Call(GetBlipInfoIdIterator, iterator) || iterator == 0)
                return 0;

            std::int32_t blip{};
            if (!Call(GetFirstBlipInfoId, blip, iterator) || blip == 0)
                return 0;

            for (int scanned = 0; scanned < MaxBlipsPerPass && blip != 0; ++scanned)
            {
                std::int32_t exists{};
                if (!Call(DoesBlipExist, exists, blip) || exists == 0)
                    break;

                std::int32_t entity{};
                if (Call(GetBlipInfoIdEntityIndex, entity, blip) && entity != 0)
                {
                    const Vehicle candidate = static_cast<Vehicle>(entity);
                    if (VehicleCargoRuntimeShared::MatchesVariation(candidate, variation))
                    {
                        if (m_SourceVehicleCandidate != candidate)
                        {
                            TUTONES_LOG_INFO("business.vehicle_cargo.source",
                                std::string("Rockstar source target resolved: variation=")
                                    + std::to_string(variation)
                                    + " entity=" + std::to_string(candidate));
                        }
                        m_SourceVehicleCandidate = candidate;
                        return candidate;
                    }
                }

                std::int32_t next{};
                if (!Call(GetNextBlipInfoId, next, iterator) || next == 0 || next == blip)
                    break;
                blip = next;
            }
            return 0;
        }

        [[nodiscard]] Vehicle FindSourceVehicle(int variation) noexcept
        {
            const Vehicle current = VehicleCargoRuntimeShared::CurrentPlayerVehicle();
            if (current != 0 && VehicleCargoRuntimeShared::MatchesVariation(current, variation))
                return current;

            if (m_SourceVehicleCandidate != 0
                && VehicleCargoRuntimeShared::MatchesVariation(m_SourceVehicleCandidate, variation))
            {
                return m_SourceVehicleCandidate;
            }

            if (const Vehicle fromBlip = FindFromRockstarBlip(variation); fromBlip != 0)
                return fromBlip;

            // Local fallback only. Remote unloaded cells are deliberately not scanned.
            const std::size_t index = static_cast<std::size_t>(variation - 1);
            const Hash expectedModel = static_cast<Hash>(
                Stats::Detail::Joaat(VehicleCargoRuntimeShared::Models[index / 3]));
            const auto ped = Natives::PlayerPedId();
            if (!ped || *ped == 0)
                return 0;
            const auto coords = VehicleNatives::GetEntityCoords(*ped, false);
            if (!coords)
                return 0;

            const auto nearby = VehicleNatives::GetClosestVehicle(
                coords->x, coords->y, coords->z, 1500.0f, expectedModel, 70);
            if (nearby && *nearby != 0
                && VehicleCargoRuntimeShared::MatchesVariation(*nearby, variation))
            {
                m_SourceVehicleCandidate = *nearby;
                return *nearby;
            }
            return 0;
        }

        [[nodiscard]] bool EnsureControl(Vehicle vehicle) noexcept
        {
            if (vehicle == 0 || !ResolveHandlers())
                return false;

            std::int32_t hasControl{};
            if (!Call(NetworkHasControlOfEntity, hasControl, vehicle))
                return false;
            if (hasControl != 0)
                return true;

            std::int32_t requested{};
            static_cast<void>(Call(NetworkRequestControlOfEntity, requested, vehicle));
            ++m_ControlAttempts;
            return false;
        }

        [[nodiscard]] bool Acquire(Vehicle vehicle, std::int64_t now) noexcept
        {
            const auto ped = Natives::PlayerPedId();
            if (!ped || *ped == 0 || vehicle == 0)
                return false;

            if (VehicleCargoRuntimeShared::CurrentPlayerVehicle() == vehicle)
            {
                if (m_AcquiredAtMs == 0)
                    m_AcquiredAtMs = now;
                return (now - m_AcquiredAtMs) >= AcquireSettleMs;
            }

            m_AcquiredAtMs = 0;
            if ((now - m_LastAcquireAttemptMs) < AcquireRetryMs)
                return false;

            m_LastAcquireAttemptMs = now;
            static_cast<void>(VehicleNatives::SetPedIntoVehicle(*ped, vehicle, -1));
            return false;
        }

        bool QueueEvaluate(bool manual)
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            if (Runtime::GameRuntime::Get().Enqueue([this, manual] { Evaluate(manual); }))
                return true;

            m_Pending.store(false, std::memory_order_release);
            Stop(false, false, false, false, false, 0, 0, "Game-thread queue unavailable");
            return false;
        }

        void ResetCycle() noexcept
        {
            m_LaunchRequested = false;
            m_LaunchRequestedAtMs = 0;
            m_RequestedVariation = 0;
            m_SourceVehicleCandidate = 0;
            m_ControlAttempts = 0;
            m_LastAcquireAttemptMs = 0;
            m_AcquiredAtMs = 0;
        }

        void Evaluate(bool)
        {
            if (!m_Active.load(std::memory_order_acquire))
                return Finish(false, false, false, false, false, 0, 0, "Instant Source is idle");

            bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            if (!sessionStarted || !*sessionStarted)
                return Stop(false, false, false, false, false, 0, 0, "Join GTA Online before using Instant Source");

            auto& scripts = Script::ScriptRuntime::Get();
            if (!scripts.IsReady())
                return Finish(false, true, false, false, false, 0, 0, "Enhanced script runtime unavailable");

            const auto playerId = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::PlayerId);
            if (!playerId || *playerId < 0 || *playerId >= VehicleCargoRuntimeShared::MaxPlayers)
                return Finish(false, true, false, false, false, 0, 0, "PLAYER_ID unavailable");

            const auto* cargo = scripts.FindThread(BusinessScriptMonitorRuntime::VehicleCargoScriptHash);
            const bool cargoRunning = cargo && cargo->stack;
            const auto now = NowMs();
            auto* pages = GamePointers::Get().ScriptGlobals();

            if (!cargoRunning)
            {
                if (m_LaunchRequested)
                {
                    if ((now - m_LaunchRequestedAtMs) < MissionStartTimeoutMs)
                        return Finish(true, true, false, false, false, m_RequestedVariation, 0,
                            "Source request sent; waiting for GB_VEHICLE_EXPORT");

                    return Stop(false, true, false, false, false, 0, 0,
                        "Vehicle Cargo source mission did not start before timeout");
                }

                auto& launcher = VehicleCargoAutoSourceRuntime::Get();
                launcher.SetEnabled(false);
                if (!launcher.QueueSourceNow())
                    return Finish(false, true, false, false, false, 0, 0,
                        "Vehicle Cargo source launcher is busy; retrying");

                m_LaunchRequested = true;
                m_LaunchRequestedAtMs = now;
                return Finish(true, true, false, false, false, 0, 0,
                    "Launching genuine Vehicle Cargo source activity 178");
            }

            const int activity = VehicleCargoRuntimeShared::CurrentActivity(pages, *playerId);
            if (activity != VehicleCargoRuntimeShared::SourceActivity)
            {
                if (activity == VehicleCargoRuntimeShared::SellActivity)
                    return Stop(false, true, true, false, false, 0, 0,
                        "Vehicle Cargo sell activity 188 is running; Instant Source will not touch it");
                return Finish(true, true, true, false, false, 0, 0,
                    "GB_VEHICLE_EXPORT is running; waiting for source activity 178");
            }

            m_LaunchRequested = true;
            int variation = VehicleCargoRuntimeShared::RequestedVariation(pages, *playerId);
            if (variation > 0)
                m_RequestedVariation = variation;
            if (m_RequestedVariation <= 0)
                return Finish(false, true, true, false, false, 0, 0,
                    "Source activity 178 is running; waiting for VehicleExport variation");

            const Vehicle sourceVehicle = FindSourceVehicle(m_RequestedVariation);
            if (sourceVehicle == 0)
                return Finish(true, true, true, false, false, m_RequestedVariation, 0,
                    "Waiting for Rockstar's exact source-vehicle blip/entity");

            m_SourceVehicleCandidate = sourceVehicle;
            if (!EnsureControl(sourceVehicle))
            {
                if (m_ControlAttempts >= MaxControlAttempts)
                {
                    m_ControlAttempts = 0;
                    m_SourceVehicleCandidate = 0;
                    return Finish(false, true, true, true, false, m_RequestedVariation, 0,
                        "Source target resolved but network control timed out; resolving again");
                }
                return Finish(true, true, true, true, false, m_RequestedVariation, sourceVehicle,
                    std::string("Source target resolved; requesting network control (")
                        + std::to_string(m_ControlAttempts) + "/" + std::to_string(MaxControlAttempts) + ")");
            }

            if (!Acquire(sourceVehicle, now))
                return Finish(true, true, true, true, false, m_RequestedVariation, sourceVehicle,
                    "Source runtime owns the exact mission car; acquiring driver seat");

            TUTONES_LOG_INFO("business.vehicle_cargo.source",
                std::string("Instant source acquired: variation=") + std::to_string(m_RequestedVariation)
                    + " entity=" + std::to_string(sourceVehicle));

            m_Active.store(false, std::memory_order_release);
            Finish(true, true, true, true, true, m_RequestedVariation, sourceVehicle,
                "Instant Source complete; exact Rockstar source vehicle acquired and ready for delivery runtime");
        }

        void Stop(bool success, bool sessionReady, bool missionRunning, bool targetResolved,
            bool vehicleReady, int variation, Vehicle vehicle, std::string message) noexcept
        {
            m_Active.store(false, std::memory_order_release);
            Finish(success, sessionReady, missionRunning, targetResolved, vehicleReady,
                variation, vehicle, std::move(message));
        }

        void Finish(bool success, bool sessionReady, bool missionRunning, bool targetResolved,
            bool vehicleReady, int variation, Vehicle vehicle, std::string message) noexcept
        {
            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot.active = m_Active.load(std::memory_order_acquire);
                m_Snapshot.sessionReady = sessionReady;
                m_Snapshot.missionRunning = missionRunning;
                m_Snapshot.targetResolved = targetResolved;
                m_Snapshot.networkControl = targetResolved && vehicle != 0 && m_ControlAttempts == 0;
                m_Snapshot.vehicleReady = vehicleReady;
                m_Snapshot.lastSucceeded = success;
                m_Snapshot.variation = variation;
                m_Snapshot.vehicle = vehicle;
                m_Snapshot.message = std::move(message);
            }
            m_Pending.store(false, std::memory_order_release);
        }

        std::atomic<bool> m_Active{false};
        std::atomic<bool> m_Pending{false};
        std::atomic<std::int64_t> m_NextPollMs{0};

        bool m_LaunchRequested{};
        std::int64_t m_LaunchRequestedAtMs{};
        int m_RequestedVariation{};
        Vehicle m_SourceVehicleCandidate{};
        int m_ControlAttempts{};
        std::int64_t m_LastAcquireAttemptMs{};
        std::int64_t m_AcquiredAtMs{};

        std::array<Native::NativeHandler, HandlerCount> m_Handlers{};
        mutable std::mutex m_Mutex;
        VehicleCargoInstantSourceSnapshot m_Snapshot{};
    };
}
