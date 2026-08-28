#pragma once

#include "BusinessScriptMonitorRuntime.hpp"
#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/Stats.hpp"
#include "../../game/native/NativeInvoker.hpp"
#include "../../game/script/ScriptGlobal.hpp"
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
    struct VehicleCargoAutoSourceSnapshot final
    {
        bool enabled{};
        bool pending{};
        bool sessionReady{};
        bool warehouseReady{};
        bool lastSucceeded{};
        int warehouseProperty{};
        int warehouseStock{};
        int lastVehicleId{};
        int lastWarehouseSlot{-1};
        std::string message{"Instant Auto Source is off"};
    };

    // Instant Vehicle Cargo sourcing deliberately does not launch activity 178.
    // It mirrors Rockstar's own ADD_VEHICLE_TO_IE_WAREHOUSE path:
    //   1) write MPX_IE_WH_OWNED_VEHICLE_<slot>
    //   2) read the stat back
    //   3) mirror the value into the local IE warehouse broadcast array/count
    // This keeps the normal Vehicle Warehouse entrance free because no steal
    // mission is created and no world vehicle has to cross the garage trigger.
    class VehicleCargoAutoSourceRuntime final
    {
    public:
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
            std::scoped_lock lock(m_Mutex);
            m_Message = enabled
                ? "Instant Auto Source armed; vehicles will be written straight into the Vehicle Warehouse"
                : "Instant Auto Source is off";
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
            if (!m_Enabled.load(std::memory_order_acquire))
                return;

            const auto now = NowMs();
            auto next = m_NextPollMs.load(std::memory_order_acquire);
            if (now < next)
                return;
            if (!m_NextPollMs.compare_exchange_strong(next, now + AutoSourceIntervalMs, std::memory_order_acq_rel))
                return;

            static_cast<void>(QueuePoll(false));
        }

        [[nodiscard]] VehicleCargoAutoSourceSnapshot Snapshot() const
        {
            VehicleCargoAutoSourceSnapshot out;
            out.enabled = m_Enabled.load(std::memory_order_acquire);
            out.pending = m_Pending.load(std::memory_order_acquire);

            std::scoped_lock lock(m_Mutex);
            out.sessionReady = m_SessionReady;
            out.warehouseReady = m_WarehouseReady;
            out.lastSucceeded = m_LastSucceeded;
            out.warehouseProperty = m_WarehouseProperty;
            out.warehouseStock = m_WarehouseStock;
            out.lastVehicleId = m_LastVehicleId;
            out.lastWarehouseSlot = m_LastWarehouseSlot;
            out.message = m_Message;
            return out;
        }

    private:
        static constexpr std::int64_t AutoSourceIntervalMs = 1500;

        VehicleCargoAutoSourceRuntime() = default;

        [[nodiscard]] static std::int64_t NowMs() noexcept
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        }

        [[nodiscard]] static std::string WarehouseVehicleStat(int slot)
        {
            return std::string("MPX_IE_WH_OWNED_VEHICLE_") + std::to_string(slot);
        }

        [[nodiscard]] static bool StoredCodeMatches(int stored, int candidate) noexcept
        {
            return stored == candidate || stored == 1000 + candidate;
        }

        [[nodiscard]] int SelectVehicleId(
            const std::array<int, IEWarehouseVehicleSlots>& stored,
            int occupiedCount) noexcept
        {
            const auto isPresent = [&stored](int candidate) noexcept {
                for (const int value : stored)
                {
                    if (StoredCodeMatches(value, candidate))
                        return true;
                }
                return false;
            };

            // Rockstar's Import/Export list is 32 model groups x 3 plate/tier
            // variants. Below 32 cars, avoid an entire group when any variant is
            // already present. At 32+, only reject the exact occupied variant.
            const std::uint64_t seed = static_cast<std::uint64_t>(NowMs())
                + (static_cast<std::uint64_t>(m_SourceVariationNonce++) * 29ull);
            const int start = static_cast<int>(seed % 96ull);

            for (int attempt = 0; attempt < 96; ++attempt)
            {
                const int candidate = ((start + (attempt * 17)) % 96) + 1;
                bool blocked = false;

                if (occupiedCount < 32)
                {
                    const int groupStart = (((candidate - 1) / 3) * 3) + 1;
                    blocked = isPresent(groupStart)
                        || isPresent(groupStart + 1)
                        || isPresent(groupStart + 2);
                }
                else
                {
                    blocked = isPresent(candidate);
                }

                if (!blocked)
                    return candidate;
            }

            return 0;
        }

        bool QueuePoll(bool manual)
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            SetPending(manual
                ? "Sourcing one Vehicle Cargo car straight into the warehouse"
                : "Auto Source is adding the next Vehicle Cargo car to the warehouse");

            if (Runtime::GameRuntime::Get().Enqueue([this, manual] { Evaluate(manual); }))
                return true;

            Finish(false, false, false, 0, 0, 0, -1, "Game-thread queue unavailable");
            return false;
        }

        void Evaluate(bool manual) noexcept
        {
            bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            if (!sessionStarted || !*sessionStarted)
                return Finish(false, false, false, 0, 0, 0, -1,
                    "Join GTA Online before using Instant Auto Source");

            auto& scripts = Script::ScriptRuntime::Get();
            if (scripts.IsReady())
            {
                const auto* cargo = scripts.FindThread(BusinessScriptMonitorRuntime::VehicleCargoScriptHash);
                if (cargo && cargo->stack)
                {
                    return Finish(false, true, false, 0, 0, 0, -1,
                        "Finish the active Vehicle Cargo mission first; direct Auto Source will not alter activity 178");
                }
            }

            const auto playerId = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::PlayerId);
            if (!playerId || *playerId < 0 || *playerId >= MaxPlayers)
                return Finish(false, true, false, 0, 0, 0, -1, "PLAYER_ID unavailable");

            auto* pages = GamePointers::Get().ScriptGlobals();
            if (!pages)
                return Finish(false, true, false, 0, 0, 0, -1,
                    "Enhanced script globals are unavailable");

            const auto characterIndex = Stats::GetCharIndex();
            if (!characterIndex)
                return Finish(false, true, false, 0, 0, 0, -1,
                    "MP character stats are unavailable");

            const auto playerEntry = Script::ScriptGlobal(PlayerFreemodeGlobal)
                .At(static_cast<std::size_t>(*playerId), PlayerFreemodeEntrySize);
            const auto warehouseBase = playerEntry.At(PropertyDataOffset + IEWarehouseDataOffset);

            int* warehouseIndex = warehouseBase.At(IEWarehouseIndexOffset).As<int>(pages);
            int* liveVehicleCount = warehouseBase.At(IEWarehouseVehicleCountOffset).As<int>(pages);
            const auto liveVehicles = warehouseBase.At(IEWarehouseVehiclesOffset);
            int* liveArrayCount = liveVehicles.As<int>(pages);

            if (!warehouseIndex || !liveVehicleCount || !liveArrayCount
                || *liveArrayCount != IEWarehouseVehicleSlots)
            {
                return Finish(false, true, false, 0, 0, 0, -1,
                    "Enhanced Vehicle Warehouse live layout validation failed");
            }

            if (*warehouseIndex == 0)
                return Finish(false, true, false, 0, 0, 0, -1,
                    "Purchase a Vehicle Warehouse before using Instant Auto Source");

            std::array<int*, IEWarehouseVehicleSlots> liveSlots{};
            for (int slot = 0; slot < IEWarehouseVehicleSlots; ++slot)
            {
                liveSlots[static_cast<std::size_t>(slot)] = liveVehicles
                    .At(static_cast<std::size_t>(slot), 1)
                    .As<int>(pages);
                if (!liveSlots[static_cast<std::size_t>(slot)])
                {
                    return Finish(false, true, false, *warehouseIndex, 0, 0, -1,
                        "Vehicle Warehouse live slot validation failed");
                }
            }

            std::array<int, IEWarehouseVehicleSlots> stored{};
            int occupiedCount = 0;
            int emptySlot = -1;
            for (int slot = 0; slot < IEWarehouseVehicleSlots; ++slot)
            {
                const auto value = Stats::GetInt(WarehouseVehicleStat(slot), *characterIndex);
                if (!value)
                {
                    return Finish(false, true, false, *warehouseIndex, occupiedCount, 0, -1,
                        "Vehicle Warehouse persistent slot read failed");
                }

                stored[static_cast<std::size_t>(slot)] = *value;
                if (*value != 0)
                    ++occupiedCount;
                else if (emptySlot < 0)
                    emptySlot = slot;
            }

            if (emptySlot < 0 || occupiedCount >= IEWarehouseVehicleSlots)
            {
                m_Enabled.store(false, std::memory_order_release);
                return Finish(true, true, true, *warehouseIndex, IEWarehouseVehicleSlots, 0, -1,
                    "Vehicle Warehouse is full; Instant Auto Source stopped");
            }

            const int vehicleId = SelectVehicleId(stored, occupiedCount);
            if (vehicleId <= 0 || vehicleId > 96)
            {
                m_Enabled.store(false, std::memory_order_release);
                return Finish(false, true, true, *warehouseIndex, occupiedCount, 0, -1,
                    "No valid non-duplicate Vehicle Cargo ID is available; Auto Source stopped");
            }

            const std::string statName = WarehouseVehicleStat(emptySlot);
            if (!Stats::SetInt(statName, vehicleId, *characterIndex))
            {
                return Finish(false, true, true, *warehouseIndex, occupiedCount, 0, -1,
                    "Vehicle Warehouse persistent write was rejected");
            }

            const auto confirmation = Stats::GetInt(statName, *characterIndex);
            if (!confirmation || *confirmation != vehicleId)
            {
                return Finish(false, true, true, *warehouseIndex, occupiedCount, 0, -1,
                    "Vehicle Warehouse persistent slot failed read-back verification");
            }

            // Mirror Rockstar ADD_VEHICLE_TO_GLOBAL_BROADCAST_DATA so the local
            // warehouse state sees the new vehicle immediately in this session.
            *liveSlots[static_cast<std::size_t>(emptySlot)] = vehicleId;
            *liveVehicleCount = occupiedCount + 1;

            const bool liveVerified = *liveSlots[static_cast<std::size_t>(emptySlot)] == vehicleId
                && *liveVehicleCount == occupiedCount + 1;
            if (!liveVerified)
            {
                return Finish(false, true, true, *warehouseIndex, occupiedCount + 1,
                    vehicleId, emptySlot,
                    "Vehicle was saved but the live warehouse cache failed verification; re-enter the warehouse to refresh");
            }

            TUTONES_LOG_INFO(
                "business.vehicle_cargo",
                std::string("Instant-sourced Vehicle Cargo directly to warehouse: vehicleId=")
                    + std::to_string(vehicleId)
                    + " slot=" + std::to_string(emptySlot)
                    + " stock=" + std::to_string(occupiedCount + 1)
                    + "/40 property=" + std::to_string(*warehouseIndex));

            Finish(true, true, true, *warehouseIndex, occupiedCount + 1,
                vehicleId, emptySlot,
                std::string("Vehicle ID ") + std::to_string(vehicleId)
                    + " sourced straight into warehouse slot " + std::to_string(emptySlot)
                    + "; no activity 178 was started");

            if (!manual && occupiedCount + 1 >= IEWarehouseVehicleSlots)
                m_Enabled.store(false, std::memory_order_release);
        }

        void SetPending(std::string message)
        {
            std::scoped_lock lock(m_Mutex);
            m_LastSucceeded = false;
            m_Message = std::move(message);
        }

        void Finish(
            bool success,
            bool sessionReady,
            bool warehouseReady,
            int warehouseProperty,
            int warehouseStock,
            int vehicleId,
            int warehouseSlot,
            std::string message) noexcept
        {
            {
                std::scoped_lock lock(m_Mutex);
                m_SessionReady = sessionReady;
                m_WarehouseReady = warehouseReady;
                m_LastSucceeded = success;
                m_WarehouseProperty = warehouseProperty;
                m_WarehouseStock = warehouseStock;
                if (vehicleId > 0)
                    m_LastVehicleId = vehicleId;
                if (warehouseSlot >= 0)
                    m_LastWarehouseSlot = warehouseSlot;
                m_Message = std::move(message);
            }
            m_Pending.store(false, std::memory_order_release);
        }

        std::atomic<bool> m_Enabled{false};
        std::atomic<bool> m_Pending{false};
        std::atomic<std::int64_t> m_NextPollMs{0};

        std::uint64_t m_SourceVariationNonce{};

        mutable std::mutex m_Mutex;
        bool m_SessionReady{};
        bool m_WarehouseReady{};
        bool m_LastSucceeded{};
        int m_WarehouseProperty{};
        int m_WarehouseStock{};
        int m_LastVehicleId{};
        int m_LastWarehouseSlot{-1};
        std::string m_Message{"Instant Auto Source is off"};
    };
}
