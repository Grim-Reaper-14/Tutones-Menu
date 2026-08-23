#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

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
                m_Message = enabled ? "Scanning live Enhanced tunables" : "Disabling No Idle";
            }

            if (enabled)
            {
                m_Resolved.store(false, std::memory_order_release);
                m_Globals = {};
                m_Originals = {};
                EnsureTick();
            }
            else
            {
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
                m_Globals = {};
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
        static constexpr std::uint32_t Joaat(const char* text) noexcept
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

        static constexpr std::uint32_t TunablesRegistrationHash = Joaat("tunables_registration");
        static constexpr std::size_t TunableBase = 0x40001;

        static constexpr std::array<int, 4> IdleDefaults{
            120000, 300000, 600000, 900000,
        };
        static constexpr std::array<int, 4> ConstrainedDefaults{
            30000, 60000, 90000, 120000,
        };
        static constexpr std::array<int, 8> AllDefaults{
            120000, 300000, 600000, 900000,
            30000, 60000, 90000, 120000,
        };

        static constexpr std::size_t HistoricalIdleOffset = 87;
        static constexpr std::size_t HistoricalConstrainedOffset = 8420;

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

            if (success)
            {
                for (const std::size_t global : m_Globals)
                {
                    const int* value = Script::ScriptGlobal(global).As<int>(globals);
                    if (!value || *value != INT_MAX)
                    {
                        success = false;
                        break;
                    }
                }
            }

            if (success)
            {
                SetMessage("No Idle active - all 8 live tunables verified");
            }
            else
            {
                m_Resolved.store(false, std::memory_order_release);
                m_Globals = {};
                m_Originals = {};
                SetMessage("No Idle write verification failed - rescanning");
            }

            EnsureTick();
        }

        template<std::size_t N>
        [[nodiscard]] static std::vector<std::size_t> FindPatternMatches(
            std::int64_t** globals,
            std::size_t tunableCount,
            const std::array<int, N>& pattern) noexcept
        {
            std::vector<std::size_t> matches;
            if (!globals || tunableCount < N)
                return matches;

            for (std::size_t offset = 0; offset + N <= tunableCount; ++offset)
            {
                bool equal = true;
                for (std::size_t index = 0; index < N; ++index)
                {
                    const int* value = Script::ScriptGlobal(TunableBase + offset + index).As<int>(globals);
                    if (!value || *value != pattern[index])
                    {
                        equal = false;
                        break;
                    }
                }

                if (equal)
                    matches.push_back(offset);
            }

            return matches;
        }

        [[nodiscard]] static std::optional<std::pair<std::size_t, std::size_t>> SelectBestPair(
            const std::vector<std::size_t>& idleMatches,
            const std::vector<std::size_t>& constrainedMatches) noexcept
        {
            if (idleMatches.empty() || constrainedMatches.empty())
                return std::nullopt;

            std::optional<std::pair<std::size_t, std::size_t>> best;
            std::size_t bestScore = std::numeric_limits<std::size_t>::max();

            for (const std::size_t idle : idleMatches)
            {
                for (const std::size_t constrained : constrainedMatches)
                {
                    if (idle == constrained)
                        continue;

                    const std::size_t idleDelta = idle > HistoricalIdleOffset
                        ? idle - HistoricalIdleOffset
                        : HistoricalIdleOffset - idle;
                    const std::size_t constrainedDelta = constrained > HistoricalConstrainedOffset
                        ? constrained - HistoricalConstrainedOffset
                        : HistoricalConstrainedOffset - constrained;
                    const std::size_t score = idleDelta + constrainedDelta;

                    if (!best || score < bestScore)
                    {
                        best = std::pair<std::size_t, std::size_t>{idle, constrained};
                        bestScore = score;
                    }
                }
            }

            return best;
        }

        [[nodiscard]] bool ResolveOnGameThread() noexcept
        {
            if (m_Resolved.load(std::memory_order_acquire))
                return true;

            auto& scripts = Script::ScriptRuntime::Get();
            auto** globals = scripts.Globals();
            auto* program = scripts.FindProgram(TunablesRegistrationHash);

            if (!scripts.IsReady() || !globals)
            {
                SetMessage("Waiting for shared script globals");
                return false;
            }

            if (!program)
            {
                SetMessage("Waiting for tunables_registration to load");
                return false;
            }

            if (program->globalCount <= TunableBase)
            {
                SetMessage("tunables_registration global range is invalid");
                return false;
            }

            const std::size_t tunableCount = static_cast<std::size_t>(program->globalCount) - TunableBase;
            const auto idleMatches = FindPatternMatches(globals, tunableCount, IdleDefaults);
            const auto constrainedMatches = FindPatternMatches(globals, tunableCount, ConstrainedDefaults);
            const auto pair = SelectBestPair(idleMatches, constrainedMatches);

            if (!pair)
            {
                SetMessage("Live idle timer signatures not found - waiting for current session tunables");
                return false;
            }

            const auto [idleOffset, constrainedOffset] = *pair;
            for (std::size_t index = 0; index < 4; ++index)
            {
                m_Globals[index] = TunableBase + idleOffset + index;
                m_Globals[index + 4] = TunableBase + constrainedOffset + index;
            }

            std::array<int, 8> current{};
            for (std::size_t index = 0; index < m_Globals.size(); ++index)
            {
                const int* value = Script::ScriptGlobal(m_Globals[index]).As<int>(globals);
                if (!value)
                {
                    m_Globals = {};
                    SetMessage("Resolved idle global became unreadable");
                    return false;
                }
                current[index] = *value;
            }

            if (current != AllDefaults)
            {
                m_Globals = {};
                SetMessage("Live idle timer verification changed during scan - retrying");
                return false;
            }

            m_Originals = current;
            m_Resolved.store(true, std::memory_order_release);

            TUTONES_LOG_INFO(
                "game.noidle",
                std::string("Resolved live Enhanced no-idle tunables: idle +")
                    + std::to_string(idleOffset)
                    + ", constrained +"
                    + std::to_string(constrainedOffset));

            SetMessage(
                std::string("Resolved live timers at +")
                    + std::to_string(idleOffset)
                    + " / +"
                    + std::to_string(constrainedOffset));
            return true;
        }

        void RestoreOnGameThread() noexcept
        {
            if (!m_Resolved.exchange(false, std::memory_order_acq_rel))
            {
                m_Globals = {};
                m_Originals = {};
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
