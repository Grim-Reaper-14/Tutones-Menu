#pragma once

#include "../../game/GamePointers.hpp"
#include "../../game/NetworkPlayerNatives.hpp"
#include "../../game/PlayerNatives.hpp"
#include "../../game/VehicleNatives.hpp"
#include "../../game/native/NativeCallContext.hpp"
#include "../../game/native/NativeHandlerValidation.hpp"
#include "../../game/native/NativeInvoker.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::NetworkFeatures
{
    enum class NetworkPlayerToolAction : std::uint8_t
    {
        None,
        Spectate,
        StopSpectating,
        TeleportToPlayer,
        SetWaypoint,
    };

    struct NetworkPlayerToolsSnapshot final
    {
        bool pending{};
        bool spectating{};
        int spectatingPlayer{-1};
        NetworkPlayerToolAction lastAction{NetworkPlayerToolAction::None};
        bool haveResult{};
        bool lastSucceeded{};
        std::string message{"Ready"};
    };

    class NetworkPlayerToolsRuntime final
    {
    public:
        static NetworkPlayerToolsRuntime& Get() noexcept
        {
            static NetworkPlayerToolsRuntime instance;
            return instance;
        }

        [[nodiscard]] bool QueueSpectate(int playerId)
        {
            return QueueAction(NetworkPlayerToolAction::Spectate, playerId);
        }

        [[nodiscard]] bool QueueStopSpectating()
        {
            return QueueAction(NetworkPlayerToolAction::StopSpectating, -1);
        }

        [[nodiscard]] bool QueueTeleportToPlayer(int playerId)
        {
            return QueueAction(NetworkPlayerToolAction::TeleportToPlayer, playerId);
        }

        [[nodiscard]] bool QueueWaypointToPlayer(int playerId)
        {
            return QueueAction(NetworkPlayerToolAction::SetWaypoint, playerId);
        }

        [[nodiscard]] NetworkPlayerToolsSnapshot Snapshot() const
        {
            NetworkPlayerToolsSnapshot out;
            out.pending = m_Pending.load(std::memory_order_acquire);
            out.spectatingPlayer = m_SpectatingPlayer.load(std::memory_order_acquire);
            out.spectating = out.spectatingPlayer >= 0;

            std::scoped_lock lock(m_Mutex);
            out.lastAction = m_LastAction;
            out.haveResult = m_HaveResult;
            out.lastSucceeded = m_LastSucceeded;
            out.message = m_Message;
            return out;
        }

    private:
        struct NativeProgram final
        {
            std::byte pad00[0x2C]{};
            std::uint32_t nativeCount{};
            std::byte pad30[0x10]{};
            Native::NativeHandler* nativeEntrypoints{};
            std::byte pad48[0x38]{};
        };

        static_assert(offsetof(NativeProgram, nativeCount) == 0x2C);
        static_assert(offsetof(NativeProgram, nativeEntrypoints) == 0x40);
        static_assert(sizeof(NativeProgram) == 0x80);

        enum HandlerIndex : std::size_t
        {
            SetInSpectatorMode,
            SetNewWaypoint,
            HandlerCount,
        };

        // GTA V Enhanced 1.73 / b1158.13 targets verified against the current
        // YimMenuV2 Enhanced crossmap.
        static constexpr std::array<std::uint64_t, HandlerCount> HandlerHashes{
            0xF11FEC6A04FD7226ull, // NETWORK_SET_IN_SPECTATOR_MODE
            0xF8D9A55D2F2892CCull, // SET_NEW_WAYPOINT
        };

        static constexpr float Pi = 3.14159265358979323846f;
        static constexpr float TeleportBehindDistance = 3.0f;
        static constexpr float TeleportHeightOffset = 0.6f;

        NetworkPlayerToolsRuntime() = default;
        ~NetworkPlayerToolsRuntime() = default;
        NetworkPlayerToolsRuntime(const NetworkPlayerToolsRuntime&) = delete;
        NetworkPlayerToolsRuntime& operator=(const NetworkPlayerToolsRuntime&) = delete;

        [[nodiscard]] bool ResolveHandlers() noexcept
        {
            bool ready = true;
            for (const auto handler : m_Handlers)
                ready = ready && handler != nullptr;
            if (ready)
                return true;

            if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread())
                return false;

            const auto initNativeTables = GamePointers::Get().InitNativeTables();
            if (!initNativeTables)
                return false;

            auto slots = HandlerHashes;
            NativeProgram program{};
            program.nativeCount = static_cast<std::uint32_t>(slots.size());
            program.nativeEntrypoints = reinterpret_cast<Native::NativeHandler*>(slots.data());
            initNativeTables(&program);

            return Native::AssignValidatedHandlers(slots, m_Handlers);
        }

        [[nodiscard]] bool SessionActive() const noexcept
        {
            const bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            return sessionStarted && *sessionStarted;
        }

        [[nodiscard]] std::optional<Ped> ResolveTargetPed(int playerId) noexcept
        {
            if (playerId < 0 || playerId >= 32 || !SessionActive())
                return std::nullopt;

            const auto active = NetworkPlayerNatives::IsPlayerActive(playerId);
            if (!active || !*active)
                return std::nullopt;

            const auto ped = NetworkPlayerNatives::GetPlayerPedScriptIndex(playerId);
            if (!ped || *ped == 0)
                return std::nullopt;

            const auto exists = Natives::DoesEntityExist(*ped);
            if (!exists || !*exists)
                return std::nullopt;

            return *ped;
        }

        [[nodiscard]] std::optional<Native::NativeVector3> EntityCoords(Entity entity) const noexcept
        {
            if (entity == 0)
                return std::nullopt;

            return Native::NativeInvoker::Invoke<Native::NativeVector3>(
                Native::NativeId::GetEntityCoords,
                entity,
                std::int32_t{0});
        }

        [[nodiscard]] Entity ResolveLocalTeleportEntity(bool& inVehicle) const noexcept
        {
            inVehicle = false;
            const auto ped = PlayerNatives::PlayerPedId();
            if (!ped || *ped == 0)
                return 0;

            const auto pedExists = Natives::DoesEntityExist(*ped);
            if (!pedExists || !*pedExists)
                return 0;

            const auto driving = Natives::IsPedInAnyVehicle(*ped, false);
            if (driving && *driving)
            {
                const auto vehicle = Natives::GetVehiclePedIsIn(*ped, false);
                if (vehicle && *vehicle != 0)
                {
                    const auto vehicleExists = Natives::DoesEntityExist(*vehicle);
                    if (vehicleExists && *vehicleExists)
                    {
                        inVehicle = true;
                        return *vehicle;
                    }
                }
            }

            return *ped;
        }

        [[nodiscard]] bool SetSpectatorMode(bool enabled, Ped targetPed) noexcept
        {
            if (targetPed == 0 || !ResolveHandlers())
                return false;

            Native::CallContext context;
            if (!context.PushArg(std::int32_t{enabled ? 1 : 0}) || !context.PushArg(targetPed))
                return false;

            m_Handlers[SetInSpectatorMode](&context);
            return true;
        }

        [[nodiscard]] bool SetWaypoint(float x, float y) noexcept
        {
            if (!std::isfinite(x) || !std::isfinite(y) || !ResolveHandlers())
                return false;

            Native::CallContext context;
            if (!context.PushArg(x) || !context.PushArg(y))
                return false;

            m_Handlers[SetNewWaypoint](&context);
            return true;
        }

        [[nodiscard]] bool QueueAction(NetworkPlayerToolAction action, int playerId)
        {
            if (!Native::NativeRegistry::Get().IsReady())
                return false;

            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            {
                std::scoped_lock lock(m_Mutex);
                m_HaveResult = false;
                m_LastSucceeded = false;
                m_LastAction = action;
                m_Message = "Player action queued";
            }

            if (Runtime::GameRuntime::Get().Enqueue([this, action, playerId] {
                    ExecuteOnGameThread(action, playerId);
                }))
            {
                return true;
            }

            m_Pending.store(false, std::memory_order_release);
            SetResult(action, false, "GTA script-thread queue is unavailable");
            return false;
        }

        void ExecuteOnGameThread(NetworkPlayerToolAction action, int playerId) noexcept
        {
            if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread() || !ResolveHandlers())
            {
                Finish(action, false, "Enhanced player-tool natives are unavailable");
                return;
            }

            switch (action)
            {
            case NetworkPlayerToolAction::Spectate:
                ExecuteSpectate(playerId);
                return;
            case NetworkPlayerToolAction::StopSpectating:
                ExecuteStopSpectating();
                return;
            case NetworkPlayerToolAction::TeleportToPlayer:
                ExecuteTeleport(playerId);
                return;
            case NetworkPlayerToolAction::SetWaypoint:
                ExecuteWaypoint(playerId);
                return;
            case NetworkPlayerToolAction::None:
            default:
                Finish(action, false, "No player action was requested");
                return;
            }
        }

        void ExecuteSpectate(int playerId) noexcept
        {
            const auto localPlayer = PlayerNatives::PlayerId();
            if (localPlayer && *localPlayer == playerId)
            {
                Finish(NetworkPlayerToolAction::Spectate, false, "Cannot spectate the local player");
                return;
            }

            const auto targetPed = ResolveTargetPed(playerId);
            if (!targetPed)
            {
                Finish(NetworkPlayerToolAction::Spectate, false, "Selected player is no longer active or streamed");
                return;
            }

            const int previousPlayer = m_SpectatingPlayer.load(std::memory_order_acquire);
            if (previousPlayer >= 0 && previousPlayer != playerId)
            {
                const auto previousPed = ResolveTargetPed(previousPlayer);
                const auto fallbackPed = PlayerNatives::PlayerPedId();
                const Ped disablePed = previousPed ? *previousPed : (fallbackPed ? *fallbackPed : 0);
                if (disablePed != 0)
                    static_cast<void>(SetSpectatorMode(false, disablePed));
            }

            if (!SetSpectatorMode(true, *targetPed))
            {
                Finish(NetworkPlayerToolAction::Spectate, false, "Failed to enable spectator mode");
                return;
            }

            m_SpectatingPlayer.store(playerId, std::memory_order_release);
            Finish(NetworkPlayerToolAction::Spectate, true, "Spectating selected player");
        }

        void ExecuteStopSpectating() noexcept
        {
            const int previousPlayer = m_SpectatingPlayer.load(std::memory_order_acquire);
            if (previousPlayer < 0)
            {
                Finish(NetworkPlayerToolAction::StopSpectating, true, "Spectator mode is already off");
                return;
            }

            const auto previousPed = ResolveTargetPed(previousPlayer);
            const auto localPed = PlayerNatives::PlayerPedId();
            const Ped disablePed = previousPed ? *previousPed : (localPed ? *localPed : 0);
            if (disablePed == 0 || !SetSpectatorMode(false, disablePed))
            {
                Finish(NetworkPlayerToolAction::StopSpectating, false, "Failed to disable spectator mode");
                return;
            }

            m_SpectatingPlayer.store(-1, std::memory_order_release);
            Finish(NetworkPlayerToolAction::StopSpectating, true, "Spectator mode disabled");
        }

        void ExecuteWaypoint(int playerId) noexcept
        {
            const auto targetPed = ResolveTargetPed(playerId);
            if (!targetPed)
            {
                Finish(NetworkPlayerToolAction::SetWaypoint, false, "Selected player is no longer active or streamed");
                return;
            }

            const auto coords = EntityCoords(*targetPed);
            if (!coords || !std::isfinite(coords->x) || !std::isfinite(coords->y))
            {
                Finish(NetworkPlayerToolAction::SetWaypoint, false, "Selected player coordinates are unavailable");
                return;
            }

            if (!SetWaypoint(coords->x, coords->y))
            {
                Finish(NetworkPlayerToolAction::SetWaypoint, false, "Failed to set player waypoint");
                return;
            }

            Finish(NetworkPlayerToolAction::SetWaypoint, true, "Waypoint set to selected player");
        }

        void ExecuteTeleport(int playerId) noexcept
        {
            const auto localPlayer = PlayerNatives::PlayerId();
            if (localPlayer && *localPlayer == playerId)
            {
                Finish(NetworkPlayerToolAction::TeleportToPlayer, false, "Already targeting the local player");
                return;
            }

            const auto targetPed = ResolveTargetPed(playerId);
            if (!targetPed)
            {
                Finish(NetworkPlayerToolAction::TeleportToPlayer, false, "Selected player is no longer active or streamed");
                return;
            }

            const auto targetCoords = EntityCoords(*targetPed);
            if (!targetCoords
                || !std::isfinite(targetCoords->x)
                || !std::isfinite(targetCoords->y)
                || !std::isfinite(targetCoords->z))
            {
                Finish(NetworkPlayerToolAction::TeleportToPlayer, false, "Selected player coordinates are unavailable");
                return;
            }

            float heading{};
            if (const auto targetHeading = Native::NativeInvoker::Invoke<float>(
                    Native::NativeId::GetEntityHeading,
                    *targetPed))
            {
                heading = *targetHeading;
            }

            const float radians = heading * (Pi / 180.0f);
            Native::NativeVector3 destination{
                targetCoords->x - std::sin(radians) * TeleportBehindDistance,
                targetCoords->y + std::cos(radians) * TeleportBehindDistance,
                targetCoords->z + TeleportHeightOffset};

            bool localInVehicle{};
            const Entity localEntity = ResolveLocalTeleportEntity(localInVehicle);
            if (localEntity == 0)
            {
                Finish(NetworkPlayerToolAction::TeleportToPlayer, false, "Local player/vehicle is unavailable");
                return;
            }

            static_cast<void>(Native::NativeInvoker::InvokeVoid(
                Native::NativeId::RequestCollisionAtCoord,
                destination.x,
                destination.y,
                destination.z));

            const bool moved = Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetEntityCoordsNoOffset,
                localEntity,
                destination.x,
                destination.y,
                destination.z,
                std::int32_t{1},
                std::int32_t{1},
                std::int32_t{1});

            if (!moved)
            {
                Finish(NetworkPlayerToolAction::TeleportToPlayer, false, "Failed to move the local player/vehicle");
                return;
            }

            static_cast<void>(Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetEntityHeading,
                localEntity,
                heading));

            if (localInVehicle)
                static_cast<void>(Natives::SetVehicleOnGroundProperly(localEntity, 5.0f));

            Finish(NetworkPlayerToolAction::TeleportToPlayer, true, "Teleported beside selected player");
        }

        void Finish(NetworkPlayerToolAction action, bool success, std::string message) noexcept
        {
            m_Pending.store(false, std::memory_order_release);
            SetResult(action, success, std::move(message));
        }

        void SetResult(NetworkPlayerToolAction action, bool success, std::string message) noexcept
        {
            std::scoped_lock lock(m_Mutex);
            m_LastAction = action;
            m_HaveResult = true;
            m_LastSucceeded = success;
            m_Message = std::move(message);
        }

        std::array<Native::NativeHandler, HandlerCount> m_Handlers{};
        std::atomic<bool> m_Pending{false};
        std::atomic<int> m_SpectatingPlayer{-1};

        mutable std::mutex m_Mutex;
        NetworkPlayerToolAction m_LastAction{NetworkPlayerToolAction::None};
        bool m_HaveResult{};
        bool m_LastSucceeded{};
        std::string m_Message{"Ready"};
    };
}
