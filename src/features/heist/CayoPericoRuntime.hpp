#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/PlayerNatives.hpp"
#include "../../game/Stats.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../game/script/ScriptLocal.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::Heist
{
    namespace CayoPericoEnhanced173
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

        inline constexpr std::uint32_t PlanningHash = Joaat("heist_island_planning");
        inline constexpr std::size_t PlanningReloadLocal = 1580;
        inline constexpr int PlanningReloadValue = 2;

        // Enhanced 1.73 b1158.13 decompile: Global_1982548[player /*53*/].
        inline constexpr std::size_t PlayerStateGlobal = 1982548;
        inline constexpr std::size_t PlayerStateEntrySize = 53;
        inline constexpr std::size_t ProgressionFlagsOffset = 1;
        inline constexpr std::size_t IntelFlagsOffset = 5;
        inline constexpr std::size_t TargetVariationOffset = 14; // f_5.f_9

        inline constexpr std::uint32_t GrapplingMask = 0x0000000Fu;     // bits 0..3
        inline constexpr std::uint32_t GuardClothingMask = 0x000000F0u; // bits 4..7
        inline constexpr std::uint32_t BoltCuttersMask = 0x00000F00u;   // bits 8..11
        inline constexpr std::uint32_t PowerStationMask = 1u << 14;
        inline constexpr std::uint32_t SupplyTruckMask = 1u << 15;
        inline constexpr std::uint32_t ControlTowerMask = 1u << 16;

        inline constexpr int NormalDifficulty = 126823;
        inline constexpr int HardDifficulty = 131055;

        inline constexpr int TargetCount = 6;
        inline constexpr std::array<const char*, TargetCount> TargetNames{
            "Sinsimito Tequila",
            "Ruby Necklace",
            "Bearer Bonds",
            "Pink Diamond",
            "Madrazo Files",
            "Panther Statue",
        };

        inline constexpr int WeaponCount = 5;
        inline constexpr std::array<const char*, WeaponCount> WeaponNames{
            "Aggressor",
            "Conspirator",
            "Crack Shot",
            "Saboteur",
            "Marksman",
        };

        [[nodiscard]] inline const char* PrimaryTargetName(int target) noexcept
        {
            if (target < 0 || target >= TargetCount)
                return "Unknown";
            return TargetNames[static_cast<std::size_t>(target)];
        }

        [[nodiscard]] inline const char* WeaponLoadoutName(int weapon) noexcept
        {
            if (weapon < 1 || weapon > WeaponCount)
                return "Unknown";
            return WeaponNames[static_cast<std::size_t>(weapon - 1)];
        }

        [[nodiscard]] inline const char* DifficultyName(int difficulty) noexcept
        {
            if (difficulty == NormalDifficulty)
                return "Normal";
            if (difficulty == HardDifficulty)
                return "Hard";
            return "Custom / Unknown";
        }

        [[nodiscard]] inline bool ValidDifficulty(int difficulty) noexcept
        {
            return difficulty == NormalDifficulty || difficulty == HardDifficulty;
        }

        [[nodiscard]] inline const char* RequiredPrimaryEquipment(int targetVariation) noexcept
        {
            // heist_island_planning::func_492 maps variations 2/4 to safe-code prep;
            // every other observed variation uses the plasma-cutter prep path.
            return targetVariation == 2 || targetVariation == 4
                ? "Safe Code"
                : "Plasma Cutter";
        }
    }

    struct CayoPericoSnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool sessionStarted{};
        bool nativeReady{};
        bool globalsReady{};
        bool planningRunning{};
        bool planningReloaded{};
        int playerId{-1};
        std::uint32_t progressionFlags{};
        std::uint32_t intelFlags{};
        int targetVariation{-1};
        int primaryTarget{-1};
        int difficulty{};
        int weaponLoadout{};
        bool powerStationScoped{};
        bool controlTowerScoped{};
        bool boltCuttersScoped{};
        bool grapplingScoped{};
        bool guardClothingScoped{};
        bool supplyTruckScoped{};
        std::string message{"Press Refresh Cayo State"};
    };

    class CayoPericoRuntime final
    {
    public:
        static CayoPericoRuntime& Get() noexcept
        {
            static CayoPericoRuntime instance;
            return instance;
        }

        [[nodiscard]] bool QueueRefresh()
        {
            return Queue("Reading Enhanced Cayo planning state", [this] {
                CayoPericoSnapshot state;
                const bool success = CaptureState(state);
                Finish(
                    success,
                    std::move(state),
                    success
                        ? "Cayo planning state refreshed"
                        : "Unable to read the Enhanced Cayo planning state");
            });
        }

        [[nodiscard]] bool QueueSetup(int primaryTarget, int difficulty, int weaponLoadout)
        {
            if (primaryTarget < 0 || primaryTarget >= CayoPericoEnhanced173::TargetCount
                || !CayoPericoEnhanced173::ValidDifficulty(difficulty)
                || weaponLoadout < 1 || weaponLoadout > CayoPericoEnhanced173::WeaponCount)
            {
                return false;
            }

            return Queue("Applying verified Cayo setup state", [this, primaryTarget, difficulty, weaponLoadout] {
                CayoPericoSnapshot state;
                if (!RequireOnlineAndNatives(state))
                {
                    Finish(false, std::move(state), "Join GTA Online and wait for the native backend before changing Cayo setup state");
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
                    {"MPX_H4CNF_TARGET", primaryTarget, 0},
                    {"MPX_H4_PROGRESS", difficulty, 0},
                    {"MPX_H4CNF_BS_GEN", 262143, 0},
                    {"MPX_H4CNF_BS_ENTR", 63, 0},
                    {"MPX_H4CNF_BS_ABIL", 63, 0},
                    {"MPX_H4CNF_WEP_DISRP", 3, 0},
                    {"MPX_H4CNF_ARM_DISRP", 3, 0},
                    {"MPX_H4CNF_HEL_DISRP", 3, 0},
                    {"MPX_H4CNF_APPROACH", -1, 0},
                    {"MPX_H4CNF_BOLTCUT", 4424, 0},
                    {"MPX_H4CNF_UNIFORM", 5256, 0},
                    {"MPX_H4CNF_GRAPPEL", 5156, 0},
                    {"MPX_H4_MISSIONS", -1, 0},
                    {"MPX_H4CNF_WEAPONS", weaponLoadout, 0},
                    {"MPX_H4CNF_TROJAN", 5, 0},
                    {"MPX_H4_PLAYTHROUGH_STATUS", 100, 0},
                }};

                for (auto& write : writes)
                {
                    const auto original = Stats::GetInt(write.name, *characterIndex);
                    if (!original)
                    {
                        CaptureState(state);
                        Finish(false, std::move(state), std::string("Unable to capture original Cayo stat: ") + write.name);
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
                            ? std::string("Cayo setup write failed verification at ") + failedStat + "; original state restored"
                            : std::string("Cayo setup write failed at ") + failedStat + "; rollback could not be fully verified");
                    return;
                }

                bool planningRunning = false;
                bool planningReloaded = false;
                auto& scripts = Script::ScriptRuntime::Get();
                if (scripts.IsReady())
                {
                    if (auto* thread = scripts.FindThread(CayoPericoEnhanced173::PlanningHash))
                    {
                        planningRunning = thread->context.threadId != 0
                            && thread->context.state != Types::ScriptThreadState::Killed;
                        if (planningRunning)
                        {
                            if (int* reload = Script::ScriptLocal(thread, CayoPericoEnhanced173::PlanningReloadLocal).As<int>())
                            {
                                const int originalReload = *reload;
                                *reload = CayoPericoEnhanced173::PlanningReloadValue;
                                planningReloaded = *reload == CayoPericoEnhanced173::PlanningReloadValue;
                                if (!planningReloaded)
                                    *reload = originalReload;
                            }
                        }
                    }
                }

                CaptureState(state);
                state.primaryTarget = primaryTarget;
                state.difficulty = difficulty;
                state.weaponLoadout = weaponLoadout;
                state.planningRunning = planningRunning;
                state.planningReloaded = planningReloaded;

                TUTONES_LOG_INFO(
                    "heist.cayo",
                    std::string("Applied Cayo setup target=")
                        + CayoPericoEnhanced173::PrimaryTargetName(primaryTarget)
                        + " difficulty=" + CayoPericoEnhanced173::DifficultyName(difficulty)
                        + " weapon=" + CayoPericoEnhanced173::WeaponLoadoutName(weaponLoadout));

                if (planningReloaded)
                {
                    Finish(true, std::move(state), "Cayo setup applied and the active planning board was reloaded");
                }
                else if (planningRunning)
                {
                    Finish(true, std::move(state), "Cayo setup applied; planning board reload was unavailable, so close and reopen the board");
                }
                else
                {
                    Finish(true, std::move(state), "Cayo setup applied; open the Kosatka planning board to load the new state");
                }
            });
        }

        [[nodiscard]] CayoPericoSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            CayoPericoSnapshot state = m_Snapshot;
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

        using SetupWriteArray = std::array<StatWrite, 16>;

        CayoPericoRuntime() = default;
        CayoPericoRuntime(const CayoPericoRuntime&) = delete;
        CayoPericoRuntime& operator=(const CayoPericoRuntime&) = delete;

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

            CayoPericoSnapshot state;
            Finish(false, std::move(state), "GTA script-thread queue is unavailable");
            return false;
        }

        [[nodiscard]] bool RequireOnlineAndNatives(CayoPericoSnapshot& state) const noexcept
        {
            const bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            state.sessionStarted = sessionStarted && *sessionStarted;
            state.nativeReady = Native::NativeRegistry::Get().CanInvokeOnCurrentThread();
            return state.sessionStarted && state.nativeReady;
        }

        [[nodiscard]] bool CaptureState(CayoPericoSnapshot& state) const noexcept
        {
            const bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            state.sessionStarted = sessionStarted && *sessionStarted;
            state.nativeReady = Native::NativeRegistry::Get().CanInvokeOnCurrentThread();

            auto& scripts = Script::ScriptRuntime::Get();
            auto** globals = scripts.Globals();
            state.globalsReady = globals != nullptr;

            if (scripts.IsReady())
            {
                if (const auto* thread = scripts.FindThread(CayoPericoEnhanced173::PlanningHash))
                {
                    state.planningRunning = thread->context.threadId != 0
                        && thread->context.state != Types::ScriptThreadState::Killed;
                }
            }

            if (!state.sessionStarted || !state.nativeReady || !globals)
                return false;

            const auto player = PlayerNatives::PlayerId();
            if (!player || *player < 0 || *player >= 32)
                return false;

            state.playerId = *player;
            const auto playerState = Script::ScriptGlobal(CayoPericoEnhanced173::PlayerStateGlobal)
                .At(static_cast<std::size_t>(*player), CayoPericoEnhanced173::PlayerStateEntrySize);

            const auto* progressionFlags = playerState
                .At(CayoPericoEnhanced173::ProgressionFlagsOffset)
                .As<std::int32_t>(globals);
            const auto* intelFlags = playerState
                .At(CayoPericoEnhanced173::IntelFlagsOffset)
                .As<std::int32_t>(globals);
            const auto* targetVariation = playerState
                .At(CayoPericoEnhanced173::TargetVariationOffset)
                .As<std::int32_t>(globals);

            if (!progressionFlags || !intelFlags || !targetVariation)
                return false;

            state.progressionFlags = static_cast<std::uint32_t>(*progressionFlags);
            state.intelFlags = static_cast<std::uint32_t>(*intelFlags);
            state.targetVariation = *targetVariation;

            const auto characterIndex = Stats::GetCharIndex();
            if (characterIndex)
            {
                if (const auto target = Stats::GetInt("MPX_H4CNF_TARGET", *characterIndex))
                    state.primaryTarget = *target;
                if (const auto difficulty = Stats::GetInt("MPX_H4_PROGRESS", *characterIndex))
                    state.difficulty = *difficulty;
                if (const auto weapon = Stats::GetInt("MPX_H4CNF_WEAPONS", *characterIndex))
                    state.weaponLoadout = *weapon;
            }

            const std::uint32_t flags = state.intelFlags;
            state.powerStationScoped = (flags & CayoPericoEnhanced173::PowerStationMask) != 0;
            state.controlTowerScoped = (flags & CayoPericoEnhanced173::ControlTowerMask) != 0;
            state.boltCuttersScoped = (flags & CayoPericoEnhanced173::BoltCuttersMask) != 0;
            state.grapplingScoped = (flags & CayoPericoEnhanced173::GrapplingMask) != 0;
            state.guardClothingScoped = (flags & CayoPericoEnhanced173::GuardClothingMask) != 0;
            state.supplyTruckScoped = (flags & CayoPericoEnhanced173::SupplyTruckMask) != 0;
            return true;
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

        void Finish(bool success, CayoPericoSnapshot state, std::string message) noexcept
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
        CayoPericoSnapshot m_Snapshot{};
    };
}
