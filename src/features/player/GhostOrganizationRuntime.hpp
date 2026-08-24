#pragma once

#include "../../game/GamePointers.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>

namespace Tutones::Game::PlayerFeatures
{
    struct GhostOrganizationState final
    {
        bool enabled{};
        bool applied{};
        bool sessionStarted{};
        bool freemodeReady{};
        bool scriptGlobalsReady{};
        bool safeToModify{};
    };

    class GhostOrganizationRuntime final
    {
    public:
        static GhostOrganizationRuntime& Get() noexcept
        {
            static GhostOrganizationRuntime instance;
            return instance;
        }

        [[nodiscard]] GhostOrganizationState Snapshot() const noexcept
        {
            std::scoped_lock lock(m_Mutex);
            return m_State;
        }

        void SetEnabled(bool enabled) noexcept
        {
            m_Enabled.store(enabled, std::memory_order_release);

            bool expected = false;
            if (m_Running.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            {
                if (!QueueNextTick())
                    m_Running.store(false, std::memory_order_release);
            }
        }

    private:
        GhostOrganizationRuntime() = default;

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

        bool QueueNextTick() noexcept
        {
            return Runtime::GameRuntime::Get().Enqueue([this] { TickOnGameThread(); });
        }

        void TickOnGameThread() noexcept
        {
            const bool enabled = m_Enabled.load(std::memory_order_acquire);
            GhostOrganizationState state{};
            state.enabled = enabled;

            auto& pointers = GamePointers::Get();
            auto& scripts = Script::ScriptRuntime::Get();
            auto** globals = scripts.Globals();
            auto* sessionStarted = pointers.IsSessionStarted();

            state.scriptGlobalsReady = globals != nullptr;
            state.sessionStarted = sessionStarted && *sessionStarted;
            state.freemodeReady = scripts.FindThread(FreemodeHash) != nullptr;

            auto* flags = globals
                ? Script::ScriptGlobal(FreemodeGlobal).At(GhostOrganizationFlagsOffset).As<std::int32_t>(globals)
                : nullptr;

            state.safeToModify = state.scriptGlobalsReady
                && state.sessionStarted
                && state.freemodeReady
                && flags;

            if (state.safeToModify)
            {
                constexpr std::int32_t GhostOrganizationMask = 1 << GhostOrganizationBit;
                if (enabled)
                {
                    *flags |= GhostOrganizationMask;
                    m_Applied.store(true, std::memory_order_release);
                }
                else if (m_Applied.load(std::memory_order_acquire))
                {
                    *flags &= ~GhostOrganizationMask;
                    m_Applied.store(false, std::memory_order_release);
                }
            }
            else if (!state.sessionStarted)
            {
                m_Applied.store(false, std::memory_order_release);
            }

            state.applied = m_Applied.load(std::memory_order_acquire) && state.sessionStarted;
            {
                std::scoped_lock lock(m_Mutex);
                m_State = state;
            }

            if (enabled || m_Applied.load(std::memory_order_acquire))
            {
                if (!QueueNextTick())
                    m_Running.store(false, std::memory_order_release);
            }
            else
            {
                m_Running.store(false, std::memory_order_release);
            }
        }

        static constexpr std::uint32_t FreemodeHash = Joaat("freemode");

        // Current Enhanced mapping verified against the same Freemode generation
        // used by Tutones' updated Off Radar globals. Ghost Organization is the
        // Freemode ghost flag (bit 2) layered on top of normal Off Radar state.
        static constexpr std::size_t FreemodeGlobal = 2733190;
        static constexpr std::size_t GhostOrganizationFlagsOffset = 3758;
        static constexpr int GhostOrganizationBit = 2;

        std::atomic<bool> m_Running{false};
        std::atomic<bool> m_Enabled{false};
        std::atomic<bool> m_Applied{false};
        mutable std::mutex m_Mutex;
        GhostOrganizationState m_State{};
    };
}
