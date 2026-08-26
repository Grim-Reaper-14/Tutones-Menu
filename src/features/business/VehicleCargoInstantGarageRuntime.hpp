#pragma once

#include "BusinessScriptMonitorRuntime.hpp"
#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/NetshoppingNatives.hpp"
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
    struct VehicleCargoInstantGarageSnapshot final
    {
        bool enabled{};
        bool pending{};
        bool sessionReady{};
        bool warehouseReady{};
        bool transactionReady{};
        bool lastSucceeded{};
        int warehouseSlot{-1};
        int sourceVariation{};
        int warehouseStock{};
        int transactionId{-1};
        std::string message{"Instant Garage is off"};
    };

    class VehicleCargoInstantGarageRuntime final
    {
    public:
        // Enhanced GPBD_FM / PROPERTY_DATA / IE_WAREHOUSE_DATA.
        static constexpr std::size_t PlayerFreemodeGlobal = 1845347;
        static constexpr std::size_t PlayerFreemodeEntrySize = 884;
        static constexpr std::size_t PropertyDataOffset = 260;
        static constexpr std::size_t IEWarehouseDataOffset = 156;
        static constexpr std::size_t IEWarehouseIndexOffset = 0;
        static constexpr std::size_t IEWarehouseVehicleCountOffset = 1;
        static constexpr std::size_t IEWarehouseVehiclesOffset = 2;
        static constexpr int IEWarehouseVehicleSlots = 40;
        static constexpr int MaxPlayers = 32;

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

            m_NextPollMs.store(0, std::memory_order_release);
            std::scoped_lock lock(m_Mutex);
            m_Snapshot.message = enabled
                ? "Instant Garage armed; sourced vehicles will be stored directly"
                : "Instant Garage is off";
        }

        [[nodiscard]] bool Enabled() const noexcept
        {
            return m_Enabled.load(std::memory_order_acquire);
        }

        bool QueueStoreNow()
        {
            return QueueStore(true);
        }

        void Tick() noexcept
        {
            if (!m_Enabled.load(std::memory_order_acquire))
                return;

            const auto now = NowMs();
            auto next = m_NextPollMs.load(std::memory_order_acquire);
            if (now < next)
                return;
            if (!m_NextPollMs.compare_exchange_strong(next, now + PollIntervalMs, std::memory_order_acq_rel))
                return;

            static_cast<void>(QueueStore(false));
        }

        [[nodiscard]] VehicleCargoInstantGarageSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            auto out = m_Snapshot;
            out.enabled = m_Enabled.load(std::memory_order_acquire);
            out.pending = m_Pending.load(std::memory_order_acquire);
            return out;
        }

    private:
        static constexpr std::uint32_t ShopControllerHash = 0x39DA738Bu;

        // appimportexport stores MP_STAT_IE_WH_OWNED_VEHICLE_<slot>_v0 through
        // this server basket. These are category/action hashes, not tunables.
        static constexpr std::uint32_t VehicleWarehouseBasketCategory = 0xA6E56D90u;
        static constexpr std::uint32_t VehicleWarehouseBasketAction = 0xD548DED3u;

        static constexpr std::int64_t PollIntervalMs = 750;
        static constexpr std::int64_t SuccessfulStoreSpacingMs = 5000;
        static constexpr std::int64_t FailureBackoffMs = 5000;

        class ScriptTlsScope final
        {
        public:
            ScriptTlsScope(Types::TlsContext* tls, Types::ScriptThread* thread) noexcept
                : m_Tls(tls)
            {
                if (!m_Tls || !thread)
                    return;

                m_OriginalThread = m_Tls->currentScriptThread;
                m_OriginalActive = m_Tls->scriptThreadActive;
                m_Tls->currentScriptThread = thread;
                m_Tls->scriptThreadActive = true;
                m_Active = true;
            }

            ~ScriptTlsScope()
            {
                if (!m_Active)
                    return;
                m_Tls->scriptThreadActive = m_OriginalActive;
                m_Tls->currentScriptThread = m_OriginalThread;
            }

            [[nodiscard]] bool Active() const noexcept { return m_Active; }

        private:
            Types::TlsContext* m_Tls{};
            Types::ScriptThread* m_OriginalThread{};
            bool m_OriginalActive{};
            bool m_Active{};
        };

        VehicleCargoInstantGarageRuntime() = default;

        [[nodiscard]] static std::int64_t NowMs() noexcept
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        }

        [[nodiscard]] static bool StoredCodeMatches(int stored, int candidate) noexcept
        {
            return stored == candidate || stored == 1000 + candidate;
        }

        bool QueueStore(bool manual)
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            if (Runtime::GameRuntime::Get().Enqueue([this, manual] { ExecuteOnGameThread(manual); }))
                return true;

            Finish(false, false, false, false, -1, 0, 0, -1, "Game-thread queue unavailable");
            return false;
        }

        bool ResolveWarehouse(
            std::int64_t** pages,
            int playerId,
            int& outFreeSlot,
            int& outVariation,
            int& outStock,
            int*& outVehicleCount,
            int*& outFreeSlotGlobal,
            std::string& outFailure) noexcept
        {
            outFreeSlot = -1;
            outVariation = 0;
            outStock = 0;
            outVehicleCount = nullptr;
            outFreeSlotGlobal = nullptr;
            outFailure.clear();

            if (!pages || playerId < 0 || playerId >= MaxPlayers)
            {
                outFailure = "Enhanced Vehicle Warehouse globals unavailable";
                return false;
            }

            const auto playerEntry = Script::ScriptGlobal(PlayerFreemodeGlobal)
                .At(static_cast<std::size_t>(playerId), PlayerFreemodeEntrySize);
            const auto warehouseBase = playerEntry.At(PropertyDataOffset + IEWarehouseDataOffset);

            int* warehouseIndex = warehouseBase.At(IEWarehouseIndexOffset).As<int>(pages);
            int* vehicleCount = warehouseBase.At(IEWarehouseVehicleCountOffset).As<int>(pages);
            const auto vehicles = warehouseBase.At(IEWarehouseVehiclesOffset);
            int* vehiclesCountHeader = vehicles.As<int>(pages);

            if (!warehouseIndex || !vehicleCount || !vehiclesCountHeader
                || *vehiclesCountHeader != IEWarehouseVehicleSlots)
            {
                outFailure = "Enhanced Vehicle Warehouse layout validation failed";
                return false;
            }

            if (*warehouseIndex == 0)
            {
                outFailure = "Purchase a Vehicle Warehouse before using Instant Garage";
                return false;
            }

            std::array<int, IEWarehouseVehicleSlots> stored{};
            int actualStock = 0;
            for (int i = 0; i < IEWarehouseVehicleSlots; ++i)
            {
                int* value = vehicles.At(static_cast<std::size_t>(i), 1).As<int>(pages);
                if (!value)
                {
                    outFailure = "Enhanced Vehicle Warehouse inventory is unavailable";
                    return false;
                }

                stored[static_cast<std::size_t>(i)] = *value;
                if (*value != 0)
                    ++actualStock;
                else if (outFreeSlot < 0)
                {
                    outFreeSlot = i;
                    outFreeSlotGlobal = value;
                }
            }

            if (outFreeSlot < 0 || !outFreeSlotGlobal || actualStock >= IEWarehouseVehicleSlots)
            {
                outFailure = "Vehicle Warehouse is full (40/40)";
                return false;
            }

            int selectionCount = *vehicleCount;
            if (selectionCount < 0)
                selectionCount = actualStock;
            if (selectionCount > IEWarehouseVehicleSlots)
                selectionCount = IEWarehouseVehicleSlots;
            if (selectionCount < actualStock)
                selectionCount = actualStock;

            const auto isPresent = [&stored](int candidate) noexcept {
                for (const int value : stored)
                    if (VehicleCargoInstantGarageRuntime::StoredCodeMatches(value, candidate))
                        return true;
                return false;
            };

            const std::uint64_t seed = static_cast<std::uint64_t>(NowMs())
                + (static_cast<std::uint64_t>(m_SourceVariationNonce++) * 29ull);
            const int start = static_cast<int>(seed % 96ull);

            for (int attempt = 0; attempt < 96; ++attempt)
            {
                const int candidate = ((start + (attempt * 17)) % 96) + 1;
                bool blocked = false;

                if (selectionCount < 32)
                {
                    const int groupStart = (((candidate - 1) / 3) * 3) + 1;
                    blocked = isPresent(groupStart) || isPresent(groupStart + 1) || isPresent(groupStart + 2);
                }
                else
                {
                    blocked = isPresent(candidate);
                }

                if (!blocked)
                {
                    outVariation = candidate;
                    outStock = actualStock;
                    outVehicleCount = vehicleCount;
                    return true;
                }
            }

            outFailure = "No valid Vehicle Cargo source variation is available";
            return false;
        }

        [[nodiscard]] static std::uint32_t VehicleWarehouseStatHash(int slot)
        {
            const std::string statName = std::string("MP_STAT_IE_WH_OWNED_VEHICLE_")
                + std::to_string(slot) + "_v0";
            return Stats::Detail::Joaat(statName);
        }

        void ExecuteOnGameThread(bool manual) noexcept
        {
            if (!manual && !m_Enabled.load(std::memory_order_acquire))
                return Finish(true, false, false, false, -1, 0, 0, -1, "Instant Garage is off");

            bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            if (!sessionStarted || !*sessionStarted)
            {
                m_NotBeforeMs = NowMs() + FailureBackoffMs;
                return Finish(false, false, false, false, -1, 0, 0, -1,
                    "Join GTA Online before using Instant Garage");
            }

            const auto now = NowMs();
            if (!manual && now < m_NotBeforeMs)
                return Finish(true, true, true, true, -1, 0, 0, -1,
                    "Instant Garage waiting before the next warehouse transaction");

            const auto playerId = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::PlayerId);
            if (!playerId || *playerId < 0 || *playerId >= MaxPlayers)
            {
                m_NotBeforeMs = now + FailureBackoffMs;
                return Finish(false, true, false, false, -1, 0, 0, -1, "PLAYER_ID unavailable");
            }

            auto* pages = GamePointers::Get().ScriptGlobals();
            int freeSlot = -1;
            int variation = 0;
            int stock = 0;
            int* vehicleCount = nullptr;
            int* freeSlotGlobal = nullptr;
            std::string failure;
            if (!ResolveWarehouse(pages, *playerId, freeSlot, variation, stock,
                    vehicleCount, freeSlotGlobal, failure))
            {
                m_NotBeforeMs = now + FailureBackoffMs;
                if (failure.find("full") != std::string::npos)
                    m_Enabled.store(false, std::memory_order_release);
                return Finish(false, true, false, false, freeSlot, variation, stock, -1,
                    failure.empty() ? "Unable to resolve Vehicle Warehouse" : std::move(failure));
            }

            const auto useServerTransactions = NetshoppingNatives::UseServerTransactions();
            if (!useServerTransactions || !*useServerTransactions)
            {
                m_NotBeforeMs = now + FailureBackoffMs;
                return Finish(false, true, true, false, freeSlot, variation, stock, -1,
                    "Enhanced server transactions are unavailable");
            }

            auto& scripts = Script::ScriptRuntime::Get();
            auto* shopController = scripts.FindThread(ShopControllerHash);
            if (!scripts.IsReady() || !shopController || !shopController->stack)
            {
                m_NotBeforeMs = now + FailureBackoffMs;
                return Finish(false, true, true, false, freeSlot, variation, stock, -1,
                    "shop_controller is unavailable");
            }

            auto* tls = Types::TlsContext::Get();
            ScriptTlsScope scope(tls, shopController);
            if (!scope.Active())
            {
                m_NotBeforeMs = now + FailureBackoffMs;
                return Finish(false, true, true, false, freeSlot, variation, stock, -1,
                    "Unable to enter shop_controller script context");
            }

            const auto basketActive = NetshoppingNatives::BasketIsActive();
            if (!basketActive)
            {
                m_NotBeforeMs = now + FailureBackoffMs;
                return Finish(false, true, true, false, freeSlot, variation, stock, -1,
                    "Unable to query the Enhanced netshop basket");
            }
            if (*basketActive)
                static_cast<void>(NetshoppingNatives::BasketEnd());

            int transactionId{-1};
            const auto began = NetshoppingNatives::BasketStart(
                &transactionId,
                static_cast<Hash>(VehicleWarehouseBasketCategory),
                static_cast<Hash>(VehicleWarehouseBasketAction),
                4);
            if (!began || !*began || transactionId < 0)
            {
                static_cast<void>(NetshoppingNatives::BasketEnd());
                m_NotBeforeMs = now + FailureBackoffMs;
                return Finish(false, true, true, true, freeSlot, variation, stock, transactionId,
                    "Vehicle Warehouse basket start failed");
            }

            NetshoppingNatives::BasketItem item{};
            item.primaryHash = static_cast<std::int64_t>(VehicleWarehouseStatHash(freeSlot));
            item.secondaryHash = 0;
            item.value = 0;
            item.statValue = 0;

            const auto added = NetshoppingNatives::BasketAddItem(&item, variation);
            if (!added || !*added)
            {
                static_cast<void>(NetshoppingNatives::BasketEnd());
                m_NotBeforeMs = now + FailureBackoffMs;
                return Finish(false, true, true, true, freeSlot, variation, stock, transactionId,
                    "Vehicle Warehouse basket item failed");
            }

            const auto checkout = NetshoppingNatives::CheckoutStart(transactionId);
            if (!checkout || !*checkout)
            {
                static_cast<void>(NetshoppingNatives::BasketEnd());
                m_NotBeforeMs = now + FailureBackoffMs;
                return Finish(false, true, true, true, freeSlot, variation, stock, transactionId,
                    "Vehicle Warehouse checkout failed");
            }

            // Mirror the accepted basket into GPBD_FM immediately. The server basket
            // owns persistence; this write only keeps the live warehouse display/state
            // in sync until Rockstar refreshes the player business data.
            if (freeSlotGlobal && *freeSlotGlobal == 0)
                *freeSlotGlobal = variation;
            if (vehicleCount)
            {
                int nextCount = stock + 1;
                if (nextCount > IEWarehouseVehicleSlots)
                    nextCount = IEWarehouseVehicleSlots;
                *vehicleCount = nextCount;
            }

            m_NotBeforeMs = now + SuccessfulStoreSpacingMs;

            TUTONES_LOG_INFO("business.vehicle_cargo",
                std::string("Instant Vehicle Cargo stored: slot=") + std::to_string(freeSlot)
                    + " variation=" + std::to_string(variation)
                    + " stock=" + std::to_string(stock + 1)
                    + " transaction=" + std::to_string(transactionId));

            Finish(true, true, true, true, freeSlot, variation, stock + 1, transactionId,
                std::string("Vehicle Cargo variation ") + std::to_string(variation)
                    + " stored in warehouse slot " + std::to_string(freeSlot + 1));
        }

        void Finish(
            bool success,
            bool sessionReady,
            bool warehouseReady,
            bool transactionReady,
            int warehouseSlot,
            int sourceVariation,
            int stock,
            int transactionId,
            std::string message) noexcept
        {
            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot.sessionReady = sessionReady;
                m_Snapshot.warehouseReady = warehouseReady;
                m_Snapshot.transactionReady = transactionReady;
                m_Snapshot.lastSucceeded = success;
                m_Snapshot.warehouseSlot = warehouseSlot;
                m_Snapshot.sourceVariation = sourceVariation;
                m_Snapshot.warehouseStock = stock;
                m_Snapshot.transactionId = transactionId;
                m_Snapshot.message = std::move(message);
            }
            m_Pending.store(false, std::memory_order_release);
        }

        std::atomic<bool> m_Enabled{false};
        std::atomic<bool> m_Pending{false};
        std::atomic<std::int64_t> m_NextPollMs{0};
        std::int64_t m_NotBeforeMs{};
        std::uint32_t m_SourceVariationNonce{};

        mutable std::mutex m_Mutex;
        VehicleCargoInstantGarageSnapshot m_Snapshot{};
    };
}
