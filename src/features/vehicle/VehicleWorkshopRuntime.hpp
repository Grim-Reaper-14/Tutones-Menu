#pragma once

#include "../../game/GameState.hpp"
#include "../../game/Natives.hpp"
#include "../../game/VehicleNatives.hpp"
#include "../../game/vehicle/VehicleModels.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace Tutones::Game::Mods
{
    enum class VehicleWorkshopAction : unsigned char
    {
        None,
        SetPerformanceLevel,
        SetTurbo,
        MaxPerformance,
        StockPerformance,
    };

    enum class VehicleWorkshopResult : unsigned char
    {
        Idle,
        Queued,
        Verified,
        Failed,
        Stale,
    };

    struct VehicleWorkshopSnapshot final
    {
        VehicleWorkshopSnapshot()
        {
            modCounts.fill(-1);
            currentMods.fill(-1);
        }

        Vehicle vehicle{};
        Hash model{};
        int vehicleClass{-1};
        std::string displayName{"Current Vehicle"};
        std::string modelCode{};
        std::string plate{};

        std::array<int, 50> modCounts{};
        std::array<int, 50> currentMods{};
        int supportedModSlots{};
        int availableModOptions{};
        bool turbo{};
        bool capabilitiesReady{};
        bool valid{};

        VehicleWorkshopAction lastAction{VehicleWorkshopAction::None};
        VehicleWorkshopResult lastResult{VehicleWorkshopResult::Idle};
        int lastSlot{-1};
        int lastRequested{-1};
        int lastObserved{-1};
    };

    class VehicleWorkshopRuntime final
    {
    public:
        using Clock = std::chrono::steady_clock;
        static constexpr auto LiveRefreshInterval = std::chrono::milliseconds{500};
        static constexpr std::array<int, 5> PerformanceSlots{{11, 12, 13, 15, 16}};
        static constexpr int TurboSlot = 18;

        static VehicleWorkshopRuntime& Get() noexcept
        {
            static VehicleWorkshopRuntime instance;
            return instance;
        }

        [[nodiscard]] VehicleWorkshopSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            return m_Snapshot;
        }

        void RequestRefresh(Vehicle vehicle) noexcept
        {
            if (vehicle == 0)
            {
                ClearIfNeeded();
                return;
            }

            const auto now = NowMilliseconds();
            const Vehicle lastVehicle = m_LastQueuedVehicle.load(std::memory_order_acquire);
            const auto lastRefresh = m_LastRefreshMilliseconds.load(std::memory_order_acquire);
            if (lastVehicle == vehicle && now - lastRefresh < LiveRefreshInterval.count())
                return;

            bool expected = false;
            if (!m_RefreshPending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return;

            m_LastQueuedVehicle.store(vehicle, std::memory_order_release);
            if (!Runtime::GameRuntime::Get().Enqueue([this, vehicle] {
                    RefreshOnGameThread(vehicle);
                }))
            {
                m_RefreshPending.store(false, std::memory_order_release);
            }
        }

        [[nodiscard]] bool QueueSetPerformanceLevel(int modType, int modIndex)
        {
            if (!IsPerformanceSlot(modType) || modIndex < -1)
                return false;

            const Vehicle expectedVehicle = CurrentVehicle();
            if (expectedVehicle == 0)
                return false;

            RecordAction(
                VehicleWorkshopAction::SetPerformanceLevel,
                VehicleWorkshopResult::Queued,
                modType,
                modIndex,
                -1);

            const bool queued = Runtime::GameRuntime::Get().Enqueue([this, expectedVehicle, modType, modIndex] {
                if (CurrentVehicle() != expectedVehicle)
                {
                    RecordAction(
                        VehicleWorkshopAction::SetPerformanceLevel,
                        VehicleWorkshopResult::Stale,
                        modType,
                        modIndex,
                        -1);
                    return;
                }

                bool success = Natives::SetVehicleModKit(expectedVehicle, 0);
                const auto count = success ? Natives::GetNumVehicleMods(expectedVehicle, modType) : std::nullopt;
                if (!count || *count <= 0 || modIndex >= *count)
                    success = false;

                if (success)
                {
                    if (modIndex < 0)
                        success = Natives::RemoveVehicleMod(expectedVehicle, modType);
                    else
                        success = Natives::SetVehicleMod(expectedVehicle, modType, modIndex, false);
                }

                const auto observed = Natives::GetVehicleMod(expectedVehicle, modType);
                success = success && observed && *observed == modIndex;
                RecordAction(
                    VehicleWorkshopAction::SetPerformanceLevel,
                    success ? VehicleWorkshopResult::Verified : VehicleWorkshopResult::Failed,
                    modType,
                    modIndex,
                    observed.value_or(-1));
                RefreshOnGameThread(expectedVehicle);
            });

            if (!queued)
            {
                RecordAction(
                    VehicleWorkshopAction::SetPerformanceLevel,
                    VehicleWorkshopResult::Failed,
                    modType,
                    modIndex,
                    -1);
            }
            return queued;
        }

        [[nodiscard]] bool QueueTurbo(bool enabled)
        {
            const Vehicle expectedVehicle = CurrentVehicle();
            if (expectedVehicle == 0)
                return false;

            RecordAction(
                VehicleWorkshopAction::SetTurbo,
                VehicleWorkshopResult::Queued,
                TurboSlot,
                enabled ? 1 : 0,
                -1);

            const bool queued = Runtime::GameRuntime::Get().Enqueue([this, expectedVehicle, enabled] {
                if (CurrentVehicle() != expectedVehicle)
                {
                    RecordAction(
                        VehicleWorkshopAction::SetTurbo,
                        VehicleWorkshopResult::Stale,
                        TurboSlot,
                        enabled ? 1 : 0,
                        -1);
                    return;
                }

                bool success = Natives::SetVehicleModKit(expectedVehicle, 0);
                const auto count = success ? Natives::GetNumVehicleMods(expectedVehicle, TurboSlot) : std::nullopt;
                if (!count || *count <= 0)
                    success = false;

                if (success)
                    success = Natives::ToggleVehicleMod(expectedVehicle, TurboSlot, enabled);

                const auto observed = Natives::IsToggleModOn(expectedVehicle, TurboSlot);
                success = success && observed && *observed == enabled;
                RecordAction(
                    VehicleWorkshopAction::SetTurbo,
                    success ? VehicleWorkshopResult::Verified : VehicleWorkshopResult::Failed,
                    TurboSlot,
                    enabled ? 1 : 0,
                    observed ? (*observed ? 1 : 0) : -1);
                RefreshOnGameThread(expectedVehicle);
            });

            if (!queued)
            {
                RecordAction(
                    VehicleWorkshopAction::SetTurbo,
                    VehicleWorkshopResult::Failed,
                    TurboSlot,
                    enabled ? 1 : 0,
                    -1);
            }
            return queued;
        }

        [[nodiscard]] bool QueueMaxPerformance()
        {
            return QueuePerformancePackage(true);
        }

        [[nodiscard]] bool QueueStockPerformance()
        {
            return QueuePerformancePackage(false);
        }

    private:
        VehicleWorkshopRuntime() = default;
        ~VehicleWorkshopRuntime() = default;
        VehicleWorkshopRuntime(const VehicleWorkshopRuntime&) = delete;
        VehicleWorkshopRuntime& operator=(const VehicleWorkshopRuntime&) = delete;

        [[nodiscard]] static std::int64_t NowMilliseconds() noexcept
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                Clock::now().time_since_epoch()).count();
        }

        [[nodiscard]] static bool IsPerformanceSlot(int modType) noexcept
        {
            for (const int slot : PerformanceSlots)
            {
                if (slot == modType)
                    return true;
            }
            return false;
        }

        [[nodiscard]] static Vehicle CurrentVehicle() noexcept
        {
            const auto state = GameState::Get().Snapshot();
            if (!state.nativeRuntimeReady || !state.inVehicle || state.vehicle == 0)
                return 0;
            return state.vehicle;
        }

        [[nodiscard]] static Hash Joaat(std::string_view text) noexcept
        {
            std::uint32_t hash{};
            for (const unsigned char raw : text)
            {
                const unsigned char c = raw >= 'A' && raw <= 'Z'
                    ? static_cast<unsigned char>(raw + ('a' - 'A'))
                    : raw;
                hash += c;
                hash += hash << 10;
                hash ^= hash >> 6;
            }
            hash += hash << 3;
            hash ^= hash >> 11;
            hash += hash << 15;
            return hash;
        }

        [[nodiscard]] static std::string ResolveModelCode(Hash model)
        {
            for (const char* name : VehicleCatalogs::VehicleModels)
            {
                if (name && Joaat(name) == model)
                    return name;
            }
            return {};
        }

        [[nodiscard]] static std::string ResolveLocalizedVehicleName(Hash model)
        {
            std::string display;
            const auto rawName = VehicleNatives::GetDisplayNameFromVehicleModel(model);
            if (rawName && *rawName && **rawName && std::string_view(*rawName) != "CARNOTFOUND")
            {
                const auto localized = VehicleNatives::GetLabelText(*rawName);
                if (localized && *localized && **localized && std::string_view(*localized) != "NULL")
                    display = *localized;
                else
                    display = *rawName;
            }

            std::string make;
            const auto rawMake = VehicleNatives::GetMakeNameFromVehicleModel(model);
            if (rawMake && *rawMake && **rawMake && std::string_view(*rawMake) != "CARNOTFOUND")
            {
                const auto localized = VehicleNatives::GetLabelText(*rawMake);
                if (localized && *localized && **localized && std::string_view(*localized) != "NULL")
                    make = *localized;
                else
                    make = *rawMake;
            }

            if (!make.empty() && !display.empty() && display.rfind(make, 0) != 0)
                display = make + " " + display;
            if (display.empty())
                display = !make.empty() ? make : "Current Vehicle";
            return display;
        }

        void RefreshOnGameThread(Vehicle expectedVehicle) noexcept
        {
            if (expectedVehicle == 0 || CurrentVehicle() != expectedVehicle)
            {
                m_RefreshPending.store(false, std::memory_order_release);
                return;
            }

            VehicleWorkshopSnapshot next;
            bool fullScan = true;
            {
                std::scoped_lock lock(m_Mutex);
                if (m_Snapshot.valid && m_Snapshot.vehicle == expectedVehicle)
                {
                    next = m_Snapshot;
                    fullScan = !m_Snapshot.capabilitiesReady;
                }
            }

            const auto model = Natives::GetEntityModel(expectedVehicle);
            if (!model || !Natives::SetVehicleModKit(expectedVehicle, 0))
            {
                m_RefreshPending.store(false, std::memory_order_release);
                return;
            }

            next.vehicle = expectedVehicle;
            next.model = *model;
            next.valid = true;
            next.vehicleClass = VehicleNatives::GetVehicleClassFromName(*model).value_or(-1);
            next.displayName = ResolveLocalizedVehicleName(*model);
            next.modelCode = ResolveModelCode(*model);
            next.plate = VehicleNatives::GetVehicleNumberPlateText(expectedVehicle).value_or(std::string{});

            if (fullScan)
            {
                next.supportedModSlots = 0;
                next.availableModOptions = 0;
                for (int modType = 0; modType < 50; ++modType)
                {
                    const auto count = Natives::GetNumVehicleMods(expectedVehicle, modType);
                    next.modCounts[static_cast<std::size_t>(modType)] = count.value_or(-1);
                    if (count && *count > 0)
                    {
                        ++next.supportedModSlots;
                        next.availableModOptions += *count;
                        next.currentMods[static_cast<std::size_t>(modType)] =
                            Natives::GetVehicleMod(expectedVehicle, modType).value_or(-1);
                    }
                    else
                    {
                        next.currentMods[static_cast<std::size_t>(modType)] = -1;
                    }
                }
                next.capabilitiesReady = true;
            }

            for (const int modType : PerformanceSlots)
            {
                const auto count = Natives::GetNumVehicleMods(expectedVehicle, modType);
                if (count)
                    next.modCounts[static_cast<std::size_t>(modType)] = *count;
                next.currentMods[static_cast<std::size_t>(modType)] =
                    Natives::GetVehicleMod(expectedVehicle, modType).value_or(-1);
            }

            const auto turboCount = Natives::GetNumVehicleMods(expectedVehicle, TurboSlot);
            if (turboCount)
                next.modCounts[static_cast<std::size_t>(TurboSlot)] = *turboCount;
            next.turbo = Natives::IsToggleModOn(expectedVehicle, TurboSlot).value_or(false);

            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot = std::move(next);
            }

            m_LastRefreshMilliseconds.store(NowMilliseconds(), std::memory_order_release);
            m_RefreshPending.store(false, std::memory_order_release);
        }

        [[nodiscard]] bool QueuePerformancePackage(bool maxed)
        {
            const Vehicle expectedVehicle = CurrentVehicle();
            if (expectedVehicle == 0)
                return false;

            const auto action = maxed
                ? VehicleWorkshopAction::MaxPerformance
                : VehicleWorkshopAction::StockPerformance;
            RecordAction(action, VehicleWorkshopResult::Queued, -1, maxed ? 1 : 0, -1);

            const bool queued = Runtime::GameRuntime::Get().Enqueue([this, expectedVehicle, maxed, action] {
                if (CurrentVehicle() != expectedVehicle)
                {
                    RecordAction(action, VehicleWorkshopResult::Stale, -1, maxed ? 1 : 0, -1);
                    return;
                }

                bool success = Natives::SetVehicleModKit(expectedVehicle, 0);
                bool anySupported = false;
                for (const int modType : PerformanceSlots)
                {
                    const auto count = Natives::GetNumVehicleMods(expectedVehicle, modType);
                    if (!count)
                    {
                        success = false;
                        continue;
                    }
                    if (*count <= 0)
                        continue;

                    anySupported = true;
                    const int target = maxed ? *count - 1 : -1;
                    const bool dispatched = maxed
                        ? Natives::SetVehicleMod(expectedVehicle, modType, target, false)
                        : Natives::RemoveVehicleMod(expectedVehicle, modType);
                    const auto observed = Natives::GetVehicleMod(expectedVehicle, modType);
                    success = dispatched && observed && *observed == target && success;
                }

                const auto turboCount = Natives::GetNumVehicleMods(expectedVehicle, TurboSlot);
                if (!turboCount)
                {
                    success = false;
                }
                else if (*turboCount > 0)
                {
                    anySupported = true;
                    const bool dispatched = Natives::ToggleVehicleMod(expectedVehicle, TurboSlot, maxed);
                    const auto observed = Natives::IsToggleModOn(expectedVehicle, TurboSlot);
                    success = dispatched && observed && *observed == maxed && success;
                }

                success = success && anySupported;
                RecordAction(
                    action,
                    success ? VehicleWorkshopResult::Verified : VehicleWorkshopResult::Failed,
                    -1,
                    maxed ? 1 : 0,
                    -1);
                RefreshOnGameThread(expectedVehicle);
            });

            if (!queued)
                RecordAction(action, VehicleWorkshopResult::Failed, -1, maxed ? 1 : 0, -1);
            return queued;
        }

        void RecordAction(
            VehicleWorkshopAction action,
            VehicleWorkshopResult result,
            int slot,
            int requested,
            int observed) noexcept
        {
            std::scoped_lock lock(m_Mutex);
            m_Snapshot.lastAction = action;
            m_Snapshot.lastResult = result;
            m_Snapshot.lastSlot = slot;
            m_Snapshot.lastRequested = requested;
            m_Snapshot.lastObserved = observed;
        }

        void ClearIfNeeded() noexcept
        {
            std::scoped_lock lock(m_Mutex);
            if (!m_Snapshot.valid && m_Snapshot.vehicle == 0)
                return;

            const auto lastAction = m_Snapshot.lastAction;
            const auto lastResult = m_Snapshot.lastResult;
            m_Snapshot = VehicleWorkshopSnapshot{};
            m_Snapshot.lastAction = lastAction;
            m_Snapshot.lastResult = lastResult;
            m_LastQueuedVehicle.store(0, std::memory_order_release);
            m_LastRefreshMilliseconds.store(0, std::memory_order_release);
        }

        mutable std::mutex m_Mutex;
        VehicleWorkshopSnapshot m_Snapshot{};
        std::atomic<bool> m_RefreshPending{false};
        std::atomic<Vehicle> m_LastQueuedVehicle{0};
        std::atomic<std::int64_t> m_LastRefreshMilliseconds{0};
    };
}
