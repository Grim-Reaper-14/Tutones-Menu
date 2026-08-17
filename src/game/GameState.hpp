#pragma once

#include "Natives.hpp"

#include <cstdint>
#include <mutex>

namespace Tutones::Game
{
    struct GameSnapshot final
    {
        bool nativeRuntimeReady{};
        Ped playerPed{};
        bool inVehicle{};
        Vehicle vehicle{};
        Hash vehicleModel{};
        std::uint64_t sequence{};
    };

    class GameState final
    {
    public:
        static GameState& Get() noexcept;

        void Tick() noexcept;
        void Reset() noexcept;
        [[nodiscard]] GameSnapshot Snapshot() const noexcept;

    private:
        GameState() = default;
        ~GameState() = default;
        GameState(const GameState&) = delete;
        GameState& operator=(const GameState&) = delete;

        mutable std::mutex m_Mutex;
        GameSnapshot m_Snapshot{};
        std::uint64_t m_LastPollMs{};
        bool m_LoggedReadFailure{};
    };
}
