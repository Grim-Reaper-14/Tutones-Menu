#pragma once

#include "../../game/GamePointers.hpp"
#include "../../game/PlayerNatives.hpp"
#include "../../game/Stats.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::Business
{
    namespace BailOfficeEnhanced173
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

        inline constexpr std::uint32_t AppHash = Joaat("appBailOffice");
        inline constexpr std::size_t FlowGlobal = 1985024;
        inline constexpr std::size_t PlayerEntrySize = 149;

        // Current Enhanced decompile access is:
        // Global_1985024[player /*149*/].f_109.f_1[index /*3*/]
        // with fields { mission, target, reward }.
        inline constexpr std::size_t BountyFlowOffset = 109;
        inline constexpr std::size_t StandardTargetsArrayOffset = 1;
        inline constexpr std::size_t StandardTargetSize = 3;
        inline constexpr int StandardTargetCount = 3;
        inline constexpr int StandardCompletionPackedBool = 42274;
        inline constexpr int MostWantedPackedInt = 19014;

        inline constexpr std::array<const char*, 24> TargetNames{
            "Grace Whitney",
            "Chaz Lieberman",
            "Leroy O'Neil",
            "Brock Thompson",
            "Cleo Song",
            "Omar Garcia",
            "Beau Duggan",
            "Bill Duggan",
            "Hunter Duggan",
            "Lil Prince",
            "Xavier Fremond",
            "Jalen Kennedy",
            "Rylee Rose",
            "Serenity Pierce",
            "Angel Kenney",
            "Sabrina Gray",
            "India Wood",
            "Brigitte Foster",
            "Cook Kenzie",
            "Marquel Green",
            "Colby Wright",
            "Ricky Ji",
            "Tommy Lim",
            "LJ Ha",
        };

        [[nodiscard]] inline const char* TargetName(int target) noexcept
        {
            if (target < 0 || target >= static_cast<int>(TargetNames.size()))
                return "Unknown bounty target";
            return TargetNames[static_cast<std::size_t>(target)];
        }
    }

    struct BailOfficeTarget final
    {
        int mission{-1};
        int target{-1};
        int reward{};
        bool completed{};
        bool readable{};
    };

    struct BailOfficeSnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool sessionStarted{};
        bool nativeReady{};
        bool globalsReady{};
        bool appRunning{};
        int playerId{-1};
        std::array<BailOfficeTarget, BailOfficeEnhanced173::StandardTargetCount> standardTargets{};
        int mostWantedRotation{-1};
        bool mostWantedReadable{};
        std::string message{"Press Refresh Bail Office"};
    };

    class BailOfficeRuntime final
    {
    public:
        static BailOfficeRuntime& Get() noexcept
        {
            static BailOfficeRuntime instance;
            return instance;
        }

        [[nodiscard]] bool QueueRefresh()
        {
            return Queue("Reading Enhanced Bail Office target state", [this] {
                BailOfficeSnapshot state;
                const bool success = CaptureState(state);
                Finish(
                    success,
                    std::move(state),
                    success
                        ? "Bail Office target state refreshed"
                        : "Unable to read the Enhanced Bail Office target state");
            });
        }

        [[nodiscard]] BailOfficeSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            BailOfficeSnapshot state = m_Snapshot;
            state.pending = m_Pending.load(std::memory_order_acquire);
            return state;
        }

    private:
        BailOfficeRuntime() = default;
        BailOfficeRuntime(const BailOfficeRuntime&) = delete;
        BailOfficeRuntime& operator=(const BailOfficeRuntime&) = delete;

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

            BailOfficeSnapshot state;
            Finish(false, std::move(state), "GTA script-thread queue is unavailable");
            return false;
        }

        [[nodiscard]] bool CaptureState(BailOfficeSnapshot& state) const noexcept
        {
            using namespace BailOfficeEnhanced173;

            const bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            state.sessionStarted = sessionStarted && *sessionStarted;
            state.nativeReady = Native::NativeRegistry::Get().CanInvokeOnCurrentThread();

            auto& scripts = Script::ScriptRuntime::Get();
            auto** globals = scripts.Globals();
            state.globalsReady = globals != nullptr;
            if (scripts.IsReady())
            {
                if (const auto* thread = scripts.FindThread(AppHash))
                {
                    state.appRunning = thread->context.threadId != 0
                        && thread->context.state != Types::ScriptThreadState::Killed;
                }
            }

            if (!state.sessionStarted || !state.nativeReady || !globals)
                return false;

            const auto player = PlayerNatives::PlayerId();
            if (!player || *player < 0 || *player >= 32)
                return false;
            state.playerId = *player;

            const auto bounty = Script::ScriptGlobal(FlowGlobal)
                .At(static_cast<std::size_t>(*player), PlayerEntrySize)
                .At(BountyFlowOffset);
            const auto standard = bounty.At(StandardTargetsArrayOffset);

            bool anyReadable = false;
            for (std::size_t index = 0; index < state.standardTargets.size(); ++index)
            {
                const auto entry = standard.At(index, StandardTargetSize);
                const auto* mission = entry.At(0).As<std::int32_t>(globals);
                const auto* target = entry.At(1).As<std::int32_t>(globals);
                const auto* reward = entry.At(2).As<std::int32_t>(globals);
                const auto completed = Stats::GetPackedBool(
                    StandardCompletionPackedBool + static_cast<int>(index),
                    -1);

                if (!mission || !target || !reward || !completed)
                    continue;

                auto& destination = state.standardTargets[index];
                destination.mission = *mission;
                destination.target = *target;
                destination.reward = *reward;
                destination.completed = *completed;
                destination.readable = true;
                anyReadable = true;
            }

            const auto mostWanted = Stats::GetPackedInt(MostWantedPackedInt, -1);
            if (mostWanted)
            {
                state.mostWantedRotation = *mostWanted;
                state.mostWantedReadable = true;
                anyReadable = true;
            }

            return anyReadable;
        }

        void Finish(bool success, BailOfficeSnapshot state, std::string message) noexcept
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
        BailOfficeSnapshot m_Snapshot{};
    };
}
