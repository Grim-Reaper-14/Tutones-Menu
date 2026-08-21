#pragma once

#include "EnhancedCatalog.hpp"
#include "../../game/script/ScriptPatchRuntime.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>

namespace Tutones::Game::NetworkFeatures
{
    enum class ServiceTransactionResult : std::uint8_t
    {
        None,
        Queued,
        Dispatched,
        SessionUnavailable,
        ServerTransactionsUnavailable,
        CatalogItemInvalid,
        PriceUnavailable,
        ShopControllerUnavailable,
        DispatchFailed,
    };

    struct NetworkSnapshot final
    {
        bool running{};
        bool sessionStarted{};
        bool scriptGlobalsReady{};
        bool silencePhoneCalls{};
        bool phoneGlobalsReady{};
        int lastSilencedCaller{-1};
        std::uint64_t silencedCalls{};
        bool disableDeathBarriers{};
        bool patchHookActive{};
        bool freemodeLoaded{};
        bool deathBarrierSupported{};
        bool deathBarrierApplied{};
        CooldownObservations cooldowns{};
        RewardObservations rewards{};

        bool transactionPending{};
        std::uint32_t lastTransactionHash{};
        int lastTransactionPrice{};
        int lastTransactionIndex{-1};
        ServiceTransactionResult lastTransactionResult{ServiceTransactionResult::None};
    };

    class NetworkRuntime final
    {
    public:
        static NetworkRuntime& Get() noexcept;

        bool Start();
        void Stop() noexcept;
        [[nodiscard]] bool IsRunning() const noexcept;
        void SetSilencePhoneCalls(bool enabled) noexcept;
        void SetDisableDeathBarriers(bool enabled) noexcept;
        [[nodiscard]] bool QueueServiceTransaction(std::uint32_t serviceHash);
        [[nodiscard]] NetworkSnapshot Snapshot() const noexcept;

    private:
        NetworkRuntime() = default;
        ~NetworkRuntime() = default;
        NetworkRuntime(const NetworkRuntime&) = delete;
        NetworkRuntime& operator=(const NetworkRuntime&) = delete;

        bool QueueNextTick();
        void TickOnGameThread() noexcept;
        void ExecuteServiceTransactionOnGameThread(std::uint32_t serviceHash) noexcept;
        void RecordServiceTransaction(
            std::uint32_t serviceHash,
            int price,
            int transactionIndex,
            ServiceTransactionResult result) noexcept;
        void PublishSnapshot(const NetworkSnapshot& snapshot) noexcept;

        std::atomic<bool> m_Running{false};
        std::atomic<bool> m_SilencePhoneCalls{false};
        std::atomic<bool> m_DisableDeathBarriers{false};
        std::atomic<bool> m_TransactionPending{false};
        Script::ScriptPatchHandle m_DeathBarrierPatch{};
        int m_LastSilencedCaller{-1};
        std::uint64_t m_SilencedCalls{};
        mutable std::mutex m_Mutex;
        NetworkSnapshot m_Snapshot{};
    };
}
