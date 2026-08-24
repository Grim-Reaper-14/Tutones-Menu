#include "PersonalVehicleRuntime.hpp"

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/Natives.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace Tutones::Game::PersonalVehicles
{
    namespace
    {
        constexpr std::size_t MpsvGlobal = 1583778;
        constexpr std::size_t MpsvEntrySize = 143;
        constexpr std::size_t MpsvMaxEntries = 607;
        constexpr std::size_t MpsvVehicleModelOffset = 66;
        constexpr std::size_t MpsvFlagsOffset = 104;

        constexpr std::uint32_t TriggerSpawnToggleBit = 1u << 0;
        constexpr std::uint32_t DestroyedBit = 1u << 1;
        constexpr std::uint32_t HasInsuranceBit = 1u << 2;
        constexpr std::uint32_t ImpoundedBit = 1u << 6;
        constexpr std::uint32_t UnknownRepairBit = 1u << 16;

        constexpr std::size_t FreemodeGeneralGlobal = 2733326;
        constexpr std::size_t PersonalVehicleIndexOffset = 301;

        [[nodiscard]] bool ValidMpsvId(int vehicleId, std::int64_t** pages) noexcept
        {
            if (!pages || vehicleId < 0 || vehicleId >= static_cast<int>(MpsvMaxEntries))
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
    }

    bool PersonalVehicleRuntime::QueueReturnCurrent()
    {
        int vehicleId = -1;
        {
            std::scoped_lock lock(m_Mutex);
            if (!IsRunning() || m_ActionBusy || !m_Snapshot.sessionStarted || !m_Snapshot.scriptGlobalsReady)
                return false;
            if (m_Snapshot.currentVehicleId < 0)
                return false;

            vehicleId = m_Snapshot.currentVehicleId;
            m_ActionBusy = true;
            m_Snapshot.actionPending = true;
        }

        const bool queued = Runtime::GameRuntime::Get().Enqueue([this, vehicleId] {
            bool success = false;
            auto* pages = GamePointers::Get().ScriptGlobals();
            bool* sessionStarted = GamePointers::Get().IsSessionStarted();

            if (pages && sessionStarted && *sessionStarted && ValidMpsvId(vehicleId, pages))
            {
                int* currentHandle = Script::ScriptGlobal(FreemodeGeneralGlobal)
                    .At(PersonalVehicleIndexOffset)
                    .As<int>(pages);
                std::uint32_t* flags = VehicleFlags(vehicleId, pages);

                if (currentHandle && flags && *currentHandle != 0)
                {
                    const auto exists = Natives::DoesEntityExist(*currentHandle);
                    if (exists && *exists)
                    {
                        // Decompiled MPSV behavior: clearing TRIGGER_SPAWN_TOGGLE on
                        // the live personal-vehicle entry tells freemode to return the
                        // active PV to storage. Preserve every unrelated MPSV flag.
                        *flags &= ~TriggerSpawnToggleBit;
                        success = (*flags & TriggerSpawnToggleBit) == 0;
                    }
                }
            }

            RecordAction(PersonalVehicleAction::ReturnToStorage, vehicleId, success);
            m_NextRefresh = {};
            TUTONES_LOG_INFO(
                "vehicle.personal",
                success
                    ? "Submitted decompile-backed Return Personal Vehicle to Storage state"
                    : "Return Personal Vehicle to Storage state was rejected");
        });

        if (queued)
            return true;

        std::scoped_lock lock(m_Mutex);
        m_ActionBusy = false;
        m_Snapshot.actionPending = false;
        return false;
    }

    bool PersonalVehicleRuntime::QueueRepairAll()
    {
        {
            std::scoped_lock lock(m_Mutex);
            if (!IsRunning() || m_ActionBusy || !m_Snapshot.sessionStarted || !m_Snapshot.scriptGlobalsReady)
                return false;
            m_ActionBusy = true;
            m_Snapshot.actionPending = true;
        }

        const bool queued = Runtime::GameRuntime::Get().Enqueue([this] {
            bool success = false;
            int repaired{};
            auto* pages = GamePointers::Get().ScriptGlobals();
            bool* sessionStarted = GamePointers::Get().IsSessionStarted();

            if (pages && sessionStarted && *sessionStarted)
            {
                if (const int* size = Script::ScriptGlobal(MpsvGlobal).As<int>(pages))
                {
                    const int count = std::clamp(*size, 0, static_cast<int>(MpsvMaxEntries));
                    success = true;
                    for (int vehicleId = 0; vehicleId < count; ++vehicleId)
                    {
                        const auto entry = Script::ScriptGlobal(MpsvGlobal)
                            .At(static_cast<std::size_t>(vehicleId), MpsvEntrySize);
                        const Hash* model = entry.At(MpsvVehicleModelOffset).As<Hash>(pages);
                        std::uint32_t* flags = entry.At(MpsvFlagsOffset).As<std::uint32_t>(pages);
                        if (!model || *model == 0 || !flags)
                            continue;

                        const bool repairable = (*flags & DestroyedBit) != 0
                            && (*flags & HasInsuranceBit) != 0;
                        if (!repairable)
                            continue;

                        // Mirrors the decompiled MPSV repair state used by the
                        // selected-vehicle repair path: clear destroyed, impounded
                        // and the additional repair-state bit, leaving insurance set.
                        *flags &= ~(DestroyedBit | ImpoundedBit | UnknownRepairBit);
                        if ((*flags & (DestroyedBit | ImpoundedBit | UnknownRepairBit)) == 0)
                            ++repaired;
                        else
                            success = false;
                    }
                }
            }

            RecordAction(PersonalVehicleAction::RepairAll, -1, success);
            m_NextRefresh = {};
            TUTONES_LOG_INFO(
                "vehicle.personal",
                std::string("Repair All personal vehicles: ")
                    + (success ? "success, repaired=" : "incomplete, repaired=")
                    + std::to_string(repaired));
        });

        if (queued)
            return true;

        std::scoped_lock lock(m_Mutex);
        m_ActionBusy = false;
        m_Snapshot.actionPending = false;
        return false;
    }
}
