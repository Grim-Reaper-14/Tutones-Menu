#pragma once

#include "../../game/MiscNatives.hpp"
#include "../../game/PlayerNatives.hpp"
#include "../../game/WorldNatives.hpp"
#include "../../game/native/NativeInvoker.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

namespace Tutones::Game::World
{
    struct WorldSnapshot final
    {
        float pedDensity{1.0f};
        float scenarioPedDensity{1.0f};
        float vehicleDensity{1.0f};
        float randomVehicleDensity{1.0f};
        float parkedVehicleDensity{1.0f};
        bool densityLoopRunning{};
        bool freezeClock{};
        bool blackout{};
        int clockHour{-1};
        int clockMinute{-1};
        bool actionPending{};
        bool haveResult{};
        bool lastSucceeded{};
        std::string message{"Ready"};
    };

    class WorldRuntime final
    {
    public:
        static WorldRuntime& Get() noexcept
        {
            static WorldRuntime instance;
            return instance;
        }

        [[nodiscard]] WorldSnapshot Snapshot() const
        {
            WorldSnapshot out;
            out.pedDensity = m_PedDensity.load(std::memory_order_acquire);
            out.scenarioPedDensity = m_ScenarioPedDensity.load(std::memory_order_acquire);
            out.vehicleDensity = m_VehicleDensity.load(std::memory_order_acquire);
            out.randomVehicleDensity = m_RandomVehicleDensity.load(std::memory_order_acquire);
            out.parkedVehicleDensity = m_ParkedVehicleDensity.load(std::memory_order_acquire);
            out.densityLoopRunning = m_DensityLoopRunning.load(std::memory_order_acquire);
            out.freezeClock = m_FreezeClock.load(std::memory_order_acquire);
            out.blackout = m_Blackout.load(std::memory_order_acquire);
            out.clockHour = m_ClockHour.load(std::memory_order_acquire);
            out.clockMinute = m_ClockMinute.load(std::memory_order_acquire);
            out.actionPending = m_ActionPending.load(std::memory_order_acquire);

            std::scoped_lock lock(m_StatusMutex);
            out.haveResult = m_HaveResult;
            out.lastSucceeded = m_LastSucceeded;
            out.message = m_Message;
            return out;
        }

        void SetPedDensity(float value) noexcept
        {
            m_PedDensity.store(ClampDensity(value), std::memory_order_release);
            EnsureDensityLoop();
        }

        void SetScenarioPedDensity(float value) noexcept
        {
            m_ScenarioPedDensity.store(ClampDensity(value), std::memory_order_release);
            EnsureDensityLoop();
        }

        void SetVehicleDensity(float value) noexcept
        {
            m_VehicleDensity.store(ClampDensity(value), std::memory_order_release);
            EnsureDensityLoop();
        }

        void SetRandomVehicleDensity(float value) noexcept
        {
            m_RandomVehicleDensity.store(ClampDensity(value), std::memory_order_release);
            EnsureDensityLoop();
        }

        void SetParkedVehicleDensity(float value) noexcept
        {
            m_ParkedVehicleDensity.store(ClampDensity(value), std::memory_order_release);
            EnsureDensityLoop();
        }

        void ResetDensity() noexcept
        {
            m_PedDensity.store(1.0f, std::memory_order_release);
            m_ScenarioPedDensity.store(1.0f, std::memory_order_release);
            m_VehicleDensity.store(1.0f, std::memory_order_release);
            m_RandomVehicleDensity.store(1.0f, std::memory_order_release);
            m_ParkedVehicleDensity.store(1.0f, std::memory_order_release);
        }

        bool QueueSetTime(int hour, int minute)
        {
            hour = std::clamp(hour, 0, 23);
            minute = std::clamp(minute, 0, 59);
            return QueueAction("Set clock time", [hour, minute] {
                return MiscNatives::SetClockTime(hour, minute, 0);
            });
        }

        bool QueueFreezeClock(bool enabled)
        {
            return QueueAction(enabled ? "Freeze clock" : "Resume clock", [this, enabled] {
                const bool success = MiscNatives::PauseClock(enabled);
                if (success)
                    m_FreezeClock.store(enabled, std::memory_order_release);
                return success;
            });
        }

        bool QueueWeather(std::string weather)
        {
            if (weather.empty())
                return false;
            return QueueAction("Apply weather", [weather = std::move(weather)] {
                return MiscNatives::SetWeatherTypeNowPersist(weather.c_str());
            });
        }

        bool QueueBlackout(bool enabled)
        {
            return QueueAction(enabled ? "Enable blackout" : "Disable blackout", [this, enabled] {
                const bool success = MiscNatives::SetArtificialLightsState(enabled);
                if (success)
                    m_Blackout.store(enabled, std::memory_order_release);
                return success;
            });
        }

        bool QueueClearPeds(float radius)
        {
            return QueueClearArea("Clear nearby peds", radius, [](const Native::NativeVector3& coords, float r) {
                return WorldNatives::ClearAreaOfPeds(coords.x, coords.y, coords.z, r, 0);
            });
        }

        bool QueueClearVehicles(float radius)
        {
            return QueueClearArea("Clear nearby ambient vehicles", radius, [](const Native::NativeVector3& coords, float r) {
                return WorldNatives::ClearAreaOfVehicles(coords.x, coords.y, coords.z, r);
            });
        }

        bool QueueClearObjects(float radius)
        {
            return QueueClearArea("Clear nearby objects", radius, [](const Native::NativeVector3& coords, float r) {
                return WorldNatives::ClearAreaOfObjects(coords.x, coords.y, coords.z, r, 0);
            });
        }

        bool QueueClearAmbient(float radius)
        {
            radius = std::clamp(radius, 5.0f, 250.0f);
            return QueueAction("Clear nearby ambient world", [radius] {
                const auto coords = LocalPlayerCoords();
                if (!coords)
                    return false;

                bool success = true;
                success = WorldNatives::ClearAreaOfPeds(coords->x, coords->y, coords->z, radius, 0) && success;
                success = WorldNatives::ClearAreaOfObjects(coords->x, coords->y, coords->z, radius, 0) && success;
                success = WorldNatives::ClearAreaOfVehicles(coords->x, coords->y, coords->z, radius) && success;
                return success;
            });
        }

        void RequestClockSample() noexcept
        {
            bool expected = false;
            if (!m_ClockSamplePending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return;

            if (!Runtime::GameRuntime::Get().Enqueue([this] {
                    if (const auto hour = MiscNatives::GetClockHours())
                        m_ClockHour.store(*hour, std::memory_order_release);
                    if (const auto minute = MiscNatives::GetClockMinutes())
                        m_ClockMinute.store(*minute, std::memory_order_release);
                    m_ClockSamplePending.store(false, std::memory_order_release);
                }))
            {
                m_ClockSamplePending.store(false, std::memory_order_release);
            }
        }

    private:
        WorldRuntime() = default;

        [[nodiscard]] static float ClampDensity(float value) noexcept
        {
            if (!std::isfinite(value))
                return 1.0f;
            return std::clamp(value, 0.0f, 1.0f);
        }

        [[nodiscard]] bool HasDensityOverride() const noexcept
        {
            constexpr float epsilon = 0.001f;
            return std::fabs(m_PedDensity.load(std::memory_order_acquire) - 1.0f) > epsilon
                || std::fabs(m_ScenarioPedDensity.load(std::memory_order_acquire) - 1.0f) > epsilon
                || std::fabs(m_VehicleDensity.load(std::memory_order_acquire) - 1.0f) > epsilon
                || std::fabs(m_RandomVehicleDensity.load(std::memory_order_acquire) - 1.0f) > epsilon
                || std::fabs(m_ParkedVehicleDensity.load(std::memory_order_acquire) - 1.0f) > epsilon;
        }

        void EnsureDensityLoop() noexcept
        {
            if (!HasDensityOverride())
                return;

            bool expected = false;
            if (!m_DensityLoopRunning.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return;

            if (!Runtime::GameRuntime::Get().Enqueue([this] { DensityTick(); }))
                m_DensityLoopRunning.store(false, std::memory_order_release);
        }

        void DensityTick() noexcept
        {
            if (!HasDensityOverride())
            {
                m_DensityLoopRunning.store(false, std::memory_order_release);
                return;
            }

            const float peds = m_PedDensity.load(std::memory_order_acquire);
            const float scenario = m_ScenarioPedDensity.load(std::memory_order_acquire);
            const float vehicles = m_VehicleDensity.load(std::memory_order_acquire);
            const float randomVehicles = m_RandomVehicleDensity.load(std::memory_order_acquire);
            const float parked = m_ParkedVehicleDensity.load(std::memory_order_acquire);

            bool success = true;
            success = WorldNatives::SetPedDensity(peds) && success;
            success = WorldNatives::SetScenarioPedDensity(scenario, scenario) && success;
            success = WorldNatives::SetVehicleDensity(vehicles) && success;
            success = WorldNatives::SetRandomVehicleDensity(randomVehicles) && success;
            success = WorldNatives::SetParkedVehicleDensity(parked) && success;

            if (!success)
            {
                m_DensityLoopRunning.store(false, std::memory_order_release);
                SetResult(false, "Population density natives became unavailable");
                return;
            }

            if (!Runtime::GameRuntime::Get().Enqueue([this] { DensityTick(); }))
                m_DensityLoopRunning.store(false, std::memory_order_release);
        }

        template <typename Fn>
        bool QueueAction(std::string label, Fn&& action)
        {
            bool expected = false;
            if (!m_ActionPending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            {
                std::scoped_lock lock(m_StatusMutex);
                m_HaveResult = false;
                m_LastSucceeded = false;
                m_Message = label + " queued";
            }

            const std::string failureLabel = label;
            std::function<bool()> fn(std::forward<Fn>(action));
            if (!Runtime::GameRuntime::Get().Enqueue(
                    [this, label = std::move(label), fn = std::move(fn)]() mutable {
                        const bool success = fn && fn();
                        SetResult(success, success ? label + " complete" : label + " failed");
                    }))
            {
                m_ActionPending.store(false, std::memory_order_release);
                SetResult(false, failureLabel + " queue unavailable");
                return false;
            }
            return true;
        }

        template <typename Fn>
        bool QueueClearArea(const char* label, float radius, Fn&& clear)
        {
            radius = std::clamp(radius, 5.0f, 250.0f);
            return QueueAction(label ? label : "Clear area", [radius, fn = std::forward<Fn>(clear)]() mutable {
                const auto coords = LocalPlayerCoords();
                return coords && fn(*coords, radius);
            });
        }

        [[nodiscard]] static std::optional<Native::NativeVector3> LocalPlayerCoords() noexcept
        {
            const auto ped = PlayerNatives::PlayerPedId();
            if (!ped || *ped == 0)
                return std::nullopt;

            const auto coords = Native::NativeInvoker::Invoke<Native::NativeVector3>(
                Native::NativeId::GetEntityCoords,
                *ped,
                std::int32_t{0});
            if (!coords || !std::isfinite(coords->x) || !std::isfinite(coords->y) || !std::isfinite(coords->z))
                return std::nullopt;
            return coords;
        }

        void SetResult(bool success, std::string message) noexcept
        {
            m_ActionPending.store(false, std::memory_order_release);
            std::scoped_lock lock(m_StatusMutex);
            m_HaveResult = true;
            m_LastSucceeded = success;
            m_Message = std::move(message);
        }

        std::atomic<float> m_PedDensity{1.0f};
        std::atomic<float> m_ScenarioPedDensity{1.0f};
        std::atomic<float> m_VehicleDensity{1.0f};
        std::atomic<float> m_RandomVehicleDensity{1.0f};
        std::atomic<float> m_ParkedVehicleDensity{1.0f};
        std::atomic<bool> m_DensityLoopRunning{false};
        std::atomic<bool> m_FreezeClock{false};
        std::atomic<bool> m_Blackout{false};
        std::atomic<int> m_ClockHour{-1};
        std::atomic<int> m_ClockMinute{-1};
        std::atomic<bool> m_ClockSamplePending{false};
        std::atomic<bool> m_ActionPending{false};
        mutable std::mutex m_StatusMutex;
        bool m_HaveResult{};
        bool m_LastSucceeded{};
        std::string m_Message{"Ready"};
    };
}
