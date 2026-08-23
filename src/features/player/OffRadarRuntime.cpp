#include "OffRadarRuntime.hpp"

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/PlayerNatives.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>

namespace Tutones::Game::PlayerFeatures
{
    namespace
    {
        constexpr std::uint32_t Joaat(const char* text) noexcept
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

        constexpr std::uint32_t FreemodeHash = Joaat("freemode");
        constexpr std::size_t GlobalPlayerBd = 2658296;
        constexpr std::size_t GlobalPlayerEntrySize = 468;
        constexpr std::size_t FreemodeStateOffset = 0;
        constexpr std::size_t OffRadarActiveOffset = 214;
        constexpr std::size_t OffRadarNetworkTimeGlobal = 2673276;
        constexpr std::size_t OffRadarNetworkTimeOffset = 58;
        constexpr std::int32_t FreemodeRunning = 4;
    }

    OffRadarRuntime& OffRadarRuntime::Get() noexcept
    {
        static OffRadarRuntime instance;
        return instance;
    }

    bool OffRadarRuntime::Start()
    {
        bool expected = false;
        if (!m_Running.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            return true;

        if (QueueNextTick())
        {
            TUTONES_LOG_INFO("player.otr", "Off Radar runtime scheduled on the GTA script thread");
            return true;
        }

        m_Running.store(false, std::memory_order_release);
        TUTONES_LOG_ERROR("player.otr", "Off Radar runtime failed to queue its first GTA script-thread tick");
        return false;
    }

    void OffRadarRuntime::Stop() noexcept
    {
        if (!m_Running.exchange(false, std::memory_order_acq_rel))
            return;

        m_Enabled.store(false, std::memory_order_release);
        {
            std::scoped_lock lock(m_Mutex);
            m_State.enabled = false;
        }

        QueueDisableCleanup();
        TUTONES_LOG_INFO("player.otr", "Off Radar runtime stopped and broadcast-state cleanup was attempted before teardown");
    }

    bool OffRadarRuntime::IsRunning() const noexcept
    {
        return m_Running.load(std::memory_order_acquire);
    }

    OffRadarState OffRadarRuntime::Snapshot() const noexcept
    {
        std::scoped_lock lock(m_Mutex);
        return m_State;
    }

    void OffRadarRuntime::SetEnabled(bool enabled) noexcept
    {
        m_Enabled.store(enabled, std::memory_order_release);
        std::scoped_lock lock(m_Mutex);
        m_State.enabled = enabled;
    }

    bool OffRadarRuntime::QueueNextTick()
    {
        if (!IsRunning())
            return false;
        return Runtime::GameRuntime::Get().Enqueue([this] { TickOnGameThread(); });
    }

    void OffRadarRuntime::TickOnGameThread() noexcept
    {
        if (!IsRunning())
            return;

        ApplyOnGameThread(m_Enabled.load(std::memory_order_acquire));

        if (IsRunning() && !QueueNextTick())
        {
            TUTONES_LOG_ERROR("player.otr", "Off Radar runtime lost its GTA script-thread scheduling slot and is restoring state");
            // We are still on the GTA script thread here, so Stop() can run the cleanup
            // synchronously instead of enqueueing work into a queue that just failed.
            Stop();
        }
    }

    void OffRadarRuntime::ApplyOnGameThread(bool requestedEnabled) noexcept
    {
        OffRadarState state{};
        state.enabled = requestedEnabled;

        auto& pointers = GamePointers::Get();
        auto& scriptRuntime = Script::ScriptRuntime::Get();
        auto** globals = scriptRuntime.Globals();
        auto* sessionStarted = pointers.IsSessionStarted();
        auto* networkTime = pointers.NetworkTime();

        state.scriptGlobalsReady = globals != nullptr;
        state.sessionStarted = sessionStarted && *sessionStarted;
        state.networkTime = networkTime ? *networkTime : 0;

        auto* freemode = scriptRuntime.FindThread(FreemodeHash);
        state.freemodeReady = freemode != nullptr;

        const auto player = PlayerNatives::PlayerId();
        if (!globals || !sessionStarted || !networkTime || !player || *player < 0 || *player >= 32)
        {
            if (!state.sessionStarted)
                m_Applied.store(false, std::memory_order_release);
            state.applied = m_Applied.load(std::memory_order_acquire) && state.sessionStarted;
            PublishState(state);
            return;
        }

        const auto playerIndex = static_cast<std::size_t>(*player);
        auto playerEntry = Script::ScriptGlobal(GlobalPlayerBd).At(playerIndex, GlobalPlayerEntrySize);
        auto* freemodeState = playerEntry.At(FreemodeStateOffset).As<std::int32_t>(globals);
        auto* offRadarActive = playerEntry.At(OffRadarActiveOffset).As<std::int32_t>(globals);
        auto* networkTimeGlobal = Script::ScriptGlobal(OffRadarNetworkTimeGlobal)
            .At(OffRadarNetworkTimeOffset)
            .As<std::int32_t>(globals);

        state.safeToModify = state.sessionStarted
            && state.freemodeReady
            && freemodeState
            && *freemodeState == FreemodeRunning
            && offRadarActive
            && networkTimeGlobal;

        if (state.safeToModify)
        {
            if (requestedEnabled)
            {
                *networkTimeGlobal = static_cast<std::int32_t>(*networkTime);
                *offRadarActive = 1;
                m_Applied.store(true, std::memory_order_release);
                state.lastRefreshTime = *networkTime;
            }
            else if (m_Applied.load(std::memory_order_acquire))
            {
                *offRadarActive = 0;
                m_Applied.store(false, std::memory_order_release);
            }
        }
        else if (!state.sessionStarted)
        {
            m_Applied.store(false, std::memory_order_release);
        }

        state.applied = m_Applied.load(std::memory_order_acquire) && state.sessionStarted;
        PublishState(state);
    }

    void OffRadarRuntime::QueueDisableCleanup() noexcept
    {
        auto& runtime = Runtime::GameRuntime::Get();
        if (!runtime.IsInitialized())
        {
            m_Applied.store(false, std::memory_order_release);
            return;
        }

        if (runtime.IsOnGameThread())
        {
            ApplyOnGameThread(false);
            return;
        }

        const auto cleaned = std::make_shared<std::atomic<bool>>(false);
        if (!runtime.Enqueue([this, cleaned] {
                ApplyOnGameThread(false);
                cleaned->store(true, std::memory_order_release);
            }))
        {
            return;
        }

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
        while (!cleaned->load(std::memory_order_acquire)
            && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void OffRadarRuntime::PublishState(const OffRadarState& state) noexcept
    {
        std::scoped_lock lock(m_Mutex);
        m_State = state;
    }
}
