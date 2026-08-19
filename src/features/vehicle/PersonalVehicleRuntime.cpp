#include "PersonalVehicleRuntime.hpp"

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/VehicleNatives.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../game/script/ScriptGlobal.hpp"
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
        constexpr std::size_t GarageSlotsGlobal = 1945138;
        constexpr auto RefreshInterval = std::chrono::seconds(10);

        struct GarageDefinition final
        {
            std::size_t offset;
            int size;
            const char* label;
        };

        constexpr GarageDefinition GarageDefinitions[] = {
            {0, 13, "Property Garage 1"},
            {13, 13, "Property Garage 2"},
            {26, 13, "Property Garage 3"},
            {39, 13, "Property Garage 4"},
            {52, 13, "Property Garage 5"},
            {65, 10, "MC Clubhouse"},
            {75, 13, "Property Garage 6"},
            {88, 20, "Office Garage 1"},
            {108, 20, "Office Garage 2"},
            {128, 20, "Office Garage 3"},
            {148, 8, "Vehicle Warehouse"},
            {159, 20, "Hangar"},
            {179, 11, "Facility"},
            {191, 1, "Nightclub Service Entrance"},
            {192, 10, "Nightclub B2"},
            {202, 10, "Nightclub B3"},
            {212, 10, "Nightclub B4"},
            {227, 10, "Arena Workshop"},
            {237, 10, "Arena Workshop B1"},
            {247, 10, "Arena Workshop B2"},
            {258, 10, "Casino Penthouse Garage"},
            {268, 10, "Arcade Garage"},
            {281, 13, "Property Garage 7"},
            {294, 13, "Property Garage 8"},
            {307, 10, "Auto Shop"},
            {317, 20, "Agency Garage"},
            {337, 13, "Property Garage 9"},
            {350, 13, "Property Garage 10"},
            {363, 50, "Eclipse Blvd Garage"},
            {415, 100, "Vinewood Club Garage"},
            {515, 2, "Bail Office Garage"},
            {537, 10, "Garment Factory Garage"},
            {547, 20, "The Tongva Estate Garage"},
            {567, 20, "Richman Villa Garage"},
            {587, 20, "The Vinewood Residence Garage"},
            {156, 1, "Mobile Operations Center"},
            {224, 3, "Nightclub B1"},
            {223, 1, "Terrorbyte"},
            {278, 1, "Kosatka"},
        };

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
            if (definition.offset == 363)
            {
                const int floor = ((slot - 1) / 10) + 1;
                return std::string(definition.label) + " B" + std::to_string(floor);
            }
            return definition.label;
        }

        [[nodiscard]] std::string FindGarage(int vehicleId, std::int64_t** pages)
        {
            for (const auto& definition : GarageDefinitions)
            {
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

        m_NextRefresh = {};
        {
            std::scoped_lock lock(m_Mutex);
            m_Snapshot.running = true;
        }

        if (QueueNextTick())
        {
            TUTONES_LOG_INFO("vehicle.personal", "Personal vehicle reader scheduled on the GTA script thread");
            return true;
        }

        m_Running.store(false, std::memory_order_release);
        {
            std::scoped_lock lock(m_Mutex);
            m_Snapshot.running = false;
        }
        TUTONES_LOG_ERROR("vehicle.personal", "Personal vehicle reader failed to queue its first GTA script-thread tick");
        return false;
    }

    void PersonalVehicleRuntime::Stop() noexcept
    {
        if (!m_Running.exchange(false, std::memory_order_acq_rel))
            return;

        std::scoped_lock lock(m_Mutex);
        m_Snapshot.running = false;
        TUTONES_LOG_INFO("vehicle.personal", "Personal vehicle reader stopped");
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
        if (m_NextRefresh.time_since_epoch().count() == 0 || now >= m_NextRefresh)
        {
            RefreshOnGameThread();
            m_NextRefresh = now + RefreshInterval;
        }

        if (IsRunning() && !QueueNextTick())
        {
            m_Running.store(false, std::memory_order_release);
            std::scoped_lock lock(m_Mutex);
            m_Snapshot.running = false;
            TUTONES_LOG_ERROR("vehicle.personal", "Personal vehicle reader lost its GTA script-thread scheduling slot and stopped");
        }
    }

    void PersonalVehicleRuntime::PublishUnavailable(bool globalsReady, bool nativeReady) noexcept
    {
        std::scoped_lock lock(m_Mutex);
        const std::uint64_t revision = m_Snapshot.revision + 1;
        m_Snapshot = {};
        m_Snapshot.revision = revision;
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
        PersonalVehicleSnapshot next{};
        next.running = IsRunning();
        next.scriptGlobalsReady = true;
        next.nativeReady = true;
        next.sourceArraySize = count;
        next.vehicles.reserve(count);

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
            vehicle.garage = FindGarage(vehicle.id, pages);
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
            m_Snapshot = std::move(next);
        }
    }
}
