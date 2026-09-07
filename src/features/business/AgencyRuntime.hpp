#pragma once

#include "../../game/GamePointers.hpp"
#include "../../game/PlayerNatives.hpp"
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
            return Queue("Reading Enhanced Agency contract-board state", [this] {
                AgencySnapshot state;
                const bool success = CaptureState(state);
                Finish(
                    success,
                    std::move(state),
                    success
                        ? "Agency contract-board state refreshed"
                        : "Unable to read Enhanced Agency state");
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
            {
                return false;
            }

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

            const auto contracts = flow.At(ContractsArrayOffset);
            bool anyContract = false;
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
                anyContract = true;
            }

            return anyContract || state.contractCount >= 0;
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
