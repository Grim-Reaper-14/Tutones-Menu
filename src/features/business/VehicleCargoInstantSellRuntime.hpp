#pragma once

#include "VehicleCargoNativeBridge.hpp"
#include "VehicleCargoRuntimeShared.hpp"
#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/Natives.hpp"
#include "../../game/native/NativeCallContext.hpp"
#include "../../game/native/NativeInvoker.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

namespace Tutones::Game::Business
{
    struct VehicleCargoInstantSellSnapshot final
    {
        bool enabled{};
        bool pending{};
        bool sessionReady{};
        bool sellActivity{};
        bool vehicleReady{};
        bool objectiveReady{};
        bool deliveryIssued{};
        bool haveResult{};
        bool lastSucceeded{};
        int stagesCompleted{};
        int controlAttempts{};
        Vehicle vehicle{};
        float targetX{};
        float targetY{};
        float targetZ{};
        std::string message{"Instant Vehicle Cargo Sell is disabled"};
    };

    // Activity-188-only export helper. It does not alter warehouse inventory,
    // sell-price globals or payout state. Instead it discovers coordinate blips
    // created after the export starts and moves the player's actual export car
    // once through each Rockstar route objective. gb_vehicle_export remains the
    // authority that advances mission stages, pays the sale and ends activity.
    class VehicleCargoInstantSellRuntime final
    {
    public:
        static VehicleCargoInstantSellRuntime& Get() noexcept
        {
            static VehicleCargoInstantSellRuntime instance;
            return instance;
        }

        void SetEnabled(bool enabled) noexcept
        {
            m_Enabled.store(enabled, std::memory_order_release);
            if (!enabled)
            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot.enabled = false;
                m_Snapshot.message = "Instant Vehicle Cargo Sell is disabled";
            }
        }

        [[nodiscard]] bool Enabled() const noexcept
        {
            return m_Enabled.load(std::memory_order_acquire);
        }

        void Tick() noexcept
        {
            if (!m_Enabled.load(std::memory_order_acquire))
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

        [[nodiscard]] VehicleCargoInstantSellSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            auto out = m_Snapshot;
            out.enabled = m_Enabled.load(std::memory_order_acquire);
            out.pending = m_Pending.load(std::memory_order_acquire);
            return out;
        }

    private:
        struct ObjectiveSample final
        {
            std::int32_t blip{};
            Native::NativeVector3 coords{};
        };

        static constexpr std::int64_t PollIntervalMs = 350;
        static constexpr std::int64_t CollisionPreloadMs = 900;
        static constexpr std::int64_t StageObserveMs = 4500;
        static constexpr int MaxControlAttempts = 24;
        static constexpr int MaxStages = 6;
        static constexpr std::size_t MaxTrackedBlips = 128;
        static constexpr float ObjectiveMergeDistance = 24.0f;
        static constexpr float StageDistance = 5.0f;
        static constexpr float ApproachSpeed = 3.5f;
        static constexpr float Pi = 3.14159265358979323846f;

        VehicleCargoInstantSellRuntime() = default;

        [[nodiscard]] static std::int64_t NowMs() noexcept
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        }

        [[nodiscard]] static float DistanceSquared(
            const Native::NativeVector3& a,
            const Native::NativeVector3& b) noexcept
        {
            const float dx = a.x - b.x;
            const float dy = a.y - b.y;
            const float dz = a.z - b.z;
            return (dx * dx) + (dy * dy) + (dz * dz);
        }

        [[nodiscard]] static bool ValidObjective(const Native::NativeVector3& value) noexcept
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z)
                && std::fabs(value.x) < 10000.0f
                && std::fabs(value.y) < 10000.0f
                && value.z > -500.0f && value.z < 2500.0f
                && (std::fabs(value.x) > 0.01f || std::fabs(value.y) > 0.01f || std::fabs(value.z) > 0.01f);
        }

        [[nodiscard]] bool QueueEvaluate() noexcept
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            if (Runtime::GameRuntime::Get().Enqueue([this] { Evaluate(); }))
                return true;

            m_Pending.store(false, std::memory_order_release);
            return false;
        }

        // The pre-sale blip baseline intentionally survives mission-state
        // resets. It is refreshed while activity 188 is NOT running, then held
        // stable for the duration of the export so only newly-created mission
        // coordinate blips can become movement targets.
        void ResetMissionState() noexcept
        {
            m_InSellActivity = false;
            m_TargetVehicle = 0;
            m_ProcessedCount = 0;
            m_StagesCompleted = 0;
            m_ControlAttempts = 0;
            m_ObjectiveArmedAtMs = 0;
            m_StageIssuedAtMs = 0;
            m_HaveArmedObjective = false;
            m_WaitingForStageAdvance = false;
            m_ArmedObjective = {};
        }

        [[nodiscard]] bool ContainsBlip(
            const std::array<std::int32_t, MaxTrackedBlips>& values,
            std::size_t count,
            std::int32_t blip) const noexcept
        {
            for (std::size_t i = 0; i < count; ++i)
            {
                if (values[i] == blip)
                    return true;
            }
            return false;
        }

        static void RememberBlip(
            std::array<std::int32_t, MaxTrackedBlips>& values,
            std::size_t& count,
            std::int32_t blip) noexcept
        {
            if (blip == 0)
                return;
            for (std::size_t i = 0; i < count; ++i)
            {
                if (values[i] == blip)
                    return;
            }
            if (count < values.size())
                values[count++] = blip;
        }

        [[nodiscard]] bool EnumerateCoordinateObjectives(
            std::array<ObjectiveSample, MaxTrackedBlips>& out,
            std::size_t& outCount) noexcept
        {
            outCount = 0;
            auto& native = VehicleCargoNativeBridge::Get();

            std::int32_t iterator = 0;
            if (!native.GetBlipIterator(iterator))
                return false;

            std::int32_t blip = 0;
            if (!native.GetFirstBlip(iterator, blip))
                return false;

            for (std::size_t guard = 0; guard < MaxTrackedBlips && blip != 0; ++guard)
            {
                bool exists = false;
                if (native.BlipExists(blip, exists) && exists)
                {
                    Entity entity = 0;
                    Native::NativeVector3 coords{};
                    if (native.GetBlipEntity(blip, entity)
                        && entity == 0
                        && native.GetBlipCoords(blip, coords)
                        && ValidObjective(coords)
                        && outCount < out.size())
                    {
                        out[outCount++] = ObjectiveSample{blip, coords};
                    }
                }

                std::int32_t next = 0;
                if (!native.GetNextBlip(iterator, next) || next == blip)
                    break;
                blip = next;
            }
            return true;
        }

        void RefreshBaseline() noexcept
        {
            std::array<ObjectiveSample, MaxTrackedBlips> samples{};
            std::size_t count = 0;
            if (!EnumerateCoordinateObjectives(samples, count))
                return;

            m_BaselineCount = 0;
            for (std::size_t i = 0; i < count; ++i)
                RememberBlip(m_BaselineBlips, m_BaselineCount, samples[i].blip);
        }

        [[nodiscard]] bool FindNewObjective(
            Native::NativeVector3& outCoords,
            std::array<std::int32_t, MaxTrackedBlips>& outBlips,
            std::size_t& outBlipCount,
            bool& ambiguous) noexcept
        {
            ambiguous = false;
            outBlipCount = 0;

            std::array<ObjectiveSample, MaxTrackedBlips> samples{};
            std::size_t count = 0;
            if (!EnumerateCoordinateObjectives(samples, count))
                return false;

            bool haveCluster = false;
            for (std::size_t i = 0; i < count; ++i)
            {
                const auto& sample = samples[i];
                if (ContainsBlip(m_BaselineBlips, m_BaselineCount, sample.blip)
                    || ContainsBlip(m_ProcessedBlips, m_ProcessedCount, sample.blip))
                {
                    continue;
                }

                if (!haveCluster)
                {
                    outCoords = sample.coords;
                    haveCluster = true;
                }
                else if (DistanceSquared(outCoords, sample.coords)
                    > ObjectiveMergeDistance * ObjectiveMergeDistance)
                {
                    ambiguous = true;
                    return false;
                }

                RememberBlip(outBlips, outBlipCount, sample.blip);
            }
            return haveCluster;
        }

        [[nodiscard]] bool EnsureControl(Vehicle vehicle) noexcept
        {
            auto& native = VehicleCargoNativeBridge::Get();
            bool hasControl = false;
            if (!native.NetworkHasControl(vehicle, hasControl))
                return false;
            if (hasControl)
            {
                m_ControlAttempts = 0;
                return true;
            }

            static_cast<void>(native.NetworkRequestControl(vehicle));
            ++m_ControlAttempts;
            return false;
        }

        [[nodiscard]] bool IssueObjectiveApproach(
            Vehicle vehicle,
            const Native::NativeVector3& target) noexcept
        {
            const auto current = Native::NativeInvoker::Invoke<Native::NativeVector3>(
                Native::NativeId::GetEntityCoords,
                vehicle,
                std::int32_t{1});
            if (!current)
                return false;

            float dx = target.x - current->x;
            float dy = target.y - current->y;
            const float length = std::sqrt((dx * dx) + (dy * dy));
            if (length < 0.001f)
            {
                dx = 0.0f;
                dy = 1.0f;
            }
            else
            {
                dx /= length;
                dy /= length;
            }

            const float stageX = target.x - (dx * StageDistance);
            const float stageY = target.y - (dy * StageDistance);
            const float stageZ = target.z + 0.20f;
            float heading = std::atan2(-dx, dy) * (180.0f / Pi);
            if (heading < 0.0f)
                heading += 360.0f;

            auto& native = VehicleCargoNativeBridge::Get();
            if (!native.RequestCollisionAt(stageX, stageY, stageZ)
                || !native.RequestCollisionAt(target.x, target.y, target.z)
                || !native.SetCoordsNoOffset(vehicle, stageX, stageY, stageZ)
                || !native.SetHeading(vehicle, heading)
                || !native.SetVelocity(vehicle, dx * ApproachSpeed, dy * ApproachSpeed, 0.0f))
            {
                return false;
            }
            return true;
        }

        void Publish(bool finalState, bool success, bool sessionReady, bool sellActivity,
            bool vehicleReady, bool objectiveReady, bool deliveryIssued,
            Vehicle vehicle, const Native::NativeVector3& target, std::string message) noexcept
        {
            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot.enabled = m_Enabled.load(std::memory_order_acquire);
                m_Snapshot.sessionReady = sessionReady;
                m_Snapshot.sellActivity = sellActivity;
                m_Snapshot.vehicleReady = vehicleReady;
                m_Snapshot.objectiveReady = objectiveReady;
                m_Snapshot.deliveryIssued = deliveryIssued;
                m_Snapshot.haveResult = finalState;
                m_Snapshot.lastSucceeded = success;
                m_Snapshot.stagesCompleted = m_StagesCompleted;
                m_Snapshot.controlAttempts = m_ControlAttempts;
                m_Snapshot.vehicle = vehicle;
                m_Snapshot.targetX = target.x;
                m_Snapshot.targetY = target.y;
                m_Snapshot.targetZ = target.z;
                m_Snapshot.message = std::move(message);
            }
            m_Pending.store(false, std::memory_order_release);
        }

        void Evaluate()
        {
            if (!m_Enabled.load(std::memory_order_acquire))
                return Publish(false, false, false, false, false, false, false, 0, {},
                    "Instant Vehicle Cargo Sell is disabled");

            bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            if (!sessionStarted || !*sessionStarted)
            {
                ResetMissionState();
                return Publish(false, false, false, false, false, false, false, 0, {},
                    "Join GTA Online before using Instant Vehicle Cargo Sell");
            }

            const auto playerId = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::PlayerId);
            auto* pages = GamePointers::Get().ScriptGlobals();
            if (!playerId || *playerId < 0 || *playerId >= VehicleCargoRuntimeShared::MaxPlayers || !pages)
                return Publish(false, false, true, false, false, false, false, 0, {},
                    "Waiting for Enhanced freemode globals");

            const int activity = VehicleCargoRuntimeShared::CurrentActivity(pages, *playerId);
            if (activity != VehicleCargoRuntimeShared::SellActivity)
            {
                RefreshBaseline();
                if (m_InSellActivity && m_StagesCompleted > 0)
                {
                    TUTONES_LOG_INFO("business.vehicle_cargo.sell",
                        std::string("Vehicle Cargo export ended after instant delivery stages=")
                            + std::to_string(m_StagesCompleted));
                    const int completedStages = m_StagesCompleted;
                    ResetMissionState();
                    m_StagesCompleted = completedStages;
                    return Publish(true, true, true, false, false, false, true, 0, {},
                        "Instant Vehicle Cargo Sell complete; Rockstar ended activity 188 and owns the payout/save");
                }

                ResetMissionState();
                return Publish(false, false, true, false, false, false, false, 0, {},
                    "Armed; waiting for Vehicle Cargo sell activity 188");
            }

            if (!m_InSellActivity)
            {
                m_InSellActivity = true;
                m_TargetVehicle = 0;
                m_ProcessedCount = 0;
                m_StagesCompleted = 0;
                m_HaveArmedObjective = false;
                m_WaitingForStageAdvance = false;
                m_ObjectiveArmedAtMs = 0;
                m_StageIssuedAtMs = 0;
                m_ControlAttempts = 0;
            }

            const Vehicle currentVehicle = VehicleCargoRuntimeShared::CurrentPlayerVehicle();
            if (currentVehicle == 0)
                return Publish(false, false, true, true, false, false, m_StagesCompleted > 0,
                    0, {}, "Sell activity 188 is active; waiting for you to enter the export vehicle");

            const int variation = VehicleCargoRuntimeShared::RequestedVariation(pages, *playerId);
            if (variation > 0 && !VehicleCargoRuntimeShared::MatchesVariation(currentVehicle, variation))
                return Publish(false, false, true, true, false, false, m_StagesCompleted > 0,
                    currentVehicle, {}, "Sell activity is active, but the current vehicle does not match Rockstar's export variation");

            if (m_TargetVehicle == 0)
                m_TargetVehicle = currentVehicle;
            if (currentVehicle != m_TargetVehicle)
                return Publish(false, false, true, true, false, false, m_StagesCompleted > 0,
                    currentVehicle, {}, "Export vehicle changed during activity 188; instant movement paused safely");

            if (m_StagesCompleted >= MaxStages)
                return Publish(true, false, true, true, true, false, true,
                    currentVehicle, {}, "Sell mission exceeded the guarded objective-stage limit; no more movement will be issued");

            const auto now = NowMs();
            if (m_WaitingForStageAdvance)
            {
                if ((now - m_StageIssuedAtMs) < StageObserveMs)
                    return Publish(false, false, true, true, true, true, true,
                        currentVehicle, m_ArmedObjective,
                        "Export objective entered; waiting for gb_vehicle_export to advance or finish the sale");

                m_WaitingForStageAdvance = false;
                m_HaveArmedObjective = false;
                m_ObjectiveArmedAtMs = 0;
            }

            Native::NativeVector3 objective{};
            std::array<std::int32_t, MaxTrackedBlips> objectiveBlips{};
            std::size_t objectiveBlipCount = 0;
            bool ambiguous = false;
            if (!FindNewObjective(objective, objectiveBlips, objectiveBlipCount, ambiguous))
            {
                return Publish(false, false, true, true, true, false, m_StagesCompleted > 0,
                    currentVehicle, {}, ambiguous
                        ? "Multiple new coordinate objectives are active; movement withheld until Rockstar leaves one unambiguous route target"
                        : "Sell activity 188 is active; waiting for Rockstar's export route objective");
            }

            if (!m_HaveArmedObjective || DistanceSquared(m_ArmedObjective, objective) > 4.0f)
            {
                m_ArmedObjective = objective;
                m_HaveArmedObjective = true;
                m_ObjectiveArmedAtMs = now;
                m_ControlAttempts = 0;
            }

            if (!EnsureControl(currentVehicle))
            {
                if (m_ControlAttempts >= MaxControlAttempts)
                    return Publish(true, false, true, true, true, true, m_StagesCompleted > 0,
                        currentVehicle, objective,
                        "Could not obtain network control of the export vehicle; sell movement stopped safely");

                return Publish(false, false, true, true, true, true, m_StagesCompleted > 0,
                    currentVehicle, objective,
                    std::string("Requesting export-vehicle network control (")
                        + std::to_string(m_ControlAttempts) + "/" + std::to_string(MaxControlAttempts) + ")");
            }

            auto& native = VehicleCargoNativeBridge::Get();
            if (!native.RequestCollisionAt(objective.x, objective.y, objective.z))
                return Publish(true, false, true, true, true, true, m_StagesCompleted > 0,
                    currentVehicle, objective,
                    "Export objective collision preload native unavailable; no movement issued");

            if ((now - m_ObjectiveArmedAtMs) < CollisionPreloadMs)
                return Publish(false, false, true, true, true, true, m_StagesCompleted > 0,
                    currentVehicle, objective, "Preloading the Rockstar export objective before movement");

            if (VehicleCargoRuntimeShared::CurrentActivity(pages, *playerId) != VehicleCargoRuntimeShared::SellActivity
                || VehicleCargoRuntimeShared::CurrentPlayerVehicle() != currentVehicle)
            {
                return Publish(false, false, true, false, false, false, m_StagesCompleted > 0,
                    0, {}, "Vehicle Cargo state changed during objective preload; movement aborted");
            }

            if (!IssueObjectiveApproach(currentVehicle, objective))
                return Publish(true, false, true, true, true, true, m_StagesCompleted > 0,
                    currentVehicle, objective, "Controlled export-objective approach failed; no blind retry issued");

            for (std::size_t i = 0; i < objectiveBlipCount; ++i)
                RememberBlip(m_ProcessedBlips, m_ProcessedCount, objectiveBlips[i]);

            ++m_StagesCompleted;
            m_StageIssuedAtMs = now;
            m_WaitingForStageAdvance = true;

            TUTONES_LOG_INFO("business.vehicle_cargo.sell",
                std::string("Vehicle Cargo instant export objective issued; stage=")
                    + std::to_string(m_StagesCompleted)
                    + " target=(" + std::to_string(objective.x)
                    + "," + std::to_string(objective.y)
                    + "," + std::to_string(objective.z) + ")");

            Publish(false, false, true, true, true, true, true,
                currentVehicle, objective,
                "Export vehicle moved once through Rockstar's active sell objective; waiting for mission progression");
        }

        std::atomic<bool> m_Enabled{false};
        std::atomic<bool> m_Pending{false};
        std::atomic<std::int64_t> m_NextPollMs{0};
        mutable std::mutex m_Mutex;
        VehicleCargoInstantSellSnapshot m_Snapshot{};

        bool m_InSellActivity{};
        Vehicle m_TargetVehicle{};
        std::array<std::int32_t, MaxTrackedBlips> m_BaselineBlips{};
        std::size_t m_BaselineCount{};
        std::array<std::int32_t, MaxTrackedBlips> m_ProcessedBlips{};
        std::size_t m_ProcessedCount{};
        int m_StagesCompleted{};
        int m_ControlAttempts{};
        bool m_HaveArmedObjective{};
        bool m_WaitingForStageAdvance{};
        Native::NativeVector3 m_ArmedObjective{};
        std::int64_t m_ObjectiveArmedAtMs{};
        std::int64_t m_StageIssuedAtMs{};
    };
}
