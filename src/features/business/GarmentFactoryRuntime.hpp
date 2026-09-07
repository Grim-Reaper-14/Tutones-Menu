#pragma once

#include "../../game/GamePointers.hpp"
#include "../../game/PlayerNatives.hpp"
#include "../../game/Stats.hpp"
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

        inline constexpr const char* ActiveRobberyStat = "MPX_HACKER24_ACTIVE_ROB";
        inline constexpr const char* GeneralBitsStat = "MPX_HACKER24_GEN_BS";
        inline constexpr const char* SafeCashStat = "MPX_HDEN24_SAFE_CASH_VALUE";
        inline constexpr std::size_t SafeCollectGlobal = 2708883;
        inline constexpr std::uint32_t PrepMask = (1u << 2) | (1u << 3) | (1u << 4);
        inline constexpr int FileCount = 4;
        inline constexpr int UnbrickGeneralBits = -24607;

        [[nodiscard]] inline const char* FibFileName(int file) noexcept
        {
            switch (file)
            {
            case -1: return "None";
            case 0: return "The Black Box File";
            case 1: return "The Brute Force File";
            case 2: return "The Fine Art File";
            case 3: return "The Project Breakaway File";
            default: return "Unknown FIB File";
            }
        }
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
        int activeRobberyStat{-1};
        int generalBitsStat{};
        bool prepsComplete{};
        int safeCash{};
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
                        ? "Garment Factory state refreshed"
                        : "Unable to read Garment Factory Hacker24 state");
            });
        }

        [[nodiscard]] bool QueueSetActiveFile(int file)
        {
            using namespace GarmentFactoryEnhanced173;
            if (file < -1 || file >= FileCount)
                return false;

            return Queue("Updating active FIB File", [this, file] {
                const auto original = Stats::GetInt(ActiveRobberyStat);
                if (!original)
                {
                    GarmentFactorySnapshot state;
                    static_cast<void>(CaptureState(state));
                    Finish(false, std::move(state), "Unable to read HACKER24_ACTIVE_ROB");
                    return;
                }

                if (!Stats::SetInt(ActiveRobberyStat, file))
                {
                    GarmentFactorySnapshot state;
                    static_cast<void>(CaptureState(state));
                    Finish(false, std::move(state), "Active FIB File write was rejected");
                    return;
                }

                const auto verified = Stats::GetInt(ActiveRobberyStat);
                if (!verified || *verified != file)
                {
                    static_cast<void>(Stats::SetInt(ActiveRobberyStat, *original));
                    GarmentFactorySnapshot rolledBack;
                    static_cast<void>(CaptureState(rolledBack));
                    Finish(false, std::move(rolledBack), "Active FIB File verification failed and was rolled back");
                    return;
                }

                GarmentFactorySnapshot after;
                const bool captured = CaptureState(after);
                Finish(
                    captured,
                    std::move(after),
                    captured
                        ? std::string("Active FIB File set to ") + FibFileName(file) + "; reopen the Garment Factory computer if it was already open"
                        : "FIB File changed but state refresh failed");
            });
        }

        [[nodiscard]] bool QueueSetPrepsComplete(bool complete)
        {
            using namespace GarmentFactoryEnhanced173;
            return Queue(complete ? "Completing FIB File preps" : "Resetting FIB File preps", [this, complete] {
                const auto original = Stats::GetInt(GeneralBitsStat);
                if (!original)
                {
                    GarmentFactorySnapshot state;
                    static_cast<void>(CaptureState(state));
                    Finish(false, std::move(state), "Unable to read HACKER24_GEN_BS");
                    return;
                }

                const auto originalBits = static_cast<std::uint32_t>(*original);
                const auto updatedBits = complete
                    ? (originalBits | PrepMask)
                    : (originalBits & ~PrepMask);
                const int updated = static_cast<std::int32_t>(updatedBits);

                if (!Stats::SetInt(GeneralBitsStat, updated))
                {
                    GarmentFactorySnapshot state;
                    static_cast<void>(CaptureState(state));
                    Finish(false, std::move(state), "FIB File prep write was rejected");
                    return;
                }

                const auto verified = Stats::GetInt(GeneralBitsStat);
                const bool verifiedState = verified &&
                    (complete
                        ? ((static_cast<std::uint32_t>(*verified) & PrepMask) == PrepMask)
                        : ((static_cast<std::uint32_t>(*verified) & PrepMask) == 0));
                if (!verifiedState)
                {
                    static_cast<void>(Stats::SetInt(GeneralBitsStat, *original));
                    GarmentFactorySnapshot rolledBack;
                    static_cast<void>(CaptureState(rolledBack));
                    Finish(false, std::move(rolledBack), "FIB File prep verification failed and was rolled back");
                    return;
                }

                GarmentFactorySnapshot after;
                const bool captured = CaptureState(after);
                Finish(
                    captured,
                    std::move(after),
                    captured
                        ? (complete ? "All three FIB File preps marked complete" : "FIB File prep completion reset")
                        : "FIB File preps changed but state refresh failed");
            });
        }

        [[nodiscard]] bool QueueUnbrickComputer()
        {
            using namespace GarmentFactoryEnhanced173;
            return Queue("Unbricking Garment Factory computer", [this] {
                const auto original = Stats::GetInt(GeneralBitsStat);
                if (!original)
                {
                    GarmentFactorySnapshot state;
                    static_cast<void>(CaptureState(state));
                    Finish(false, std::move(state), "Unable to read HACKER24_GEN_BS");
                    return;
                }

                if (!Stats::SetInt(GeneralBitsStat, UnbrickGeneralBits))
                {
                    GarmentFactorySnapshot state;
                    static_cast<void>(CaptureState(state));
                    Finish(false, std::move(state), "Garment Factory unbrick write was rejected");
                    return;
                }

                const auto verified = Stats::GetInt(GeneralBitsStat);
                if (!verified || *verified != UnbrickGeneralBits)
                {
                    static_cast<void>(Stats::SetInt(GeneralBitsStat, *original));
                    GarmentFactorySnapshot rolledBack;
                    static_cast<void>(CaptureState(rolledBack));
                    Finish(false, std::move(rolledBack), "Garment Factory unbrick verification failed and was rolled back");
                    return;
                }

                GarmentFactorySnapshot after;
                const bool captured = CaptureState(after);
                Finish(
                    captured,
                    std::move(after),
                    captured
                        ? "Garment Factory computer state reset; reopen the computer"
                        : "Computer state reset but refresh failed");
            });
        }

        [[nodiscard]] bool QueueCollectSafe()
        {
            using namespace GarmentFactoryEnhanced173;
            return Queue("Requesting Garment Factory safe collection", [this] {
                GarmentFactorySnapshot before;
                if (!CaptureState(before))
                {
                    Finish(false, std::move(before), "Garment Factory state is unavailable");
                    return;
                }
                if (before.safeCash <= 0)
                {
                    Finish(false, std::move(before), "Garment Factory safe is empty");
                    return;
                }

                auto** globals = Script::ScriptRuntime::Get().Globals();
                if (!globals)
                {
                    Finish(false, std::move(before), "Script globals are unavailable");
                    return;
                }

                auto* collect = Script::ScriptGlobal(SafeCollectGlobal).As<std::int32_t>(globals);
                if (!collect)
                {
                    Finish(false, std::move(before), "Garment Factory safe-collect global is unavailable");
                    return;
                }

                const std::int32_t original = *collect;
                *collect = 1;
                if (*collect != 1)
                {
                    *collect = original;
                    Finish(false, std::move(before), "Garment Factory safe-collect request failed");
                    return;
                }

                GarmentFactorySnapshot after;
                static_cast<void>(CaptureState(after));
                Finish(true, std::move(after), "Garment Factory safe collection requested");
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

            if (const auto active = Stats::GetInt(ActiveRobberyStat))
                state.activeRobberyStat = *active;
            if (const auto general = Stats::GetInt(GeneralBitsStat))
            {
                state.generalBitsStat = *general;
                state.prepsComplete =
                    (static_cast<std::uint32_t>(*general) & PrepMask) == PrepMask;
            }
            if (const auto safe = Stats::GetInt(SafeCashStat))
                state.safeCash = *safe;

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
