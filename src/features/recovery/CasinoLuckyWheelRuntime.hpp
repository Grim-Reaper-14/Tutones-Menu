#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/PlayerNatives.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../game/script/ScriptLocal.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::Recovery
{
    namespace CasinoLuckyWheelDetail
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
    }

    struct CasinoLuckyWheelSnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool localAvailable{};
        int playerId{-1};
        std::size_t localIndex{};
        int localValue{};
        std::string message{"Ready"};
    };

    class CasinoLuckyWheelRuntime final
    {
    public:
        static constexpr std::size_t TunablesGlobal = 262145;
        static constexpr std::size_t MaxSpinsOffset = 26855;
        static constexpr std::size_t AdditionalSpinsOffset = 26856;
        static constexpr std::size_t GtaPlusMaxSpinsOffset = 37458;
        static constexpr std::uint32_t LuckyWheelScriptHash = CasinoLuckyWheelDetail::Joaat("casino_lucky_wheel");

        // casino_lucky_wheel declares the per-player array at local 150. The
        // ScriptLocal array form used by the verified implementation resolves
        // 150 + 1 + (PLAYER_ID * 5), where +1 skips the array header slot.
        static constexpr std::size_t PlayerLocalArrayBase = 150;
        static constexpr std::size_t PlayerLocalArrayHeader = 1;
        static constexpr std::size_t PlayerLocalStride = 5;
        static constexpr int MinPrize = 0;
        static constexpr int MaxPrize = 19;

        static CasinoLuckyWheelRuntime& Get() noexcept
        {
            static CasinoLuckyWheelRuntime instance;
            return instance;
        }

        bool QueueApplySuppliedGlobals()
        {
            return Queue("Lucky Wheel globals queued", [this] {
                auto* pages = RequireGlobals();
                if (!pages)
                    return;

                int* maxSpins = Script::ScriptGlobal(TunablesGlobal).At(MaxSpinsOffset).As<int>(pages);
                int* additionalSpins = Script::ScriptGlobal(TunablesGlobal).At(AdditionalSpinsOffset).As<int>(pages);
                int* gtaPlusMaxSpins = Script::ScriptGlobal(TunablesGlobal).At(GtaPlusMaxSpinsOffset).As<int>(pages);
                if (!maxSpins || !additionalSpins || !gtaPlusMaxSpins)
                    return Finish(false, "One or more Lucky Wheel globals are unavailable");

                *maxSpins = 1;
                *additionalSpins = 1;
                *gtaPlusMaxSpins = 2;

                const bool success = *maxSpins == 1 && *additionalSpins == 1 && *gtaPlusMaxSpins == 2;
                if (success)
                    TUTONES_LOG_INFO("recovery.casino", "Applied supplied Enhanced Lucky Wheel globals f_26855=1, f_26856=1, f_37458=2");

                Finish(success, success ? "Supplied Lucky Wheel globals applied" : "Lucky Wheel globals failed read-back verification");
            });
        }

        bool QueueSetPrize(int prize)
        {
            if (prize < MinPrize || prize > MaxPrize)
                return false;

            return Queue("Lucky Wheel prize queued", [this, prize] {
                bool* sessionStarted = GamePointers::Get().IsSessionStarted();
                if (!sessionStarted || !*sessionStarted)
                    return Finish(false, "Join GTA Online before setting a Lucky Wheel prize");

                auto& scripts = Script::ScriptRuntime::Get();
                if (!scripts.IsReady())
                    return Finish(false, "Shared Enhanced script runtime is unavailable");

                auto* thread = scripts.FindThread(LuckyWheelScriptHash);
                if (!thread || !thread->stack)
                    return Finish(false, "casino_lucky_wheel is not active; approach/use the Lucky Wheel first");

                const auto player = PlayerNatives::PlayerId();
                if (!player || *player < 0)
                    return Finish(false, "PLAYER_ID could not be resolved");

                const auto index = PlayerLocalArrayBase
                    + PlayerLocalArrayHeader
                    + static_cast<std::size_t>(*player) * PlayerLocalStride;
                const auto stackSlots = static_cast<std::size_t>(thread->context.stackSize) / sizeof(std::uint64_t);
                if (index >= stackSlots)
                    return Finish(false, "Lucky Wheel prize local is outside the active script stack");

                int* prizeOutcome = Script::ScriptLocal(thread, index).As<int>();
                if (!prizeOutcome)
                    return Finish(false, "Lucky Wheel prize local is unavailable");

                auto* pages = GamePointers::Get().ScriptGlobals();
                if (!pages)
                    return Finish(false, "Enhanced script globals are unavailable");

                int* additionalSpins = Script::ScriptGlobal(TunablesGlobal).At(AdditionalSpinsOffset).As<int>(pages);
                int* gtaPlusMaxSpins = Script::ScriptGlobal(TunablesGlobal).At(GtaPlusMaxSpinsOffset).As<int>(pages);
                if (!additionalSpins || !gtaPlusMaxSpins)
                    return Finish(false, "Lucky Wheel spin globals are unavailable");

                // Match the verified Enhanced selector flow: permit the extra spin,
                // keep the GTA+ daily spin limit supplied for this build, then set
                // the selected 0-19 prize outcome in the active wheel script.
                *additionalSpins = 1;
                *gtaPlusMaxSpins = 2;
                *prizeOutcome = prize;

                const bool success = *additionalSpins == 1
                    && *gtaPlusMaxSpins == 2
                    && *prizeOutcome == prize;

                {
                    std::scoped_lock lock(m_Mutex);
                    m_LocalAvailable = true;
                    m_PlayerId = *player;
                    m_LocalIndex = index;
                    m_LocalValue = *prizeOutcome;
                }

                if (success)
                {
                    TUTONES_LOG_INFO(
                        "recovery.casino",
                        std::string("Set Lucky Wheel prize outcome to ") + std::to_string(prize)
                            + " at local " + std::to_string(index));
                }

                Finish(success, success ? "Selected Lucky Wheel prize applied" : "Lucky Wheel prize failed read-back verification");
            });
        }

        bool QueueInspectPlayerLocal()
        {
            return Queue("Lucky Wheel player local inspection queued", [this] {
                bool* sessionStarted = GamePointers::Get().IsSessionStarted();
                if (!sessionStarted || !*sessionStarted)
                    return Finish(false, "Join GTA Online before inspecting casino_lucky_wheel");

                auto& scripts = Script::ScriptRuntime::Get();
                if (!scripts.IsReady())
                    return Finish(false, "Shared Enhanced script runtime is unavailable");

                auto* thread = scripts.FindThread(LuckyWheelScriptHash);
                if (!thread || !thread->stack)
                    return Finish(false, "casino_lucky_wheel is not active");

                const auto player = PlayerNatives::PlayerId();
                if (!player || *player < 0)
                    return Finish(false, "PLAYER_ID could not be resolved");

                const auto index = PlayerLocalArrayBase
                    + PlayerLocalArrayHeader
                    + static_cast<std::size_t>(*player) * PlayerLocalStride;
                const auto stackSlots = static_cast<std::size_t>(thread->context.stackSize) / sizeof(std::uint64_t);
                if (index >= stackSlots)
                    return Finish(false, "Lucky Wheel prize local is outside the active script stack");

                int* value = Script::ScriptLocal(thread, index).As<int>();
                if (!value)
                    return Finish(false, "casino_lucky_wheel prize local is unavailable");

                {
                    std::scoped_lock lock(m_Mutex);
                    m_LocalAvailable = true;
                    m_PlayerId = *player;
                    m_LocalIndex = index;
                    m_LocalValue = *value;
                }

                TUTONES_LOG_DEBUG("recovery.casino", std::string("casino_lucky_wheel prize local ") + std::to_string(index) + " = " + std::to_string(*value));
                Finish(true, "Lucky Wheel prize local inspected");
            });
        }

        [[nodiscard]] CasinoLuckyWheelSnapshot Snapshot() const
        {
            CasinoLuckyWheelSnapshot snapshot;
            snapshot.pending = m_Pending.load(std::memory_order_acquire);
            std::scoped_lock lock(m_Mutex);
            snapshot.haveResult = m_HaveResult;
            snapshot.lastSucceeded = m_LastSucceeded;
            snapshot.localAvailable = m_LocalAvailable;
            snapshot.playerId = m_PlayerId;
            snapshot.localIndex = m_LocalIndex;
            snapshot.localValue = m_LocalValue;
            snapshot.message = m_Message;
            return snapshot;
        }

    private:
        CasinoLuckyWheelRuntime() = default;

        template<typename Callback>
        bool Queue(std::string pendingMessage, Callback&& callback)
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            SetPending(std::move(pendingMessage));
            if (Runtime::GameRuntime::Get().Enqueue(std::forward<Callback>(callback)))
                return true;

            Finish(false, "Game-thread queue unavailable");
            return false;
        }

        [[nodiscard]] std::int64_t** RequireGlobals()
        {
            bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            if (!sessionStarted || !*sessionStarted)
            {
                Finish(false, "Join GTA Online before using Lucky Wheel globals");
                return nullptr;
            }

            auto* pages = GamePointers::Get().ScriptGlobals();
            if (!pages)
            {
                Finish(false, "Enhanced script globals are unavailable");
                return nullptr;
            }
            return pages;
        }

        void SetPending(std::string message)
        {
            std::scoped_lock lock(m_Mutex);
            m_HaveResult = false;
            m_LastSucceeded = false;
            m_Message = std::move(message);
        }

        void Finish(bool success, std::string message)
        {
            {
                std::scoped_lock lock(m_Mutex);
                m_HaveResult = true;
                m_LastSucceeded = success;
                m_Message = std::move(message);
            }
            m_Pending.store(false, std::memory_order_release);
        }

        std::atomic<bool> m_Pending{false};
        mutable std::mutex m_Mutex;
        bool m_HaveResult{};
        bool m_LastSucceeded{};
        bool m_LocalAvailable{};
        int m_PlayerId{-1};
        std::size_t m_LocalIndex{};
        int m_LocalValue{};
        std::string m_Message{"Ready"};
    };
}
