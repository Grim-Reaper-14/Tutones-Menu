#pragma once

#include "BusinessScriptMonitorRuntime.hpp"
#include "VehicleCargoRuntimeShared.hpp"
#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/Natives.hpp"
#include "../../game/native/NativeCallContext.hpp"
#include "../../game/native/NativeHandlerValidation.hpp"
#include "../../game/native/NativeInvoker.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
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

    // Owns only the delivery half of Vehicle Cargo. It never starts a source
    // mission and never searches for the source entity. The caller must provide
    // the exact already-acquired Rockstar source vehicle and its variation.
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

            bool expected = false;
            if (!m_Active.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            m_TargetVehicle = vehicle;
            m_TargetVariation = variation;
            m_InitialWarehouseStock = -1;
            m_DeliveryIssued = false;
            m_DeliveryAttempts = 0;
            m_LastDeliveryAtMs = 0;
            m_ControlAttempts = 0;
            m_NextPollMs.store(0, std::memory_order_release);

            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot = {};
                m_Snapshot.active = true;
                m_Snapshot.vehicle = vehicle;
                m_Snapshot.variation = variation;
                m_Snapshot.message = "Instant Delivery armed; validating Rockstar's Vehicle Warehouse gate";
            }
            return QueueEvaluate(true);
        }

        void Cancel() noexcept
        {
            m_Active.store(false, std::memory_order_release);
            ResetState();
            std::scoped_lock lock(m_Mutex);
            m_Snapshot.active = false;
            m_Snapshot.pending = false;
            m_Snapshot.message = "Instant Delivery cancelled; warehouse entrance is untouched";
        }

        void ClearResult() noexcept
        {
            std::scoped_lock lock(m_Mutex);
            m_Snapshot.haveResult = false;
            m_Snapshot.lastSucceeded = false;
            m_Snapshot.deliveryIssued = false;
            m_Snapshot.vehicle = 0;
            m_Snapshot.variation = 0;
            if (!m_Active.load(std::memory_order_acquire))
                m_Snapshot.message = "Instant Delivery result cleared";
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

        [[nodiscard]] VehicleCargoDeliverySnapshot Snapshot() const
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
            RequestCollision,
            SetEntityCoords,
            SetEntityHeading,
            FreezeEntity,
            SetEntityVelocity,
            HandlerCount,
        };

        static constexpr std::array<std::uint64_t, HandlerCount> HandlerHashes{{
            0xF093E270C0B6B318ull, // NETWORK_REQUEST_CONTROL_OF_ENTITY
            0x1B1A446EFA398EB5ull, // NETWORK_HAS_CONTROL_OF_ENTITY
            0xEA2D52183C7EA9CFull, // REQUEST_COLLISION_AT_COORD
            0x62C438C53BB57AFDull, // SET_ENTITY_COORDS_NO_OFFSET
            0x5C96CEA06531AB03ull, // SET_ENTITY_HEADING
            0x5D7CD709B34C90F0ull, // FREEZE_ENTITY_POSITION
            0x1AB7223AC0702871ull, // SET_ENTITY_VELOCITY
        }};

        static constexpr std::int64_t PollIntervalMs = 250;
        static constexpr std::int64_t DeliveryObserveMs = 3000;
        static constexpr int MaxDeliveryAttempts = 2;
        static constexpr int MaxControlAttempts = 40;
        static constexpr float StagingDistance = 4.0f;
        static constexpr float ApproachSpeed = 6.0f;
        static constexpr float Pi = 3.14159265358979323846f;

        VehicleCargoDeliveryRuntime() = default;

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

        template<typename... Args>
        [[nodiscard]] bool CallVoid(std::size_t index, Args... args) const noexcept
        {
            if (index >= m_Handlers.size() || !m_Handlers[index])
                return false;

            Native::CallContext context;
            if (!(context.PushArg(args) && ...))
                return false;
            m_Handlers[index](&context);
            context.FixVectors();
            return true;
        }

        [[nodiscard]] bool EnsureControl(Vehicle vehicle) noexcept
        {
            if (vehicle == 0 || !ResolveHandlers())
                return false;

            std::int32_t hasControl{};
            if (!Call(NetworkHasControlOfEntity, hasControl, vehicle))
                return false;
            if (hasControl != 0)
            {
                m_ControlAttempts = 0;
                return true;
            }

            std::int32_t requested{};
            static_cast<void>(Call(NetworkRequestControlOfEntity, requested, vehicle));
            ++m_ControlAttempts;
            return false;
        }

        // Put the validated source car just outside the actual transition point
        // and give it a short forward approach. This crosses Rockstar's trigger
        // instead of repeatedly pinning the car directly on the garage marker.
        [[nodiscard]] bool IssueWarehouseApproach(
            Vehicle vehicle,
            const VehicleCargoRuntimeShared::WarehouseTarget& target) noexcept
        {
            if (vehicle == 0 || !ResolveHandlers())
                return false;

            const float radians = target.heading * (Pi / 180.0f);
            const float forwardX = -std::sin(radians);
            const float forwardY = std::cos(radians);
            const float stageX = target.x - (forwardX * StagingDistance);
            const float stageY = target.y - (forwardY * StagingDistance);
            const float stageZ = target.z + 0.20f;

            static_cast<void>(CallVoid(RequestCollision, stageX, stageY, stageZ));
            static_cast<void>(CallVoid(RequestCollision, target.x, target.y, target.z));
            static_cast<void>(CallVoid(FreezeEntity, vehicle, std::int32_t{1}));

            const bool moved = CallVoid(
                SetEntityCoords,
                vehicle,
                stageX,
                stageY,
                stageZ,
                std::int32_t{1},
                std::int32_t{1},
                std::int32_t{1});
            if (!moved)
            {
                static_cast<void>(CallVoid(FreezeEntity, vehicle, std::int32_t{0}));
                return false;
            }

            static_cast<void>(CallVoid(SetEntityHeading, vehicle, target.heading));
            static_cast<void>(CallVoid(SetEntityVelocity,
                vehicle,
                forwardX * ApproachSpeed,
                forwardY * ApproachSpeed,
                0.0f));
            static_cast<void>(CallVoid(FreezeEntity, vehicle, std::int32_t{0}));
            return true;
        }

        bool QueueEvaluate(bool manual)
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            if (Runtime::GameRuntime::Get().Enqueue([this, manual] { Evaluate(manual); }))
                return true;

            m_Pending.store(false, std::memory_order_release);
            Stop(false, false, false, false, false, false, 0, 0,
                m_TargetVariation, m_DeliveryAttempts, m_TargetVehicle,
                "Game-thread queue unavailable");
            return false;
        }

        void ResetState() noexcept
        {
            m_TargetVehicle = 0;
            m_TargetVariation = 0;
            m_InitialWarehouseStock = -1;
            m_DeliveryIssued = false;
            m_DeliveryAttempts = 0;
            m_LastDeliveryAtMs = 0;
            m_ControlAttempts = 0;
        }

        void Evaluate(bool)
        {
            if (!m_Active.load(std::memory_order_acquire))
                return Finish(false, false, false, false, false, false, 0, 0,
                    m_TargetVariation, m_DeliveryAttempts, m_TargetVehicle,
                    "Instant Delivery is idle");

            bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            if (!sessionStarted || !*sessionStarted)
                return Stop(false, false, false, false, false, false, 0, 0,
                    m_TargetVariation, m_DeliveryAttempts, m_TargetVehicle,
                    "Join GTA Online before using Instant Delivery");

            auto& scripts = Script::ScriptRuntime::Get();
            if (!scripts.IsReady())
                return Finish(false, true, false, false, false, m_DeliveryIssued, 0, 0,
                    m_TargetVariation, m_DeliveryAttempts, m_TargetVehicle,
                    "Enhanced script runtime unavailable");

            const auto playerId = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::PlayerId);
            if (!playerId || *playerId < 0 || *playerId >= VehicleCargoRuntimeShared::MaxPlayers)
                return Finish(false, true, false, false, false, m_DeliveryIssued, 0, 0,
                    m_TargetVariation, m_DeliveryAttempts, m_TargetVehicle,
                    "PLAYER_ID unavailable");

            VehicleCargoRuntimeShared::WarehouseTarget warehouse{};
            int warehouseProperty = 0;
            int warehouseStock = 0;
            if (!VehicleCargoRuntimeShared::ReadWarehouse(warehouseProperty, warehouseStock, warehouse))
                return Stop(false, true, false, false, false, m_DeliveryIssued, 0, 0,
                    m_TargetVariation, m_DeliveryAttempts, m_TargetVehicle,
                    "Vehicle Warehouse not available; delivery runtime stopped without touching an entrance");

            if (warehouseStock >= 40)
                return Stop(false, true, true, false, false, m_DeliveryIssued,
                    warehouseProperty, warehouseStock, m_TargetVariation, m_DeliveryAttempts,
                    m_TargetVehicle, "Vehicle Warehouse is full (40/40)");

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
                        warehouseProperty, warehouseStock, m_TargetVariation, m_DeliveryAttempts,
                        m_TargetVehicle, "Instant Delivery complete; Rockstar ended the source mission and owns the save");
                }

                return Stop(false, true, true, false, false, false,
                    warehouseProperty, warehouseStock, m_TargetVariation, m_DeliveryAttempts,
                    m_TargetVehicle, "Vehicle Cargo source mission ended before delivery began");
            }

            auto* pages = GamePointers::Get().ScriptGlobals();
            const int activity = VehicleCargoRuntimeShared::CurrentActivity(pages, *playerId);
            if (activity != VehicleCargoRuntimeShared::SourceActivity)
            {
                if (activity == VehicleCargoRuntimeShared::SellActivity)
                    return Stop(false, true, true, false, false, false,
                        warehouseProperty, warehouseStock, m_TargetVariation, m_DeliveryAttempts,
                        m_TargetVehicle, "Sell activity 188 is running; delivery runtime will not touch it");

                return Finish(true, true, true, false, false, m_DeliveryIssued,
                    warehouseProperty, warehouseStock, m_TargetVariation, m_DeliveryAttempts,
                    m_TargetVehicle, "Waiting for Vehicle Cargo source activity 178");
            }

            const bool gateReady = VehicleCargoRuntimeShared::RockstarWarehouseGateReady(pages, *playerId);
            if (!gateReady)
                return Finish(true, true, true, false, false, m_DeliveryIssued,
                    warehouseProperty, warehouseStock, m_TargetVariation, m_DeliveryAttempts,
                    m_TargetVehicle,
                    "Waiting for Rockstar ContrabandDeliveryType registration; warehouse entrance is untouched");

            const bool sourceValid = VehicleCargoRuntimeShared::MatchesVariation(m_TargetVehicle, m_TargetVariation);
            if (!sourceValid)
                return Stop(false, true, true, true, false, m_DeliveryIssued,
                    warehouseProperty, warehouseStock, m_TargetVariation, m_DeliveryAttempts,
                    m_TargetVehicle,
                    "Delivery target no longer matches Rockstar's requested model/plate; warehouse entrance left untouched");

            if (VehicleCargoRuntimeShared::CurrentPlayerVehicle() != m_TargetVehicle)
                return Finish(true, true, true, true, true, m_DeliveryIssued,
                    warehouseProperty, warehouseStock, m_TargetVariation, m_DeliveryAttempts,
                    m_TargetVehicle,
                    "Registered source car is valid; waiting for source runtime/player possession before delivery");

            if (warehouseStock > m_InitialWarehouseStock)
            {
                TUTONES_LOG_INFO("business.vehicle_cargo.delivery",
                    std::string("Vehicle Cargo delivery completed by stock increment: ")
                        + std::to_string(m_InitialWarehouseStock) + " -> " + std::to_string(warehouseStock));
                return Stop(true, true, true, true, true, true,
                    warehouseProperty, warehouseStock, m_TargetVariation, m_DeliveryAttempts,
                    m_TargetVehicle, "Instant Delivery complete; Rockstar warehouse stock increased");
            }

            if (!EnsureControl(m_TargetVehicle))
            {
                if (m_ControlAttempts >= MaxControlAttempts)
                    return Stop(false, true, true, true, true, m_DeliveryIssued,
                        warehouseProperty, warehouseStock, m_TargetVariation, m_DeliveryAttempts,
                        m_TargetVehicle,
                        "Delivery runtime could not obtain network control; automatic movement stopped");

                return Finish(true, true, true, true, true, m_DeliveryIssued,
                    warehouseProperty, warehouseStock, m_TargetVariation, m_DeliveryAttempts,
                    m_TargetVehicle,
                    std::string("Delivery runtime requesting network control (")
                        + std::to_string(m_ControlAttempts) + "/" + std::to_string(MaxControlAttempts) + ")");
            }

            const auto now = NowMs();
            if (m_DeliveryIssued && (now - m_LastDeliveryAtMs) < DeliveryObserveMs)
                return Finish(true, true, true, true, true, true,
                    warehouseProperty, warehouseStock, m_TargetVariation, m_DeliveryAttempts,
                    m_TargetVehicle,
                    "Warehouse approach issued; waiting for Rockstar's garage transition/save");

            if (m_DeliveryAttempts >= MaxDeliveryAttempts)
                return Stop(false, true, true, true, true, true,
                    warehouseProperty, warehouseStock, m_TargetVariation, m_DeliveryAttempts,
                    m_TargetVehicle,
                    "Rockstar did not complete the transition after two controlled approaches; delivery stopped so the garage cannot be pinned");

            if (!IssueWarehouseApproach(m_TargetVehicle, warehouse))
                return Stop(false, true, true, true, true, false,
                    warehouseProperty, warehouseStock, m_TargetVariation, m_DeliveryAttempts,
                    m_TargetVehicle,
                    "Warehouse approach natives unavailable; delivery stopped without further movement");

            m_DeliveryIssued = true;
            ++m_DeliveryAttempts;
            m_LastDeliveryAtMs = now;

            TUTONES_LOG_INFO("business.vehicle_cargo.delivery",
                std::string("Vehicle Cargo warehouse approach issued: variation=")
                    + std::to_string(m_TargetVariation)
                    + " property=" + std::to_string(warehouseProperty)
                    + " attempt=" + std::to_string(m_DeliveryAttempts));

            Finish(true, true, true, true, true, true,
                warehouseProperty, warehouseStock, m_TargetVariation, m_DeliveryAttempts,
                m_TargetVehicle,
                "Registered source vehicle staged outside the real warehouse transition and moving through Rockstar's trigger");
        }

        void Stop(bool success, bool sessionReady, bool warehouseReady, bool gateReady,
            bool sourceVehicleValid, bool deliveryIssued, int warehouseProperty, int warehouseStock,
            int variation, int attempts, Vehicle vehicle, std::string message) noexcept
        {
            m_Active.store(false, std::memory_order_release);
            Finish(success, sessionReady, warehouseReady, gateReady, sourceVehicleValid,
                deliveryIssued, warehouseProperty, warehouseStock, variation, attempts,
                vehicle, std::move(message), true);
        }

        void Finish(bool success, bool sessionReady, bool warehouseReady, bool gateReady,
            bool sourceVehicleValid, bool deliveryIssued, int warehouseProperty, int warehouseStock,
            int variation, int attempts, Vehicle vehicle, std::string message, bool haveResult = false) noexcept
        {
            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot.active = m_Active.load(std::memory_order_acquire);
                m_Snapshot.sessionReady = sessionReady;
                m_Snapshot.warehouseReady = warehouseReady;
                m_Snapshot.rockstarGateReady = gateReady;
                m_Snapshot.sourceVehicleValid = sourceVehicleValid;
                m_Snapshot.deliveryIssued = deliveryIssued;
                m_Snapshot.haveResult = haveResult;
                m_Snapshot.lastSucceeded = success;
                m_Snapshot.warehouseProperty = warehouseProperty;
                m_Snapshot.warehouseStock = warehouseStock;
                m_Snapshot.variation = variation;
                m_Snapshot.attempts = attempts;
                m_Snapshot.vehicle = vehicle;
                m_Snapshot.message = std::move(message);
            }
            m_Pending.store(false, std::memory_order_release);
        }

        std::atomic<bool> m_Active{false};
        std::atomic<bool> m_Pending{false};
        std::atomic<std::int64_t> m_NextPollMs{0};

        Vehicle m_TargetVehicle{};
        int m_TargetVariation{};
        int m_InitialWarehouseStock{-1};
        bool m_DeliveryIssued{};
        int m_DeliveryAttempts{};
        std::int64_t m_LastDeliveryAtMs{};
        int m_ControlAttempts{};

        std::array<Native::NativeHandler, HandlerCount> m_Handlers{};
        mutable std::mutex m_Mutex;
        VehicleCargoDeliverySnapshot m_Snapshot{};
    };
}
