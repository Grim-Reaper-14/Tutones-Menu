#include "VehicleModificationRuntime.hpp"

#include "../../game/GameState.hpp"
#include "../../game/PlayerNatives.hpp"
#include "../../game/VehicleNatives.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

namespace Tutones::Game::Mods
{
    namespace
    {
        constexpr int MinModType = 0;
        constexpr int MaxModType = 49;
        constexpr int TurboModType = 18;
        constexpr int TireSmokeModType = 20;
        constexpr int XenonModType = 22;
        constexpr float Pi = 3.14159265358979323846f;
        constexpr float SpawnDistance = 5.0f;

        [[nodiscard]] constexpr bool ValidModType(int modType) noexcept
        {
            return modType >= MinModType && modType <= MaxModType;
        }

        [[nodiscard]] constexpr bool ValidToggleModType(int modType) noexcept
        {
            return modType == 17 || modType == 18 || modType == 19
                || modType == 20 || modType == 21 || modType == 22;
        }
    }

    VehicleModificationRuntime& VehicleModificationRuntime::Get() noexcept
    {
        static VehicleModificationRuntime instance;
        return instance;
    }

    bool VehicleModificationRuntime::Start()
    {
        bool expected = false;
        if (!m_Running.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            return true;

        if (QueueNextTick())
            return true;

        m_Running.store(false, std::memory_order_release);
        return false;
    }

    void VehicleModificationRuntime::Stop() noexcept
    {
        m_Running.store(false, std::memory_order_release);
    }

    bool VehicleModificationRuntime::IsRunning() const noexcept
    {
        return m_Running.load(std::memory_order_acquire);
    }

    void VehicleModificationRuntime::SetObservedModType(int modType) noexcept
    {
        m_ObservedModType.store(std::clamp(modType, MinModType, MaxModType), std::memory_order_release);
    }

    VehicleModificationSnapshot VehicleModificationRuntime::Snapshot() const noexcept
    {
        std::scoped_lock lock(m_Mutex);
        return m_Snapshot;
    }

    bool VehicleModificationRuntime::QueueRepair()
    {
        return QueueVehicleOperation(VehicleModAction::Repair, [](Vehicle vehicle) {
            return Natives::SetVehicleFixed(vehicle);
        });
    }

    bool VehicleModificationRuntime::QueueClean()
    {
        return QueueVehicleOperation(VehicleModAction::Clean, [](Vehicle vehicle) {
            return Natives::SetVehicleDirtLevel(vehicle, 0.0f);
        });
    }

    bool VehicleModificationRuntime::QueueFlipUpright()
    {
        return QueueVehicleOperation(VehicleModAction::FlipUpright, [](Vehicle vehicle) {
            const auto result = Natives::SetVehicleOnGroundProperly(vehicle, 5.0f);
            return result.value_or(false);
        });
    }

    bool VehicleModificationRuntime::QueueMaxVehicle()
    {
        return QueueVehicleOperation(VehicleModAction::MaxVehicle, [this](Vehicle vehicle) {
            return MaxVehicle(vehicle);
        });
    }

    bool VehicleModificationRuntime::QueueSpawnVehicle(std::string_view modelName, bool spawnInside, bool maxed)
    {
        const Hash model = Joaat(modelName);
        if (modelName.empty() || model == 0 || !IsRunning())
            return false;

        return Runtime::GameRuntime::Get().Enqueue([this, model, spawnInside, maxed] {
            const auto inCdImage = PlayerNatives::IsModelInCdimage(model);
            const auto validModel = PlayerNatives::IsModelValid(model);
            const auto isVehicle = VehicleNatives::IsModelAVehicle(model);
            if (!inCdImage || !validModel || !isVehicle || !*inCdImage || !*validModel || !*isVehicle)
            {
                RecordAction(VehicleModAction::SpawnVehicle, false, false);
                return;
            }

            if (!PlayerNatives::RequestModel(model))
            {
                RecordAction(VehicleModAction::SpawnVehicle, false, false);
                return;
            }

            if (m_PendingSpawnModel != 0 && m_PendingSpawnModel != model)
                static_cast<void>(PlayerNatives::SetModelAsNoLongerNeeded(m_PendingSpawnModel));

            m_PendingSpawnModel = model;
            m_PendingSpawnInside = spawnInside;
            m_PendingSpawnMaxed = maxed;
            m_SpawnDeadline = Clock::now() + SpawnTimeout;
            SetSpawnPending(model, true);
        });
    }

    bool VehicleModificationRuntime::QueueSetMod(int modType, int modIndex, bool customTires)
    {
        if (!ValidModType(modType) || modIndex < 0)
            return false;

        return QueueVehicleOperation(VehicleModAction::SetMod, [modType, modIndex, customTires](Vehicle vehicle) {
            const auto count = Natives::GetNumVehicleMods(vehicle, modType);
            if (!count || modIndex >= *count)
                return false;
            if (!Natives::SetVehicleModKit(vehicle, 0))
                return false;
            return Natives::SetVehicleMod(vehicle, modType, modIndex, customTires);
        });
    }

    bool VehicleModificationRuntime::QueueRemoveMod(int modType)
    {
        if (!ValidModType(modType))
            return false;

        return QueueVehicleOperation(VehicleModAction::RemoveMod, [modType](Vehicle vehicle) {
            if (!Natives::SetVehicleModKit(vehicle, 0))
                return false;
            return Natives::RemoveVehicleMod(vehicle, modType);
        });
    }

    bool VehicleModificationRuntime::QueueToggleMod(int modType, bool enabled)
    {
        if (!ValidToggleModType(modType))
            return false;

        return QueueVehicleOperation(VehicleModAction::ToggleMod, [modType, enabled](Vehicle vehicle) {
            if (!Natives::SetVehicleModKit(vehicle, 0))
                return false;
            return Natives::ToggleVehicleMod(vehicle, modType, enabled);
        });
    }

    bool VehicleModificationRuntime::QueueWheelType(int wheelType)
    {
        if (wheelType < 0 || wheelType > 12)
            return false;

        return QueueVehicleOperation(VehicleModAction::SetWheelType, [wheelType](Vehicle vehicle) {
            if (!Natives::SetVehicleModKit(vehicle, 0))
                return false;
            return Natives::SetVehicleWheelType(vehicle, wheelType);
        });
    }

    Vehicle VehicleModificationRuntime::CurrentVehicle() const noexcept
    {
        const auto snapshot = GameState::Get().Snapshot();
        if (!snapshot.nativeRuntimeReady || !snapshot.inVehicle || snapshot.vehicle == 0)
            return 0;
        return snapshot.vehicle;
    }

    bool VehicleModificationRuntime::QueueNextTick()
    {
        if (!IsRunning())
            return false;

        return Runtime::GameRuntime::Get().Enqueue([this] {
            TickOnGameThread();
        });
    }

    void VehicleModificationRuntime::TickOnGameThread() noexcept
    {
        if (!IsRunning())
            return;

        ProcessPendingSpawn();

        const Vehicle vehicle = CurrentVehicle();
        const int observed = m_ObservedModType.load(std::memory_order_acquire);
        const auto now = Clock::now();

        if (vehicle == 0)
        {
            m_LastVehicle = 0;
            m_LastObservedModType = -1;
            m_NextRefresh = {};
            ClearSnapshot();
        }
        else if (vehicle != m_LastVehicle
            || observed != m_LastObservedModType
            || m_NextRefresh == Clock::time_point{}
            || now >= m_NextRefresh)
        {
            m_LastVehicle = vehicle;
            m_LastObservedModType = observed;
            static_cast<void>(Refresh(vehicle));
            m_NextRefresh = now + RefreshInterval;
        }

        if (IsRunning() && !QueueNextTick())
            m_Running.store(false, std::memory_order_release);
    }

    void VehicleModificationRuntime::ProcessPendingSpawn() noexcept
    {
        if (m_PendingSpawnModel == 0)
            return;

        const Hash model = m_PendingSpawnModel;
        const auto loaded = PlayerNatives::HasModelLoaded(model);
        if (!loaded)
        {
            if (Clock::now() >= m_SpawnDeadline)
            {
                static_cast<void>(PlayerNatives::SetModelAsNoLongerNeeded(model));
                m_PendingSpawnModel = 0;
                SetSpawnPending(0, false);
                RecordAction(VehicleModAction::SpawnVehicle, false, false);
            }
            return;
        }

        if (!*loaded)
        {
            if (Clock::now() >= m_SpawnDeadline)
            {
                static_cast<void>(PlayerNatives::SetModelAsNoLongerNeeded(model));
                m_PendingSpawnModel = 0;
                SetSpawnPending(0, false);
                RecordAction(VehicleModAction::SpawnVehicle, false, false);
            }
            else
            {
                static_cast<void>(PlayerNatives::RequestModel(model));
            }
            return;
        }

        const auto ped = PlayerNatives::PlayerPedId();
        const auto coords = ped ? VehicleNatives::GetEntityCoords(*ped, false) : std::nullopt;
        const auto heading = ped ? VehicleNatives::GetEntityHeading(*ped) : std::nullopt;
        if (!ped || *ped == 0 || !coords || !heading)
        {
            static_cast<void>(PlayerNatives::SetModelAsNoLongerNeeded(model));
            m_PendingSpawnModel = 0;
            SetSpawnPending(0, false);
            RecordAction(VehicleModAction::SpawnVehicle, false, false);
            return;
        }

        const float radians = *heading * (Pi / 180.0f);
        const float spawnX = coords->x - std::sin(radians) * SpawnDistance;
        const float spawnY = coords->y + std::cos(radians) * SpawnDistance;
        const float spawnZ = coords->z + 0.5f;

        const auto spawned = VehicleNatives::CreateVehicle(model, spawnX, spawnY, spawnZ, *heading, true, false, false);
        bool success = spawned && *spawned != 0;
        if (success)
        {
            static_cast<void>(Natives::SetVehicleOnGroundProperly(*spawned, 5.0f));
            if (m_PendingSpawnMaxed)
                success = MaxVehicle(*spawned) && success;
            if (m_PendingSpawnInside)
                success = VehicleNatives::SetPedIntoVehicle(*ped, *spawned, -1) && success;

            std::scoped_lock lock(m_Mutex);
            m_Snapshot.lastSpawnedVehicle = *spawned;
        }

        static_cast<void>(PlayerNatives::SetModelAsNoLongerNeeded(model));
        m_PendingSpawnModel = 0;
        SetSpawnPending(0, false);
        RecordAction(VehicleModAction::SpawnVehicle, success, false);
    }

    bool VehicleModificationRuntime::Refresh(Vehicle vehicle) noexcept
    {
        if (vehicle == 0)
        {
            ClearSnapshot();
            return false;
        }

        const int modType = m_ObservedModType.load(std::memory_order_acquire);
        const auto count = Natives::GetNumVehicleMods(vehicle, modType);
        const auto current = Natives::GetVehicleMod(vehicle, modType);
        const auto wheelType = Natives::GetVehicleWheelType(vehicle);
        const auto turbo = Natives::IsToggleModOn(vehicle, TurboModType);
        const auto tireSmoke = Natives::IsToggleModOn(vehicle, TireSmokeModType);
        const auto xenon = Natives::IsToggleModOn(vehicle, XenonModType);

        if (!count || !current || !wheelType || !turbo || !tireSmoke || !xenon)
        {
            ClearSnapshot();
            return false;
        }

        bool customTires = false;
        if (modType == 23 || modType == 24)
        {
            const auto variation = Natives::GetVehicleModVariation(vehicle, modType);
            if (!variation)
            {
                ClearSnapshot();
                return false;
            }
            customTires = *variation;
        }

        std::scoped_lock lock(m_Mutex);
        m_Snapshot.vehicle = vehicle;
        m_Snapshot.observedModType = modType;
        m_Snapshot.modCount = std::max(0, *count);
        m_Snapshot.currentMod = *current;
        m_Snapshot.customTires = customTires;
        m_Snapshot.wheelType = std::clamp(*wheelType, 0, 12);
        m_Snapshot.turbo = *turbo;
        m_Snapshot.tireSmoke = *tireSmoke;
        m_Snapshot.xenon = *xenon;
        m_Snapshot.valid = true;
        return true;
    }

    bool VehicleModificationRuntime::MaxVehicle(Vehicle vehicle) noexcept
    {
        if (vehicle == 0 || !Natives::SetVehicleModKit(vehicle, 0))
            return false;

        bool success = true;
        for (int modType = MinModType; modType <= MaxModType; ++modType)
        {
            if (ValidToggleModType(modType))
            {
                success = Natives::ToggleVehicleMod(vehicle, modType, true) && success;
                continue;
            }

            const auto count = Natives::GetNumVehicleMods(vehicle, modType);
            if (!count)
            {
                success = false;
                continue;
            }
            if (*count > 0)
                success = Natives::SetVehicleMod(vehicle, modType, *count - 1, false) && success;
        }
        return success;
    }

    bool VehicleModificationRuntime::QueueVehicleOperation(
        VehicleModAction action,
        std::function<bool(Vehicle)> apply)
    {
        const Vehicle expectedVehicle = CurrentVehicle();
        if (expectedVehicle == 0 || !apply)
            return false;

        return Runtime::GameRuntime::Get().Enqueue([
            this,
            expectedVehicle,
            action,
            apply = std::move(apply)]() mutable {
            if (CurrentVehicle() != expectedVehicle)
            {
                RecordAction(action, false, true);
                return;
            }

            const bool success = apply(expectedVehicle);
            if (success)
                static_cast<void>(Refresh(expectedVehicle));
            RecordAction(action, success, false);
        });
    }

    void VehicleModificationRuntime::RecordAction(VehicleModAction action, bool success, bool stale) noexcept
    {
        std::scoped_lock lock(m_Mutex);
        m_Snapshot.lastAction = action;
        m_Snapshot.lastActionSucceeded = success;
        m_Snapshot.lastActionRejectedAsStale = stale;
    }

    void VehicleModificationRuntime::SetSpawnPending(Hash model, bool pending) noexcept
    {
        std::scoped_lock lock(m_Mutex);
        m_Snapshot.spawnPending = pending;
        m_Snapshot.pendingSpawnModel = pending ? model : 0;
    }

    void VehicleModificationRuntime::ClearSnapshot() noexcept
    {
        std::scoped_lock lock(m_Mutex);
        const auto lastAction = m_Snapshot.lastAction;
        const bool lastSuccess = m_Snapshot.lastActionSucceeded;
        const bool lastStale = m_Snapshot.lastActionRejectedAsStale;
        const bool spawnPending = m_Snapshot.spawnPending;
        const Hash pendingSpawnModel = m_Snapshot.pendingSpawnModel;
        const Vehicle lastSpawnedVehicle = m_Snapshot.lastSpawnedVehicle;
        m_Snapshot = {};
        m_Snapshot.currentMod = -1;
        m_Snapshot.lastAction = lastAction;
        m_Snapshot.lastActionSucceeded = lastSuccess;
        m_Snapshot.lastActionRejectedAsStale = lastStale;
        m_Snapshot.spawnPending = spawnPending;
        m_Snapshot.pendingSpawnModel = pendingSpawnModel;
        m_Snapshot.lastSpawnedVehicle = lastSpawnedVehicle;
    }

    Hash VehicleModificationRuntime::Joaat(std::string_view text) noexcept
    {
        std::uint32_t hash{};
        for (const char raw : text)
        {
            char c = raw;
            if (c >= 'A' && c <= 'Z')
                c = static_cast<char>(c - 'A' + 'a');
            hash += static_cast<std::uint8_t>(c);
            hash += hash << 10;
            hash ^= hash >> 6;
        }
        hash += hash << 3;
        hash ^= hash >> 11;
        hash += hash << 15;
        return hash;
    }
}
