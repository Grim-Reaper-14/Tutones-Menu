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

        // Enhanced 1.73 b1158.13 decompile: Global_1982548[player /*53*/].
        inline constexpr std::size_t PlayerStateGlobal = 1982548;
        inline constexpr std::size_t PlayerStateEntrySize = 53;
        inline constexpr std::size_t ProgressionFlagsOffset = 1;
        inline constexpr std::size_t IntelFlagsOffset = 5;
        inline constexpr std::size_t TargetVariationOffset = 14; // f_5.f_9

        inline constexpr std::uint32_t GrapplingMask = 0x0000000Fu;    // bits 0..3
        inline constexpr std::uint32_t GuardClothingMask = 0x000000F0u; // bits 4..7
        inline constexpr std::uint32_t BoltCuttersMask = 0x00000F00u;   // bits 8..11
        inline constexpr std::uint32_t PowerStationMask = 1u << 14;
        inline constexpr std::uint32_t SupplyTruckMask = 1u << 15;
        inline constexpr std::uint32_t ControlTowerMask = 1u << 16;

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
        int playerId{-1};
        std::uint32_t progressionFlags{};
        std::uint32_t intelFlags{};
        int targetVariation{-1};
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
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot.haveResult = false;
                m_Snapshot.lastSucceeded = false;
                m_Snapshot.message = "Reading Enhanced Cayo planning state";
            }

            if (Runtime::GameRuntime::Get().Enqueue([this] {
                    CayoPericoSnapshot state;
                    const bool success = CaptureState(state);
                    Finish(
                        success,
                        std::move(state),
                        success
                            ? "Cayo planning state refreshed"
                            : "Unable to read the Enhanced Cayo planning state");
                }))
            {
                return true;
            }

            CayoPericoSnapshot state;
            Finish(false, std::move(state), "GTA script-thread queue is unavailable");
            return false;
        }

        [[nodiscard]] CayoPericoSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            CayoPericoSnapshot state = m_Snapshot;
            state.pending = m_Pending.load(std::memory_order_acquire);
            return state;
        }

    private:
        CayoPericoRuntime() = default;
        CayoPericoRuntime(const CayoPericoRuntime&) = delete;
        CayoPericoRuntime& operator=(const CayoPericoRuntime&) = delete;

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

            const std::uint32_t flags = state.intelFlags;
            state.powerStationScoped = (flags & CayoPericoEnhanced173::PowerStationMask) != 0;
            state.controlTowerScoped = (flags & CayoPericoEnhanced173::ControlTowerMask) != 0;
            state.boltCuttersScoped = (flags & CayoPericoEnhanced173::BoltCuttersMask) != 0;
            state.grapplingScoped = (flags & CayoPericoEnhanced173::GrapplingMask) != 0;
            state.guardClothingScoped = (flags & CayoPericoEnhanced173::GuardClothingMask) != 0;
            state.supplyTruckScoped = (flags & CayoPericoEnhanced173::SupplyTruckMask) != 0;
            return true;
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
