#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::StreetDealer
{
    // GTA V Enhanced Online 1.73 / b1158.13 (Acid Labs decompile).
    // Keep every raw index in this build-specific block so future game builds
    // can invalidate the resolver without touching UI code.
    namespace Enhanced173
    {
        inline constexpr std::size_t FreemodeGlobal = 2733326;
        inline constexpr std::size_t DealerBlockOffset = 5635;
        inline constexpr std::size_t ActiveLocationOffset = DealerBlockOffset + 22;
        inline constexpr std::size_t ActiveDealerOffset = DealerBlockOffset + 23;
        inline constexpr std::size_t DealerStride = 7;
        inline constexpr std::size_t DealerCount = 3;

        // fm_street_dealer.c copies these five fields into its dealer/product state.
        // Their exact semantic names are intentionally withheld until the state
        // machine is fully cross-mapped.
        inline constexpr std::size_t ProductField1 = 1;
        inline constexpr std::size_t ProductField2 = 2;
        inline constexpr std::size_t ProductField3 = 3;
        inline constexpr std::size_t ProductField4 = 4;
        inline constexpr std::size_t ProductField5 = 5;
        inline constexpr std::size_t CompletedField = 6;

        inline constexpr std::array<int, DealerCount> CompletionPackedStats{42076, 42077, 42078};
    }

    struct DealerRecord final
    {
        int value1{};
        int value2{};
        int value3{};
        int value4{};
        int value5{};
        bool completed{};
    };

    struct Snapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool sessionStarted{};
        bool globalsReady{};
        bool layoutValid{};
        int activeLocation{-1};
        int activeDealer{-1};
        std::array<DealerRecord, Enhanced173::DealerCount> dealers{};
        std::string message{"Ready"};
    };

    class Runtime final
    {
    public:
        static Runtime& Get() noexcept
        {
            static Runtime instance;
            return instance;
        }

        bool QueueRefresh()
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            SetPending("Reading Enhanced Street Dealer state");
            if (Tutones::Runtime::GameRuntime::Get().Enqueue([this] {
                Snapshot state;
                if (bool* sessionStarted = GamePointers::Get().IsSessionStarted())
                    state.sessionStarted = *sessionStarted;

                if (!state.sessionStarted)
                    return Finish(false, std::move(state), "Join GTA Online before reading Street Dealer state");

                auto* pages = GamePointers::Get().ScriptGlobals();
                state.globalsReady = pages != nullptr;
                if (!state.globalsReady)
                    return Finish(false, std::move(state), "Script globals are unavailable");

                const Script::ScriptGlobal root(Enhanced173::FreemodeGlobal);
                const auto readInt = [pages](Script::ScriptGlobal global, int& output) -> bool
                {
                    const int* value = global.As<int>(pages);
                    if (!value)
                        return false;
                    output = *value;
                    return true;
                };

                if (!readInt(root.At(Enhanced173::ActiveLocationOffset), state.activeLocation) ||
                    !readInt(root.At(Enhanced173::ActiveDealerOffset), state.activeDealer))
                {
                    return Finish(false, std::move(state), "Unable to read Street Dealer globals");
                }

                // fm_street_dealer clears both values to -1 during shutdown. A valid
                // active dealer is 0..2; the location resolver contains the rotating
                // location set. Reject impossible values so a shifted build cannot be
                // mistaken for valid state.
                const bool locationPlausible = state.activeLocation >= -1 && state.activeLocation <= 49;
                const bool dealerPlausible = state.activeDealer >= -1 &&
                    state.activeDealer < static_cast<int>(Enhanced173::DealerCount);
                if (!locationPlausible || !dealerPlausible)
                    return Finish(false, std::move(state), "Enhanced Street Dealer layout validation failed");

                const auto dealerArray = root.At(Enhanced173::DealerBlockOffset);
                for (std::size_t dealer = 0; dealer < state.dealers.size(); ++dealer)
                {
                    // ScriptGlobal::At(index, stride) accounts for the GTA script-array
                    // length slot before element zero. This matches
                    // Global_2733326.f_5635[dealer /*7*/] in the Enhanced decompile.
                    const auto record = dealerArray.At(dealer, Enhanced173::DealerStride);
                    auto& output = state.dealers[dealer];
                    int completed{};
                    if (!readInt(record.At(Enhanced173::ProductField1), output.value1) ||
                        !readInt(record.At(Enhanced173::ProductField2), output.value2) ||
                        !readInt(record.At(Enhanced173::ProductField3), output.value3) ||
                        !readInt(record.At(Enhanced173::ProductField4), output.value4) ||
                        !readInt(record.At(Enhanced173::ProductField5), output.value5) ||
                        !readInt(record.At(Enhanced173::CompletedField), completed))
                    {
                        return Finish(false, std::move(state), "Unable to read Street Dealer record");
                    }
                    output.completed = completed != 0;
                }

                state.layoutValid = true;
                TUTONES_LOG_DEBUG("street_dealer", "Enhanced Street Dealer state refreshed");
                Finish(true, std::move(state), "Enhanced Street Dealer state refreshed");
            }))
            {
                return true;
            }

            Finish(false, {}, "Game-thread queue unavailable");
            return false;
        }

        [[nodiscard]] Snapshot GetSnapshot() const
        {
            Snapshot snapshot;
            snapshot.pending = m_Pending.load(std::memory_order_acquire);
            std::scoped_lock lock(m_Mutex);
            snapshot.haveResult = m_State.haveResult;
            snapshot.lastSucceeded = m_State.lastSucceeded;
            snapshot.sessionStarted = m_State.sessionStarted;
            snapshot.globalsReady = m_State.globalsReady;
            snapshot.layoutValid = m_State.layoutValid;
            snapshot.activeLocation = m_State.activeLocation;
            snapshot.activeDealer = m_State.activeDealer;
            snapshot.dealers = m_State.dealers;
            snapshot.message = m_State.message;
            return snapshot;
        }

    private:
        Runtime() = default;

        void SetPending(std::string message)
        {
            std::scoped_lock lock(m_Mutex);
            m_State.haveResult = false;
            m_State.lastSucceeded = false;
            m_State.message = std::move(message);
        }

        void Finish(bool success, Snapshot state, std::string message)
        {
            {
                std::scoped_lock lock(m_Mutex);
                state.pending = false;
                state.haveResult = true;
                state.lastSucceeded = success;
                state.message = std::move(message);
                m_State = std::move(state);
            }
            m_Pending.store(false, std::memory_order_release);
        }

        std::atomic<bool> m_Pending{false};
        mutable std::mutex m_Mutex;
        Snapshot m_State{};
    };
}
