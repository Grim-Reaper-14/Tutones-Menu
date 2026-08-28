#pragma once

#include "VehicleCargoAutoSourceRuntime.hpp"
#include "VehicleCargoDeliveryRuntime.hpp"
#include "VehicleCargoInstantSourceRuntime.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::Business
{
    struct VehicleCargoInstantGarageSnapshot final
    {
        bool enabled{};
        bool pending{};
        bool sessionReady{};
        bool warehouseReady{};
        bool missionRunning{};
        bool sourceVehicleReady{};
        bool deliveryIssued{};
        bool lastSucceeded{};
        int warehouseProperty{};
        int sourceVariation{};
        int warehouseStock{};
        std::string sourceMessage{"Instant Source is idle"};
        std::string deliveryMessage{"Instant Delivery is idle"};
        std::string message{"Full Vehicle Cargo pipeline is off"};
    };

    // Coordinator only. Source acquisition and warehouse delivery live in their
    // own runtimes. This class never resolves natives, searches blips, reads
    // warehouse globals, or moves entities itself.
    class VehicleCargoInstantGarageRuntime final
    {
    public:
        static VehicleCargoInstantGarageRuntime& Get() noexcept
        {
            static VehicleCargoInstantGarageRuntime instance;
            return instance;
        }

        void SetEnabled(bool enabled) noexcept
        {
            const bool previous = m_Enabled.exchange(enabled, std::memory_order_acq_rel);
            if (previous == enabled)
                return;

            if (enabled)
            {
                VehicleCargoAutoSourceRuntime::Get().SetEnabled(false);
                m_NextCycleNotBeforeMs = 0;
            }
            else
            {
                m_ManualCycleActive.store(false, std::memory_order_release);
                ResetPipeline(true);
            }

            std::scoped_lock lock(m_Mutex);
            m_Snapshot.enabled = enabled;
            m_Snapshot.message = enabled
                ? "Full pipeline armed: dedicated source runtime -> dedicated delivery runtime"
                : "Full Vehicle Cargo pipeline is off";
        }

        [[nodiscard]] bool Enabled() const noexcept
        {
            return m_Enabled.load(std::memory_order_acquire);
        }

        bool QueueStoreNow()
        {
            m_ManualCycleActive.store(true, std::memory_order_release);
            m_NextCycleNotBeforeMs = 0;
            return StartSourceIfNeeded();
        }

        void Tick() noexcept
        {
            const bool enabled = m_Enabled.load(std::memory_order_acquire);
            const bool manual = m_ManualCycleActive.load(std::memory_order_acquire);
            if (!enabled && !manual)
                return;

            const auto now = NowMs();
            auto next = m_NextPollMs.load(std::memory_order_acquire);
            if (now < next)
                return;
            if (!m_NextPollMs.compare_exchange_strong(next, now + PollIntervalMs, std::memory_order_acq_rel))
                return;

            auto& source = VehicleCargoInstantSourceRuntime::Get();
            auto& delivery = VehicleCargoDeliveryRuntime::Get();
            const auto sourceState = source.Snapshot();
            const auto deliveryState = delivery.Snapshot();

            if (m_DeliveryStarted)
            {
                if (deliveryState.active)
                {
                    StoreSnapshot(sourceState, deliveryState, false,
                        "Delivery stage is active in its own runtime");
                    return;
                }

                if (deliveryState.haveResult)
                {
                    if (!deliveryState.lastSucceeded)
                    {
                        m_Enabled.store(false, std::memory_order_release);
                        m_ManualCycleActive.store(false, std::memory_order_release);
                        source.Cancel();
                        StoreSnapshot(sourceState, deliveryState, false,
                            std::string("Delivery stage stopped: ") + deliveryState.message);
                        return;
                    }

                    source.ClearResult();
                    delivery.ClearResult();
                    m_SourceStarted = false;
                    m_DeliveryStarted = false;
                    m_NextCycleNotBeforeMs = now + NextCycleDelayMs;

                    if (!enabled)
                        m_ManualCycleActive.store(false, std::memory_order_release);

                    StoreSnapshot(sourceState, deliveryState, true,
                        "Full Vehicle Cargo cycle completed; source and delivery runtimes both finished");
                    return;
                }

                StoreSnapshot(sourceState, deliveryState, false,
                    "Delivery runtime is between state updates");
                return;
            }

            if (m_SourceStarted)
            {
                if (sourceState.active)
                {
                    StoreSnapshot(sourceState, deliveryState, false,
                        "Source stage is active in its own runtime");
                    return;
                }

                if (sourceState.vehicleReady && sourceState.vehicle != 0 && sourceState.variation > 0)
                {
                    if (delivery.QueueDelivery(sourceState.vehicle, sourceState.variation))
                    {
                        m_DeliveryStarted = true;
                        source.ClearResult();
                        StoreSnapshot(sourceState, deliveryState, false,
                            "Source stage complete; exact vehicle handed to dedicated delivery runtime");
                        return;
                    }

                    StoreSnapshot(sourceState, deliveryState, false,
                        "Source stage complete; delivery runtime is busy, retrying handoff");
                    return;
                }

                // A started source runtime that becomes inactive without a vehicle
                // is a failed source stage. Do not start delivery and do not touch
                // the warehouse entrance.
                m_Enabled.store(false, std::memory_order_release);
                m_ManualCycleActive.store(false, std::memory_order_release);
                delivery.Cancel();
                StoreSnapshot(sourceState, deliveryState, false,
                    std::string("Source stage stopped before handoff: ") + sourceState.message);
                return;
            }

            if (now < m_NextCycleNotBeforeMs)
            {
                StoreSnapshot(sourceState, deliveryState, true,
                    "Previous cycle completed; waiting before the next source request");
                return;
            }

            if (!StartSourceIfNeeded())
            {
                StoreSnapshot(sourceState, deliveryState, false,
                    "Dedicated source runtime is busy; retrying shortly");
                return;
            }

            const auto startedSource = source.Snapshot();
            StoreSnapshot(startedSource, deliveryState, false,
                "Started dedicated Vehicle Cargo source runtime");
        }

        [[nodiscard]] VehicleCargoInstantGarageSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            auto out = m_Snapshot;
            out.enabled = m_Enabled.load(std::memory_order_acquire);
            out.pending = VehicleCargoInstantSourceRuntime::Get().Snapshot().pending
                || VehicleCargoDeliveryRuntime::Get().Snapshot().pending;
            return out;
        }

    private:
        static constexpr std::int64_t PollIntervalMs = 250;
        static constexpr std::int64_t NextCycleDelayMs = 2000;

        VehicleCargoInstantGarageRuntime() = default;

        [[nodiscard]] static std::int64_t NowMs() noexcept
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        }

        bool StartSourceIfNeeded()
        {
            if (m_SourceStarted || m_DeliveryStarted)
                return true;

            auto& source = VehicleCargoInstantSourceRuntime::Get();
            auto& delivery = VehicleCargoDeliveryRuntime::Get();
            if (delivery.Snapshot().active)
                return false;

            delivery.ClearResult();
            source.ClearResult();
            if (!source.QueueSourceNow())
                return false;

            m_SourceStarted = true;
            return true;
        }

        void ResetPipeline(bool cancelStages) noexcept
        {
            m_SourceStarted = false;
            m_DeliveryStarted = false;
            m_NextCycleNotBeforeMs = 0;
            m_NextPollMs.store(0, std::memory_order_release);

            if (cancelStages)
            {
                VehicleCargoInstantSourceRuntime::Get().Cancel();
                VehicleCargoDeliveryRuntime::Get().Cancel();
            }
        }

        void StoreSnapshot(
            const VehicleCargoInstantSourceSnapshot& source,
            const VehicleCargoDeliverySnapshot& delivery,
            bool success,
            std::string message) noexcept
        {
            std::scoped_lock lock(m_Mutex);
            m_Snapshot.enabled = m_Enabled.load(std::memory_order_acquire);
            m_Snapshot.pending = source.pending || delivery.pending;
            m_Snapshot.sessionReady = source.sessionReady || delivery.sessionReady;
            m_Snapshot.warehouseReady = delivery.warehouseReady;
            m_Snapshot.missionRunning = source.missionRunning || delivery.active || delivery.deliveryIssued;
            m_Snapshot.sourceVehicleReady = source.vehicleReady || delivery.sourceVehicleValid;
            m_Snapshot.deliveryIssued = delivery.deliveryIssued;
            m_Snapshot.lastSucceeded = success;
            m_Snapshot.warehouseProperty = delivery.warehouseProperty;
            m_Snapshot.sourceVariation = source.variation > 0 ? source.variation : delivery.variation;
            m_Snapshot.warehouseStock = delivery.warehouseStock;
            m_Snapshot.sourceMessage = source.message;
            m_Snapshot.deliveryMessage = delivery.message;
            m_Snapshot.message = std::move(message);
        }

        std::atomic<bool> m_Enabled{false};
        std::atomic<bool> m_ManualCycleActive{false};
        std::atomic<std::int64_t> m_NextPollMs{0};

        bool m_SourceStarted{};
        bool m_DeliveryStarted{};
        std::int64_t m_NextCycleNotBeforeMs{};

        mutable std::mutex m_Mutex;
        VehicleCargoInstantGarageSnapshot m_Snapshot{};
    };
}
