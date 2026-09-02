#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/Natives.hpp"
#include "../../game/VehicleNatives.hpp"
#include "../../game/native/NativeInvoker.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::Heist
{
    namespace ExoticExportEnhanced173
    {
        inline constexpr std::size_t RandomEventsGlobal = 1882345;
        inline constexpr std::size_t EventArrayOffset = 1;
        inline constexpr std::size_t EventStride = 15;
        inline constexpr std::size_t ExoticExportsEventIndex = 3;

        // GSBD_RandomEvents::EventData uses the Enhanced RANDOM_EVENTS_DATA layout.
        inline constexpr std::size_t StateField = 0;
        inline constexpr std::size_t VariationField = 5;
        inline constexpr std::size_t SubvariationField = 6;
        inline constexpr std::size_t TriggerPositionField = 10;
        inline constexpr std::size_t TriggerRangeField = 13;

        inline constexpr std::size_t VehicleListDataOffset = 362;
        inline constexpr std::size_t VehicleListVariationOffset = 363;

        inline constexpr int StateInactive = 0;
        inline constexpr int StateAvailable = 1;
        inline constexpr int StateActive = 2;
        inline constexpr int StateCleanup = 3;
    }

    [[nodiscard]] inline const char* ExoticExportStateName(int state) noexcept
    {
        switch (state)
        {
        case ExoticExportEnhanced173::StateInactive: return "Inactive";
        case ExoticExportEnhanced173::StateAvailable: return "Available";
        case ExoticExportEnhanced173::StateActive: return "Active";
        case ExoticExportEnhanced173::StateCleanup: return "Cleanup";
        default: return "Unknown";
        }
    }

    struct ExoticExportSnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool sessionStarted{};
        bool globalsReady{};
        bool layoutValid{};
        bool coordinatesValid{};
        int eventState{-1};
        int eventVariation{-1};
        int eventSubvariation{-1};
        int vehicleListIndex{-1};
        int vehicleListVariation{-1};
        float x{};
        float y{};
        float z{};
        float triggerRange{};
        std::string message{"Ready"};
    };

    class ExoticExportRuntime final
    {
    public:
        static ExoticExportRuntime& Get() noexcept
        {
            static ExoticExportRuntime instance;
            return instance;
        }

        [[nodiscard]] bool QueueRefresh()
        {
            return Queue("Refreshing live Exotic Export state", [this] {
                ExoticExportSnapshot state;
                std::string error;
                const bool success = ReadLiveState(state, error);
                if (!success)
                {
                    Finish(false, std::move(state), std::move(error));
                    return;
                }

                TUTONES_LOG_DEBUG(
                    "heist.exotic_export",
                    std::string("Exotic Export state refreshed; state=")
                        + ExoticExportStateName(state.eventState)
                        + " variation=" + std::to_string(state.eventVariation)
                        + " subvariation=" + std::to_string(state.eventSubvariation));

                if (state.coordinatesValid)
                {
                    Finish(true, std::move(state), "Active Exotic Export vehicle located");
                }
                else if (state.eventState == ExoticExportEnhanced173::StateInactive)
                {
                    Finish(true, std::move(state), "Exotic Export event is inactive; Rockstar has not spawned an export vehicle yet");
                }
                else
                {
                    Finish(true, std::move(state), "Exotic Export event exists, but live trigger coordinates are not published yet");
                }
            });
        }

        [[nodiscard]] bool QueueWaypointToActive()
        {
            return Queue("Setting waypoint to active Exotic Export", [this] {
                ExoticExportSnapshot state;
                std::string error;
                if (!ReadLiveState(state, error))
                {
                    Finish(false, std::move(state), std::move(error));
                    return;
                }

                if (!state.coordinatesValid)
                {
                    Finish(false, std::move(state), "No spawned Exotic Export currently has valid coordinates");
                    return;
                }

                if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread())
                {
                    Finish(false, std::move(state), "Waypoint native is unavailable on the GTA script thread");
                    return;
                }

                ExoticExportSnapshot confirmed;
                if (!ConfirmLiveTarget(state, confirmed, error))
                {
                    Finish(false, std::move(confirmed), std::move(error));
                    return;
                }

                if (!Native::NativeInvoker::InvokeVoid(
                        Native::NativeId::SetNewWaypoint,
                        confirmed.x,
                        confirmed.y))
                {
                    Finish(false, std::move(confirmed), "Failed to invoke the Exotic Export waypoint native");
                    return;
                }

                TUTONES_LOG_INFO(
                    "heist.exotic_export",
                    std::string("Waypoint set to active Exotic Export at ")
                        + std::to_string(confirmed.x) + ", " + std::to_string(confirmed.y));
                Finish(true, std::move(confirmed), "Waypoint set to the active Exotic Export vehicle");
            });
        }

        [[nodiscard]] bool QueueTeleportToActive()
        {
            return Queue("Locating active Exotic Export", [this] {
                ExoticExportSnapshot state;
                std::string error;
                if (!ReadLiveState(state, error))
                {
                    Finish(false, std::move(state), std::move(error));
                    return;
                }

                if (!state.coordinatesValid)
                {
                    Finish(false, std::move(state), "No spawned Exotic Export currently has valid coordinates");
                    return;
                }

                if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread())
                {
                    Finish(false, std::move(state), "Teleport natives are unavailable on the GTA script thread");
                    return;
                }

                if (!Native::NativeInvoker::InvokeVoid(
                        Native::NativeId::RequestCollisionAtCoord,
                        state.x,
                        state.y,
                        state.z))
                {
                    Finish(false, std::move(state), "Could not request collision at the Exotic Export location");
                    return;
                }

                if (!QueueTeleportContinuation(std::move(state), 1))
                {
                    Finish(false, ExoticExportSnapshot{}, "GTA script-thread queue became unavailable during collision warmup");
                }
            });
        }

        [[nodiscard]] ExoticExportSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            ExoticExportSnapshot state = m_Snapshot;
            state.pending = m_Pending.load(std::memory_order_acquire);
            return state;
        }

    private:
        static constexpr std::size_t CollisionWarmupTicks = 3;

        ExoticExportRuntime() = default;
        ExoticExportRuntime(const ExoticExportRuntime&) = delete;
        ExoticExportRuntime& operator=(const ExoticExportRuntime&) = delete;

        template<typename Callback>
        [[nodiscard]] bool Queue(std::string pendingMessage, Callback&& callback)
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot.haveResult = false;
                m_Snapshot.lastSucceeded = false;
                m_Snapshot.message = std::move(pendingMessage);
            }

            if (Runtime::GameRuntime::Get().Enqueue(std::forward<Callback>(callback)))
                return true;

            m_Pending.store(false, std::memory_order_release);
            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot.haveResult = true;
                m_Snapshot.lastSucceeded = false;
                m_Snapshot.message = "GTA script-thread queue is unavailable";
            }
            return false;
        }

        [[nodiscard]] bool ReadLiveState(ExoticExportSnapshot& state, std::string& error) const noexcept
        {
            const bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            state.sessionStarted = sessionStarted && *sessionStarted;
            if (!state.sessionStarted)
            {
                error = "Join GTA Online before locating Exotic Export vehicles";
                return false;
            }

            auto* pages = GamePointers::Get().ScriptGlobals();
            state.globalsReady = pages != nullptr;
            if (!state.globalsReady)
            {
                error = "Enhanced script globals are unavailable";
                return false;
            }

            const Script::ScriptGlobal root(ExoticExportEnhanced173::RandomEventsGlobal);
            const auto eventArray = root.At(ExoticExportEnhanced173::EventArrayOffset);
            const auto event = eventArray.At(
                ExoticExportEnhanced173::ExoticExportsEventIndex,
                ExoticExportEnhanced173::EventStride);

            const auto readInt = [pages](Script::ScriptGlobal global, int& output) noexcept
            {
                const int* value = global.As<int>(pages);
                if (!value)
                    return false;
                output = *value;
                return true;
            };

            const auto readFloat = [pages](Script::ScriptGlobal global, float& output) noexcept
            {
                const float* value = global.As<float>(pages);
                if (!value)
                    return false;
                output = *value;
                return true;
            };

            if (!readInt(event.At(ExoticExportEnhanced173::StateField), state.eventState)
                || !readInt(event.At(ExoticExportEnhanced173::VariationField), state.eventVariation)
                || !readInt(event.At(ExoticExportEnhanced173::SubvariationField), state.eventSubvariation)
                || !readFloat(event.At(ExoticExportEnhanced173::TriggerPositionField), state.x)
                || !readFloat(event.At(ExoticExportEnhanced173::TriggerPositionField + 1), state.y)
                || !readFloat(event.At(ExoticExportEnhanced173::TriggerPositionField + 2), state.z)
                || !readFloat(event.At(ExoticExportEnhanced173::TriggerRangeField), state.triggerRange)
                || !readInt(root.At(ExoticExportEnhanced173::VehicleListDataOffset), state.vehicleListIndex)
                || !readInt(root.At(ExoticExportEnhanced173::VehicleListVariationOffset), state.vehicleListVariation))
            {
                error = "Unable to read the Enhanced Exotic Export random-event block";
                return false;
            }

            state.layoutValid = state.eventState >= ExoticExportEnhanced173::StateInactive
                && state.eventState <= ExoticExportEnhanced173::StateCleanup
                && state.eventVariation >= -1 && state.eventVariation < 1024
                && state.eventSubvariation >= -1 && state.eventSubvariation < 1024
                && state.vehicleListIndex >= -1 && state.vehicleListIndex < 1024
                && state.vehicleListVariation >= -1 && state.vehicleListVariation < 1024
                && std::isfinite(state.triggerRange)
                && state.triggerRange >= 0.0f && state.triggerRange <= 5000.0f;
            if (!state.layoutValid)
            {
                error = "Enhanced Exotic Export layout validation failed";
                return false;
            }

            state.coordinatesValid =
                (state.eventState == ExoticExportEnhanced173::StateAvailable
                    || state.eventState == ExoticExportEnhanced173::StateActive)
                && std::isfinite(state.x)
                && std::isfinite(state.y)
                && std::isfinite(state.z)
                && std::fabs(state.x) < 10000.0f
                && std::fabs(state.y) < 10000.0f
                && state.z > -1000.0f
                && state.z < 3000.0f
                && (std::fabs(state.x) > 0.01f || std::fabs(state.y) > 0.01f);

            return true;
        }

        [[nodiscard]] bool ConfirmLiveTarget(
            const ExoticExportSnapshot& expected,
            ExoticExportSnapshot& confirmed,
            std::string& error) const noexcept
        {
            if (!ReadLiveState(confirmed, error))
                return false;
            if (!confirmed.coordinatesValid || !SameLiveTarget(expected, confirmed))
            {
                error = "The active Exotic Export changed or despawned; refresh before trying again";
                return false;
            }
            return true;
        }

        [[nodiscard]] static bool SameLiveTarget(
            const ExoticExportSnapshot& expected,
            const ExoticExportSnapshot& current) noexcept
        {
            const float dx = expected.x - current.x;
            const float dy = expected.y - current.y;
            const float dz = expected.z - current.z;
            return expected.eventState == current.eventState
                && expected.eventVariation == current.eventVariation
                && expected.eventSubvariation == current.eventSubvariation
                && expected.vehicleListIndex == current.vehicleListIndex
                && expected.vehicleListVariation == current.vehicleListVariation
                && (dx * dx + dy * dy) <= 0.25f
                && std::fabs(dz) <= 0.5f;
        }

        [[nodiscard]] bool QueueTeleportContinuation(
            ExoticExportSnapshot expected,
            std::size_t attempt)
        {
            return Runtime::GameRuntime::Get().Enqueue(
                [this, expected = std::move(expected), attempt]() mutable {
                    ContinueTeleport(std::move(expected), attempt);
                });
        }

        void ContinueTeleport(ExoticExportSnapshot expected, std::size_t attempt)
        {
            if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread())
            {
                Finish(false, std::move(expected), "Teleport natives became unavailable during collision warmup");
                return;
            }

            ExoticExportSnapshot confirmed;
            std::string error;
            if (!ConfirmLiveTarget(expected, confirmed, error))
            {
                Finish(false, std::move(confirmed), std::move(error));
                return;
            }

            if (!Native::NativeInvoker::InvokeVoid(
                    Native::NativeId::RequestCollisionAtCoord,
                    confirmed.x,
                    confirmed.y,
                    confirmed.z))
            {
                Finish(false, std::move(confirmed), "Collision streaming failed during Exotic Export teleport");
                return;
            }

            if (attempt < CollisionWarmupTicks)
            {
                if (!QueueTeleportContinuation(std::move(confirmed), attempt + 1))
                    Finish(false, ExoticExportSnapshot{}, "GTA script-thread queue became unavailable during collision warmup");
                return;
            }

            TeleportToConfirmedTarget(std::move(confirmed));
        }

        void TeleportToConfirmedTarget(ExoticExportSnapshot state)
        {
            const auto ped = Natives::PlayerPedId();
            if (!ped || *ped == 0)
            {
                Finish(false, std::move(state), "Local player ped is unavailable");
                return;
            }

            const auto pedExists = Natives::DoesEntityExist(*ped);
            if (!pedExists || !*pedExists)
            {
                Finish(false, std::move(state), "Local player ped does not exist");
                return;
            }

            Entity target = *ped;
            bool inVehicle = false;
            const auto insideVehicle = Natives::IsPedInAnyVehicle(*ped, true);
            if (insideVehicle && *insideVehicle)
            {
                const auto vehicle = Natives::GetVehiclePedIsIn(*ped, true);
                if (!vehicle || *vehicle == 0)
                {
                    Finish(false, std::move(state), "Current vehicle is unavailable");
                    return;
                }

                const auto vehicleExists = Natives::DoesEntityExist(*vehicle);
                if (!vehicleExists || !*vehicleExists)
                {
                    Finish(false, std::move(state), "Current vehicle no longer exists");
                    return;
                }

                target = *vehicle;
                inVehicle = true;
                auto haveControl = Native::NativeInvoker::Invoke<std::int32_t>(
                    Native::NativeId::NetworkHasControlOfEntity,
                    target);
                if (!haveControl || *haveControl == 0)
                {
                    static_cast<void>(Native::NativeInvoker::Invoke<std::int32_t>(
                        Native::NativeId::NetworkRequestControlOfEntity,
                        target));
                    haveControl = Native::NativeInvoker::Invoke<std::int32_t>(
                        Native::NativeId::NetworkHasControlOfEntity,
                        target);
                }
                if (!haveControl || *haveControl == 0)
                {
                    Finish(false, std::move(state), "Network control of the current vehicle is unavailable");
                    return;
                }
            }

            ExoticExportSnapshot finalState;
            std::string error;
            if (!ConfirmLiveTarget(state, finalState, error))
            {
                Finish(false, std::move(finalState), std::move(error));
                return;
            }

            const float destinationX = finalState.x + 2.5f;
            const float destinationY = finalState.y;
            const float destinationZ = finalState.z + (inVehicle ? 1.0f : 0.5f);

            static_cast<void>(Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetEntityVelocity,
                target,
                0.0f,
                0.0f,
                0.0f));

            if (!Native::NativeInvoker::InvokeVoid(
                    Native::NativeId::SetEntityCoordsNoOffset,
                    target,
                    destinationX,
                    destinationY,
                    destinationZ,
                    std::int32_t{1},
                    std::int32_t{1},
                    std::int32_t{1}))
            {
                Finish(false, std::move(finalState), "Failed to invoke the Exotic Export teleport native");
                return;
            }

            if (inVehicle)
                static_cast<void>(Natives::SetVehicleOnGroundProperly(target, 5.0f));

            const auto actual = VehicleNatives::GetEntityCoords(target, false);
            if (!actual)
            {
                Finish(false, std::move(finalState), "Unable to verify the Exotic Export teleport destination");
                return;
            }

            const float dx = actual->x - destinationX;
            const float dy = actual->y - destinationY;
            const float dz = actual->z - destinationZ;
            if ((dx * dx + dy * dy) > 36.0f || std::fabs(dz) > 15.0f)
            {
                Finish(false, std::move(finalState), "Exotic Export teleport did not reach the verified destination");
                return;
            }

            TUTONES_LOG_INFO(
                "heist.exotic_export",
                std::string("Teleported to active Exotic Export at ")
                    + std::to_string(finalState.x) + ", "
                    + std::to_string(finalState.y) + ", "
                    + std::to_string(finalState.z));
            Finish(true, std::move(finalState), "Teleported beside the active Exotic Export vehicle");
        }

        void Finish(bool success, ExoticExportSnapshot state, std::string message) noexcept
        {
            state.pending = false;
            state.haveResult = true;
            state.lastSucceeded = success;
            state.message = std::move(message);

            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot = std::move(state);
            }
            m_Pending.store(false, std::memory_order_release);
        }

        std::atomic<bool> m_Pending{false};
        mutable std::mutex m_Mutex;
        ExoticExportSnapshot m_Snapshot{};
    };
}
