#pragma once

#include "../../game/GamePointers.hpp"
#include "../../game/PlayerNatives.hpp"
#include "../../game/Stats.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../game/tunables/TunableRegistry.hpp"
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
    namespace AgencyEnhanced173
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

        inline constexpr std::uint32_t SecurityAppHash = Joaat("appfixersecurity");
        inline constexpr std::size_t FlowGlobal = 1985024;
        inline constexpr std::size_t PlayerEntrySize = 149;
        inline constexpr std::size_t FixerFlowOffset = 27;

        inline constexpr std::size_t GeneralFlagsOffset = 0;
        inline constexpr std::size_t CompletedFlagsOffset = 1;
        inline constexpr std::size_t PayphoneBonusMethodOffset = 2;
        inline constexpr std::size_t StoryFlagsOffset = 3;
        inline constexpr std::size_t StoryStrandOffset = 6;
        inline constexpr std::size_t StoryCooldownOffset = 7;
        inline constexpr std::size_t FixerFlagsOffset = 8;
        inline constexpr std::size_t ShortTripsOffset = 9;
        inline constexpr std::size_t ContractCountOffset = 10;
        inline constexpr std::size_t EarningsOffset = 18;
        inline constexpr std::size_t ContractsArrayOffset = 19;
        inline constexpr std::size_t ContractSize = 3;
        inline constexpr int ContractSlots = 3;
        inline constexpr int ContractTypes = 6;
        inline constexpr int ContractDifficulties = 3;

        inline constexpr std::size_t SecuritySessionGlobal = 1984422;
        inline constexpr std::size_t SecuritySessionFlagsOffset = 1;
        inline constexpr std::uint32_t SecurityContractDelayMask = 0x80000000u;

        inline constexpr const char* StoryBitsStat = "MPX_FIXER_STORY_BS";
        inline constexpr const char* StoryStrandStat = "MPX_FIXER_STORY_STRAND";
        inline constexpr const char* GeneralBitsStat = "MPX_FIXER_GENERAL_BS";
        inline constexpr const char* CompletedBitsStat = "MPX_FIXER_COMPLETED_BS";
        inline constexpr const char* StoryCooldownStat = "MPX_FIXER_STORY_COOLDOWN";
        inline constexpr const char* SafeCashStat = "MPX_FIXER_SAFE_CASH_VALUE";
        inline constexpr std::size_t SafeCollectGlobal = 2708850;

        inline constexpr const char* FinalePayoutTunable = "FIXER_FINALE_LEADER_CASH_REWARD";
        inline constexpr const char* StoryCooldownPosixTunable = "FIXER_STORY_COOLDOWN_POSIX";
        inline constexpr const char* SecurityContractCooldownTunable = "FIXER_SECURITY_CONTRACT_COOLDOWN_TIME";
        inline constexpr const char* PayphoneCooldownTunable = "REQUEST_FRANKLIN_PAYPHONE_HIT_COOLDOWN";
        inline constexpr int MaximumFinalePayout = 2500000;

        inline constexpr std::array<int, 12> StoryContractValues{
            3, 4, 12, 28, 60, 123, 254, 508, 1020, 2044, 2045, 4095,
        };

        [[nodiscard]] inline const char* SecurityContractName(int type) noexcept
        {
            switch (type)
            {
            case 0: return "Recover Valuables";
            case 1: return "Vehicle Recovery";
            case 2: return "Gang Termination";
            case 3: return "Rescue Operation";
            case 4: return "Asset Protection";
            case 5: return "Liquidize Assets";
            default: return "Unknown Security Contract";
            }
        }

        [[nodiscard]] inline const char* SecurityContractDifficultyName(int difficulty) noexcept
        {
            switch (difficulty)
            {
            case 0: return "Professional";
            case 1: return "Specialist";
            case 2: return "Specialist+";
            default: return "Unknown";
            }
        }

        [[nodiscard]] inline const char* StoryContractName(int value) noexcept
        {
            switch (value)
            {
            case 3: return "Reset / None";
            case 4: return "Nightclub Setup";
            case 12: return "Marina Setup";
            case 28: return "Nightlife Leak";
            case 60: return "Country Club Setup";
            case 123: return "Guest List Setup";
            case 254: return "High Society Leak";
            case 508: return "Davis Setup";
            case 1020: return "Ballas Setup";
            case 2044: return "South Central Leak";
            case 2045: return "Studio Time";
            case 4095: return "Don't Fuck With Dre";
            default: return "Unknown Dre Contract State";
            }
        }

        [[nodiscard]] inline int StoryStrandFromContract(int value) noexcept
        {
            if (value < 18)
                return 0;
            if (value < 128)
                return 1;
            if (value < 2044)
                return 2;
            return -1;
        }
    }

    struct AgencyContractSlot final
    {
        int type{-1};
        int difficulty{};
        int reward{};
        bool readable{};
    };

    struct AgencySnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool sessionStarted{};
        bool nativeReady{};
        bool globalsReady{};
        bool securityAppRunning{};
        bool securityContractDelayActive{};
        int playerId{-1};
        std::uint32_t generalFlags{};
        std::uint32_t completedFlags{};
        int payphoneBonusMethod{-1};
        std::uint32_t storyFlags{};
        int storyStrand{-1};
        int storyCooldown{};
        std::uint32_t fixerFlags{};
        std::uint32_t shortTrips{};
        int contractCount{};
        int earnings{};
        int persistentStoryBits{};
        int persistentStoryStrand{};
        int persistentGeneralBits{};
        int persistentCompletedBits{};
        int persistentStoryCooldown{};
        int safeCash{};
        int finalePayout{};
        bool finalePayoutReadable{};
        std::array<AgencyContractSlot, AgencyEnhanced173::ContractSlots> contracts{};
        std::string message{"Press Refresh Agency"};
    };

    class AgencyRuntime final
    {
    public:
        static AgencyRuntime& Get() noexcept
        {
            static AgencyRuntime instance;
            return instance;
        }

        [[nodiscard]] bool QueueRefresh()
        {
            return Queue("Reading Enhanced Agency state", [this] {
                AgencySnapshot state;
                const bool success = CaptureState(state);
                Finish(success, std::move(state), success ? "Agency state refreshed" : "Unable to read Enhanced Agency state");
            });
        }

        [[nodiscard]] bool QueueSetContractSlot(int slot, int type, int difficulty)
        {
            using namespace AgencyEnhanced173;
            if (slot < 0 || slot >= ContractSlots || type < 0 || type >= ContractTypes || difficulty < 0 || difficulty >= ContractDifficulties)
                return false;

            return Queue("Updating Security Contract board slot", [this, slot, type, difficulty] {
                AgencySnapshot before;
                if (!CaptureState(before))
                {
                    Finish(false, std::move(before), "Agency flow is unavailable");
                    return;
                }

                auto** globals = Script::ScriptRuntime::Get().Globals();
                if (!globals || before.playerId < 0)
                {
                    Finish(false, std::move(before), "Agency globals are unavailable");
                    return;
                }

                const auto flow = Script::ScriptGlobal(FlowGlobal)
                    .At(static_cast<std::size_t>(before.playerId), PlayerEntrySize)
                    .At(FixerFlowOffset);
                const auto contract = flow.At(ContractsArrayOffset).At(static_cast<std::size_t>(slot), ContractSize);

                auto* typeSlot = contract.At(0).As<std::int32_t>(globals);
                auto* difficultySlot = contract.At(1).As<std::int32_t>(globals);
                if (!typeSlot || !difficultySlot)
                {
                    Finish(false, std::move(before), "Security Contract slot is unavailable");
                    return;
                }

                const std::int32_t originalType = *typeSlot;
                const std::int32_t originalDifficulty = *difficultySlot;
                *typeSlot = type;
                *difficultySlot = difficulty;

                if (*typeSlot != type || *difficultySlot != difficulty)
                {
                    *typeSlot = originalType;
                    *difficultySlot = originalDifficulty;
                    AgencySnapshot rolledBack;
                    static_cast<void>(CaptureState(rolledBack));
                    Finish(false, std::move(rolledBack), "Security Contract write failed and was rolled back");
                    return;
                }

                AgencySnapshot after;
                if (!CaptureState(after)
                    || !after.contracts[static_cast<std::size_t>(slot)].readable
                    || after.contracts[static_cast<std::size_t>(slot)].type != type
                    || after.contracts[static_cast<std::size_t>(slot)].difficulty != difficulty)
                {
                    *typeSlot = originalType;
                    *difficultySlot = originalDifficulty;
                    AgencySnapshot rolledBack;
                    static_cast<void>(CaptureState(rolledBack));
                    Finish(false, std::move(rolledBack), "Security Contract verification failed and was rolled back");
                    return;
                }

                Finish(true, std::move(after), "Security Contract board slot updated; reopen The Contract app if it was already open");
            });
        }

        [[nodiscard]] bool QueueApplyStoryContract(int contract)
        {
            using namespace AgencyEnhanced173;
            bool valid = false;
            for (const int value : StoryContractValues)
                valid = valid || value == contract;
            if (!valid)
                return false;

            return Queue("Applying Dr. Dre contract progression", [this, contract] {
                const auto oldStory = Stats::GetInt(StoryBitsStat);
                const auto oldStrand = Stats::GetInt(StoryStrandStat);
                const auto oldGeneral = Stats::GetInt(GeneralBitsStat);
                const auto oldCompleted = Stats::GetInt(CompletedBitsStat);
                if (!oldStory || !oldStrand || !oldGeneral || !oldCompleted)
                {
                    AgencySnapshot state;
                    static_cast<void>(CaptureState(state));
                    Finish(false, std::move(state), "Unable to capture current Agency story stats");
                    return;
                }

                const int strand = StoryStrandFromContract(contract);
                bool success = Stats::SetInt(StoryBitsStat, contract);
                success = Stats::SetInt(StoryStrandStat, strand) && success;
                success = Stats::SetInt(GeneralBitsStat, -1) && success;
                success = Stats::SetInt(CompletedBitsStat, -1) && success;

                const auto story = Stats::GetInt(StoryBitsStat);
                const auto verifyStrand = Stats::GetInt(StoryStrandStat);
                const auto general = Stats::GetInt(GeneralBitsStat);
                const auto completed = Stats::GetInt(CompletedBitsStat);
                success = success && story && *story == contract
                    && verifyStrand && *verifyStrand == strand
                    && general && *general == -1
                    && completed && *completed == -1;

                if (!success)
                {
                    static_cast<void>(Stats::SetInt(StoryBitsStat, *oldStory));
                    static_cast<void>(Stats::SetInt(StoryStrandStat, *oldStrand));
                    static_cast<void>(Stats::SetInt(GeneralBitsStat, *oldGeneral));
                    static_cast<void>(Stats::SetInt(CompletedBitsStat, *oldCompleted));
                    AgencySnapshot rolledBack;
                    static_cast<void>(CaptureState(rolledBack));
                    Finish(false, std::move(rolledBack), "Agency story progression verification failed and was rolled back");
                    return;
                }

                AgencySnapshot after;
                const bool captured = CaptureState(after);
                Finish(captured, std::move(after), captured
                    ? std::string("Dre contract/preps set to ") + StoryContractName(contract) + "; reopen the Agency computer if needed"
                    : "Agency story stats changed but state refresh failed");
            });
        }

        [[nodiscard]] bool QueueSetFinalePayout(int payout)
        {
            using namespace AgencyEnhanced173;
            if (payout < 0 || payout > MaximumFinalePayout)
                return false;

            return Queue("Updating Agency finale payout", [this, payout] {
                auto** globals = Script::ScriptRuntime::Get().Globals();
                const auto resolved = Tunables::TunableRegistry::Get().Resolve(FinalePayoutTunable);
                auto* target = globals && resolved ? resolved->As<std::int32_t>(globals) : nullptr;
                if (!target)
                {
                    AgencySnapshot state;
                    static_cast<void>(CaptureState(state));
                    Finish(false, std::move(state), "Agency finale payout tunable is unavailable");
                    return;
                }

                const std::int32_t original = *target;
                *target = payout;
                if (*target != payout)
                {
                    *target = original;
                    AgencySnapshot state;
                    static_cast<void>(CaptureState(state));
                    Finish(false, std::move(state), "Agency payout verification failed and was rolled back");
                    return;
                }

                AgencySnapshot after;
                const bool captured = CaptureState(after);
                Finish(captured, std::move(after), captured ? "Agency finale payout updated" : "Payout changed but state refresh failed");
            });
        }

        [[nodiscard]] bool QueueKillCooldowns()
        {
            using namespace AgencyEnhanced173;
            return Queue("Clearing Agency cooldowns", [this] {
                auto** globals = Script::ScriptRuntime::Get().Globals();
                if (!globals)
                {
                    AgencySnapshot state;
                    static_cast<void>(CaptureState(state));
                    Finish(false, std::move(state), "Script globals are unavailable");
                    return;
                }

                const std::array<const char*, 3> names{
                    StoryCooldownPosixTunable,
                    SecurityContractCooldownTunable,
                    PayphoneCooldownTunable,
                };
                std::array<std::int32_t*, 3> targets{};
                std::array<std::int32_t, 3> originals{};
                for (std::size_t index = 0; index < names.size(); ++index)
                {
                    const auto resolved = Tunables::TunableRegistry::Get().Resolve(names[index]);
                    targets[index] = globals && resolved ? resolved->As<std::int32_t>(globals) : nullptr;
                    if (!targets[index])
                    {
                        AgencySnapshot state;
                        static_cast<void>(CaptureState(state));
                        Finish(false, std::move(state), std::string("Agency tunable unavailable: ") + names[index]);
                        return;
                    }
                    originals[index] = *targets[index];
                }

                const auto oldCooldown = Stats::GetInt(StoryCooldownStat);
                if (!oldCooldown)
                {
                    AgencySnapshot state;
                    static_cast<void>(CaptureState(state));
                    Finish(false, std::move(state), "FIXER_STORY_COOLDOWN stat is unavailable");
                    return;
                }

                for (auto* target : targets)
                    *target = 0;
                bool success = Stats::SetInt(StoryCooldownStat, -1);
                for (const auto* target : targets)
                    success = success && target && *target == 0;
                const auto verifiedCooldown = Stats::GetInt(StoryCooldownStat);
                success = success && verifiedCooldown && *verifiedCooldown == -1;

                if (!success)
                {
                    for (std::size_t index = 0; index < targets.size(); ++index)
                        *targets[index] = originals[index];
                    static_cast<void>(Stats::SetInt(StoryCooldownStat, *oldCooldown));
                    AgencySnapshot rolledBack;
                    static_cast<void>(CaptureState(rolledBack));
                    Finish(false, std::move(rolledBack), "Agency cooldown verification failed and was rolled back");
                    return;
                }

                AgencySnapshot after;
                const bool captured = CaptureState(after);
                Finish(captured, std::move(after), captured ? "Dre, Security Contract and Payphone cooldowns cleared" : "Cooldowns changed but state refresh failed");
            });
        }

        [[nodiscard]] bool QueueCollectSafe()
        {
            using namespace AgencyEnhanced173;
            return Queue("Requesting Agency safe collection", [this] {
                AgencySnapshot before;
                if (!CaptureState(before))
                {
                    Finish(false, std::move(before), "Agency state is unavailable");
                    return;
                }
                if (before.safeCash <= 0)
                {
                    Finish(false, std::move(before), "Agency safe is empty");
                    return;
                }

                auto** globals = Script::ScriptRuntime::Get().Globals();
                auto* collect = globals ? Script::ScriptGlobal(SafeCollectGlobal).As<std::int32_t>(globals) : nullptr;
                if (!collect)
                {
                    Finish(false, std::move(before), "Agency safe-collect global is unavailable");
                    return;
                }

                const std::int32_t original = *collect;
                *collect = 1;
                if (*collect != 1)
                {
                    *collect = original;
                    Finish(false, std::move(before), "Agency safe-collect request failed");
                    return;
                }

                AgencySnapshot after;
                static_cast<void>(CaptureState(after));
                Finish(true, std::move(after), "Agency safe collection requested");
            });
        }

        [[nodiscard]] bool QueueClearStoryCooldown()
        {
            using namespace AgencyEnhanced173;
            return Queue("Clearing Dr. Dre story replay cooldown", [this] {
                AgencySnapshot before;
                if (!CaptureState(before))
                {
                    Finish(false, std::move(before), "Agency flow is unavailable");
                    return;
                }

                auto** globals = Script::ScriptRuntime::Get().Globals();
                if (!globals || before.playerId < 0)
                {
                    Finish(false, std::move(before), "Agency globals are unavailable");
                    return;
                }

                const auto flow = Script::ScriptGlobal(FlowGlobal)
                    .At(static_cast<std::size_t>(before.playerId), PlayerEntrySize)
                    .At(FixerFlowOffset);
                auto* cooldown = flow.At(StoryCooldownOffset).As<std::int32_t>(globals);
                if (!cooldown)
                {
                    Finish(false, std::move(before), "Dr. Dre story cooldown slot is unavailable");
                    return;
                }

                const std::int32_t original = *cooldown;
                *cooldown = 0;
                if (*cooldown != 0)
                {
                    *cooldown = original;
                    AgencySnapshot rolledBack;
                    static_cast<void>(CaptureState(rolledBack));
                    Finish(false, std::move(rolledBack), "Dr. Dre cooldown write failed and was rolled back");
                    return;
                }

                AgencySnapshot after;
                if (!CaptureState(after) || after.storyCooldown != 0)
                {
                    *cooldown = original;
                    AgencySnapshot rolledBack;
                    static_cast<void>(CaptureState(rolledBack));
                    Finish(false, std::move(rolledBack), "Dr. Dre cooldown verification failed and was rolled back");
                    return;
                }

                Finish(true, std::move(after), "Dr. Dre story replay cooldown cleared");
            });
        }

        [[nodiscard]] bool QueueClearSecurityContractDelay()
        {
            using namespace AgencyEnhanced173;
            return Queue("Clearing Security Contract short delay", [this] {
                AgencySnapshot before;
                if (!CaptureState(before))
                {
                    Finish(false, std::move(before), "Agency flow is unavailable");
                    return;
                }

                auto** globals = Script::ScriptRuntime::Get().Globals();
                if (!globals)
                {
                    Finish(false, std::move(before), "Agency globals are unavailable");
                    return;
                }

                auto* flags = Script::ScriptGlobal(SecuritySessionGlobal).At(SecuritySessionFlagsOffset).As<std::int32_t>(globals);
                if (!flags)
                {
                    Finish(false, std::move(before), "Security Contract delay state is unavailable");
                    return;
                }

                const std::int32_t original = *flags;
                const std::uint32_t cleared = static_cast<std::uint32_t>(original) & ~SecurityContractDelayMask;
                *flags = static_cast<std::int32_t>(cleared);
                if ((static_cast<std::uint32_t>(*flags) & SecurityContractDelayMask) != 0)
                {
                    *flags = original;
                    AgencySnapshot rolledBack;
                    static_cast<void>(CaptureState(rolledBack));
                    Finish(false, std::move(rolledBack), "Security Contract delay write failed and was rolled back");
                    return;
                }

                AgencySnapshot after;
                if (!CaptureState(after) || after.securityContractDelayActive)
                {
                    *flags = original;
                    AgencySnapshot rolledBack;
                    static_cast<void>(CaptureState(rolledBack));
                    Finish(false, std::move(rolledBack), "Security Contract delay verification failed and was rolled back");
                    return;
                }

                Finish(true, std::move(after), "Current Security Contract short delay cleared");
            });
        }

        [[nodiscard]] AgencySnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            AgencySnapshot state = m_Snapshot;
            state.pending = m_Pending.load(std::memory_order_acquire);
            return state;
        }

    private:
        AgencyRuntime() = default;
        AgencyRuntime(const AgencyRuntime&) = delete;
        AgencyRuntime& operator=(const AgencyRuntime&) = delete;

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

            AgencySnapshot state;
            Finish(false, std::move(state), "GTA script-thread queue is unavailable");
            return false;
        }

        [[nodiscard]] bool CaptureState(AgencySnapshot& state) const noexcept
        {
            using namespace AgencyEnhanced173;

            const bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            state.sessionStarted = sessionStarted && *sessionStarted;
            state.nativeReady = Native::NativeRegistry::Get().CanInvokeOnCurrentThread();

            auto& scripts = Script::ScriptRuntime::Get();
            auto** globals = scripts.Globals();
            state.globalsReady = globals != nullptr;
            if (scripts.IsReady())
            {
                if (const auto* thread = scripts.FindThread(SecurityAppHash))
                {
                    state.securityAppRunning = thread->context.threadId != 0
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
                .At(FixerFlowOffset);

            const auto* generalFlags = flow.At(GeneralFlagsOffset).As<std::int32_t>(globals);
            const auto* completedFlags = flow.At(CompletedFlagsOffset).As<std::int32_t>(globals);
            const auto* payphoneBonus = flow.At(PayphoneBonusMethodOffset).As<std::int32_t>(globals);
            const auto* storyFlags = flow.At(StoryFlagsOffset).As<std::int32_t>(globals);
            const auto* storyStrand = flow.At(StoryStrandOffset).As<std::int32_t>(globals);
            const auto* storyCooldown = flow.At(StoryCooldownOffset).As<std::int32_t>(globals);
            const auto* fixerFlags = flow.At(FixerFlagsOffset).As<std::int32_t>(globals);
            const auto* shortTrips = flow.At(ShortTripsOffset).As<std::int32_t>(globals);
            const auto* contractCount = flow.At(ContractCountOffset).As<std::int32_t>(globals);
            const auto* earnings = flow.At(EarningsOffset).As<std::int32_t>(globals);
            if (!generalFlags || !completedFlags || !payphoneBonus || !storyFlags || !storyStrand
                || !storyCooldown || !fixerFlags || !shortTrips || !contractCount || !earnings)
                return false;

            state.generalFlags = static_cast<std::uint32_t>(*generalFlags);
            state.completedFlags = static_cast<std::uint32_t>(*completedFlags);
            state.payphoneBonusMethod = *payphoneBonus;
            state.storyFlags = static_cast<std::uint32_t>(*storyFlags);
            state.storyStrand = *storyStrand;
            state.storyCooldown = *storyCooldown;
            state.fixerFlags = static_cast<std::uint32_t>(*fixerFlags);
            state.shortTrips = static_cast<std::uint32_t>(*shortTrips);
            state.contractCount = *contractCount;
            state.earnings = *earnings;

            if (const auto* sessionFlags = Script::ScriptGlobal(SecuritySessionGlobal).At(SecuritySessionFlagsOffset).As<std::int32_t>(globals))
            {
                state.securityContractDelayActive = (static_cast<std::uint32_t>(*sessionFlags) & SecurityContractDelayMask) != 0;
            }

            const auto contracts = flow.At(ContractsArrayOffset);
            for (std::size_t index = 0; index < state.contracts.size(); ++index)
            {
                const auto contract = contracts.At(index, ContractSize);
                const auto* type = contract.At(0).As<std::int32_t>(globals);
                const auto* difficulty = contract.At(1).As<std::int32_t>(globals);
                const auto* reward = contract.At(2).As<std::int32_t>(globals);
                if (!type || !difficulty || !reward)
                    continue;

                auto& destination = state.contracts[index];
                destination.type = *type;
                destination.difficulty = *difficulty;
                destination.reward = *reward;
                destination.readable = true;
            }

            if (const auto value = Stats::GetInt(StoryBitsStat))
                state.persistentStoryBits = *value;
            if (const auto value = Stats::GetInt(StoryStrandStat))
                state.persistentStoryStrand = *value;
            if (const auto value = Stats::GetInt(GeneralBitsStat))
                state.persistentGeneralBits = *value;
            if (const auto value = Stats::GetInt(CompletedBitsStat))
                state.persistentCompletedBits = *value;
            if (const auto value = Stats::GetInt(StoryCooldownStat))
                state.persistentStoryCooldown = *value;
            if (const auto value = Stats::GetInt(SafeCashStat))
                state.safeCash = *value;

            if (const auto payout = Tunables::TunableRegistry::Get().Resolve(FinalePayoutTunable))
            {
                if (const auto* value = payout->As<std::int32_t>(globals))
                {
                    state.finalePayout = *value;
                    state.finalePayoutReadable = true;
                }
            }

            return true;
        }

        void Finish(bool success, AgencySnapshot state, std::string message) noexcept
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
        AgencySnapshot m_Snapshot{};
    };
}
