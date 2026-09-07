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
    namespace GarmentFactoryEnhanced173
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

        inline constexpr std::uint32_t HackerTruckAppHash = Joaat("apphackertruck");
        inline constexpr std::size_t FlowGlobal = 1985024;
        inline constexpr std::size_t PlayerEntrySize = 149;
        inline constexpr std::size_t Hacker24FlowOffset = 121;

        inline constexpr std::size_t GeneralFlagsOffset = 0;
        inline constexpr std::size_t InstanceFlagsOffset = 1;
        inline constexpr std::size_t ActiveRobberyOffset = 2;
        inline constexpr std::size_t Unknown3Offset = 3;
        inline constexpr std::size_t HackerFlagsOffset = 4;
        inline constexpr std::size_t Unknown5Offset = 5;
        inline constexpr std::size_t PackedBool51273Offset = 14;
        inline constexpr std::size_t PackedBool51274Offset = 15;
        inline constexpr std::size_t PackedBool51275Offset = 16;
    }

    struct GarmentFactorySnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool sessionStarted{};
        bool nativeReady{};
        bool globalsReady{};
        bool hackerTruckAppRunning{};
        int playerId{-1};
        std::uint32_t generalFlags{};
        std::uint32_t instanceFlags{};
        int activeRobbery{-1};
        int unknown3{};
        std::uint32_t hackerFlags{};
        int unknown5{};
        bool packedBool51273{};
        bool packedBool51274{};
        bool packedBool51275{};
        std::string message{"Press Refresh Garment Factory"};
    };

    class GarmentFactoryRuntime final
    {
    public:
        static GarmentFactoryRuntime& Get() noexcept
        {
            static GarmentFactoryRuntime instance;
            return instance;
        }

        [[nodiscard]] bool QueueRefresh()
        {
            return Queue("Reading current Garment Factory Hacker24 flow", [this] {
                GarmentFactorySnapshot state;
                const bool success = CaptureState(state);
                Finish(
                    success,
                    std::move(state),
                    success
                        ? "Garment Factory player-flow state refreshed"
                        : "Unable to read Garment Factory Hacker24 flow");
            });
        }

        [[nodiscard]] GarmentFactorySnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            GarmentFactorySnapshot state = m_Snapshot;
            state.pending = m_Pending.load(std::memory_order_acquire);
            return state;
        }

    private:
        GarmentFactoryRuntime() = default;
        GarmentFactoryRuntime(const GarmentFactoryRuntime&) = delete;
        GarmentFactoryRuntime& operator=(const GarmentFactoryRuntime&) = delete;

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

            GarmentFactorySnapshot state;
            Finish(false, std::move(state), "GTA script-thread queue is unavailable");
            return false;
        }

        [[nodiscard]] bool CaptureState(GarmentFactorySnapshot& state) const noexcept
        {
            using namespace GarmentFactoryEnhanced173;

            const bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            state.sessionStarted = sessionStarted && *sessionStarted;
            state.nativeReady = Native::NativeRegistry::Get().CanInvokeOnCurrentThread();

            auto& scripts = Script::ScriptRuntime::Get();
            auto** globals = scripts.Globals();
            state.globalsReady = globals != nullptr;
            if (scripts.IsReady())
            {
                if (const auto* thread = scripts.FindThread(HackerTruckAppHash))
                {
                    state.hackerTruckAppRunning = thread->context.threadId != 0
                        && thread->context.state != Types::ScriptThreadState::Killed;
                }
            }

            if (!state.sessionStarted || !state.nativeReady || !globals)
                return false;

            const auto player = PlayerNatives::PlayerId();
            if (!player || *player < 0 || *player >= 32)
                return false;
            state.playerId = *player;

            const auto flow = Script::ScriptGlobal(FlowGlobal)
                .At(static_cast<std::size_t>(*player), PlayerEntrySize)
                .At(Hacker24FlowOffset);

            const auto* generalFlags = flow.At(GeneralFlagsOffset).As<std::int32_t>(globals);
            const auto* instanceFlags = flow.At(InstanceFlagsOffset).As<std::int32_t>(globals);
            const auto* activeRobbery = flow.At(ActiveRobberyOffset).As<std::int32_t>(globals);
            const auto* unknown3 = flow.At(Unknown3Offset).As<std::int32_t>(globals);
            const auto* hackerFlags = flow.At(HackerFlagsOffset).As<std::int32_t>(globals);
            const auto* unknown5 = flow.At(Unknown5Offset).As<std::int32_t>(globals);
            const auto* packed51273 = flow.At(PackedBool51273Offset).As<std::int32_t>(globals);
            const auto* packed51274 = flow.At(PackedBool51274Offset).As<std::int32_t>(globals);
            const auto* packed51275 = flow.At(PackedBool51275Offset).As<std::int32_t>(globals);
            if (!generalFlags || !instanceFlags || !activeRobbery || !unknown3 || !hackerFlags
                || !unknown5 || !packed51273 || !packed51274 || !packed51275)
            {
                return false;
            }

            state.generalFlags = static_cast<std::uint32_t>(*generalFlags);
            state.instanceFlags = static_cast<std::uint32_t>(*instanceFlags);
            state.activeRobbery = *activeRobbery;
            state.unknown3 = *unknown3;
            state.hackerFlags = static_cast<std::uint32_t>(*hackerFlags);
            state.unknown5 = *unknown5;
            state.packedBool51273 = *packed51273 != 0;
            state.packedBool51274 = *packed51274 != 0;
            state.packedBool51275 = *packed51275 != 0;
            return true;
        }

        void Finish(bool success, GarmentFactorySnapshot state, std::string message) noexcept
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
        GarmentFactorySnapshot m_Snapshot{};
    };
}
