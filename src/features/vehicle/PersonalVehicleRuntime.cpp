#include "PersonalVehicleRuntime.hpp"

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/Stats.hpp"
#include "../../game/VehicleNatives.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../game/script/ScriptLocal.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <set>
#include <string>
#include <utility>

namespace Tutones::Game::PersonalVehicles
{
    namespace
    {
        constexpr std::size_t MpsvGlobal = 1583778;
        constexpr std::size_t MpsvEntrySize = 143;
        constexpr std::size_t MpsvMaxEntries = 607;
        constexpr std::size_t MpsvPlateOffset = 1;
        constexpr std::size_t MpsvVehicleModelOffset = 66;
        constexpr std::size_t MpsvFlagsOffset = 104;

        constexpr std::uint32_t TriggerSpawnToggleBit = 1u << 0;
        constexpr std::uint32_t DestroyedBit = 1u << 1;
        constexpr std::uint32_t HasInsuranceBit = 1u << 2;
        constexpr std::uint32_t ImpoundedBit = 1u << 6;
        constexpr std::uint32_t UnknownRepairBit = 1u << 16;

        constexpr std::size_t GarageSlotsGlobal = 1945138;

        constexpr std::size_t FreemodeGeneralGlobal = 2733326;
        constexpr std::size_t PersonalVehicleIndexOffset = 301;
        constexpr std::size_t PersonalVehicleRequestedOffset = 575;
        constexpr std::size_t NodeDistanceCheckOffset = 590;
        constexpr std::size_t RequestedPersonalVehicleIdOffset = 639;
        constexpr std::size_t ExecImpoundOffset = 642;

        constexpr std::size_t SavedMpGlobals = 2359296;
        constexpr std::size_t SavedMpEntrySize = 5574;
        constexpr std::size_t SavedLastCarOffset = 683;

        constexpr std::size_t FreemodeRequestLocalBase = 19672;
        constexpr std::ptrdiff_t FreemodeRequestLocalOffset = 176;

        constexpr auto RefreshInterval = std::chrono::seconds(10);
        constexpr auto RequestStepDelay = std::chrono::milliseconds(100);
        constexpr auto DespawnTimeout = std::chrono::seconds(3);

        constexpr std::uint32_t Joaat(const char* text) noexcept
        {
            std::uint32_t hash{};
            while (text && *text)
            {
                char c = *text++;
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

        constexpr std::uint32_t FreemodeHash = Joaat("freemode");

        struct GarageDefinition final
        {
            int property;
            std::size_t offset;
            int size;
            const char* label;
            const char* ownershipStat;
            bool alwaysOwned;
        };

        constexpr GarageDefinition GarageDefinitions[] = {
            {0, 0, 13, "Property Garage 1", "MPX_PROPERTY_HOUSE", false},
            {1, 13, 13, "Property Garage 2", "MPX_MULTI_PROPERTY_1", false},
            {2, 26, 13, "Property Garage 3", "MPX_MULTI_PROPERTY_2", false},
            {3, 39, 13, "Property Garage 4", "MPX_MULTI_PROPERTY_3", false},
            {4, 52, 13, "Property Garage 5", "MPX_MULTI_PROPERTY_4", false},
            {6, 65, 10, "MC Clubhouse", "MPX_PROP_CLUBHOUSE", false},
            {7, 75, 13, "Property Garage 6", "MPX_MULTI_PROPERTY_5", false},
            {8, 88, 20, "Office Garage 1", "MPX_PROP_OFFICE_GAR1", false},
            {9, 108, 20, "Office Garage 2", "MPX_PROP_OFFICE_GAR2", false},
            {10, 128, 20, "Office Garage 3", "MPX_PROP_OFFICE_GAR3", false},
            {11, 148, 8, "Vehicle Warehouse", "MPX_PROP_IE_WAREHOUSE", false},
            {12, 159, 20, "Hangar", "MPX_PROP_HANGAR", false},
            {13, 179, 11, "Facility", "MPX_PROP_DEFUNCBASE", false},
            {14, 191, 1, "Nightclub Service Entrance", "MPX_PROP_NIGHTCLUB", false},
            {15, 192, 10, "Nightclub B2", "MPX_PROP_MEGAWARE_GAR1", false},
            {16, 202, 10, "Nightclub B3", "MPX_PROP_MEGAWARE_GAR2", false},
            {17, 212, 10, "Nightclub B4", "MPX_PROP_MEGAWARE_GAR3", false},
            {18, 227, 10, "Arena Workshop", "MPX_PROP_ARENAWARS_GAR1", false},
            {19, 237, 10, "Arena Workshop B1", "MPX_PROP_ARENAWARS_GAR2", false},
            {20, 247, 10, "Arena Workshop B2", "MPX_PROP_ARENAWARS_GAR3", false},
            {21, 258, 10, "Casino Penthouse Garage", "MPX_PROP_CASINO_GAR1", false},
            {22, 268, 10, "Arcade Garage", "MPX_PROP_ARCADE_GAR1", false},
            {23, 281, 13, "Property Garage 7", "MPX_MULTI_PROPERTY_6", false},
            {24, 294, 13, "Property Garage 8", "MPX_MULTI_PROPERTY_7", false},
            {25, 307, 10, "Auto Shop", "MPX_PROP_AUTO_SHOP", false},
            {26, 317, 20, "Agency Garage", "MPX_PROP_SECURITY_OFFICE_GAR", false},
            {27, 337, 13, "Property Garage 9", "MPX_MULTI_PROPERTY_8", false},
            {28, 350, 13, "Property Garage 10", "MPX_MULTI_PROPERTY_9", false},
            {29, 363, 50, "Eclipse Blvd Garage", "MPX_MULTSTOREY_GAR_OWNED", false},
            {30, 415, 100, "Vinewood Club Garage", nullptr, true},
            {31, 515, 2, "Bail Office Garage", "MPX_PROP_BAIL_OFFICE", false},
            {32, 537, 10, "Garment Factory Garage", "MPX_PROP_HACKER_DEN", false},
            {33, 547, 20, "The Tongva Estate Garage", "MPX_MANSION_TH_OWNED", false},
            {34, 567, 20, "Richman Villa Garage", "MPX_MANSION_AJ_OWNED", false},
            {35, 587, 20, "The Vinewood Residence Garage", "MPX_MANSION_MD_OWNED", false},
            {36, 156, 1, "Mobile Operations Center", nullptr, true},
            {37, 224, 3, "Nightclub B1", nullptr, true},
            {38, 223, 1, "Terrorbyte", nullptr, true},
            {39, 278, 1, "Kosatka", nullptr, true},
        };
        constexpr std::size_t GarageDefinitionCount = sizeof(GarageDefinitions) / sizeof(GarageDefinitions[0]);

        struct GarageOwnershipState final
        {
            std::array<bool, GarageDefinitionCount> owned{};
            std::size_t ownedSources{};
            int characterIndex{-1};
            bool statsReady{true};
        };

        [[nodiscard]] GarageOwnershipState ReadGarageOwnership() noexcept
        {
            GarageOwnershipState state{};
            const auto characterIndex = Stats::GetCharIndex();
            if (characterIndex)
                state.characterIndex = *characterIndex;
            else
                state.statsReady = false;

            for (std::size_t index = 0; index < GarageDefinitionCount; ++index)
            {
                const auto& definition = GarageDefinitions[index];
                if (definition.alwaysOwned)
                {
                    state.owned[index] = true;
                    ++state.ownedSources;
                    continue;
                }

                if (!definition.ownershipStat || state.characterIndex < 0)
                {
                    state.statsReady = false;
                    continue;
                }

                const auto value = Stats::GetInt(definition.ownershipStat, state.characterIndex);
                if (!value)
                {
                    state.statsReady = false;
                    continue;
                }

                if (*value > 0)
                {
                    state.owned[index] = true;
                    ++state.ownedSources;
                }
            }

            return state;
        }

        [[nodiscard]] std::string ReadFixedLabel(const char* value, std::size_t maxLength)
        {
            if (!value || maxLength == 0)
                return {};

            std::size_t length{};
            while (length < maxLength && value[length] != '\0')
                ++length;
            return std::string(value, length);
        }

        [[nodiscard]] std::string ResolveVehicleName(Hash model)
        {
            if (const auto label = VehicleNatives::GetDisplayNameFromVehicleModel(model); label && *label && (*label)[0] != '\0')
            {
                if (const auto localized = VehicleNatives::GetLabelText(*label); localized && *localized && (*localized)[0] != '\0')
                    return *localized;
                return *label;
            }

            char fallback[16]{};
            std::snprintf(fallback, sizeof(fallback), "0x%08X", static_cast<unsigned int>(model));
            return fallback;
        }

        [[nodiscard]] std::string GarageLabel(const GarageDefinition& definition, int slot)
        {
            if (definition.property == 29)
            {
                const int floor = ((slot - 1) / 10) + 1;
                return std::string(definition.label) + " B" + std::to_string(floor);
            }
            return definition.label;
        }

        [[nodiscard]] std::string FindGarage(
            int vehicleId,
            std::int64_t** pages,
            const GarageOwnershipState& ownership)
        {
            for (std::size_t definitionIndex = 0; definitionIndex < GarageDefinitionCount; ++definitionIndex)
            {
                if (!ownership.owned[definitionIndex])
                    continue;

                const auto& definition = GarageDefinitions[definitionIndex];
                for (int slot = 1; slot <= definition.size; ++slot)
                {
                    const auto global = Script::ScriptGlobal(GarageSlotsGlobal).At(definition.offset).At(static_cast<std::size_t>(slot));
                    const int* item = global.As<int>(pages);
                    if (item && (*item - 1) == vehicleId)
                        return GarageLabel(definition, slot);
                }
            }
            return {};
        }

        [[nodiscard]] bool ValidMpsvId(int vehicleId, std::int64_t** pages) noexcept
        {
            if (vehicleId < 0 || vehicleId >= static_cast<int>(MpsvMaxEntries) || !pages)
                return false;
            const int* size = Script::ScriptGlobal(MpsvGlobal).As<int>(pages);
            return size && vehicleId < std::clamp(*size, 0, static_cast<int>(MpsvMaxEntries));
        }

        [[nodiscard]] std::uint32_t* VehicleFlags(int vehicleId, std::int64_t** pages) noexcept
        {
            if (!ValidMpsvId(vehicleId, pages))
                return nullptr;
            return Script::ScriptGlobal(MpsvGlobal)
                .At(static_cast<std::size_t>(vehicleId), MpsvEntrySize)
                .At(MpsvFlagsOffset)
                .As<std::uint32_t>(pages);
        }

        [[nodiscard]] bool LiveVehicleRecordIsValid(int vehicleId, std::int64_t** pages) noexcept
        {
            if (!ValidMpsvId(vehicleId, pages))
                return false;
            const auto entry = Script::ScriptGlobal(MpsvGlobal).At(static_cast<std::size_t>(vehicleId), MpsvEntrySize);
            const Hash* model = entry.At(MpsvVehicleModelOffset).As<Hash>(pages);
            if (!model || *model == 0)
                return false;
            const auto valid = VehicleNatives::IsModelAVehicle(*model);
            return valid && *valid;
        }
    }

    PersonalVehicleRuntime& PersonalVehicleRuntime::Get() noexcept
    {
        static PersonalVehicleRuntime instance;
        return instance;
    }

    bool PersonalVehicleRuntime::Start()
    {
        bool expected = false;
        if (!m_Running.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            return true;

        m_RequestStage = RequestStage::Idle;
        m_RequestVehicleId = -1;
        m_RequestDeadline = {};
        m_RequestStageReady = {};
        m_NextRefresh = {};
        {
            std::scoped_lock lock(m_Mutex);
            m_QueuedAction = PersonalVehicleAction::None;
            m_QueuedVehicleId = -1;
            m_ActionBusy = false;
            m_Snapshot.running = true;
            m_Snapshot.actionPending = false;
        }

        if (QueueNextTick())
        {
            TUTONES_LOG_INFO("vehicle.personal", "Personal vehicle runtime scheduled on the GTA script thread");
            return true;
        }

        m_Running.store(false, std::memory_order_release);
        {
            std::scoped_lock lock(m_Mutex);
            m_Snapshot.running = false;
        }
        TUTONES_LOG_ERROR("vehicle.personal", "Personal vehicle runtime failed to queue its first GTA script-thread tick");
        return false;
    }

    void PersonalVehicleRuntime::Stop() noexcept
    {
        if (!m_Running.exchange(false, std::memory_order_acq_rel))
            return;

        std::scoped_lock lock(m_Mutex);
        m_QueuedAction = PersonalVehicleAction::None;
        m_QueuedVehicleId = -1;
        m_ActionBusy = false;
        m_Snapshot.actionPending = false;
        m_Snapshot.running = false;
        TUTONES_LOG_INFO("vehicle.personal", "Personal vehicle runtime stopped");
    }

    bool PersonalVehicleRuntime::IsRunning() const noexcept
    {
        return m_Running.load(std::memory_order_acquire);
    }

    PersonalVehicleSnapshot PersonalVehicleRuntime::Snapshot() const
    {
        std::scoped_lock lock(m_Mutex);
        return m_Snapshot;
    }

    bool PersonalVehicleRuntime::QueueRepair(int vehicleId)
    {
        return QueueAction(PersonalVehicleAction::Repair, vehicleId);
    }

    bool PersonalVehicleRuntime::QueueRequest(int vehicleId)
    {
        return QueueAction(PersonalVehicleAction::Request, vehicleId);
    }

    bool PersonalVehicleRuntime::QueueAction(PersonalVehicleAction action, int vehicleId)
    {
        if (!IsRunning() || action == PersonalVehicleAction::None || vehicleId < 0 || vehicleId >= static_cast<int>(MpsvMaxEntries))
            return false;

        std::scoped_lock lock(m_Mutex);
        if (m_ActionBusy)
            return false;

        m_ActionBusy = true;
        m_QueuedAction = action;
        m_QueuedVehicleId = vehicleId;
        m_Snapshot.actionPending = true;
        return true;
    }

    bool PersonalVehicleRuntime::QueueNextTick()
    {
        if (!IsRunning())
            return false;
        return Runtime::GameRuntime::Get().Enqueue([this] { TickOnGameThread(); });
    }

    void PersonalVehicleRuntime::TickOnGameThread() noexcept
    {
        if (!IsRunning())
            return;

        const auto now = Clock::now();
        ProcessActionsOnGameThread(now);

        if (m_NextRefresh.time_since_epoch().count() == 0 || now >= m_NextRefresh)
        {
            RefreshOnGameThread();
            m_NextRefresh = now + RefreshInterval;
        }

        if (IsRunning() && !QueueNextTick())
        {
            m_Running.store(false, std::memory_order_release);
            std::scoped_lock lock(m_Mutex);
            m_ActionBusy = false;
            m_Snapshot.actionPending = false;
            m_Snapshot.running = false;
            TUTONES_LOG_ERROR("vehicle.personal", "Personal vehicle runtime lost its GTA script-thread scheduling slot and stopped");
        }
    }

    void PersonalVehicleRuntime::ProcessActionsOnGameThread(Clock::time_point now) noexcept
    {
        if (m_RequestStage != RequestStage::Idle)
        {
            ContinueRequestOnGameThread(now);
            return;
        }

        PersonalVehicleAction action = PersonalVehicleAction::None;
        int vehicleId = -1;
        {
            std::scoped_lock lock(m_Mutex);
            action = m_QueuedAction;
            vehicleId = m_QueuedVehicleId;
            m_QueuedAction = PersonalVehicleAction::None;
            m_QueuedVehicleId = -1;
        }

        if (action == PersonalVehicleAction::None)
            return;

        if (action == PersonalVehicleAction::Repair)
        {
            const bool success = RepairVehicleOnGameThread(vehicleId, true);
            RecordAction(action, vehicleId, success);
            m_NextRefresh = {};
            return;
        }

        if (action == PersonalVehicleAction::Request && !BeginRequestOnGameThread(vehicleId, now))
        {
            RecordAction(action, vehicleId, false);
            m_NextRefresh = {};
        }
    }

    bool PersonalVehicleRuntime::RepairVehicleOnGameThread(int vehicleId, bool requireRepairable) noexcept
    {
        auto* pages = GamePointers::Get().ScriptGlobals();
        if (!pages || !Native::NativeRegistry::Get().IsReady() || !LiveVehicleRecordIsValid(vehicleId, pages))
            return false;

        std::uint32_t* flags = VehicleFlags(vehicleId, pages);
        if (!flags)
            return false;

        const bool repairable = (*flags & DestroyedBit) != 0 && (*flags & HasInsuranceBit) != 0;
        if (!repairable)
            return !requireRepairable;

        *flags &= ~(DestroyedBit | ImpoundedBit | UnknownRepairBit);
        return true;
    }

    bool PersonalVehicleRuntime::BeginRequestOnGameThread(int vehicleId, Clock::time_point now) noexcept
    {
        auto* pages = GamePointers::Get().ScriptGlobals();
        bool* sessionStarted = GamePointers::Get().IsSessionStarted();
        if (!pages || !sessionStarted || !*sessionStarted || !Native::NativeRegistry::Get().IsReady())
            return false;
        if (!Script::ScriptRuntime::Get().IsReady() || !LiveVehicleRecordIsValid(vehicleId, pages))
            return false;

        auto* freemode = Script::ScriptRuntime::Get().FindThread(FreemodeHash);
        if (!freemode || !freemode->stack)
            return false;

        int* requestedVehicleId = Script::ScriptGlobal(FreemodeGeneralGlobal).At(RequestedPersonalVehicleIdOffset).As<int>(pages);
        int* currentVehicleHandle = Script::ScriptGlobal(FreemodeGeneralGlobal).At(PersonalVehicleIndexOffset).As<int>(pages);
        if (!requestedVehicleId || !currentVehicleHandle || *requestedVehicleId != -1)
            return false;

        m_RequestVehicleId = vehicleId;
        if (*currentVehicleHandle != 0)
        {
            const auto exists = Natives::DoesEntityExist(*currentVehicleHandle);
            if (!exists)
                return false;

            if (*exists)
            {
                int* currentVehicleId = Script::ScriptGlobal(SavedMpGlobals)
                    .At(0, SavedMpEntrySize)
                    .At(SavedLastCarOffset)
                    .As<int>(pages);
                if (!currentVehicleId || !ValidMpsvId(*currentVehicleId, pages))
                    return false;

                std::uint32_t* currentFlags = VehicleFlags(*currentVehicleId, pages);
                if (!currentFlags)
                    return false;

                *currentFlags &= ~TriggerSpawnToggleBit;
                m_RequestStage = RequestStage::WaitingForDespawn;
                m_RequestDeadline = now + DespawnTimeout;
                return true;
            }
        }

        if (!RepairVehicleOnGameThread(vehicleId, false))
            return false;

        m_RequestStage = RequestStage::WaitBeforeRequest;
        m_RequestStageReady = now + RequestStepDelay;
        return true;
    }

    void PersonalVehicleRuntime::ContinueRequestOnGameThread(Clock::time_point now) noexcept
    {
        auto fail = [this] {
            const int vehicleId = m_RequestVehicleId;
            m_RequestStage = RequestStage::Idle;
            m_RequestVehicleId = -1;
            m_RequestDeadline = {};
            m_RequestStageReady = {};
            RecordAction(PersonalVehicleAction::Request, vehicleId, false);
            m_NextRefresh = {};
        };

        auto* pages = GamePointers::Get().ScriptGlobals();
        bool* sessionStarted = GamePointers::Get().IsSessionStarted();
        if (!pages || !sessionStarted || !*sessionStarted)
        {
            fail();
            return;
        }

        if (m_RequestStage == RequestStage::WaitingForDespawn)
        {
            int* currentVehicleHandle = Script::ScriptGlobal(FreemodeGeneralGlobal).At(PersonalVehicleIndexOffset).As<int>(pages);
            if (!currentVehicleHandle)
            {
                fail();
                return;
            }

            bool stillExists = false;
            if (*currentVehicleHandle != 0)
            {
                const auto exists = Natives::DoesEntityExist(*currentVehicleHandle);
                if (!exists)
                {
                    fail();
                    return;
                }
                stillExists = *exists;
            }

            if (stillExists)
            {
                if (now >= m_RequestDeadline)
                    fail();
                return;
            }

            if (!RepairVehicleOnGameThread(m_RequestVehicleId, false))
            {
                fail();
                return;
            }

            m_RequestStage = RequestStage::WaitBeforeRequest;
            m_RequestStageReady = now + RequestStepDelay;
            return;
        }

        if (m_RequestStage == RequestStage::WaitBeforeRequest)
        {
            if (now < m_RequestStageReady)
                return;

            int* requested = Script::ScriptGlobal(FreemodeGeneralGlobal).At(RequestedPersonalVehicleIdOffset).As<int>(pages);
            int* requestFlag = Script::ScriptGlobal(FreemodeGeneralGlobal).At(PersonalVehicleRequestedOffset).As<int>(pages);
            int* impound = Script::ScriptGlobal(FreemodeGeneralGlobal).At(ExecImpoundOffset).As<int>(pages);
            if (!requested || !requestFlag || !impound || *requested != -1)
            {
                fail();
                return;
            }

            *requestFlag = 1;
            *impound = 0;
            *requested = m_RequestVehicleId;
            m_RequestStage = RequestStage::WaitBeforeLocalClear;
            m_RequestStageReady = now + RequestStepDelay;
            return;
        }

        if (m_RequestStage == RequestStage::WaitBeforeLocalClear)
        {
            if (now < m_RequestStageReady)
                return;

            auto* freemode = Script::ScriptRuntime::Get().FindThread(FreemodeHash);
            if (!freemode || !freemode->stack)
            {
                fail();
                return;
            }

            int* requestLocal = Script::ScriptLocal(freemode, FreemodeRequestLocalBase)
                .At(FreemodeRequestLocalOffset)
                .As<int>();
            if (!requestLocal)
            {
                fail();
                return;
            }

            *requestLocal = 0;
            const int vehicleId = m_RequestVehicleId;
            m_RequestStage = RequestStage::Idle;
            m_RequestVehicleId = -1;
            m_RequestDeadline = {};
            m_RequestStageReady = {};
            RecordAction(PersonalVehicleAction::Request, vehicleId, true);
            m_NextRefresh = {};
        }
    }

    void PersonalVehicleRuntime::RecordAction(PersonalVehicleAction action, int vehicleId, bool success) noexcept
    {
        std::scoped_lock lock(m_Mutex);
        m_ActionBusy = false;
        m_Snapshot.actionPending = false;
        m_Snapshot.lastAction = action;
        m_Snapshot.lastActionVehicleId = vehicleId;
        m_Snapshot.lastActionSucceeded = success;
    }

    void PersonalVehicleRuntime::PublishUnavailable(bool globalsReady, bool nativeReady) noexcept
    {
        std::scoped_lock lock(m_Mutex);
        const std::uint64_t revision = m_Snapshot.revision + 1;
        const auto lastAction = m_Snapshot.lastAction;
        const int lastActionVehicleId = m_Snapshot.lastActionVehicleId;
        const bool lastActionSucceeded = m_Snapshot.lastActionSucceeded;
        const bool actionPending = m_ActionBusy;
        m_Snapshot = {};
        m_Snapshot.revision = revision;
        m_Snapshot.lastAction = lastAction;
        m_Snapshot.lastActionVehicleId = lastActionVehicleId;
        m_Snapshot.lastActionSucceeded = lastActionSucceeded;
        m_Snapshot.actionPending = actionPending;
        m_Snapshot.running = IsRunning();
        m_Snapshot.scriptGlobalsReady = globalsReady;
        m_Snapshot.nativeReady = nativeReady;
    }

    void PersonalVehicleRuntime::RefreshOnGameThread() noexcept
    {
        auto* pages = GamePointers::Get().ScriptGlobals();
        const bool nativeReady = Native::NativeRegistry::Get().IsReady();
        if (!pages || !nativeReady)
        {
            PublishUnavailable(pages != nullptr, nativeReady);
            return;
        }

        const int* arraySize = Script::ScriptGlobal(MpsvGlobal).As<int>(pages);
        if (!arraySize)
        {
            PublishUnavailable(false, nativeReady);
            return;
        }

        const std::size_t count = static_cast<std::size_t>(std::clamp(*arraySize, 0, static_cast<int>(MpsvMaxEntries)));
        const GarageOwnershipState garageOwnership = ReadGarageOwnership();
        PersonalVehicleSnapshot next{};
        next.running = IsRunning();
        next.scriptGlobalsReady = true;
        next.nativeReady = true;
        next.sourceArraySize = count;
        next.ownedGarageSources = garageOwnership.ownedSources;
        next.garageCharacterIndex = garageOwnership.characterIndex;
        next.garageOwnershipStatsReady = garageOwnership.statsReady;
        next.vehicles.reserve(count);

        if (bool* sessionStarted = GamePointers::Get().IsSessionStarted())
        {
            next.sessionStarted = *sessionStarted;
            next.requestSupported = Script::ScriptRuntime::Get().IsReady();
        }

        if (int* currentVehicleId = Script::ScriptGlobal(SavedMpGlobals)
                .At(0, SavedMpEntrySize)
                .At(SavedLastCarOffset)
                .As<int>(pages))
        {
            if (ValidMpsvId(*currentVehicleId, pages))
                next.currentVehicleId = *currentVehicleId;
        }

        if (int* requestedVehicleId = Script::ScriptGlobal(FreemodeGeneralGlobal).At(RequestedPersonalVehicleIdOffset).As<int>(pages))
            next.requestedVehicleId = *requestedVehicleId;

        std::set<std::string> garages;
        for (std::size_t i = 0; i < count; ++i)
        {
            const Script::ScriptGlobal entry = Script::ScriptGlobal(MpsvGlobal).At(i, MpsvEntrySize);
            const Hash* modelPtr = entry.At(MpsvVehicleModelOffset).As<Hash>(pages);
            if (!modelPtr || *modelPtr == 0)
                continue;

            const auto isVehicle = VehicleNatives::IsModelAVehicle(*modelPtr);
            if (!isVehicle || !*isVehicle)
                continue;

            PersonalVehicleEntry vehicle{};
            vehicle.id = static_cast<int>(i);
            vehicle.model = *modelPtr;
            vehicle.displayName = ResolveVehicleName(vehicle.model);
            vehicle.plate = ReadFixedLabel(entry.At(MpsvPlateOffset).As<char>(pages), 16);
            vehicle.garage = FindGarage(vehicle.id, pages, garageOwnership);
            if (const std::uint32_t* flags = entry.At(MpsvFlagsOffset).As<std::uint32_t>(pages))
            {
                vehicle.destroyed = (*flags & DestroyedBit) != 0;
                vehicle.insured = (*flags & HasInsuranceBit) != 0;
                vehicle.impounded = (*flags & ImpoundedBit) != 0;
            }
            if (!vehicle.garage.empty())
                garages.emplace(vehicle.garage);
            next.vehicles.emplace_back(std::move(vehicle));
        }

        std::sort(next.vehicles.begin(), next.vehicles.end(), [](const PersonalVehicleEntry& left, const PersonalVehicleEntry& right) {
            if (left.garage != right.garage)
                return left.garage < right.garage;
            if (left.displayName != right.displayName)
                return left.displayName < right.displayName;
            return left.id < right.id;
        });
        next.garages.assign(garages.begin(), garages.end());

        {
            std::scoped_lock lock(m_Mutex);
            next.revision = m_Snapshot.revision + 1;
            next.lastAction = m_Snapshot.lastAction;
            next.lastActionVehicleId = m_Snapshot.lastActionVehicleId;
            next.lastActionSucceeded = m_Snapshot.lastActionSucceeded;
            next.actionPending = m_ActionBusy;
            m_Snapshot = std::move(next);
        }
    }
}
