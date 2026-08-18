#include "GameState.hpp"

#include "../core/logging/Logger.hpp"
#include "native/NativeRegistry.hpp"
#include "VehicleNatives.hpp"

#include <Windows.h>

#include <iomanip>
#include <sstream>
#include <string>

namespace Tutones::Game
{
    GameState& GameState::Get() noexcept
    {
        static GameState instance;
        return instance;
    }

    void GameState::Tick() noexcept
    {
        if (!Native::NativeRegistry::Get().IsReady())
            return;

        const auto now = ::GetTickCount64();
        if (m_LastPollMs != 0 && (now - m_LastPollMs) < 250)
            return;
        m_LastPollMs = now;

        const auto ped = Natives::PlayerPedId();
        if (!ped || *ped == 0)
        {
            if (!m_LoggedReadFailure)
            {
                m_LoggedReadFailure = true;
                TUTONES_LOG_WARN("game.state", "PLAYER_PED_ID native call did not return a valid player ped");
            }
            return;
        }

        const auto pedExists = Natives::DoesEntityExist(*ped);
        if (!pedExists || !*pedExists)
            return;

        Vehicle vehicle{};
        auto currentVehicle = VehicleNatives::GetVehiclePedIsUsing(*ped);
        if (currentVehicle && *currentVehicle != 0)
        {
            const auto exists = Natives::DoesEntityExist(*currentVehicle);
            if (exists && *exists)
                vehicle = *currentVehicle;
        }

        if (vehicle == 0)
        {
            currentVehicle = Natives::GetVehiclePedIsIn(*ped, false);
            if (currentVehicle && *currentVehicle != 0)
            {
                const auto exists = Natives::DoesEntityExist(*currentVehicle);
                if (exists && *exists)
                    vehicle = *currentVehicle;
            }
        }

        const bool inVehicle = vehicle != 0;
        Hash vehicleModel{};
        if (inVehicle)
        {
            const auto model = Natives::GetEntityModel(vehicle);
            if (model)
                vehicleModel = *model;
            m_LoggedVehicleReadFailure = false;
        }
        else if (!m_LoggedVehicleReadFailure)
        {
            m_LoggedVehicleReadFailure = true;
            TUTONES_LOG_DEBUG("game.vehicle", "No active player vehicle handle resolved this poll");
        }

        GameSnapshot previous{};
        GameSnapshot current{};
        {
            std::scoped_lock lock(m_Mutex);
            previous = m_Snapshot;
            current.nativeRuntimeReady = true;
            current.playerPed = *ped;
            current.inVehicle = inVehicle;
            current.vehicle = vehicle;
            current.vehicleModel = vehicleModel;
            current.sequence = m_Snapshot.sequence + 1;
            m_Snapshot = current;
        }

        m_LoggedReadFailure = false;

        if (!previous.nativeRuntimeReady)
        {
            std::string message("Native-backed game state online; player ped=");
            message += std::to_string(current.playerPed);
            TUTONES_LOG_INFO("game.state", message);
        }

        if (previous.playerPed != 0 && previous.playerPed != current.playerPed)
        {
            std::string message("Player ped handle changed: ");
            message += std::to_string(previous.playerPed);
            message += " -> ";
            message += std::to_string(current.playerPed);
            TUTONES_LOG_INFO("game.state", message);
        }

        if (previous.inVehicle != current.inVehicle || previous.vehicle != current.vehicle)
        {
            if (current.inVehicle && current.vehicle != 0)
            {
                std::ostringstream stream;
                stream << "Player vehicle detected; handle=" << current.vehicle
                       << ", model=0x" << std::hex << std::uppercase << current.vehicleModel;
                TUTONES_LOG_INFO("game.vehicle", stream.str());
            }
            else if (previous.inVehicle)
            {
                TUTONES_LOG_INFO("game.vehicle", "Player left the tracked vehicle");
            }
        }
        else if (current.inVehicle && current.vehicleModel != previous.vehicleModel)
        {
            std::ostringstream stream;
            stream << "Tracked vehicle model changed to 0x" << std::hex << std::uppercase << current.vehicleModel;
            TUTONES_LOG_INFO("game.vehicle", stream.str());
        }
    }

    void GameState::Reset() noexcept
    {
        std::scoped_lock lock(m_Mutex);
        m_Snapshot = {};
        m_LastPollMs = 0;
        m_LoggedReadFailure = false;
        m_LoggedVehicleReadFailure = false;
    }

    GameSnapshot GameState::Snapshot() const noexcept
    {
        std::scoped_lock lock(m_Mutex);
        return m_Snapshot;
    }
}
