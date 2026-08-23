#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <array>
#include <atomic>
#include <climits>
#include <cstddef>
#include <mutex>
#include <string>

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

            {
                std::scoped_lock lock(m_Mutex);
                m_Message = enabled ? "Resolving Yim-style idle tunables" : "Disabling No Idle";
            }

            if (enabled)
            {
                EnsureTick();
            }
            else
            {
                static_cast<void>(Runtime::GameRuntime::Get().Enqueue([this] {
                    RestoreOnGameThread();
                }));
            }
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
        // YimMenuV2 resolves these exact tunables by name. The current Enhanced
        // tunables table places them at these offsets from Global_262145.
        // We validate the values before ever writing so a future title update fails closed.
        static constexpr std::size_t TunableBase = 262145;
        static constexpr std::array<std::size_t, 8> TunableOffsets{
            87, 88, 89, 90,
            8420, 8421, 8422, 8423,
        };
        static constexpr std::array<int, 8> ExpectedDefaults{
            120000, 300000, 600000, 900000,
            30000, 60000, 90000, 120000,
        };

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

            if (!ResolveOnGameThread())
            {
                SetMessage("Idle tunables unavailable for this Enhanced build");
                EnsureTick();
                return;
            }

            auto** globals = Script::ScriptRuntime::Get().Globals();
            bool success = globals != nullptr;
            if (globals)
            {
                for (const std::size_t global : m_Globals)
                {
                    int* value = Script::ScriptGlobal(global).As<int>(globals);
                    if (!value)
                    {
                        success = false;
                        break;
                    }
                    *value = INT_MAX;
                }
            }

            SetMessage(success ? "No Idle active - 8 named timers overridden" : "No Idle write failed");
            EnsureTick();
        }

        [[nodiscard]] bool ResolveOnGameThread() noexcept
        {
            if (m_Resolved.load(std::memory_order_acquire))
                return true;

            auto& scripts = Script::ScriptRuntime::Get();
            auto** globals = scripts.Globals();
            if (!scripts.IsReady() || !globals)
                return false;

            std::array<int, 8> current{};
            for (std::size_t index = 0; index < TunableOffsets.size(); ++index)
            {
                const std::size_t global = TunableBase + TunableOffsets[index];
                int* value = Script::ScriptGlobal(global).As<int>(globals);
                if (!value)
                    return false;
                current[index] = *value;
                m_Globals[index] = global;
            }

            // Exact defaults are preferred. Also accept sane, strictly increasing
            // remote-tunable timer overrides at the same named slots.
            const bool exact = current == ExpectedDefaults;
            const auto saneGroup = [&current](std::size_t start) {
                return current[start] >= 1000
                    && current[start] < current[start + 1]
                    && current[start + 1] < current[start + 2]
                    && current[start + 2] < current[start + 3]
                    && current[start + 3] <= 3600000;
            };

            if (!exact && !(saneGroup(0) && saneGroup(4)))
            {
                m_Globals = {};
                TUTONES_LOG_WARN("game.noidle", "Named idle tunable validation failed; refusing to write unknown globals");
                return false;
            }

            m_Originals = current;
            m_Resolved.store(true, std::memory_order_release);
            TUTONES_LOG_INFO("game.noidle", "Resolved Yim-style IDLEKICK and ConstrainedKick tunables for Enhanced");
            return true;
        }

        void RestoreOnGameThread() noexcept
        {
            if (!m_Resolved.exchange(false, std::memory_order_acq_rel))
            {
                SetMessage("Off");
                return;
            }

            auto** globals = Script::ScriptRuntime::Get().Globals();
            bool success = globals != nullptr;
            if (globals)
            {
                for (std::size_t index = 0; index < m_Globals.size(); ++index)
                {
                    int* value = Script::ScriptGlobal(m_Globals[index]).As<int>(globals);
                    if (!value)
                    {
                        success = false;
                        continue;
                    }
                    *value = m_Originals[index];
                }
            }

            m_Globals = {};
            m_Originals = {};
            SetMessage(success ? "Off - original idle timers restored" : "Off - timer restore incomplete");
        }

        void SetMessage(std::string message)
        {
            std::scoped_lock lock(m_Mutex);
            m_Message = std::move(message);
        }

        std::atomic<bool> m_Enabled{false};
        std::atomic<bool> m_Resolved{false};
        std::atomic<bool> m_TickQueued{false};
        std::array<std::size_t, 8> m_Globals{};
        std::array<int, 8> m_Originals{};
        mutable std::mutex m_Mutex;
        std::string m_Message{"Off"};
    };
}
