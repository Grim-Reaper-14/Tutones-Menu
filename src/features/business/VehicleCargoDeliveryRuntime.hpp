#pragma once

#include "BusinessScriptMonitorRuntime.hpp"
#include "VehicleCargoNativeBridge.hpp"
#include "VehicleCargoRuntimeShared.hpp"
#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/Natives.hpp"
#include "../../game/native/NativeInvoker.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::Business
{
    struct VehicleCargoDeliverySnapshot final
    {
        bool active{};
        bool pending{};
        bool sessionReady{};
        bool warehouseReady{};
        bool rockstarGateReady{};
        bool sourceVehicleValid{};
        bool deliveryIssued{};
        bool haveResult{};
        bool lastSucceeded{};
        int warehouseProperty{};
        int warehouseStock{};
        int variation{};
        int attempts{};
        Vehicle vehicle{};
        std::string message{"Instant Delivery is idle"};
    };

    // Delivery-only runtime. It never launches activity 178 and never searches
    // for a source vehicle. The caller hands it an already-acquired Rockstar
    // source entity; this runtime validates that handoff again before movement.
    class VehicleCargoDeliveryRuntime final
    {
    public:
        static VehicleCargoDeliveryRuntime& Get() noexcept
        {
            static VehicleCargoDeliveryRuntime instance;
            return instance;
        }

        bool QueueDelivery(Vehicle vehicle, int variation)
        {
            if (vehicle == 0 || variation < 1 || variation > 96)
                return false;
            if (m_Pending.load(std::memory_order_acquire)
                || m_Active.load(std::memory_order_acquire))
            {
                return false;
            }

            bool expected = false;
            if (!m_Active.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            ResetState();
            m_TargetVehicle = vehicle;
            m_TargetVariation = variation;
            m_NextPollMs.store(0, std::memory_order_release);

            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot = {};
                m_Snapshot.active = true;
                m_Snapshot.vehicle = vehicle;
                m_Snapshot.variation = variation;
                m_Snapshot.message = "Instant Delivery armed; validating Rockstar source and warehouse state";
            }

            if (QueueEvaluate())
                return true;

            m_Active.store(false, std::memory_order_release);
            Publish(true, false, false, false, false, false, false,
                0, 0, variation, 0, vehicle, "Game-thread queue unavailable");
            return false;
        }

        void Cancel() noexcept
        {
            // Game-thread cycle fields are intentionally left alone here so an
            // in-flight evaluation cannot race an ImGui/UI cancellation.
            m_Active.store(false, std::memory_order_release);
            std::scoped_lock lock(m_Mutex);
            m_Snapshot.active = false;
            m_Snapshot.message = "Instant Delivery cancel requested; no further warehouse movement will be issued";
        }

        void ClearResult() noexcept
        {
            std::scoped_lock lock(m_Mutex);
            m_Snapshot.haveResult = false;
            m_Snapshot.lastSucceeded = false;
            m_Snapshot.deliveryIssued = false;
            m_Snapshot.sourceVehicleValid = false;
            m_Snapshot.rockstarGateReady = false;
            m_Snapshot.vehicle = 0;
            m_Snapshot.variation = 0;
            if (!m_Active.load(std::memory_order_acquire))
                m_Snapshot.message = "Instant Delivery result cleared";
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

        [[nodiscard]] VehicleCargoDeliverySnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            auto out = m_Snapshot;
            out.active = m_Active.load(std::memory_order_acquire);
            out.pending = m_Pending.load(std::memory_order_acquire);
            return out;
        }

    private:
        static constexpr std::int64_t PollIntervalMs = 500;
        static constexpr std::int64_t CollisionPreloadMs = 2000;
        static constexpr std::int64_t DeliveryObserveMs = 5000;
        static constexpr int MaxControlAttempts = 24;
        static constexpr float StagingDistance = 6.0f;
        static constexpr float ApproachSpeed = 3.0f;
        static constexpr float Pi = 3.14159265358979323846f;

        VehicleCargoDeliveryRuntime() = default;

        [[nodiscard]] static std::int64_t NowMs() noexcept
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        }

        [[nodiscard]] bool EnsureControl(Vehicle vehicle) noexcept
        {
            if (vehicle == 0
                || !VehicleCargoRuntimeShared::MatchesVariation(vehicle, m_TargetVariation))
            {
                return false;
            }

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

        [[nodiscard]] static void CalculateStage(
            const VehicleCargoRuntimeShared::WarehouseTarget& target,
            float& stageX,
            float& stageY,
            float& stageZ,
            float& forwardX,
            float& forwardY) noexcept
        {
            const float radians = target.heading * (Pi / 180.0f);
            forwardX = -std::sin(radians);
            forwardY = std::cos(radians);
            stageX = target.x - (forwardX * StagingDistance);
            stageY = target.y - (forwardY * StagingDistance);
            stageZ = target.z + 0.20f;
        }

        [[nodiscard]] bool PreloadWarehouse(
            const VehicleCargoRuntimeShared::WarehouseTarget& target,
            std::int64_t now) noexcept
        {
            float stageX{};
            float stageY{};
            float stageZ{};
            float forwardX{};
            float forwardY{};
            CalculateStage(target, stageX, stageY, stageZ, forwardX, forwardY);

            auto& native = VehicleCargoNativeBridge::Get();
            const bool stageRequested = native.RequestCollisionAt(stageX, stageY, stageZ);
            const bool targetRequested = native.RequestCollisionAt(target.x, target.y, target.z);
            if (!stageRequested || !targetRequested)
                return false;

            if (m_PreloadStartedAtMs == 0)
                m_PreloadStartedAtMs = now;
            return (now - m_PreloadStartedAtMs) >= CollisionPreloadMs;
        }

        [[nodiscard]] bool IssueWarehouseApproach(
            Vehicle vehicle,
            const VehicleCargoRuntimeShared::WarehouseTarget& target) noexcept
        {
            if (vehicle == 0
                || !VehicleCargoRuntimeShared::MatchesVariation(vehicle, m_TargetVariation)
                || VehicleCargoRuntimeShared::CurrentPlayerVehicle() != vehicle)
            {
                return false;
            }

            float stageX{};
            float stageY{};
            float stageZ{};
            float forwardX{};
            float forwardY{};
            CalculateStage(target, stageX, stageY, stageZ, forwardX, forwardY);

            auto& native = VehicleCargoNativeBridge::Get();
            if (!native.RequestCollisionAt(stageX, stageY, stageZ)
                || !native.RequestCollisionAt(target.x, target.y, target.z))
            {
                return false;
            }

            // No freeze/unfreeze loop. The validated mission car is moved once
            // to the loaded staging point and allowed to cross Rockstar's trigger.
            if (!native.SetCoordsNoOffset(vehicle, stageX, stageY, stageZ))
                return false;
            if (!native.SetHeading(vehicle, target.heading))
                return false;
            if (!native.SetVelocity(
                    vehicle,
                    forwardX * ApproachSpeed,
                    forwardY * ApproachSpeed,
                    0.0f))
            {
                return false;
            }
            return true;
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

        void ResetState() noexcept
        {
            m_TargetVehicle = 0;
            m_TargetVariation = 0;
            m_InitialWarehouseStock = -1;
            m_DeliveryIssued = false;
            m_DeliveryIssuedAtMs = 0;
            m_PreloadStartedAtMs = 0;
            m_ControlAttempts = 0;
        }

        void Evaluate()
        {
            if (!m_Active.load(std::memory_order_acquire))
            {
                const int variation = m_TargetVariation;
                const Vehicle vehicle = m_TargetVehicle;
                ResetState();
                return Publish(true, false, false, false, false, false, false,
                    0, 0, variation, 0, vehicle, "Instant Delivery cancelled");
            }

            bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            if (!sessionStarted || !*sessionStarted)
                return Stop(false, false, false, false, false, false,
                    0, 0, m_TargetVariation, 0, m_TargetVehicle,
                    "Join GTA Online before using Instant Delivery");

            auto& scripts = Script::ScriptRuntime::Get();
            if (!scripts.IsReady())
                return Finish(false, true, false, false, false, m_DeliveryIssued,
                    0, 0, m_TargetVariation, m_DeliveryIssued ? 1 : 0,
                    m_TargetVehicle, "Enhanced script runtime unavailable");

            const auto playerId = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::PlayerId);
            if (!playerId || *playerId < 0 || *playerId >= VehicleCargoRuntimeShared::MaxPlayers)
                return Finish(false, true, false, false, false, m_DeliveryIssued,
                    0, 0, m_TargetVariation, m_DeliveryIssued ? 1 : 0,
                    m_TargetVehicle, "PLAYER_ID unavailable");

            VehicleCargoRuntimeShared::WarehouseTarget warehouse{};
            int warehouseProperty = 0;
            int warehouseStock = 0;
            if (!VehicleCargoRuntimeShared::ReadWarehouse(warehouseProperty, warehouseStock, warehouse))
                return Stop(false, true, false, false, false, m_DeliveryIssued,
                    0, 0, m_TargetVariation, m_DeliveryIssued ? 1 : 0,
                    m_TargetVehicle,
                    "Vehicle Warehouse unavailable; delivery stopped without touching an entrance");

            if (warehouseStock >= 40)
                return Stop(false, true, true, false, false, m_DeliveryIssued,
                    warehouseProperty, warehouseStock, m_TargetVariation,
                    m_DeliveryIssued ? 1 : 0, m_TargetVehicle,
                    "Vehicle Warehouse is full (40/40)");

            if (m_InitialWarehouseStock < 0)
                m_InitialWarehouseStock = warehouseStock;

            const auto* cargo = scripts.FindThread(BusinessScriptMonitorRuntime::VehicleCargoScriptHash);
            const bool cargoRunning = cargo && cargo->stack;
            if (!cargoRunning)
            {
                if (m_DeliveryIssued)
                {
                    TUTONES_LOG_INFO("business.vehicle_cargo.delivery",
                        std::string("Vehicle Cargo delivery completed; mission ended, stock=")
                            + std::to_string(warehouseStock));
                    return Stop(true, true, true, false, true, true,
                        warehouseProperty, warehouseStock, m_TargetVariation, 1,
                        m_TargetVehicle,
                        "Instant Delivery complete; Rockstar ended the source mission and owns the save");
                }

                return Stop(false, true, true, false, false, false,
                    warehouseProperty, warehouseStock, m_TargetVariation, 0,
                    m_TargetVehicle,
                    "Vehicle Cargo source mission ended before delivery began");
            }

            auto* pages = GamePointers::Get().ScriptGlobals();
            const int activity = VehicleCargoRuntimeShared::CurrentActivity(pages, *playerId);
            if (activity != VehicleCargoRuntimeShared::SourceActivity)
            {
                if (activity == VehicleCargoRuntimeShared::SellActivity)
                    return Stop(false, true, true, false, false, false,
                        warehouseProperty, warehouseStock, m_TargetVariation, 0,
                        m_TargetVehicle,
                        "Sell activity 188 is running; delivery runtime will not touch it");

                return Finish(true, true, true, false, false, m_DeliveryIssued,
                    warehouseProperty, warehouseStock, m_TargetVariation,
                    m_DeliveryIssued ? 1 : 0, m_TargetVehicle,
                    "Waiting for Vehicle Cargo source activity 178");
            }

            const bool gateReady = VehicleCargoRuntimeShared::RockstarWarehouseGateReady(pages, *playerId);
            if (!gateReady)
                return Finish(true, true, true, false, false, m_DeliveryIssued,
                    warehouseProperty, warehouseStock, m_TargetVariation,
                    m_DeliveryIssued ? 1 : 0, m_TargetVehicle,
                    "Waiting for Rockstar ContrabandDeliveryType registration; warehouse entrance untouched");

            const bool sourceValid = VehicleCargoRuntimeShared::MatchesVariation(
                m_TargetVehicle,
                m_TargetVariation);
            if (!sourceValid)
                return Stop(false, true, true, true, false, m_DeliveryIssued,
                    warehouseProperty, warehouseStock, m_TargetVariation,
                    m_DeliveryIssued ? 1 : 0, m_TargetVehicle,
                    "Delivery target became invalid or changed; warehouse entrance left untouched");

            if (VehicleCargoRuntimeShared::CurrentPlayerVehicle() != m_TargetVehicle)
                return Finish(true, true, true, true, true, m_DeliveryIssued,
                    warehouseProperty, warehouseStock, m_TargetVariation,
                    m_DeliveryIssued ? 1 : 0, m_TargetVehicle,
                    "Registered source car is valid; waiting for player possession before delivery");

            if (warehouseStock > m_InitialWarehouseStock)
            {
                TUTONES_LOG_INFO("business.vehicle_cargo.delivery",
                    std::string("Vehicle Cargo warehouse stock increased: ")
                        + std::to_string(m_InitialWarehouseStock)
                        + " -> " + std::to_string(warehouseStock));
                return Stop(true, true, true, true, true, true,
                    warehouseProperty, warehouseStock, m_TargetVariation,
                    m_DeliveryIssued ? 1 : 0, m_TargetVehicle,
                    "Instant Delivery complete; Rockstar warehouse stock increased");
            }

            if (!EnsureControl(m_TargetVehicle))
            {
                if (m_ControlAttempts >= MaxControlAttempts)
                    return Stop(false, true, true, true, true, m_DeliveryIssued,
                        warehouseProperty, warehouseStock, m_TargetVariation,
                        m_DeliveryIssued ? 1 : 0, m_TargetVehicle,
                        "Delivery could not obtain network control; movement stopped safely");

                return Finish(true, true, true, true, true, m_DeliveryIssued,
                    warehouseProperty, warehouseStock, m_TargetVariation,
                    m_DeliveryIssued ? 1 : 0, m_TargetVehicle,
                    std::string("Delivery requesting network control (")
                        + std::to_string(m_ControlAttempts) + "/" + std::to_string(MaxControlAttempts) + ")");
            }

            const auto now = NowMs();
            if (m_DeliveryIssued)
            {
                if ((now - m_DeliveryIssuedAtMs) < DeliveryObserveMs)
                    return Finish(true, true, true, true, true, true,
                        warehouseProperty, warehouseStock, m_TargetVariation, 1,
                        m_TargetVehicle,
                        "Warehouse approach issued once; waiting for Rockstar's transition/save");

                return Stop(false, true, true, true, true, true,
                    warehouseProperty, warehouseStock, m_TargetVariation, 1,
                    m_TargetVehicle,
                    "Rockstar did not complete the single controlled approach; delivery stopped so the garage cannot be pinned");
            }

            if (!PreloadWarehouse(warehouse, now))
            {
                if (m_PreloadStartedAtMs == 0)
                    return Stop(false, true, true, true, true, false,
                        warehouseProperty, warehouseStock, m_TargetVariation, 0,
                        m_TargetVehicle,
                        "Warehouse collision preload natives unavailable; no movement issued");

                return Finish(true, true, true, true, true, false,
                    warehouseProperty, warehouseStock, m_TargetVariation, 0,
                    m_TargetVehicle,
                    "Preloading Vehicle Warehouse collision before any source-car movement");
            }

            // Revalidate immediately after the preload window and immediately
            // before the one allowed movement operation.
            if (!VehicleCargoRuntimeShared::RockstarWarehouseGateReady(pages, *playerId)
                || !VehicleCargoRuntimeShared::MatchesVariation(m_TargetVehicle, m_TargetVariation)
                || VehicleCargoRuntimeShared::CurrentPlayerVehicle() != m_TargetVehicle)
            {
                return Stop(false, true, true, false, false, false,
                    warehouseProperty, warehouseStock, m_TargetVariation, 0,
                    m_TargetVehicle,
                    "Vehicle Cargo state changed during preload; delivery aborted before movement");
            }

            if (!IssueWarehouseApproach(m_TargetVehicle, warehouse))
                return Stop(false, true, true, true, true, false,
                    warehouseProperty, warehouseStock, m_TargetVariation, 0,
                    m_TargetVehicle,
                    "Controlled warehouse approach failed; no retry will be issued");

            m_DeliveryIssued = true;
            m_DeliveryIssuedAtMs = now;

            TUTONES_LOG_INFO("business.vehicle_cargo.delivery",
                std::string("Single Vehicle Cargo warehouse approach issued: variation=")
                    + std::to_string(m_TargetVariation)
                    + " property=" + std::to_string(warehouseProperty));

            Finish(true, true, true, true, true, true,
                warehouseProperty, warehouseStock, m_TargetVariation, 1,
                m_TargetVehicle,
                "Registered source vehicle moved once through the preloaded Rockstar warehouse transition");
        }

        void Stop(bool success, bool sessionReady, bool warehouseReady, bool gateReady,
            bool sourceVehicleValid, bool deliveryIssued, int warehouseProperty,
            int warehouseStock, int variation, int attempts, Vehicle vehicle,
            std::string message) noexcept
        {
            Publish(true, success, sessionReady, warehouseReady, gateReady,
                sourceVehicleValid, deliveryIssued, warehouseProperty, warehouseStock,
                variation, attempts, vehicle, std::move(message));
        }

        void Finish(bool success, bool sessionReady, bool warehouseReady, bool gateReady,
            bool sourceVehicleValid, bool deliveryIssued, int warehouseProperty,
            int warehouseStock, int variation, int attempts, Vehicle vehicle,
            std::string message) noexcept
        {
            Publish(false, success, sessionReady, warehouseReady, gateReady,
                sourceVehicleValid, deliveryIssued, warehouseProperty, warehouseStock,
                variation, attempts, vehicle, std::move(message));
        }

        void Publish(bool finalState, bool success, bool sessionReady, bool warehouseReady,
            bool gateReady, bool sourceVehicleValid, bool deliveryIssued,
            int warehouseProperty, int warehouseStock, int variation, int attempts,
            Vehicle vehicle, std::string message) noexcept
        {
            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot.active = !finalState;
                m_Snapshot.sessionReady = sessionReady;
                m_Snapshot.warehouseReady = warehouseReady;
                m_Snapshot.rockstarGateReady = gateReady;
                m_Snapshot.sourceVehicleValid = sourceVehicleValid;
                m_Snapshot.deliveryIssued = deliveryIssued;
                m_Snapshot.haveResult = finalState;
                m_Snapshot.lastSucceeded = success;
                m_Snapshot.warehouseProperty = warehouseProperty;
                m_Snapshot.warehouseStock = warehouseStock;
                m_Snapshot.variation = variation;
                m_Snapshot.attempts = attempts;
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

        Vehicle m_TargetVehicle{};
        int m_TargetVariation{};
        int m_InitialWarehouseStock{-1};
        bool m_DeliveryIssued{};
        std::int64_t m_DeliveryIssuedAtMs{};
        std::int64_t m_PreloadStartedAtMs{};
        int m_ControlAttempts{};

        mutable std::mutex m_Mutex;
        VehicleCargoDeliverySnapshot m_Snapshot{};
    };
}
