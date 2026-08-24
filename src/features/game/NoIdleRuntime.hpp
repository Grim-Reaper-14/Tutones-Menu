#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/tunables/TunableRegistry.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace Tutones::Game::SessionFeatures
{
    struct NoIdleSnapshot final
    {
        bool enabled{};
        bool ready{};
        bool pending{};
        std::string message{"Off"};
    };

    class NoIdleRuntime final
    {
    public:
        static NoIdleRuntime& Get() noexcept
        {
            static NoIdleRuntime instance;
            return instance;
        }

        void SetEnabled(bool enabled) noexcept
        {
            const bool previous = m_Enabled.exchange(enabled, std::memory_order_acq_rel);
            if (previous == enabled)
                return;

            if (enabled)
            {
                m_Resolved.store(false, std::memory_order_release);
                SetMessage("Waiting for central tunable registry");
                EnsureTick();
            }
            else
            {
                SetMessage("Disabling No Idle");
                static_cast<void>(Runtime::GameRuntime::Get().Enqueue([this] {
                    RestoreOnGameThread();
                }));
            }
        }

        void Shutdown() noexcept
        {
            m_Enabled.store(false, std::memory_order_release);
            m_TickQueued.store(false, std::memory_order_release);

            const auto cleanup = [this] {
                RestoreOnGameThread();
            };

            auto& runtime = Runtime::GameRuntime::Get();
            if (runtime.IsOnGameThread())
            {
                cleanup();
                return;
            }

            if (!runtime.IsInitialized())
            {
                m_Resolved.store(false, std::memory_order_release);
                m_Originals = {};
                SetMessage("Off");
                return;
            }

            const auto cleaned = std::make_shared<std::atomic<bool>>(false);
            if (!runtime.Enqueue([cleanup, cleaned] {
                    cleanup();
                    cleaned->store(true, std::memory_order_release);
                }))
            {
                SetMessage("Off - restore could not be queued");
                return;
            }

            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
            while (!cleaned->load(std::memory_order_acquire)
                && std::chrono::steady_clock::now() < deadline)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            if (!cleaned->load(std::memory_order_acquire))
                TUTONES_LOG_WARN("game.noidle", "Timed out restoring idle-kick tunables during shutdown");
        }

        [[nodiscard]] NoIdleSnapshot Snapshot() const
        {
            NoIdleSnapshot snapshot;
            snapshot.enabled = m_Enabled.load(std::memory_order_acquire);
            snapshot.ready = m_Resolved.load(std::memory_order_acquire);
            snapshot.pending = m_TickQueued.load(std::memory_order_acquire);
            std::scoped_lock lock(m_Mutex);
            snapshot.message = m_Message;
            return snapshot;
        }

    private:
        static constexpr std::array<std::uint32_t, 8> IdleHashes{{
            Tunables::Joaat("IDLEKICK_WARNING1"),
            Tunables::Joaat("IDLEKICK_WARNING2"),
            Tunables::Joaat("IDLEKICK_WARNING3"),
            Tunables::Joaat("IDLEKICK_KICK"),
            Tunables::Joaat("ConstrainedKick_Warning1"),
            Tunables::Joaat("ConstrainedKick_Warning2"),
            Tunables::Joaat("ConstrainedKick_Warning3"),
            Tunables::Joaat("ConstrainedKick_Kick"),
        }};

        NoIdleRuntime() = default;

        void EnsureTick() noexcept
        {
            if (!m_Enabled.load(std::memory_order_acquire))
                return;

            bool expected = false;
            if (!m_TickQueued.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return;

            if (!Runtime::GameRuntime::Get().Enqueue([this] { TickOnGameThread(); }))
            {
                m_TickQueued.store(false, std::memory_order_release);
                SetMessage("Game-thread queue unavailable");
            }
        }

        void TickOnGameThread() noexcept
        {
            m_TickQueued.store(false, std::memory_order_release);
            if (!m_Enabled.load(std::memory_order_acquire))
                return;

            auto& registry = Tunables::TunableRegistry::Get();
            if (!registry.IsRunning() || !registry.Initialized())
            {
                SetMessage("Waiting for central tunable registry");
                EnsureTick();
                return;
            }

            if (!m_Resolved.load(std::memory_order_acquire))
            {
                std::array<int, 8> originals{};
                for (std::size_t index = 0; index < IdleHashes.size(); ++index)
                {
                    const Tunables::Tunable tunable{IdleHashes[index]};
                    const auto value = tunable.Get<int>();
                    if (!value)
                    {
                        SetMessage("Waiting for idle tunable hash registration");
                        EnsureTick();
                        return;
                    }
                    originals[index] = *value;
                }

                m_Originals = originals;
                m_Resolved.store(true, std::memory_order_release);
                TUTONES_LOG_INFO("game.noidle", "No Idle acquired all 8 tunables from the central registry");
            }

            bool success = true;
            for (const auto hash : IdleHashes)
            {
                const Tunables::Tunable tunable{hash};
                success = tunable.Set<int>(INT_MAX) && success;
            }

            if (success)
            {
                for (const auto hash : IdleHashes)
                {
                    const Tunables::Tunable tunable{hash};
                    const auto value = tunable.Get<int>();
                    if (!value || *value != INT_MAX)
                    {
                        success = false;
                        break;
                    }
                }
            }

            if (success)
            {
                SetMessage("No Idle active - 8/8 registered tunables verified");
            }
            else
            {
                m_Resolved.store(false, std::memory_order_release);
                SetMessage("No Idle tunable write verification failed - waiting for registry");
            }

            EnsureTick();
        }

        void RestoreOnGameThread() noexcept
        {
            if (!m_Resolved.exchange(false, std::memory_order_acq_rel))
            {
                m_Originals = {};
                SetMessage("Off");
                return;
            }

            bool success = true;
            for (std::size_t index = 0; index < IdleHashes.size(); ++index)
            {
                const Tunables::Tunable tunable{IdleHashes[index]};
                success = tunable.Set<int>(m_Originals[index]) && success;
            }

            m_Originals = {};
            SetMessage(success ? "Off - original idle tunables restored" : "Off - idle tunable restore incomplete");
        }

        void SetMessage(std::string message)
        {
            std::scoped_lock lock(m_Mutex);
            m_Message = std::move(message);
        }

        std::atomic<bool> m_Enabled{false};
        std::atomic<bool> m_Resolved{false};
        std::atomic<bool> m_TickQueued{false};
        std::array<int, 8> m_Originals{};
        mutable std::mutex m_Mutex;
        std::string m_Message{"Off"};
    };
}
