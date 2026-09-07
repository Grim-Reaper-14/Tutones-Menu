#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/Stats.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::Heist
{
    namespace CasinoHeistEnhanced173
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

        inline constexpr std::uint32_t PlanningHash = Joaat("gb_casino_heist_planning");

        // Enhanced 1.73 planning root used by gb_casino_heist_planning and the
        // current YimMenuV2 Enhanced Casino cut implementation.
        inline constexpr std::size_t PlanningGlobal = 1973762;
        inline constexpr std::size_t CutsOffsetA = 1497;
        inline constexpr std::size_t CutsOffsetB = 736;
        inline constexpr std::size_t CutsOffsetC = 92;

        inline constexpr int TargetCount = 4;
        inline constexpr std::array<const char*, TargetCount> TargetNames{
            "Cash",
            "Gold",
            "Artwork",
            "Diamonds",
        };

        inline constexpr int ApproachCount = 3;
        inline constexpr std::array<const char*, ApproachCount> ApproachNames{
            "Silent & Sneaky",
            "The Big Con",
            "Aggressive",
        };

        inline constexpr int CrewCount = 6;
        inline constexpr int WeaponLoadoutCount = 2;
        inline constexpr int VehicleCount = 4;
        inline constexpr int AllPoiMask = 1023;
        inline constexpr int AllAccessPointsMask = 2047;
        inline constexpr int MaxCut = 100;

        using CutArray = std::array<int, 4>;

        [[nodiscard]] inline const char* TargetName(int target) noexcept
        {
            if (target < 0 || target >= TargetCount)
                return "Unknown";
            return TargetNames[static_cast<std::size_t>(target)];
        }

        [[nodiscard]] inline const char* ApproachName(int approach) noexcept
        {
            if (approach < 1 || approach > ApproachCount)
                return "Unknown";
            return ApproachNames[static_cast<std::size_t>(approach - 1)];
        }

        [[nodiscard]] inline const char* GunmanName(int gunman) noexcept
        {
            switch (gunman)
            {
            case 1: return "Karl Abolaji";
            case 2: return "Gustavo Mota";
            case 3: return "Charlie Reed";
            case 4: return "Chester McCoy";
            case 5: return "Patrick McReary";
            case 6: return "Remove Gunman";
            default: return "Unknown";
            }
        }

        [[nodiscard]] inline const char* DriverName(int driver) noexcept
        {
            switch (driver)
            {
            case 1: return "Karim Denz";
            case 2: return "Taliana Martinez";
            case 3: return "Eddie Toh";
            case 4: return "Zach Nelson";
            case 5: return "Chester McCoy";
            case 6: return "Remove Driver";
            default: return "Unknown";
            }
        }

        [[nodiscard]] inline const char* HackerName(int hacker) noexcept
        {
            switch (hacker)
            {
            case 1: return "Rickie Lukens";
            case 2: return "Christian Feltz";
            case 3: return "Yohan Blair";
            case 4: return "Avi Schwartzman";
            case 5: return "Paige Harris";
            case 6: return "Remove Hacker";
            default: return "Unknown";
            }
        }

        [[nodiscard]] inline const char* WeaponLoadoutName(int gunman, int approach, int loadout) noexcept
        {
            if (loadout < 0 || loadout >= WeaponLoadoutCount)
                return "Unknown";

            const bool second = loadout == 1;
            switch (gunman)
            {
            case 1:
                if (approach == 1) return second ? "Machine Pistol Loadout" : "Micro SMG Loadout";
                if (approach == 2) return second ? "Shotgun Loadout" : "Micro SMG Loadout";
                if (approach == 3) return second ? "Revolver Loadout" : "Shotgun Loadout";
                break;
            case 2:
                return second ? "Shotgun Loadout" : "Rifle Loadout";
            case 3:
                if (approach == 2) return second ? "Shotgun Loadout" : "Machine Pistol Loadout";
                return second ? "Shotgun Loadout" : "SMG Loadout";
            case 4:
                if (approach == 2) return second ? "MK II Rifle Loadout" : "MK II SMG Loadout";
                return second ? "MK II Rifle Loadout" : "MK II Shotgun Loadout";
            case 5:
                if (approach == 1) return second ? "Rifle Loadout" : "Combat PDW Loadout";
                if (approach == 2) return second ? "Rifle Loadout" : "Shotgun Loadout";
                if (approach == 3) return second ? "Combat MG Loadout" : "Shotgun Loadout";
                break;
            default:
                break;
            }

            return second ? "Loadout 2" : "Loadout 1";
        }

        [[nodiscard]] inline const char* GetawayVehicleName(int driver, int vehicle) noexcept
        {
            if (vehicle < 0 || vehicle >= VehicleCount)
                return "Unknown";

            static constexpr std::array<const char*, VehicleCount> Karim{
                "Issi Classic", "Asbo", "Kanjo", "Sentinel Classic"};
            static constexpr std::array<const char*, VehicleCount> Taliana{
                "Retinue MK II", "Drifty Yosemite", "Sugoi", "Jugular"};
            static constexpr std::array<const char*, VehicleCount> Eddie{
                "Sultan Classic", "Gauntlet Classic", "Ellie", "Komoda"};
            static constexpr std::array<const char*, VehicleCount> Zach{
                "Manchez", "Stryder", "Defiler", "Lectro"};
            static constexpr std::array<const char*, VehicleCount> Chester{
                "Zhaba", "Vagrant", "Outlaw", "Everon"};

            const auto index = static_cast<std::size_t>(vehicle);
            switch (driver)
            {
            case 1: return Karim[index];
            case 2: return Taliana[index];
            case 3: return Eddie[index];
            case 4: return Zach[index];
            case 5: return Chester[index];
            default: return vehicle == 0 ? "Vehicle 1" : vehicle == 1 ? "Vehicle 2" : vehicle == 2 ? "Vehicle 3" : "Vehicle 4";
            }
        }

        [[nodiscard]] inline bool ValidSetup(
            int target,
            int approach,
            int gunman,
            int driver,
            int hacker,
            int weaponLoadout,
            int getawayVehicle) noexcept
        {
            return target >= 0 && target < TargetCount
                && approach >= 1 && approach <= ApproachCount
                && gunman >= 1 && gunman <= CrewCount
                && driver >= 1 && driver <= CrewCount
                && hacker >= 1 && hacker <= CrewCount
                && weaponLoadout >= 0 && weaponLoadout < WeaponLoadoutCount
                && getawayVehicle >= 0 && getawayVehicle < VehicleCount;
        }
    }

    struct CasinoHeistSnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool sessionStarted{};
        bool nativeReady{};
        bool globalsReady{};
        bool planningRunning{};
        bool setupReady{};
        bool cutsReady{};
        int primaryTarget{-1};
        int approach{};
        int hardApproach{};
        int gunman{};
        int driver{};
        int hacker{};
        int weaponLoadout{-1};
        int getawayVehicle{-1};
        int poiMask{};
        int accessPointsMask{};
        int disruptShipment{};
        int keyLevel{};
        CasinoHeistEnhanced173::CutArray cuts{{0, 0, 0, 0}};
        std::string message{"Press Refresh Casino State"};
    };

    class CasinoHeistRuntime final
    {
    public:
        static CasinoHeistRuntime& Get() noexcept
        {
            static CasinoHeistRuntime instance;
            return instance;
        }

        [[nodiscard]] bool QueueRefresh()
        {
            return Queue("Reading Enhanced Casino planning state", [this] {
                CasinoHeistSnapshot state;
                const bool success = CaptureState(state);
                Finish(
                    success,
                    std::move(state),
                    success
                        ? "Casino planning state refreshed"
                        : "Unable to read the Enhanced Casino planning state");
            });
        }

        [[nodiscard]] bool QueueSetup(
            int primaryTarget,
            int approach,
            bool hardMode,
            int gunman,
            int driver,
            int hacker,
            int weaponLoadout,
            int getawayVehicle)
        {
            using namespace CasinoHeistEnhanced173;
            if (!ValidSetup(primaryTarget, approach, gunman, driver, hacker, weaponLoadout, getawayVehicle))
                return false;

            return Queue("Applying verified Casino setup state", [this, primaryTarget, approach, hardMode, gunman, driver, hacker, weaponLoadout, getawayVehicle] {
                CasinoHeistSnapshot state;
                if (!RequireOnlineAndNatives(state))
                {
                    Finish(false, std::move(state), "Join GTA Online and wait for the native backend before changing Casino setup state");
                    return;
                }

                const auto characterIndex = Stats::GetCharIndex();
                if (!characterIndex)
                {
                    CaptureState(state);
                    Finish(false, std::move(state), "Unable to resolve the active GTA Online character slot");
                    return;
                }

                SetupWriteArray writes{{
                    {"MPX_H3_COMPLETEDPOSIX", -1, 0},
                    {"MPX_H3OPT_MASKS", 4, 0},
                    {"MPX_H3OPT_WEAPS", weaponLoadout, 0},
                    {"MPX_H3OPT_VEHS", getawayVehicle, 0},
                    {"MPX_CAS_HEIST_FLOW", -1, 0},
                    {"MPX_H3_LAST_APPROACH", 0, 0},
                    {"MPX_H3OPT_APPROACH", approach, 0},
                    {"MPX_H3_HARD_APPROACH", hardMode ? approach : 0, 0},
                    {"MPX_H3OPT_TARGET", primaryTarget, 0},
                    {"MPX_H3OPT_POI", AllPoiMask, 0},
                    {"MPX_H3OPT_ACCESSPOINTS", AllAccessPointsMask, 0},
                    {"MPX_H3OPT_CREWWEAP", gunman, 0},
                    {"MPX_H3OPT_CREWDRIVER", driver, 0},
                    {"MPX_H3OPT_CREWHACKER", hacker, 0},
                    {"MPX_H3OPT_DISRUPTSHIP", 3, 0},
                    {"MPX_H3OPT_BODYARMORLVL", -1, 0},
                    {"MPX_H3OPT_KEYLEVELS", 2, 0},
                    {"MPX_H3OPT_BITSET0", -1, 0},
                    {"MPX_H3OPT_BITSET1", -1, 0},
                }};

                for (auto& write : writes)
                {
                    const auto original = Stats::GetInt(write.name, *characterIndex);
                    if (!original)
                    {
                        CaptureState(state);
                        Finish(false, std::move(state), std::string("Unable to capture original Casino stat: ") + write.name);
                        return;
                    }
                    write.original = *original;
                }

                const char* failedStat = nullptr;
                for (const auto& write : writes)
                {
                    if (!Stats::SetInt(write.name, write.value, *characterIndex))
                    {
                        failedStat = write.name;
                        break;
                    }

                    const auto readback = Stats::GetInt(write.name, *characterIndex);
                    if (!readback || *readback != write.value)
                    {
                        failedStat = write.name;
                        break;
                    }
                }

                if (failedStat)
                {
                    const bool restored = RestoreSetupStats(writes, *characterIndex);
                    CaptureState(state);
                    Finish(
                        false,
                        std::move(state),
                        restored
                            ? std::string("Casino setup write failed verification at ") + failedStat + "; original state restored"
                            : std::string("Casino setup write failed at ") + failedStat + "; rollback could not be fully verified");
                    return;
                }

                CaptureState(state);
                TUTONES_LOG_INFO(
                    "heist.casino",
                    std::string("Applied Casino setup target=") + TargetName(primaryTarget)
                        + " approach=" + ApproachName(approach)
                        + " difficulty=" + (hardMode ? "Hard" : "Normal"));

                Finish(
                    true,
                    std::move(state),
                    state.planningRunning
                        ? "Casino setup applied; use Refresh Planning Board or close and reopen the Arcade board"
                        : "Casino setup applied; open the Arcade planning board to load the new state");
            });
        }

        [[nodiscard]] bool QueueCuts(const CasinoHeistEnhanced173::CutArray& requested)
        {
            using namespace CasinoHeistEnhanced173;
            for (const int cut : requested)
            {
                if (cut < 0 || cut > MaxCut)
                    return false;
            }

            return Queue("Applying Casino player cuts", [this, requested] {
                CasinoHeistSnapshot state;
                if (!RequireOnlineAndNatives(state))
                {
                    Finish(false, std::move(state), "Join GTA Online before changing Casino cuts");
                    return;
                }

                auto& scripts = Script::ScriptRuntime::Get();
                auto** globals = scripts.Globals();
                state.globalsReady = globals != nullptr;
                if (!globals)
                {
                    CaptureState(state);
                    Finish(false, std::move(state), "Casino cut globals are unavailable");
                    return;
                }

                auto base = Script::ScriptGlobal(PlanningGlobal)
                    .At(CutsOffsetA)
                    .At(CutsOffsetB)
                    .At(CutsOffsetC);

                CutArray originals{};
                for (std::size_t index = 0; index < requested.size(); ++index)
                {
                    auto* cut = base.At(index, 1).As<std::int32_t>(globals);
                    if (!cut)
                    {
                        CaptureState(state);
                        Finish(false, std::move(state), "Unable to resolve Casino cut globals");
                        return;
                    }
                    originals[index] = *cut;
                }

                bool success = true;
                for (std::size_t index = 0; index < requested.size(); ++index)
                {
                    auto* cut = base.At(index, 1).As<std::int32_t>(globals);
                    if (!cut)
                    {
                        success = false;
                        break;
                    }
                    *cut = requested[index];
                    success = *cut == requested[index] && success;
                }

                if (!success)
                {
                    for (std::size_t index = 0; index < originals.size(); ++index)
                    {
                        if (auto* cut = base.At(index, 1).As<std::int32_t>(globals))
                            *cut = originals[index];
                    }
                    CaptureState(state);
                    Finish(false, std::move(state), "Casino cut write failed verification; original cuts restored");
                    return;
                }

                CaptureState(state);
                TUTONES_LOG_INFO("heist.casino", "Applied Casino player cuts with read-back verification");
                Finish(true, std::move(state), "Casino player cuts applied");
            });
        }

        [[nodiscard]] bool QueuePlanningBoardRefresh()
        {
            using namespace CasinoHeistEnhanced173;
            return Queue("Pulsing Casino planning-board state", [this] {
                CasinoHeistSnapshot state;
                if (!RequireOnlineAndNatives(state))
                {
                    Finish(false, std::move(state), "Join GTA Online before refreshing the Casino planning board");
                    return;
                }

                const auto characterIndex = Stats::GetCharIndex();
                if (!characterIndex)
                {
                    CaptureState(state);
                    Finish(false, std::move(state), "Unable to resolve the active GTA Online character slot");
                    return;
                }

                const auto original0 = Stats::GetInt("MPX_H3OPT_BITSET0", *characterIndex);
                const auto original1 = Stats::GetInt("MPX_H3OPT_BITSET1", *characterIndex);
                if (!original0 || !original1)
                {
                    CaptureState(state);
                    Finish(false, std::move(state), "Unable to capture Casino planning-board refresh state");
                    return;
                }

                const int pulse0 = *original0 ^ 0x13579BDF;
                const int pulse1 = *original1 ^ 0x02468ACE;
                if (!Stats::SetInt("MPX_H3OPT_BITSET0", pulse0, *characterIndex)
                    || !Stats::SetInt("MPX_H3OPT_BITSET1", pulse1, *characterIndex))
                {
                    static_cast<void>(Stats::SetInt("MPX_H3OPT_BITSET0", *original0, *characterIndex));
                    static_cast<void>(Stats::SetInt("MPX_H3OPT_BITSET1", *original1, *characterIndex));
                    CaptureState(state);
                    Finish(false, std::move(state), "Casino planning-board pulse failed; original state restored");
                    return;
                }

                const auto restoreAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
                if (!Runtime::GameRuntime::Get().Enqueue([this, characterIndex = *characterIndex, original0 = *original0, original1 = *original1, restoreAt] {
                        ContinuePlanningBoardRefresh(characterIndex, original0, original1, restoreAt);
                    }))
                {
                    static_cast<void>(Stats::SetInt("MPX_H3OPT_BITSET0", *original0, *characterIndex));
                    static_cast<void>(Stats::SetInt("MPX_H3OPT_BITSET1", *original1, *characterIndex));
                    CaptureState(state);
                    Finish(false, std::move(state), "Casino planning-board refresh queue was unavailable; original state restored");
                }
            });
        }

        [[nodiscard]] CasinoHeistSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            CasinoHeistSnapshot state = m_Snapshot;
            state.pending = m_Pending.load(std::memory_order_acquire);
            return state;
        }

    private:
        struct StatWrite final
        {
            const char* name{};
            int value{};
            int original{};
        };

        using SetupWriteArray = std::array<StatWrite, 19>;

        CasinoHeistRuntime() = default;
        CasinoHeistRuntime(const CasinoHeistRuntime&) = delete;
        CasinoHeistRuntime& operator=(const CasinoHeistRuntime&) = delete;

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

            CasinoHeistSnapshot state;
            Finish(false, std::move(state), "GTA script-thread queue is unavailable");
            return false;
        }

        [[nodiscard]] bool RequireOnlineAndNatives(CasinoHeistSnapshot& state) const noexcept
        {
            const bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            state.sessionStarted = sessionStarted && *sessionStarted;
            state.nativeReady = Native::NativeRegistry::Get().CanInvokeOnCurrentThread();
            return state.sessionStarted && state.nativeReady;
        }

        [[nodiscard]] bool CaptureState(CasinoHeistSnapshot& state) const noexcept
        {
            const bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            state.sessionStarted = sessionStarted && *sessionStarted;
            state.nativeReady = Native::NativeRegistry::Get().CanInvokeOnCurrentThread();

            auto& scripts = Script::ScriptRuntime::Get();
            auto** globals = scripts.Globals();
            state.globalsReady = globals != nullptr;
            if (scripts.IsReady())
            {
                if (const auto* thread = scripts.FindThread(CasinoHeistEnhanced173::PlanningHash))
                {
                    state.planningRunning = thread->context.threadId != 0
                        && thread->context.state != Types::ScriptThreadState::Killed;
                }
            }

            if (!state.sessionStarted || !state.nativeReady)
                return false;

            const auto characterIndex = Stats::GetCharIndex();
            if (characterIndex)
            {
                const auto target = Stats::GetInt("MPX_H3OPT_TARGET", *characterIndex);
                const auto approach = Stats::GetInt("MPX_H3OPT_APPROACH", *characterIndex);
                const auto hardApproach = Stats::GetInt("MPX_H3_HARD_APPROACH", *characterIndex);
                const auto gunman = Stats::GetInt("MPX_H3OPT_CREWWEAP", *characterIndex);
                const auto driver = Stats::GetInt("MPX_H3OPT_CREWDRIVER", *characterIndex);
                const auto hacker = Stats::GetInt("MPX_H3OPT_CREWHACKER", *characterIndex);
                const auto weapon = Stats::GetInt("MPX_H3OPT_WEAPS", *characterIndex);
                const auto vehicle = Stats::GetInt("MPX_H3OPT_VEHS", *characterIndex);
                const auto poi = Stats::GetInt("MPX_H3OPT_POI", *characterIndex);
                const auto access = Stats::GetInt("MPX_H3OPT_ACCESSPOINTS", *characterIndex);
                const auto disrupt = Stats::GetInt("MPX_H3OPT_DISRUPTSHIP", *characterIndex);
                const auto keyLevel = Stats::GetInt("MPX_H3OPT_KEYLEVELS", *characterIndex);

                state.setupReady = target && approach && hardApproach && gunman && driver && hacker
                    && weapon && vehicle && poi && access && disrupt && keyLevel;
                if (state.setupReady)
                {
                    state.primaryTarget = *target;
                    state.approach = *approach;
                    state.hardApproach = *hardApproach;
                    state.gunman = *gunman;
                    state.driver = *driver;
                    state.hacker = *hacker;
                    state.weaponLoadout = *weapon;
                    state.getawayVehicle = *vehicle;
                    state.poiMask = *poi;
                    state.accessPointsMask = *access;
                    state.disruptShipment = *disrupt;
                    state.keyLevel = *keyLevel;
                }
            }

            if (globals)
            {
                auto base = Script::ScriptGlobal(CasinoHeistEnhanced173::PlanningGlobal)
                    .At(CasinoHeistEnhanced173::CutsOffsetA)
                    .At(CasinoHeistEnhanced173::CutsOffsetB)
                    .At(CasinoHeistEnhanced173::CutsOffsetC);

                state.cutsReady = true;
                for (std::size_t index = 0; index < state.cuts.size(); ++index)
                {
                    const auto* cut = base.At(index, 1).As<std::int32_t>(globals);
                    if (!cut)
                    {
                        state.cutsReady = false;
                        break;
                    }
                    state.cuts[index] = *cut;
                }
            }

            return state.setupReady || state.cutsReady;
        }

        [[nodiscard]] static bool RestoreSetupStats(const SetupWriteArray& writes, int characterIndex) noexcept
        {
            bool restored = true;
            for (const auto& write : writes)
                restored = Stats::SetInt(write.name, write.original, characterIndex) && restored;

            for (const auto& write : writes)
            {
                const auto readback = Stats::GetInt(write.name, characterIndex);
                restored = readback && *readback == write.original && restored;
            }
            return restored;
        }

        void ContinuePlanningBoardRefresh(
            int characterIndex,
            int original0,
            int original1,
            std::chrono::steady_clock::time_point restoreAt) noexcept
        {
            if (std::chrono::steady_clock::now() < restoreAt)
            {
                if (Runtime::GameRuntime::Get().Enqueue([this, characterIndex, original0, original1, restoreAt] {
                        ContinuePlanningBoardRefresh(characterIndex, original0, original1, restoreAt);
                    }))
                {
                    return;
                }
            }

            const bool restored0 = Stats::SetInt("MPX_H3OPT_BITSET0", original0, characterIndex);
            const bool restored1 = Stats::SetInt("MPX_H3OPT_BITSET1", original1, characterIndex);
            const auto verify0 = Stats::GetInt("MPX_H3OPT_BITSET0", characterIndex);
            const auto verify1 = Stats::GetInt("MPX_H3OPT_BITSET1", characterIndex);
            const bool restored = restored0 && restored1
                && verify0 && *verify0 == original0
                && verify1 && *verify1 == original1;

            CasinoHeistSnapshot state;
            CaptureState(state);
            Finish(
                restored,
                std::move(state),
                restored
                    ? "Casino planning-board refresh pulse completed"
                    : "Casino planning-board pulse completed but original refresh state could not be fully verified");
        }

        void Finish(bool success, CasinoHeistSnapshot state, std::string message) noexcept
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
        CasinoHeistSnapshot m_Snapshot{};
    };
}
