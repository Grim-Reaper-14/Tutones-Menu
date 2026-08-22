#pragma once

#include "EnhancedCatalog.hpp"
#include "../../game/script/ScriptPatchRuntime.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>

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

    struct NetworkPlayerSnapshot final
    {
        int id{-1};
        bool active{};
        bool local{};
        bool freemodeHost{};
        std::string name{};

        bool managerSlotPresent{};
        bool managerIndexMatches{};
        bool managerLocalFlag{};
        int activeIndex{-1};

        bool statsReadable{};
        int currentActivity{-1};
        int missionScriptInstance{-1};
        int team{-1};
        int rank{};
        int rp{};
        int crewRp{};
        int walletBalance{};
        int money{};
        float kdRatio{};
        int killsOnPlayers{};
        int deathsByPlayers{};
        int racesWon{};
        int racesLost{};
        int deathmatchesWon{};
        int deathmatchesLost{};
        int missionWins{};
        int totalMissionsPlayed{};
        int survivalWins{};
        int totalSurvivalsPlayed{};
        int communicationRestrictions{};
        bool canSpectate{};
        float weaponAccuracy{};
        std::uint32_t favoriteVehicleHash{};
        std::uint32_t favoriteWeaponHash{};

        int ped{};
        bool pedAvailable{};
        bool healthReadable{};
        int health{};
        int maxHealth{};
        bool armourReadable{};
        int armour{};
        bool wantedReadable{};
        int wantedLevel{};
        bool positionReadable{};
        float x{};
        float y{};
        float z{};
        bool distanceReadable{};
        float distance{};

        bool vehicleReadable{};
        int vehicle{};
        std::uint32_t vehicleModelHash{};
        int vehicleClass{-1};
        std::string vehicleMake{};
        std::string vehicleName{};
        std::string vehiclePlate{};

        bool latencyReadable{};
        float averageLatency{};
        bool packetLossReadable{};
        float averagePacketLoss{};
        bool resendReadable{};
        int highestReliableResendCount{};
    };

    struct NetworkPlayerRosterSnapshot final
    {
        bool backendReady{};
        bool managerReady{};
        int localPlayer{-1};
        int freemodeHost{-1};
        int freemodeParticipants{-1};
        int activeCount{};
        int managerMaxPlayers{};
        int managerLoadedPlayers{};
        int managerPhysicalPlayers{};
        int managerNonLocalPhysicalPlayers{};
        std::uint64_t generation{};
        std::array<NetworkPlayerSnapshot, 32> players{};
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
        NetworkPlayerRosterSnapshot playerRoster{};

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
        void RefreshPlayerRosterOnGameThread(bool sessionStarted, std::int64_t** globals) noexcept;
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
        NetworkPlayerRosterSnapshot m_PlayerRoster{};
        std::chrono::steady_clock::time_point m_LastPlayerRosterRefresh{};
        mutable std::mutex m_Mutex;
        NetworkSnapshot m_Snapshot{};
    };
}
