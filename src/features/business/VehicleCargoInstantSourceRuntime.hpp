#pragma once

#include "BusinessScriptMonitorRuntime.hpp"
#include "VehicleCargoAutoSourceRuntime.hpp"
#include "VehicleCargoNativeBridge.hpp"
#include "VehicleCargoRuntimeShared.hpp"
#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/Natives.hpp"
#include "../../game/VehicleNatives.hpp"
#include "../../game/native/NativeInvoker.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <atomic>
#include <chrono>
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

    // Source-only runtime. It may launch activity 178, resolve Rockstar's
    // registered source vehicle, obtain network control and acquire the car.
    // It never reads warehouse coordinates and never performs delivery movement.
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
            if (m_Pending.load(std::memory_order_acquire)
                || m_Active.load(std::memory_order_acquire))
            {
                return false;
            }

            bool expected = false;
            if (!m_Active.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            ResetCycle();
            m_NextPollMs.store(0, std::memory_order_release);
            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot = {};
                m_Snapshot.active = true;
                m_Snapshot.message = "Instant Source starting a genuine Vehicle Cargo source mission";
            }

            if (QueueEvaluate())
                return true;

            m_Active.store(false, std::memory_order_release);
            Publish(true, false, false, false, false, false, false, 0, 0,
                "Game-thread queue unavailable");
            return false;
        }

        void Cancel() noexcept
        {
            // Do not mutate game-thread-owned cycle fields here. A pending
            // evaluation will observe this flag, and the next source request
            // resets the cycle before reuse. This avoids UI/game-thread races.
            m_Active.store(false, std::memory_order_release);
            std::scoped_lock lock(m_Mutex);
            m_Snapshot.active = false;
            m_Snapshot.message = "Instant Source cancel requested";
        }

        void ClearResult() noexcept
        {
            std::scoped_lock lock(m_Mutex);
            m_Snapshot.vehicleReady = false;
            m_Snapshot.targetResolved = false;
            m_Snapshot.networkControl = false;
            m_Snapshot.vehicle = 0;
            m_Snapshot.variation = 0;
            if (!m_Active.load(std::memory_order_acquire))
                m_Snapshot.message = "Instant Source result handed to delivery runtime";
        }

        void Tick() noexcept
        {
            if (!m_Active.load(std::memory_order_acquire))
                return;
            if (m_Pending.load(std::memory_order_acquire))
                return;

            const auto now = NowMs();
            auto next = m_NextPollMs.load(std::memory_order_acquire);
            if (now < next)
                return;
            if (!m_NextPollMs.compare_exchange_strong(next, now + PollIntervalMs, std::memory_order_acq_rel))
                return;

            static_cast<void>(QueueEvaluate());
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
        static constexpr std::int64_t PollIntervalMs = 500;
        static constexpr std::int64_t MissionStartTimeoutMs = 20000;
        static constexpr std::int64_t BlipScanIntervalMs = 1000;
        static constexpr std::int64_t AcquireRetryMs = 500;
        static constexpr std::int64_t AcquireSettleMs = 750;
        static constexpr int MaxControlAttempts = 24;
        static constexpr int MaxAcquireAttempts = 20;
        static constexpr int MaxBlipsPerPass = 96;

        VehicleCargoInstantSourceRuntime() = default;

        [[nodiscard]] static std::int64_t NowMs() noexcept
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        }

        [[nodiscard]] Vehicle FindFromRockstarBlip(int variation) noexcept
        {
            if (variation < 1 || variation > 96)
                return 0;

            auto& native = VehicleCargoNativeBridge::Get();
            std::int32_t iterator{};
            if (!native.GetBlipIterator(iterator) || iterator == 0)
                return 0;

            std::int32_t blip{};
            if (!native.GetFirstBlip(iterator, blip) || blip == 0)
                return 0;

            int scanned = 0;
            int entityBlips = 0;
            while (blip != 0 && scanned < MaxBlipsPerPass)
            {
                ++scanned;
                bool exists = false;
                if (!native.BlipExists(blip, exists) || !exists)
                    break;

                Entity entity{};
                if (native.GetBlipEntity(blip, entity) && entity > 0)
                {
                    ++entityBlips;
                    const Vehicle candidate = static_cast<Vehicle>(entity);
                    if (VehicleCargoRuntimeShared::MatchesVariation(candidate, variation))
                    {
                        m_SourceVehicleCandidate = candidate;
                        TUTONES_LOG_INFO("business.vehicle_cargo.source",
                            std::string("Registered source target resolved: variation=")
                                + std::to_string(variation)
                                + " entity=" + std::to_string(candidate)
                                + " scanned=" + std::to_string(scanned));
                        return candidate;
                    }
                }

                std::int32_t next{};
                if (!native.GetNextBlip(iterator, next) || next == 0 || next == blip)
                    break;
                blip = next;
            }

            TUTONES_LOG_DEBUG("business.vehicle_cargo.source",
                std::string("Source blip pass complete: scanned=") + std::to_string(scanned)
                    + " entityBlips=" + std::to_string(entityBlips)
                    + " variation=" + std::to_string(variation));
            return 0;
        }

        [[nodiscard]] Vehicle FindSourceVehicle(int variation, std::int64_t now) noexcept
        {
            const Vehicle current = VehicleCargoRuntimeShared::CurrentPlayerVehicle();
            if (current != 0 && VehicleCargoRuntimeShared::MatchesVariation(current, variation))
                return current;

            if (m_SourceVehicleCandidate != 0
                && VehicleCargoRuntimeShared::MatchesVariation(m_SourceVehicleCandidate, variation))
            {
                return m_SourceVehicleCandidate;
            }

            if ((now - m_LastBlipScanMs) < BlipScanIntervalMs)
                return 0;

            m_LastBlipScanMs = now;
            return FindFromRockstarBlip(variation);
        }

        [[nodiscard]] bool EnsureControl(Vehicle vehicle) noexcept
        {
            if (vehicle == 0 || !VehicleCargoRuntimeShared::MatchesVariation(vehicle, m_RequestedVariation))
                return false;

            auto& native = VehicleCargoNativeBridge::Get();
            bool hasControl = false;
            if (!native.NetworkHasControl(vehicle, hasControl))
                return false;
            if (hasControl)
            {
                m_ControlAttempts = 0;
                m_HaveControl = true;
                return true;
            }

            m_HaveControl = false;
            static_cast<void>(native.NetworkRequestControl(vehicle));
            ++m_ControlAttempts;
            return false;
        }

        [[nodiscard]] bool Acquire(Vehicle vehicle, std::int64_t now) noexcept
        {
            if (vehicle == 0 || !VehicleCargoRuntimeShared::MatchesVariation(vehicle, m_RequestedVariation))
                return false;

            const auto ped = Natives::PlayerPedId();
            if (!ped || *ped == 0)
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
            ++m_AcquireAttempts;
            static_cast<void>(VehicleNatives::SetPedIntoVehicle(*ped, vehicle, -1));
            return false;
        }

        bool QueueEvaluate()
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            if (Runtime::GameRuntime::Get().Enqueue([this] { Evaluate(); }))
                return true;

            m_Pending.store(false, std::memory_order_release);
            return false;
        }

        void ResetCycle() noexcept
        {
            m_LaunchRequested = false;
            m_LaunchRequestedAtMs = 0;
            m_RequestedVariation = 0;
            m_SourceVehicleCandidate = 0;
            m_ControlAttempts = 0;
            m_AcquireAttempts = 0;
            m_HaveControl = false;
            m_LastAcquireAttemptMs = 0;
            m_AcquiredAtMs = 0;
            m_LastBlipScanMs = 0;
        }

        void Evaluate()
        {
            if (!m_Active.load(std::memory_order_acquire))
            {
                ResetCycle();
                return Publish(true, false, false, false, false, false, false, 0, 0,
                    "Instant Source cancelled");
            }

            bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            if (!sessionStarted || !*sessionStarted)
                return Stop(false, false, false, false, false, false, 0, 0,
                    "Join GTA Online before using Instant Source");

            auto& scripts = Script::ScriptRuntime::Get();
            if (!scripts.IsReady())
                return Finish(false, true, false, false, false, false, 0, 0,
                    "Enhanced script runtime unavailable");

            const auto playerId = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::PlayerId);
            if (!playerId || *playerId < 0 || *playerId >= VehicleCargoRuntimeShared::MaxPlayers)
                return Finish(false, true, false, false, false, false, 0, 0,
                    "PLAYER_ID unavailable");

            const auto* cargo = scripts.FindThread(BusinessScriptMonitorRuntime::VehicleCargoScriptHash);
            const bool cargoRunning = cargo && cargo->stack;
            const auto now = NowMs();
            auto* pages = GamePointers::Get().ScriptGlobals();

            if (!cargoRunning)
            {
                if (m_LaunchRequested)
                {
                    if ((now - m_LaunchRequestedAtMs) < MissionStartTimeoutMs)
                        return Finish(true, true, false, false, false, false,
                            m_RequestedVariation, 0,
                            "Source request sent; waiting for GB_VEHICLE_EXPORT");

                    return Stop(false, true, false, false, false, false, 0, 0,
                        "Vehicle Cargo source mission did not start before timeout");
                }

                auto& launcher = VehicleCargoAutoSourceRuntime::Get();
                launcher.SetEnabled(false);
                if (!launcher.QueueSourceNow())
                    return Finish(false, true, false, false, false, false, 0, 0,
                        "Vehicle Cargo source launcher is busy; retrying");

                m_LaunchRequested = true;
                m_LaunchRequestedAtMs = now;
                return Finish(true, true, false, false, false, false, 0, 0,
                    "Launching genuine Vehicle Cargo source activity 178");
            }

            const int activity = VehicleCargoRuntimeShared::CurrentActivity(pages, *playerId);
            if (activity != VehicleCargoRuntimeShared::SourceActivity)
            {
                if (activity == VehicleCargoRuntimeShared::SellActivity)
                    return Stop(false, true, true, false, false, false, 0, 0,
                        "Vehicle Cargo sell activity 188 is running; Instant Source will not touch it");

                return Finish(true, true, true, false, false, false, 0, 0,
                    "GB_VEHICLE_EXPORT is running; waiting for source activity 178");
            }

            m_LaunchRequested = true;
            const int variation = VehicleCargoRuntimeShared::RequestedVariation(pages, *playerId);
            if (variation > 0)
                m_RequestedVariation = variation;
            if (m_RequestedVariation <= 0)
                return Finish(false, true, true, false, false, false, 0, 0,
                    "Source activity 178 is running; waiting for VehicleExport variation");

            // Rockstar sets ContrabandDeliveryType when func_839 decorates the
            // real source entity. Do not scan HUD/entity blips before that point.
            if (!VehicleCargoRuntimeShared::RockstarWarehouseGateReady(pages, *playerId))
                return Finish(true, true, true, false, false, false,
                    m_RequestedVariation, 0,
                    "Waiting for Rockstar to finish registering the source entity");

            const Vehicle sourceVehicle = FindSourceVehicle(m_RequestedVariation, now);
            if (sourceVehicle == 0)
                return Finish(true, true, true, false, false, false,
                    m_RequestedVariation, 0,
                    "Rockstar source entity is registered; waiting for its attached target blip");

            m_SourceVehicleCandidate = sourceVehicle;
            if (!EnsureControl(sourceVehicle))
            {
                if (m_ControlAttempts >= MaxControlAttempts)
                {
                    m_ControlAttempts = 0;
                    m_SourceVehicleCandidate = 0;
                    return Finish(false, true, true, true, false, false,
                        m_RequestedVariation, 0,
                        "Source entity resolved but network control timed out; resolving again");
                }

                return Finish(true, true, true, true, false, false,
                    m_RequestedVariation, sourceVehicle,
                    std::string("Source target resolved; requesting network control (")
                        + std::to_string(m_ControlAttempts) + "/" + std::to_string(MaxControlAttempts) + ")");
            }

            if (!Acquire(sourceVehicle, now))
            {
                if (m_AcquireAttempts >= MaxAcquireAttempts)
                    return Stop(false, true, true, true, true, false,
                        m_RequestedVariation, sourceVehicle,
                        "Source car remained unavailable for driver-seat acquisition; source runtime stopped safely");

                return Finish(true, true, true, true, true, false,
                    m_RequestedVariation, sourceVehicle,
                    "Registered source car is controlled; acquiring driver seat");
            }

            TUTONES_LOG_INFO("business.vehicle_cargo.source",
                std::string("Instant source acquired safely: variation=")
                    + std::to_string(m_RequestedVariation)
                    + " entity=" + std::to_string(sourceVehicle));

            Publish(true, true, true, true, true, true, true,
                m_RequestedVariation, sourceVehicle,
                "Instant Source complete; Rockstar source vehicle acquired and ready for delivery runtime");
        }

        void Stop(bool success, bool sessionReady, bool missionRunning, bool targetResolved,
            bool networkControl, bool vehicleReady, int variation, Vehicle vehicle,
            std::string message) noexcept
        {
            Publish(true, success, sessionReady, missionRunning, targetResolved,
                networkControl, vehicleReady, variation, vehicle, std::move(message));
        }

        void Finish(bool success, bool sessionReady, bool missionRunning, bool targetResolved,
            bool networkControl, bool vehicleReady, int variation, Vehicle vehicle,
            std::string message) noexcept
        {
            Publish(false, success, sessionReady, missionRunning, targetResolved,
                networkControl, vehicleReady, variation, vehicle, std::move(message));
        }

        void Publish(bool finalState, bool success, bool sessionReady, bool missionRunning,
            bool targetResolved, bool networkControl, bool vehicleReady, int variation,
            Vehicle vehicle, std::string message) noexcept
        {
            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot.active = !finalState;
                m_Snapshot.sessionReady = sessionReady;
                m_Snapshot.missionRunning = missionRunning;
                m_Snapshot.targetResolved = targetResolved;
                m_Snapshot.networkControl = networkControl;
                m_Snapshot.vehicleReady = vehicleReady;
                m_Snapshot.lastSucceeded = success;
                m_Snapshot.variation = variation;
                m_Snapshot.vehicle = vehicle;
                m_Snapshot.message = std::move(message);
            }

            if (finalState)
                m_Active.store(false, std::memory_order_release);
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
        int m_AcquireAttempts{};
        bool m_HaveControl{};
        std::int64_t m_LastAcquireAttemptMs{};
        std::int64_t m_AcquiredAtMs{};
        std::int64_t m_LastBlipScanMs{};

        mutable std::mutex m_Mutex;
        VehicleCargoInstantSourceSnapshot m_Snapshot{};
    };
}
