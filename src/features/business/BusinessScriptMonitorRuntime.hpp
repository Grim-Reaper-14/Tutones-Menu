#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::Business
{
    namespace BusinessScriptMonitorDetail
    {
        [[nodiscard]] constexpr std::uint32_t Joaat(const char* text) noexcept
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
    }

    struct BusinessScriptMonitorSnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool hangarRunning{};
        bool vehicleCargoRunning{};
        std::string message{"Ready"};
    };

    class BusinessScriptMonitorRuntime final
    {
    public:
        static constexpr std::uint32_t HangarScriptHash = BusinessScriptMonitorDetail::Joaat("gb_smuggler");
        static constexpr std::uint32_t VehicleCargoScriptHash = BusinessScriptMonitorDetail::Joaat("gb_vehicle_export");

        static BusinessScriptMonitorRuntime& Get() noexcept
        {
            static BusinessScriptMonitorRuntime instance;
            return instance;
        }

        bool QueueRefresh()
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            SetPending("Refreshing Enhanced business mission scripts");
            if (Runtime::GameRuntime::Get().Enqueue([this] {
                bool* sessionStarted = GamePointers::Get().IsSessionStarted();
                if (!sessionStarted || !*sessionStarted)
                    return Finish(false, false, false, "Join GTA Online before checking business mission scripts");

                auto& scripts = Script::ScriptRuntime::Get();
                if (!scripts.IsReady())
                    return Finish(false, false, false, "Shared Enhanced script runtime is unavailable");

                const auto* hangar = scripts.FindThread(HangarScriptHash);
                const auto* vehicleCargo = scripts.FindThread(VehicleCargoScriptHash);
                const bool hangarRunning = hangar && hangar->stack;
                const bool vehicleCargoRunning = vehicleCargo && vehicleCargo->stack;

                TUTONES_LOG_DEBUG(
                    "business.scripts",
                    std::string("Business mission scripts: gb_smuggler=") + (hangarRunning ? "running" : "idle")
                        + " gb_vehicle_export=" + (vehicleCargoRunning ? "running" : "idle"));

                Finish(true, hangarRunning, vehicleCargoRunning, "Enhanced business script state refreshed");
            }))
            {
                return true;
            }

            Finish(false, false, false, "Game-thread queue unavailable");
            return false;
        }

        [[nodiscard]] BusinessScriptMonitorSnapshot Snapshot() const
        {
            BusinessScriptMonitorSnapshot snapshot;
            snapshot.pending = m_Pending.load(std::memory_order_acquire);
            std::scoped_lock lock(m_Mutex);
            snapshot.haveResult = m_HaveResult;
            snapshot.lastSucceeded = m_LastSucceeded;
            snapshot.hangarRunning = m_HangarRunning;
            snapshot.vehicleCargoRunning = m_VehicleCargoRunning;
            snapshot.message = m_Message;
            return snapshot;
        }

    private:
        BusinessScriptMonitorRuntime() = default;

        void SetPending(std::string message)
        {
            std::scoped_lock lock(m_Mutex);
            m_HaveResult = false;
            m_LastSucceeded = false;
            m_Message = std::move(message);
        }

        void Finish(bool success, bool hangarRunning, bool vehicleCargoRunning, std::string message)
        {
            {
                std::scoped_lock lock(m_Mutex);
                m_HaveResult = true;
                m_LastSucceeded = success;
                m_HangarRunning = hangarRunning;
                m_VehicleCargoRunning = vehicleCargoRunning;
                m_Message = std::move(message);
            }
            m_Pending.store(false, std::memory_order_release);
        }

        std::atomic<bool> m_Pending{false};
        mutable std::mutex m_Mutex;
        bool m_HaveResult{};
        bool m_LastSucceeded{};
        bool m_HangarRunning{};
        bool m_VehicleCargoRunning{};
        std::string m_Message{"Ready"};
    };
}
