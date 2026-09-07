#pragma once

#include "../../game/GamePointers.hpp"
#include "../../game/PlayerNatives.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::Business
{
    namespace MoneyFrontsEnhanced173
    {
        // Current Enhanced GPBD_Flow layout. The decompile uses a 149-slot
        // per-player flow entry; the current M25 (Money Fronts) block occupies
        // the final three live flow slots before the trailing heist flag.
        inline constexpr std::size_t FlowGlobal = 1985024;
        inline constexpr std::size_t PlayerEntrySize = 149;
        inline constexpr std::size_t MoneyFrontsFlowOffset = 145;
        inline constexpr std::size_t GeneralBitsOffset = 0;
        inline constexpr std::size_t FlagsOffset = 1;
        inline constexpr std::size_t CurrentMissionOffset = 2;
    }

    struct MoneyFrontsSnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool sessionStarted{};
        bool nativeReady{};
        bool globalsReady{};
        int playerId{-1};
        std::uint32_t generalBits{};
        std::uint32_t flags{};
        int currentMission{-1};
        std::string message{"Press Refresh Money Fronts"};
    };

    class MoneyFrontsRuntime final
    {
    public:
        static MoneyFrontsRuntime& Get() noexcept
        {
            static MoneyFrontsRuntime instance;
            return instance;
        }

        [[nodiscard]] bool QueueRefresh()
        {
            return Queue("Reading current Money Fronts player-flow state", [this] {
                MoneyFrontsSnapshot state;
                const bool success = CaptureState(state);
                Finish(
                    success,
                    std::move(state),
                    success
                        ? "Money Fronts player-flow state refreshed"
                        : "Unable to read Money Fronts player-flow state");
            });
        }

        [[nodiscard]] MoneyFrontsSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            MoneyFrontsSnapshot state = m_Snapshot;
            state.pending = m_Pending.load(std::memory_order_acquire);
            return state;
        }

    private:
        MoneyFrontsRuntime() = default;
        MoneyFrontsRuntime(const MoneyFrontsRuntime&) = delete;
        MoneyFrontsRuntime& operator=(const MoneyFrontsRuntime&) = delete;

        template<typename Callback>
        [[nodiscard]] bool Queue(std::string pendingMessage, Callback&& callback)
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot.haveResult = false;
                m_Snapshot.lastSucceeded = false;
                m_Snapshot.message = std::move(pendingMessage);
            }

            if (Runtime::GameRuntime::Get().Enqueue(std::forward<Callback>(callback)))
                return true;

            MoneyFrontsSnapshot state;
            Finish(false, std::move(state), "GTA script-thread queue is unavailable");
            return false;
        }

        [[nodiscard]] bool CaptureState(MoneyFrontsSnapshot& state) const noexcept
        {
            using namespace MoneyFrontsEnhanced173;

            const bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            state.sessionStarted = sessionStarted && *sessionStarted;
            state.nativeReady = Native::NativeRegistry::Get().CanInvokeOnCurrentThread();

            auto& scripts = Script::ScriptRuntime::Get();
            auto** globals = scripts.Globals();
            state.globalsReady = globals != nullptr;
            if (!state.sessionStarted || !state.nativeReady || !globals)
                return false;

            const auto player = PlayerNatives::PlayerId();
            if (!player || *player < 0 || *player >= 32)
                return false;
            state.playerId = *player;

            const auto flow = Script::ScriptGlobal(FlowGlobal)
                .At(static_cast<std::size_t>(*player), PlayerEntrySize)
                .At(MoneyFrontsFlowOffset);

            const auto* generalBits = flow.At(GeneralBitsOffset).As<std::int32_t>(globals);
            const auto* flags = flow.At(FlagsOffset).As<std::int32_t>(globals);
            const auto* currentMission = flow.At(CurrentMissionOffset).As<std::int32_t>(globals);
            if (!generalBits || !flags || !currentMission)
                return false;

            state.generalBits = static_cast<std::uint32_t>(*generalBits);
            state.flags = static_cast<std::uint32_t>(*flags);
            state.currentMission = *currentMission;
            return true;
        }

        void Finish(bool success, MoneyFrontsSnapshot state, std::string message) noexcept
        {
            state.pending = false;
            state.haveResult = true;
            state.lastSucceeded = success;
            state.message = std::move(message);
            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot = std::move(state);
            }
            m_Pending.store(false, std::memory_order_release);
        }

        std::atomic<bool> m_Pending{false};
        mutable std::mutex m_Mutex;
        MoneyFrontsSnapshot m_Snapshot{};
    };
}
