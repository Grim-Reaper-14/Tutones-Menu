#include "NetworkRuntime.hpp"

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/NetshoppingNatives.hpp"
#include "../../game/NetworkPlayerManager.hpp"
#include "../../game/NetworkPlayerNatives.hpp"
#include "../../game/PlayerNatives.hpp"
#include "../../game/VehicleNatives.hpp"
#include "../../game/native/NativeInvoker.hpp"
#include "../../game/script/ScriptFunction.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../game/script/ScriptPointer.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Tutones::Game::NetworkFeatures
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
        constexpr std::uint32_t ShopControllerHash = Joaat("shop_controller");
        constexpr std::uint32_t ServiceThresholdCategory = Joaat("CATEGORY_SERVICE_WITH_THRESHOLD");
        constexpr std::size_t PhoneCallStateGlobal = 23040;
        constexpr std::size_t PhoneCallInProgressGlobal = 23046;
        constexpr std::size_t IncomingCallGlobal = 23050;
        constexpr std::size_t CallingCharacterGlobal = 8818;

        constexpr std::size_t GpbdFmGlobal = 1845347;
        constexpr std::size_t GpbdFmEntryStride = 884;
        constexpr std::size_t EntryCurrentActivityOffset = 0;
        constexpr std::size_t EntryMissionScriptInstanceOffset = 1;
        constexpr std::size_t PlayerStatsOffset = 205;
        constexpr std::size_t PlayerStatsTeamOffset = 0;
        constexpr std::size_t PlayerStatsRpOffset = 1;
        constexpr std::size_t PlayerStatsCrewRpOffset = 2;
        constexpr std::size_t PlayerStatsWalletBalanceOffset = 3;
        constexpr std::size_t PlayerStatsRankOffset = 6;
        constexpr std::size_t PlayerStatsRacesWonOffset = 15;
        constexpr std::size_t PlayerStatsRacesLostOffset = 16;
        constexpr std::size_t PlayerStatsDeathmatchesWonOffset = 20;
        constexpr std::size_t PlayerStatsDeathmatchesLostOffset = 21;
        constexpr std::size_t PlayerStatsKdRatioOffset = 26;
        constexpr std::size_t PlayerStatsKillsOnPlayersOffset = 28;
        constexpr std::size_t PlayerStatsDeathsByPlayersOffset = 29;
        constexpr std::size_t PlayerStatsMissionWinsOffset = 45;
        constexpr std::size_t PlayerStatsTotalMissionsOffset = 46;
        constexpr std::size_t PlayerStatsSurvivalWinsOffset = 47;
        constexpr std::size_t PlayerStatsTotalSurvivalsOffset = 48;
        constexpr std::size_t PlayerStatsCommunicationRestrictionsOffset = 51;
        constexpr std::size_t PlayerStatsCanSpectateOffset = 52;
        constexpr std::size_t PlayerStatsMoneyOffset = 56;
        constexpr std::size_t PlayerStatsWeaponAccuracyOffset = 57;
        constexpr std::size_t PlayerStatsFavoriteVehicleOffset = 58;
        constexpr std::size_t PlayerStatsFavoriteWeaponOffset = 59;
        constexpr auto PlayerRosterRefreshInterval = std::chrono::milliseconds(500);

        [[nodiscard]] std::optional<Native::NativeVector3> EntityCoords(int entity) noexcept
        {
            if (entity == 0)
                return std::nullopt;

            return Native::NativeInvoker::Invoke<Native::NativeVector3>(
                Native::NativeId::GetEntityCoords,
                entity,
                std::int32_t{0});
        }

        [[nodiscard]] std::string ResolveVehicleLabel(const char* label)
        {
            if (!label || !*label)
                return {};

            if (const auto localized = VehicleNatives::GetLabelText(label);
                localized && *localized && **localized && std::string_view(*localized) != "NULL")
            {
                return std::string(*localized);
            }

            return std::string(label);
        }
    }

    NetworkRuntime& NetworkRuntime::Get() noexcept
    {
        static NetworkRuntime instance;
        return instance;
    }

    bool NetworkRuntime::Start()
    {
        bool expected = false;
        if (!m_Running.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            return true;

        m_LastSilencedCaller = -1;
        m_SilencedCalls = 0;
        m_PlayerRoster = {};
        m_LastPlayerRosterRefresh = {};
        m_TransactionPending.store(false, std::memory_order_release);

        auto& patches = Script::ScriptPatchRuntime::Get();
        if (!patches.Start())
        {
            m_Running.store(false, std::memory_order_release);
            return false;
        }

        m_DeathBarrierPatch = patches.AddPatch(
            FreemodeHash,
            Script::ScriptPointer(
                "DeathBarriersPatch",
                "2D 01 09 00 00 5D ? ? ? 56 ? ? 3A").Add(5),
            std::vector<std::uint8_t>{0x2E, 0x01, 0x00});

        if (m_DeathBarrierPatch == 0 || !QueueNextTick())
        {
            if (m_DeathBarrierPatch != 0)
                patches.RemovePatch(m_DeathBarrierPatch);
            m_DeathBarrierPatch = 0;
            patches.Stop();
            m_Running.store(false, std::memory_order_release);
            TUTONES_LOG_ERROR("network.runtime", "Network Enhancement runtime could not start");
            return false;
        }

        TUTONES_LOG_INFO("network.runtime", "Enhanced network/QoL runtime scheduled on the GTA script thread");
        return true;
    }

    void NetworkRuntime::Stop() noexcept
    {
        if (!m_Running.exchange(false, std::memory_order_acq_rel))
            return;

        m_SilencePhoneCalls.store(false, std::memory_order_release);
        m_DisableDeathBarriers.store(false, std::memory_order_release);
        m_TransactionPending.store(false, std::memory_order_release);

        auto& patches = Script::ScriptPatchRuntime::Get();
        if (m_DeathBarrierPatch != 0)
        {
            static_cast<void>(patches.SetPatchEnabled(m_DeathBarrierPatch, false));
            patches.RemovePatch(m_DeathBarrierPatch);
            m_DeathBarrierPatch = 0;
        }
        patches.Stop();

        m_PlayerRoster = {};
        m_LastPlayerRosterRefresh = {};
        std::scoped_lock lock(m_Mutex);
        m_Snapshot = {};
        TUTONES_LOG_INFO("network.runtime", "Enhanced network/QoL runtime stopped");
    }

    bool NetworkRuntime::IsRunning() const noexcept
    {
        return m_Running.load(std::memory_order_acquire);
    }

    void NetworkRuntime::SetSilencePhoneCalls(bool enabled) noexcept
    {
        m_SilencePhoneCalls.store(enabled, std::memory_order_release);
    }

    void NetworkRuntime::SetDisableDeathBarriers(bool enabled) noexcept
    {
        m_DisableDeathBarriers.store(enabled, std::memory_order_release);
    }

    bool NetworkRuntime::QueueServiceTransaction(std::uint32_t serviceHash)
    {
        if (!IsRunning() || serviceHash == 0)
            return false;

        bool expected = false;
        if (!m_TransactionPending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            return false;

        RecordServiceTransaction(serviceHash, 0, -1, ServiceTransactionResult::Queued);
        if (Runtime::GameRuntime::Get().Enqueue([this, serviceHash] {
                ExecuteServiceTransactionOnGameThread(serviceHash);
            }))
        {
            return true;
        }

        RecordServiceTransaction(serviceHash, 0, -1, ServiceTransactionResult::DispatchFailed);
        return false;
    }

    NetworkSnapshot NetworkRuntime::Snapshot() const noexcept
    {
        std::scoped_lock lock(m_Mutex);
        NetworkSnapshot snapshot = m_Snapshot;
        snapshot.transactionPending = m_TransactionPending.load(std::memory_order_acquire);
        return snapshot;
    }

    bool NetworkRuntime::QueueNextTick()
    {
        if (!IsRunning())
            return false;
        return Runtime::GameRuntime::Get().Enqueue([this] { TickOnGameThread(); });
    }

    void NetworkRuntime::TickOnGameThread() noexcept
    {
        if (!IsRunning())
            return;

        NetworkSnapshot next{};
        next.running = true;
        next.silencePhoneCalls = m_SilencePhoneCalls.load(std::memory_order_acquire);
        next.disableDeathBarriers = m_DisableDeathBarriers.load(std::memory_order_acquire);

        auto& scriptRuntime = Script::ScriptRuntime::Get();
        auto** globals = scriptRuntime.Globals();
        bool* sessionStarted = GamePointers::Get().IsSessionStarted();
        next.scriptGlobalsReady = globals != nullptr;
        next.sessionStarted = sessionStarted && *sessionStarted;

        const auto now = std::chrono::steady_clock::now();
        if (!next.sessionStarted)
        {
            if (m_PlayerRoster.backendReady || m_PlayerRoster.activeCount != 0 || m_PlayerRoster.localPlayer >= 0)
            {
                const auto generation = m_PlayerRoster.generation + 1;
                m_PlayerRoster = {};
                m_PlayerRoster.generation = generation;
            }
            m_LastPlayerRosterRefresh = {};
        }
        else if (m_LastPlayerRosterRefresh.time_since_epoch().count() == 0
            || now - m_LastPlayerRosterRefresh >= PlayerRosterRefreshInterval)
        {
            RefreshPlayerRosterOnGameThread(true, globals);
            m_LastPlayerRosterRefresh = now;
        }
        next.playerRoster = m_PlayerRoster;

        next.cooldowns = SampleCooldownTunables(globals);
        next.rewards = SampleRewardTunables(globals);
        if (globals)
        {
            int* phoneState = Script::ScriptGlobal(PhoneCallStateGlobal).As<int>(globals);
            int* inProgress = Script::ScriptGlobal(PhoneCallInProgressGlobal).As<int>(globals);
            int* incoming = Script::ScriptGlobal(IncomingCallGlobal).As<int>(globals);
            int* caller = Script::ScriptGlobal(CallingCharacterGlobal).As<int>(globals);
            next.phoneGlobalsReady = phoneState && inProgress && incoming && caller;

            if (next.silencePhoneCalls && next.sessionStarted && next.phoneGlobalsReady
                && *phoneState != 0 && *phoneState != 5 && *phoneState != 6
                && *inProgress != 0 && *incoming != 0)
            {
                *phoneState = 6;
                m_LastSilencedCaller = *caller;
                ++m_SilencedCalls;
            }
        }

        auto& patches = Script::ScriptPatchRuntime::Get();
        auto patchStatus = patches.Status(m_DeathBarrierPatch);
        const bool shouldEnable = next.disableDeathBarriers && patchStatus.supported;
        static_cast<void>(patches.SetPatchEnabled(m_DeathBarrierPatch, shouldEnable));
        patchStatus = patches.Status(m_DeathBarrierPatch);
        next.patchHookActive = patches.HookActive();
        next.freemodeLoaded = scriptRuntime.FindProgram(FreemodeHash) != nullptr;
        next.deathBarrierSupported = patchStatus.supported;
        next.deathBarrierApplied = patchStatus.active;
        next.lastSilencedCaller = m_LastSilencedCaller;
        next.silencedCalls = m_SilencedCalls;
        PublishSnapshot(next);

        if (IsRunning() && !QueueNextTick())
        {
            TUTONES_LOG_ERROR("network.runtime", "Network Enhancement runtime lost its GTA script-thread scheduling slot");
            Stop();
        }
    }

    void NetworkRuntime::RefreshPlayerRosterOnGameThread(bool sessionStarted, std::int64_t** globals) noexcept
    {
        NetworkPlayerRosterSnapshot next{};
        next.generation = m_PlayerRoster.generation + 1;
        if (!sessionStarted)
        {
            m_PlayerRoster = std::move(next);
            return;
        }

        next.backendReady = NetworkPlayerNatives::Ready();
        if (!next.backendReady)
        {
            m_PlayerRoster = std::move(next);
            return;
        }

        auto* manager = NetworkPlayerManager::Get();
        if (manager)
        {
            next.managerReady = true;
            next.managerMaxPlayers = static_cast<int>(manager->maxPlayers);
            next.managerLoadedPlayers = manager->loadedPlayerCount;
            next.managerPhysicalPlayers = manager->physicalPlayerCount;
            next.managerNonLocalPhysicalPlayers = manager->nonLocalPhysicalPlayerCount;
        }

        const auto localPlayer = PlayerNatives::PlayerId();
        if (localPlayer && *localPlayer >= 0 && *localPlayer < 32)
            next.localPlayer = *localPlayer;
        else if (manager
            && NetworkPlayerManager::IsPlayerReadable(manager->localPlayer)
            && manager->localPlayer->playerIndex < 32)
        {
            next.localPlayer = static_cast<int>(manager->localPlayer->playerIndex);
        }

        if (const auto host = NetworkPlayerNatives::GetHostOfScript("freemode", -1, 0);
            host && *host >= 0 && *host < 32)
        {
            next.freemodeHost = *host;
        }

        if (const auto participants = NetworkPlayerNatives::GetNumScriptParticipants("freemode", -1, 0);
            participants && *participants >= 0 && *participants <= 32)
        {
            next.freemodeParticipants = *participants;
        }

        std::optional<Native::NativeVector3> localCoords;
        if (next.localPlayer >= 0)
        {
            const auto localPed = NetworkPlayerNatives::GetPlayerPedScriptIndex(next.localPlayer);
            if (localPed && *localPed != 0)
                localCoords = EntityCoords(*localPed);
        }

        for (int playerId = 0; playerId < 32; ++playerId)
        {
            const auto active = NetworkPlayerNatives::IsPlayerActive(playerId);
            if (!active || !*active)
                continue;

            auto& player = next.players[static_cast<std::size_t>(playerId)];
            player.id = playerId;
            player.active = true;
            player.local = playerId == next.localPlayer;
            player.freemodeHost = playerId == next.freemodeHost;
            player.name = "Player " + std::to_string(playerId);
            ++next.activeCount;

            if (manager)
            {
                auto* managerPlayer = manager->players[static_cast<std::size_t>(playerId)];
                if (NetworkPlayerManager::IsPlayerReadable(managerPlayer))
                {
                    player.managerSlotPresent = true;
                    player.managerIndexMatches = managerPlayer->playerIndex == playerId;
                    player.managerLocalFlag = managerPlayer->IsLocalFlagSet();
                    player.activeIndex = static_cast<int>(managerPlayer->activeIndex);
                }
            }

            if (const auto name = NetworkPlayerNatives::GetPlayerName(playerId); name && !name->empty())
                player.name = *name;

            if (globals)
            {
                const auto entry = Script::ScriptGlobal(GpbdFmGlobal)
                    .At(static_cast<std::size_t>(playerId), GpbdFmEntryStride);
                const auto stats = entry.At(PlayerStatsOffset);

                int* currentActivity = entry.At(EntryCurrentActivityOffset).As<int>(globals);
                int* missionScriptInstance = entry.At(EntryMissionScriptInstanceOffset).As<int>(globals);
                int* team = stats.At(PlayerStatsTeamOffset).As<int>(globals);
                int* rp = stats.At(PlayerStatsRpOffset).As<int>(globals);
                int* crewRp = stats.At(PlayerStatsCrewRpOffset).As<int>(globals);
                int* walletBalance = stats.At(PlayerStatsWalletBalanceOffset).As<int>(globals);
                int* rank = stats.At(PlayerStatsRankOffset).As<int>(globals);
                int* racesWon = stats.At(PlayerStatsRacesWonOffset).As<int>(globals);
                int* racesLost = stats.At(PlayerStatsRacesLostOffset).As<int>(globals);
                int* deathmatchesWon = stats.At(PlayerStatsDeathmatchesWonOffset).As<int>(globals);
                int* deathmatchesLost = stats.At(PlayerStatsDeathmatchesLostOffset).As<int>(globals);
                float* kdRatio = stats.At(PlayerStatsKdRatioOffset).As<float>(globals);
                int* killsOnPlayers = stats.At(PlayerStatsKillsOnPlayersOffset).As<int>(globals);
                int* deathsByPlayers = stats.At(PlayerStatsDeathsByPlayersOffset).As<int>(globals);
                int* missionWins = stats.At(PlayerStatsMissionWinsOffset).As<int>(globals);
                int* totalMissions = stats.At(PlayerStatsTotalMissionsOffset).As<int>(globals);
                int* survivalWins = stats.At(PlayerStatsSurvivalWinsOffset).As<int>(globals);
                int* totalSurvivals = stats.At(PlayerStatsTotalSurvivalsOffset).As<int>(globals);
                int* communicationRestrictions = stats.At(PlayerStatsCommunicationRestrictionsOffset).As<int>(globals);
                int* canSpectate = stats.At(PlayerStatsCanSpectateOffset).As<int>(globals);
                int* money = stats.At(PlayerStatsMoneyOffset).As<int>(globals);
                float* weaponAccuracy = stats.At(PlayerStatsWeaponAccuracyOffset).As<float>(globals);
                std::uint32_t* favoriteVehicle = stats.At(PlayerStatsFavoriteVehicleOffset).As<std::uint32_t>(globals);
                std::uint32_t* favoriteWeapon = stats.At(PlayerStatsFavoriteWeaponOffset).As<std::uint32_t>(globals);

                if (currentActivity && missionScriptInstance && team && rp && crewRp && walletBalance && rank
                    && racesWon && racesLost && deathmatchesWon && deathmatchesLost && kdRatio
                    && killsOnPlayers && deathsByPlayers && missionWins && totalMissions
                    && survivalWins && totalSurvivals && communicationRestrictions && canSpectate
                    && money && weaponAccuracy && favoriteVehicle && favoriteWeapon)
                {
                    player.statsReadable = true;
                    player.currentActivity = *currentActivity;
                    player.missionScriptInstance = *missionScriptInstance;
                    player.team = *team;
                    player.rp = *rp;
                    player.crewRp = *crewRp;
                    player.walletBalance = *walletBalance;
                    player.rank = *rank;
                    player.racesWon = *racesWon;
                    player.racesLost = *racesLost;
                    player.deathmatchesWon = *deathmatchesWon;
                    player.deathmatchesLost = *deathmatchesLost;
                    player.kdRatio = std::isfinite(*kdRatio) ? *kdRatio : 0.0f;
                    player.killsOnPlayers = *killsOnPlayers;
                    player.deathsByPlayers = *deathsByPlayers;
                    player.missionWins = *missionWins;
                    player.totalMissionsPlayed = *totalMissions;
                    player.survivalWins = *survivalWins;
                    player.totalSurvivalsPlayed = *totalSurvivals;
                    player.communicationRestrictions = *communicationRestrictions;
                    player.canSpectate = *canSpectate != 0;
                    player.money = *money;
                    player.weaponAccuracy = std::isfinite(*weaponAccuracy) ? *weaponAccuracy : 0.0f;
                    player.favoriteVehicleHash = *favoriteVehicle;
                    player.favoriteWeaponHash = *favoriteWeapon;
                }
            }

            if (const auto wanted = PlayerNatives::GetPlayerWantedLevel(playerId))
            {
                player.wantedReadable = true;
                player.wantedLevel = *wanted;
            }

            if (const auto latency = NetworkPlayerNatives::GetAverageLatency(playerId);
                latency && std::isfinite(*latency) && *latency >= 0.0f)
            {
                player.latencyReadable = true;
                player.averageLatency = *latency;
            }

            if (const auto packetLoss = NetworkPlayerNatives::GetAveragePacketLoss(playerId);
                packetLoss && std::isfinite(*packetLoss) && *packetLoss >= 0.0f)
            {
                player.packetLossReadable = true;
                player.averagePacketLoss = *packetLoss;
            }

            if (const auto resend = NetworkPlayerNatives::GetHighestReliableResendCount(playerId);
                resend && *resend >= 0)
            {
                player.resendReadable = true;
                player.highestReliableResendCount = *resend;
            }

            const auto ped = NetworkPlayerNatives::GetPlayerPedScriptIndex(playerId);
            if (!ped || *ped == 0)
                continue;

            player.ped = *ped;
            player.pedAvailable = true;

            const auto health = PlayerNatives::GetEntityHealth(*ped);
            const auto maxHealth = PlayerNatives::GetEntityMaxHealth(*ped);
            if (health && maxHealth)
            {
                player.healthReadable = true;
                player.health = *health;
                player.maxHealth = *maxHealth;
            }

            if (const auto armour = PlayerNatives::GetPedArmour(*ped))
            {
                player.armourReadable = true;
                player.armour = *armour;
            }

            if (const auto vehicle = VehicleNatives::GetVehiclePedIsUsing(*ped); vehicle && *vehicle != 0)
            {
                player.vehicleReadable = true;
                player.vehicle = *vehicle;

                if (const auto model = PlayerNatives::GetEntityModel(*vehicle))
                {
                    player.vehicleModelHash = *model;
                    if (const auto vehicleClass = VehicleNatives::GetVehicleClassFromName(*model))
                        player.vehicleClass = *vehicleClass;
                    if (const auto makeLabel = VehicleNatives::GetMakeNameFromVehicleModel(*model); makeLabel && *makeLabel)
                        player.vehicleMake = ResolveVehicleLabel(*makeLabel);
                    if (const auto modelLabel = VehicleNatives::GetDisplayNameFromVehicleModel(*model); modelLabel && *modelLabel)
                        player.vehicleName = ResolveVehicleLabel(*modelLabel);
                }

                if (const auto plate = VehicleNatives::GetVehicleNumberPlateText(*vehicle))
                    player.vehiclePlate = *plate;
            }

            const auto coords = EntityCoords(*ped);
            if (!coords)
                continue;

            player.positionReadable = true;
            player.x = coords->x;
            player.y = coords->y;
            player.z = coords->z;

            if (localCoords)
            {
                const float dx = coords->x - localCoords->x;
                const float dy = coords->y - localCoords->y;
                const float dz = coords->z - localCoords->z;
                player.distance = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
                player.distanceReadable = std::isfinite(player.distance);
            }
        }

        m_PlayerRoster = std::move(next);
    }

    void NetworkRuntime::ExecuteServiceTransactionOnGameThread(std::uint32_t serviceHash) noexcept
    {
        bool* sessionStarted = GamePointers::Get().IsSessionStarted();
        if (!sessionStarted || !*sessionStarted)
        {
            RecordServiceTransaction(serviceHash, 0, -1, ServiceTransactionResult::SessionUnavailable);
            TUTONES_LOG_WARN("network.transaction", "Service transaction rejected because GTA Online is not active");
            return;
        }

        const auto serverTransactions = NetshoppingNatives::UseServerTransactions();
        if (!serverTransactions || !*serverTransactions)
        {
            RecordServiceTransaction(serviceHash, 0, -1, ServiceTransactionResult::ServerTransactionsUnavailable);
            TUTONES_LOG_WARN("network.transaction", "Server-backed NETSHOP transactions are unavailable");
            return;
        }

        const auto validCatalogItem = NetshoppingNatives::CatalogItemKeyIsValid(serviceHash);
        if (!validCatalogItem || !*validCatalogItem)
        {
            RecordServiceTransaction(serviceHash, 0, -1, ServiceTransactionResult::CatalogItemInvalid);
            TUTONES_LOG_WARN("network.transaction", "Requested service hash is not valid in the current NETSHOP catalog");
            return;
        }

        const auto price = NetshoppingNatives::GetPrice(serviceHash, ServiceThresholdCategory, true);
        if (!price || *price < 0)
        {
            RecordServiceTransaction(serviceHash, 0, -1, ServiceTransactionResult::PriceUnavailable);
            TUTONES_LOG_WARN("network.transaction", "NETSHOP could not resolve a current price for the service hash");
            return;
        }

        auto& scriptRuntime = Script::ScriptRuntime::Get();
        if (!scriptRuntime.IsReady()
            || !scriptRuntime.FindThread(ShopControllerHash)
            || !scriptRuntime.FindProgram(ShopControllerHash))
        {
            RecordServiceTransaction(serviceHash, *price, -1, ServiceTransactionResult::ShopControllerUnavailable);
            TUTONES_LOG_WARN("network.transaction", "shop_controller is unavailable for the service transaction helper");
            return;
        }

        static Script::ScriptFunction serviceTransaction(
            ShopControllerHash,
            Script::ScriptPointer(
                "ServiceTransaction",
                "2D 06 09 00 00 5D ? ? ? 06"));

        std::int32_t transactionIndex{-1};
        const bool dispatched = serviceTransaction.CallVoid(
            static_cast<std::int32_t>(serviceHash),
            static_cast<std::int32_t>(*price),
            &transactionIndex,
            std::int32_t{1},
            std::int32_t{0},
            std::int32_t{0});

        RecordServiceTransaction(
            serviceHash,
            *price,
            transactionIndex,
            dispatched ? ServiceTransactionResult::Dispatched : ServiceTransactionResult::ShopControllerUnavailable);

        if (dispatched)
            TUTONES_LOG_INFO("network.transaction", "Service transaction dispatched through shop_controller");
        else
            TUTONES_LOG_WARN("network.transaction", "Service transaction helper could not be resolved or executed");
    }

    void NetworkRuntime::RecordServiceTransaction(
        std::uint32_t serviceHash,
        int price,
        int transactionIndex,
        ServiceTransactionResult result) noexcept
    {
        if (result != ServiceTransactionResult::Queued)
            m_TransactionPending.store(false, std::memory_order_release);

        std::scoped_lock lock(m_Mutex);
        m_Snapshot.transactionPending = m_TransactionPending.load(std::memory_order_acquire);
        m_Snapshot.lastTransactionHash = serviceHash;
        m_Snapshot.lastTransactionPrice = price;
        m_Snapshot.lastTransactionIndex = transactionIndex;
        m_Snapshot.lastTransactionResult = result;
    }

    void NetworkRuntime::PublishSnapshot(const NetworkSnapshot& snapshot) noexcept
    {
        std::scoped_lock lock(m_Mutex);
        NetworkSnapshot merged = snapshot;
        merged.transactionPending = m_TransactionPending.load(std::memory_order_acquire);
        merged.lastTransactionHash = m_Snapshot.lastTransactionHash;
        merged.lastTransactionPrice = m_Snapshot.lastTransactionPrice;
        merged.lastTransactionIndex = m_Snapshot.lastTransactionIndex;
        merged.lastTransactionResult = m_Snapshot.lastTransactionResult;
        m_Snapshot = std::move(merged);
    }
}
