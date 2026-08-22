#include "VehicleModificationRuntime.hpp"

#include "../../game/GameState.hpp"
#include "../../game/PlayerNatives.hpp"
#include "../../game/VehicleNatives.hpp"
#include "../../game/native/NativeInvoker.hpp"
#include "../../game/vehicle/VehicleModels.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <system_error>
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
        constexpr float CloneRadius = 30.0f;
        constexpr std::size_t CatalogBatchSize = 24;

        [[nodiscard]] constexpr bool ValidModType(int modType) noexcept
        {
            return modType >= MinModType && modType <= MaxModType;
        }

        [[nodiscard]] constexpr bool ValidToggleModType(int modType) noexcept
        {
            return modType >= 17 && modType <= 22;
        }

        [[nodiscard]] constexpr int ClampByte(int value) noexcept
        {
            return value < 0 ? 0 : (value > 255 ? 255 : value);
        }

        [[nodiscard]] std::filesystem::path PresetDirectory()
        {
            const char* localAppData = std::getenv("LOCALAPPDATA");
            std::filesystem::path root = (localAppData && *localAppData) ? localAppData : ".";
            return root / "TutonesMenu" / "saved_vehicles";
        }

        [[nodiscard]] std::string FallbackModName(int modType, int modIndex)
        {
            std::string name;
            if (modType == 23 || modType == 24)
                name = "Wheel ";
            else
                name = "Option ";
            name += std::to_string(modIndex + 1);
            return name;
        }
    }

    VehicleModificationRuntime::VehicleModificationRuntime()
        : m_CatalogClasses(VehicleCatalogs::VehicleModels.size(), -1),
          m_CatalogDisplayNames(VehicleCatalogs::VehicleModels.size())
    {
        for (std::size_t i = 0; i < VehicleCatalogs::VehicleModels.size(); ++i)
            m_CatalogDisplayNames[i] = VehicleCatalogs::VehicleModels[i];
        m_Snapshot.currentMod = -1;
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

    VehicleModificationSnapshot VehicleModificationRuntime::Snapshot() const
    {
        std::scoped_lock lock(m_Mutex);
        return m_Snapshot;
    }

    VehicleCatalogSnapshot VehicleModificationRuntime::CatalogSnapshot() const
    {
        std::scoped_lock lock(m_Mutex);
        return {m_CatalogClasses, m_CatalogDisplayNames, m_CatalogCursor, m_CatalogClasses.size()};
    }

    std::vector<std::string> VehicleModificationRuntime::SavedPresetNames() const
    {
        std::vector<std::string> names;
        std::error_code error;
        const auto directory = PresetDirectory();
        if (!std::filesystem::exists(directory, error))
            return names;

        for (const auto& entry : std::filesystem::directory_iterator(directory, error))
        {
            if (error)
                break;
            if (!entry.is_regular_file(error) || entry.path().extension() != ".tutcar")
                continue;
            names.push_back(entry.path().stem().string());
        }
        std::sort(names.begin(), names.end());
        return names;
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
            const bool accepted = BeginPendingSpawn(model, spawnInside, maxed, VehicleModAction::SpawnVehicle);
            if (!accepted)
                RecordAction(VehicleModAction::SpawnVehicle, false, false);
        });
    }

    bool VehicleModificationRuntime::QueueCloneNearest(bool spawnInside)
    {
        if (!IsRunning())
            return false;

        return Runtime::GameRuntime::Get().Enqueue([this, spawnInside] {
            const auto ped = PlayerNatives::PlayerPedId();
            const auto coords = ped ? VehicleNatives::GetEntityCoords(*ped, false) : std::nullopt;
            if (!ped || *ped == 0 || !coords)
            {
                RecordAction(VehicleModAction::CloneNearest, false, false);
                return;
            }

            const auto nearest = VehicleNatives::GetClosestVehicle(
                coords->x, coords->y, coords->z, CloneRadius, 0, 70);
            if (!nearest || *nearest == 0)
            {
                RecordAction(VehicleModAction::CloneNearest, false, false);
                return;
            }

            VehiclePreset preset{};
            if (!CapturePreset(*nearest, preset)
                || !BeginPendingSpawn(preset.model, spawnInside, false, VehicleModAction::CloneNearest, preset))
            {
                RecordAction(VehicleModAction::CloneNearest, false, false);
            }
        });
    }

    bool VehicleModificationRuntime::QueueSaveCurrentPreset(std::string_view presetName)
    {
        const Vehicle expectedVehicle = CurrentVehicle();
        const std::string safeName = SanitizePresetName(presetName);
        if (expectedVehicle == 0 || safeName.empty())
            return false;

        return Runtime::GameRuntime::Get().Enqueue([this, expectedVehicle, safeName] {
            if (CurrentVehicle() != expectedVehicle)
            {
                RecordAction(VehicleModAction::SavePreset, false, true);
                return;
            }

            VehiclePreset preset{};
            const bool success = CapturePreset(expectedVehicle, preset) && SavePresetToDisk(safeName, preset);
            if (success)
            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot.lastSavedPreset = safeName;
            }
            RecordAction(VehicleModAction::SavePreset, success, false);
        });
    }

    bool VehicleModificationRuntime::QueueLoadPreset(std::string_view presetName, bool spawnInside)
    {
        const std::string safeName = SanitizePresetName(presetName);
        if (safeName.empty() || !IsRunning())
            return false;

        return Runtime::GameRuntime::Get().Enqueue([this, safeName, spawnInside] {
            VehiclePreset preset{};
            if (!LoadPresetFromDisk(safeName, preset)
                || !BeginPendingSpawn(preset.model, spawnInside, false, VehicleModAction::LoadPreset, preset))
            {
                RecordAction(VehicleModAction::LoadPreset, false, false);
                return;
            }

            std::scoped_lock lock(m_Mutex);
            m_Snapshot.lastSavedPreset = safeName;
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

    bool VehicleModificationRuntime::QueueTireSmokeColor(int red, int green, int blue)
    {
        red = ClampByte(red);
        green = ClampByte(green);
        blue = ClampByte(blue);
        return QueueVehicleOperation(VehicleModAction::SetTireSmokeColor, [red, green, blue](Vehicle vehicle) {
            return Natives::SetVehicleModKit(vehicle, 0)
                && Natives::ToggleVehicleMod(vehicle, TireSmokeModType, true)
                && VehicleNatives::SetVehicleTyreSmokeColor(vehicle, red, green, blue);
        });
    }

    bool VehicleModificationRuntime::QueueXenonColor(int colorIndex)
    {
        if (colorIndex < -1 || colorIndex > 12)
            return false;
        return QueueVehicleOperation(VehicleModAction::SetXenonColor, [colorIndex](Vehicle vehicle) {
            return Natives::SetVehicleModKit(vehicle, 0)
                && Natives::ToggleVehicleMod(vehicle, XenonModType, true)
                && VehicleNatives::SetVehicleXenonLightColor(vehicle, colorIndex);
        });
    }

    bool VehicleModificationRuntime::QueueNeonColor(int red, int green, int blue)
    {
        red = ClampByte(red);
        green = ClampByte(green);
        blue = ClampByte(blue);
        return QueueVehicleOperation(VehicleModAction::SetNeonColor, [red, green, blue](Vehicle vehicle) {
            return VehicleNatives::SetVehicleNeonColour(vehicle, red, green, blue);
        });
    }

    bool VehicleModificationRuntime::QueueNeonEnabled(int index, bool enabled)
    {
        if (index < 0 || index > 3)
            return false;
        return QueueVehicleOperation(VehicleModAction::SetNeonEnabled, [index, enabled](Vehicle vehicle) {
            return VehicleNatives::SetVehicleNeonEnabled(vehicle, index, enabled);
        });
    }

    bool VehicleModificationRuntime::QueueTyresCanBurst(bool canBurst)
    {
        return QueueVehicleOperation(VehicleModAction::SetTyresCanBurst, [canBurst](Vehicle vehicle) {
            return VehicleNatives::SetVehicleTyresCanBurst(vehicle, canBurst);
        });
    }

    bool VehicleModificationRuntime::QueueDriftTyres(bool enabled)
    {
        return QueueVehicleOperation(VehicleModAction::SetDriftTyres, [enabled](Vehicle vehicle) {
            return VehicleNatives::SetDriftTyres(vehicle, enabled);
        });
    }

    bool VehicleModificationRuntime::QueueStealthMode(bool enabled)
    {
        return QueueVehicleOperation(VehicleModAction::SetStealthMode, [enabled](Vehicle vehicle) {
            const auto model = Natives::GetEntityModel(vehicle);
            if (!model)
                return false;

            const bool deploy = !enabled;
            if (*model == Joaat("akula") || *model == Joaat("annihilator2"))
            {
                return Native::NativeInvoker::InvokeVoid(
                    Native::NativeId::SetDeployHeliStubWings,
                    vehicle,
                    static_cast<std::int32_t>(deploy),
                    std::int32_t{0});
            }
            if (*model == Joaat("raiju"))
            {
                return Native::NativeInvoker::InvokeVoid(
                    Native::NativeId::SetDeployMissileBays,
                    vehicle,
                    static_cast<std::int32_t>(deploy));
            }
            return false;
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

        ProcessCatalogBatch();
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

    void VehicleModificationRuntime::ProcessCatalogBatch() noexcept
    {
        if (m_CatalogCursor >= VehicleCatalogs::VehicleModels.size())
            return;

        std::size_t processed{};
        while (processed < CatalogBatchSize && m_CatalogCursor < VehicleCatalogs::VehicleModels.size())
        {
            const auto index = m_CatalogCursor++;
            const Hash model = Joaat(VehicleCatalogs::VehicleModels[index]);
            const auto vehicleClass = VehicleNatives::GetVehicleClassFromName(model);
            const int value = vehicleClass && *vehicleClass >= 0 && *vehicleClass <= 22 ? *vehicleClass : -2;

            std::string display = VehicleCatalogs::VehicleModels[index];
            const auto rawName = VehicleNatives::GetDisplayNameFromVehicleModel(model);
            if (rawName && *rawName && **rawName && std::string_view(*rawName) != "CARNOTFOUND")
            {
                const auto localized = VehicleNatives::GetLabelText(*rawName);
                if (localized && *localized && **localized && std::string_view(*localized) != "NULL")
                    display = *localized;
            }

            std::string make;
            const auto rawMake = VehicleNatives::GetMakeNameFromVehicleModel(model);
            if (rawMake && *rawMake && **rawMake && std::string_view(*rawMake) != "CARNOTFOUND")
            {
                const auto localizedMake = VehicleNatives::GetLabelText(*rawMake);
                if (localizedMake && *localizedMake && **localizedMake && std::string_view(*localizedMake) != "NULL")
                    make = *localizedMake;
            }
            if (!make.empty() && display.rfind(make, 0) != 0)
                display = make + " " + display;
            display += "  [";
            display += VehicleCatalogs::VehicleModels[index];
            display += "]";

            {
                std::scoped_lock lock(m_Mutex);
                m_CatalogClasses[index] = value;
                m_CatalogDisplayNames[index] = std::move(display);
            }
            ++processed;
        }
    }

    bool VehicleModificationRuntime::BeginPendingSpawn(
        Hash model,
        bool spawnInside,
        bool maxed,
        VehicleModAction action,
        std::optional<VehiclePreset> preset) noexcept
    {
        const auto inCdImage = PlayerNatives::IsModelInCdimage(model);
        const auto validModel = PlayerNatives::IsModelValid(model);
        const auto isVehicle = VehicleNatives::IsModelAVehicle(model);
        if (!inCdImage || !validModel || !isVehicle || !*inCdImage || !*validModel || !*isVehicle)
            return false;
        if (!PlayerNatives::RequestModel(model))
            return false;

        if (m_PendingSpawnModel != 0 && m_PendingSpawnModel != model)
            static_cast<void>(PlayerNatives::SetModelAsNoLongerNeeded(m_PendingSpawnModel));

        m_PendingSpawnModel = model;
        m_PendingSpawnInside = spawnInside;
        m_PendingSpawnMaxed = maxed;
        m_PendingSpawnAction = action;
        m_PendingSpawnPreset = std::move(preset);
        m_SpawnDeadline = Clock::now() + SpawnTimeout;
        SetSpawnPending(model, true);
        return true;
    }

    void VehicleModificationRuntime::ProcessPendingSpawn() noexcept
    {
        if (m_PendingSpawnModel == 0)
            return;

        const Hash model = m_PendingSpawnModel;
        const auto action = m_PendingSpawnAction;
        const auto loaded = PlayerNatives::HasModelLoaded(model);
        if (!loaded)
        {
            if (Clock::now() >= m_SpawnDeadline)
            {
                static_cast<void>(PlayerNatives::SetModelAsNoLongerNeeded(model));
                m_PendingSpawnModel = 0;
                m_PendingSpawnPreset.reset();
                SetSpawnPending(0, false);
                RecordAction(action, false, false);
            }
            return;
        }

        if (!*loaded)
        {
            if (Clock::now() >= m_SpawnDeadline)
            {
                static_cast<void>(PlayerNatives::SetModelAsNoLongerNeeded(model));
                m_PendingSpawnModel = 0;
                m_PendingSpawnPreset.reset();
                SetSpawnPending(0, false);
                RecordAction(action, false, false);
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
            m_PendingSpawnPreset.reset();
            SetSpawnPending(0, false);
            RecordAction(action, false, false);
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
            if (m_PendingSpawnPreset)
                success = ApplyPreset(*spawned, *m_PendingSpawnPreset) && success;
            else if (m_PendingSpawnMaxed)
                success = MaxVehicle(*spawned) && success;
            if (m_PendingSpawnInside)
                success = VehicleNatives::SetPedIntoVehicle(*ped, *spawned, -1) && success;

            std::scoped_lock lock(m_Mutex);
            m_Snapshot.lastSpawnedVehicle = *spawned;
        }

        static_cast<void>(PlayerNatives::SetModelAsNoLongerNeeded(model));
        m_PendingSpawnModel = 0;
        m_PendingSpawnPreset.reset();
        SetSpawnPending(0, false);
        RecordAction(action, success, false);
    }

    bool VehicleModificationRuntime::Refresh(Vehicle vehicle) noexcept
    {
        if (vehicle == 0)
        {
            ClearSnapshot();
            return false;
        }

        const int modType = m_ObservedModType.load(std::memory_order_acquire);
        static_cast<void>(Natives::SetVehicleModKit(vehicle, 0));
        const auto count = Natives::GetNumVehicleMods(vehicle, modType);
        const auto current = Natives::GetVehicleMod(vehicle, modType);
        const auto wheelType = Natives::GetVehicleWheelType(vehicle);
        if (!count || !current || !wheelType)
        {
            ClearSnapshot();
            return false;
        }

        const auto turbo = Natives::IsToggleModOn(vehicle, TurboModType);
        const auto tireSmoke = Natives::IsToggleModOn(vehicle, TireSmokeModType);
        const auto xenon = Natives::IsToggleModOn(vehicle, XenonModType);

        bool customTires = false;
        if (modType == 23 || modType == 24)
            customTires = Natives::GetVehicleModVariation(vehicle, modType).value_or(false);

        std::vector<std::string> displayNames;
        displayNames.reserve(static_cast<std::size_t>(std::max(0, *count)));
        for (int mod = 0; mod < *count; ++mod)
        {
            std::string display = FallbackModName(modType, mod);
            const auto rawLabel = VehicleNatives::GetModTextLabel(vehicle, modType, mod);
            if (rawLabel && *rawLabel && **rawLabel)
            {
                const auto localized = VehicleNatives::GetLabelText(*rawLabel);
                if (localized && *localized && **localized && std::string_view(*localized) != "NULL")
                    display = *localized;
                else
                    display = *rawLabel;
            }
            if ((modType == 23 || modType == 24) && *count > 1 && mod >= (*count / 2))
            {
                if (display.rfind("Chrome ", 0) != 0)
                    display = "Chrome " + display;
            }
            displayNames.push_back(std::move(display));
        }

        int smokeR = 255;
        int smokeG = 255;
        int smokeB = 255;
        static_cast<void>(VehicleNatives::GetVehicleTyreSmokeColor(vehicle, smokeR, smokeG, smokeB));
        const int xenonColor = VehicleNatives::GetVehicleXenonLightColor(vehicle).value_or(-1);
        std::array<bool, 4> neonEnabled{};
        for (int i = 0; i < 4; ++i)
            neonEnabled[static_cast<std::size_t>(i)] = VehicleNatives::GetVehicleNeonEnabled(vehicle, i).value_or(false);
        int neonR = 222;
        int neonG = 222;
        int neonB = 255;
        static_cast<void>(VehicleNatives::GetVehicleNeonColour(vehicle, neonR, neonG, neonB));
        const bool tyresCanBurst = VehicleNatives::GetVehicleTyresCanBurst(vehicle).value_or(true);
        const bool driftTyres = VehicleNatives::GetDriftTyresSet(vehicle).value_or(false);

        std::scoped_lock lock(m_Mutex);
        m_Snapshot.vehicle = vehicle;
        m_Snapshot.observedModType = modType;
        m_Snapshot.modCount = std::max(0, *count);
        m_Snapshot.currentMod = *current;
        m_Snapshot.customTires = customTires;
        m_Snapshot.wheelType = std::clamp(*wheelType, 0, 12);
        m_Snapshot.turbo = turbo.value_or(false);
        m_Snapshot.tireSmoke = tireSmoke.value_or(false);
        m_Snapshot.xenon = xenon.value_or(false);
        m_Snapshot.modDisplayNames = std::move(displayNames);
        m_Snapshot.tireSmokeRed = ClampByte(smokeR);
        m_Snapshot.tireSmokeGreen = ClampByte(smokeG);
        m_Snapshot.tireSmokeBlue = ClampByte(smokeB);
        m_Snapshot.xenonColor = std::clamp(xenonColor, -1, 12);
        m_Snapshot.neonEnabled = neonEnabled;
        m_Snapshot.neonRed = ClampByte(neonR);
        m_Snapshot.neonGreen = ClampByte(neonG);
        m_Snapshot.neonBlue = ClampByte(neonB);
        m_Snapshot.tyresCanBurst = tyresCanBurst;
        m_Snapshot.driftTyres = driftTyres;
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

    bool VehicleModificationRuntime::CapturePreset(Vehicle vehicle, VehiclePreset& out) noexcept
    {
        if (vehicle == 0)
            return false;

        out = {};
        out.mods.fill(-1);
        const auto model = Natives::GetEntityModel(vehicle);
        const auto wheelType = Natives::GetVehicleWheelType(vehicle);
        if (!model || !wheelType)
            return false;
        out.model = *model;
        out.wheelType = *wheelType;

        if (!Natives::GetVehicleColours(vehicle, out.primary, out.secondary))
            return false;
        if (!Natives::GetVehicleExtraColours(vehicle, out.pearlescent, out.wheelColor))
            return false;

        static_cast<void>(Natives::GetVehicleModColor1(
            vehicle, out.primaryPaintType, out.primaryModColor, out.primaryModPearlescent));
        static_cast<void>(Natives::GetVehicleModColor2(vehicle, out.secondaryPaintType, out.secondaryModColor));

        out.primaryCustom = Natives::GetIsVehiclePrimaryColourCustom(vehicle).value_or(false);
        out.secondaryCustom = Natives::GetIsVehicleSecondaryColourCustom(vehicle).value_or(false);
        if (out.primaryCustom)
            static_cast<void>(Natives::GetVehicleCustomPrimaryColour(
                vehicle, out.customPrimary[0], out.customPrimary[1], out.customPrimary[2]));
        if (out.secondaryCustom)
            static_cast<void>(Natives::GetVehicleCustomSecondaryColour(
                vehicle, out.customSecondary[0], out.customSecondary[1], out.customSecondary[2]));

        static_cast<void>(Natives::SetVehicleModKit(vehicle, 0));
        for (int modType = MinModType; modType <= MaxModType; ++modType)
        {
            const auto count = Natives::GetNumVehicleMods(vehicle, modType);
            if (count && *count > 0)
                out.mods[static_cast<std::size_t>(modType)] = Natives::GetVehicleMod(vehicle, modType).value_or(-1);
            if (modType == 23 || modType == 24)
                out.variations[static_cast<std::size_t>(modType)] = Natives::GetVehicleModVariation(vehicle, modType).value_or(false);
            if (ValidToggleModType(modType))
                out.toggles[static_cast<std::size_t>(modType)] = Natives::IsToggleModOn(vehicle, modType).value_or(false);
        }

        static_cast<void>(VehicleNatives::GetVehicleTyreSmokeColor(
            vehicle, out.tireSmoke[0], out.tireSmoke[1], out.tireSmoke[2]));
        out.xenonColor = VehicleNatives::GetVehicleXenonLightColor(vehicle).value_or(-1);
        for (int i = 0; i < 4; ++i)
            out.neonEnabled[static_cast<std::size_t>(i)] = VehicleNatives::GetVehicleNeonEnabled(vehicle, i).value_or(false);
        static_cast<void>(VehicleNatives::GetVehicleNeonColour(
            vehicle, out.neonColor[0], out.neonColor[1], out.neonColor[2]));
        out.tyresCanBurst = VehicleNatives::GetVehicleTyresCanBurst(vehicle).value_or(true);
        out.driftTyres = VehicleNatives::GetDriftTyresSet(vehicle).value_or(false);
        return true;
    }

    bool VehicleModificationRuntime::ApplyPreset(Vehicle vehicle, const VehiclePreset& preset) noexcept
    {
        if (vehicle == 0 || !Natives::SetVehicleModKit(vehicle, 0))
            return false;

        bool success = true;
        success = Natives::SetVehicleColours(vehicle, preset.primary, preset.secondary) && success;
        success = Natives::SetVehicleExtraColours(vehicle, preset.pearlescent, preset.wheelColor) && success;
        if (preset.primaryPaintType >= 0 && preset.primaryPaintType <= 5)
            success = Natives::SetVehicleModColor1(
                vehicle, preset.primaryPaintType, preset.primaryModColor, preset.primaryModPearlescent) && success;
        if (preset.secondaryPaintType >= 0 && preset.secondaryPaintType <= 5)
            success = Natives::SetVehicleModColor2(vehicle, preset.secondaryPaintType, preset.secondaryModColor) && success;

        if (preset.primaryCustom)
            success = Natives::SetVehicleCustomPrimaryColour(
                vehicle, preset.customPrimary[0], preset.customPrimary[1], preset.customPrimary[2]) && success;
        else
            static_cast<void>(Natives::ClearVehicleCustomPrimaryColour(vehicle));
        if (preset.secondaryCustom)
            success = Natives::SetVehicleCustomSecondaryColour(
                vehicle, preset.customSecondary[0], preset.customSecondary[1], preset.customSecondary[2]) && success;
        else
            static_cast<void>(Natives::ClearVehicleCustomSecondaryColour(vehicle));

        success = Natives::SetVehicleWheelType(vehicle, std::clamp(preset.wheelType, 0, 12)) && success;
        for (int modType = MinModType; modType <= MaxModType; ++modType)
        {
            if (ValidToggleModType(modType))
            {
                success = Natives::ToggleVehicleMod(
                    vehicle, modType, preset.toggles[static_cast<std::size_t>(modType)]) && success;
                continue;
            }

            const auto count = Natives::GetNumVehicleMods(vehicle, modType);
            if (!count || *count <= 0)
                continue;

            const int mod = preset.mods[static_cast<std::size_t>(modType)];
            if (mod >= 0 && mod < *count)
            {
                success = Natives::SetVehicleMod(
                    vehicle, modType, mod, preset.variations[static_cast<std::size_t>(modType)]) && success;
            }
            else
            {
                static_cast<void>(Natives::RemoveVehicleMod(vehicle, modType));
            }
        }

        success = VehicleNatives::SetVehicleTyreSmokeColor(
            vehicle, preset.tireSmoke[0], preset.tireSmoke[1], preset.tireSmoke[2]) && success;
        success = VehicleNatives::SetVehicleXenonLightColor(vehicle, std::clamp(preset.xenonColor, -1, 12)) && success;
        success = VehicleNatives::SetVehicleNeonColour(
            vehicle, preset.neonColor[0], preset.neonColor[1], preset.neonColor[2]) && success;
        for (int i = 0; i < 4; ++i)
            success = VehicleNatives::SetVehicleNeonEnabled(
                vehicle, i, preset.neonEnabled[static_cast<std::size_t>(i)]) && success;
        success = VehicleNatives::SetVehicleTyresCanBurst(vehicle, preset.tyresCanBurst) && success;
        success = VehicleNatives::SetDriftTyres(vehicle, preset.driftTyres) && success;
        return success;
    }

    bool VehicleModificationRuntime::SavePresetToDisk(std::string_view name, const VehiclePreset& preset) noexcept
    {
        try
        {
            const auto directory = PresetDirectory();
            std::filesystem::create_directories(directory);
            std::ofstream out(directory / (std::string(name) + ".tutcar"), std::ios::trunc);
            if (!out)
                return false;

            out << "TUTONES_VEHICLE_V2\n";
            out << preset.model << ' ' << preset.primary << ' ' << preset.secondary << ' '
                << preset.pearlescent << ' ' << preset.wheelColor << ' '
                << preset.primaryPaintType << ' ' << preset.primaryModColor << ' ' << preset.primaryModPearlescent << ' '
                << preset.secondaryPaintType << ' ' << preset.secondaryModColor << ' '
                << preset.primaryCustom << ' ' << preset.secondaryCustom << ' '
                << preset.customPrimary[0] << ' ' << preset.customPrimary[1] << ' ' << preset.customPrimary[2] << ' '
                << preset.customSecondary[0] << ' ' << preset.customSecondary[1] << ' ' << preset.customSecondary[2] << ' '
                << preset.wheelType << ' ' << preset.tireSmoke[0] << ' ' << preset.tireSmoke[1] << ' ' << preset.tireSmoke[2] << ' '
                << preset.xenonColor << ' ' << preset.neonColor[0] << ' ' << preset.neonColor[1] << ' ' << preset.neonColor[2] << ' '
                << preset.tyresCanBurst << ' ' << preset.driftTyres << '\n';
            for (const int value : preset.mods) out << value << ' ';
            out << '\n';
            for (const bool value : preset.variations) out << static_cast<int>(value) << ' ';
            out << '\n';
            for (const bool value : preset.toggles) out << static_cast<int>(value) << ' ';
            out << '\n';
            for (const bool value : preset.neonEnabled) out << static_cast<int>(value) << ' ';
            out << '\n';
            return out.good();
        }
        catch (...)
        {
            return false;
        }
    }

    bool VehicleModificationRuntime::LoadPresetFromDisk(std::string_view name, VehiclePreset& preset) const noexcept
    {
        try
        {
            std::ifstream in(PresetDirectory() / (std::string(name) + ".tutcar"));
            if (!in)
                return false;

            std::string version;
            std::getline(in, version);
            if (version != "TUTONES_VEHICLE_V2")
                return false;

            preset = {};
            preset.mods.fill(-1);
            int primaryCustom{};
            int secondaryCustom{};
            int tyresCanBurst{};
            int driftTyres{};
            if (!(in >> preset.model >> preset.primary >> preset.secondary
                >> preset.pearlescent >> preset.wheelColor
                >> preset.primaryPaintType >> preset.primaryModColor >> preset.primaryModPearlescent
                >> preset.secondaryPaintType >> preset.secondaryModColor
                >> primaryCustom >> secondaryCustom
                >> preset.customPrimary[0] >> preset.customPrimary[1] >> preset.customPrimary[2]
                >> preset.customSecondary[0] >> preset.customSecondary[1] >> preset.customSecondary[2]
                >> preset.wheelType >> preset.tireSmoke[0] >> preset.tireSmoke[1] >> preset.tireSmoke[2]
                >> preset.xenonColor >> preset.neonColor[0] >> preset.neonColor[1] >> preset.neonColor[2]
                >> tyresCanBurst >> driftTyres))
                return false;
            preset.primaryCustom = primaryCustom != 0;
            preset.secondaryCustom = secondaryCustom != 0;
            preset.tyresCanBurst = tyresCanBurst != 0;
            preset.driftTyres = driftTyres != 0;

            for (int& value : preset.mods)
                if (!(in >> value)) return false;
            for (bool& value : preset.variations)
            {
                int raw{};
                if (!(in >> raw)) return false;
                value = raw != 0;
            }
            for (bool& value : preset.toggles)
            {
                int raw{};
                if (!(in >> raw)) return false;
                value = raw != 0;
            }
            for (bool& value : preset.neonEnabled)
            {
                int raw{};
                if (!(in >> raw)) return false;
                value = raw != 0;
            }
            return preset.model != 0;
        }
        catch (...)
        {
            return false;
        }
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
        const std::string lastSavedPreset = m_Snapshot.lastSavedPreset;
        m_Snapshot = {};
        m_Snapshot.currentMod = -1;
        m_Snapshot.lastAction = lastAction;
        m_Snapshot.lastActionSucceeded = lastSuccess;
        m_Snapshot.lastActionRejectedAsStale = lastStale;
        m_Snapshot.spawnPending = spawnPending;
        m_Snapshot.pendingSpawnModel = pendingSpawnModel;
        m_Snapshot.lastSpawnedVehicle = lastSpawnedVehicle;
        m_Snapshot.lastSavedPreset = lastSavedPreset;
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

    std::string VehicleModificationRuntime::SanitizePresetName(std::string_view text)
    {
        std::string result;
        result.reserve(std::min<std::size_t>(text.size(), 48));
        for (char c : text)
        {
            if (result.size() >= 48)
                break;
            const bool alphaNumeric = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
            if (alphaNumeric || c == '-' || c == '_')
                result.push_back(c);
            else if (c == ' ')
                result.push_back('_');
        }
        return result;
    }
}
