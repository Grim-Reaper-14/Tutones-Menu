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
    namespace Enhanced173
    {
        inline constexpr std::size_t FreemodeGlobal = 2733326;
        inline constexpr std::size_t DealerBlockOffset = 5635;
        inline constexpr std::size_t ActiveLocationOffset = DealerBlockOffset + 22;
        inline constexpr std::size_t ActiveDealerOffset = DealerBlockOffset + 23;
        inline constexpr std::size_t DealerStride = 7;
        inline constexpr std::size_t DealerCount = 3;

        inline constexpr std::size_t PremiumProductField = 1;
        inline constexpr std::size_t CocainePayoutField = 2;
        inline constexpr std::size_t MethPayoutField = 3;
        inline constexpr std::size_t WeedPayoutField = 4;
        inline constexpr std::size_t AcidPayoutField = 5;
        inline constexpr std::size_t CompletedField = 6;

        inline constexpr int ProductCocaine = 2;
        inline constexpr int ProductMeth = 3;
        inline constexpr int ProductWeed = 4;
        inline constexpr int ProductAcid = 7;
        inline constexpr int MaximumPlausiblePayout = 1000000;

        inline constexpr std::array<int, DealerCount> CompletionPackedStats{42076, 42077, 42078};
    }

    [[nodiscard]] inline const char* ProductName(int productId) noexcept
    {
        switch (productId)
        {
        case Enhanced173::ProductCocaine: return "Cocaine";
        case Enhanced173::ProductMeth: return "Meth";
        case Enhanced173::ProductWeed: return "Weed";
        case Enhanced173::ProductAcid: return "Acid";
        default: return "Unknown";
        }
    }

    struct DealerRecord final
    {
        int premiumProduct{-1};
        int cocainePayout{};
        int methPayout{};
        int weedPayout{};
        int acidPayout{};
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

                const bool locationPlausible = state.activeLocation >= -1 && state.activeLocation <= 49;
                const bool dealerPlausible = state.activeDealer >= -1 &&
                    state.activeDealer < static_cast<int>(Enhanced173::DealerCount);
                if (!locationPlausible || !dealerPlausible)
                    return Finish(false, std::move(state), "Enhanced Street Dealer layout validation failed");

                const auto dealerArray = root.At(Enhanced173::DealerBlockOffset);
                for (std::size_t dealer = 0; dealer < state.dealers.size(); ++dealer)
                {
                    const auto record = dealerArray.At(dealer, Enhanced173::DealerStride);
                    auto& output = state.dealers[dealer];
                    int completed{};
                    if (!readInt(record.At(Enhanced173::PremiumProductField), output.premiumProduct) ||
                        !readInt(record.At(Enhanced173::CocainePayoutField), output.cocainePayout) ||
                        !readInt(record.At(Enhanced173::MethPayoutField), output.methPayout) ||
                        !readInt(record.At(Enhanced173::WeedPayoutField), output.weedPayout) ||
                        !readInt(record.At(Enhanced173::AcidPayoutField), output.acidPayout) ||
                        !readInt(record.At(Enhanced173::CompletedField), completed))
                    {
                        return Finish(false, std::move(state), "Unable to read Street Dealer record");
                    }

                    const bool premiumPlausible = output.premiumProduct == Enhanced173::ProductCocaine ||
                        output.premiumProduct == Enhanced173::ProductMeth ||
                        output.premiumProduct == Enhanced173::ProductWeed ||
                        output.premiumProduct == Enhanced173::ProductAcid;
                    const auto payoutPlausible = [](int value) noexcept
                    {
                        return value >= 0 && value <= Enhanced173::MaximumPlausiblePayout;
                    };
                    const bool payoutsPlausible = payoutPlausible(output.cocainePayout) &&
                        payoutPlausible(output.methPayout) && payoutPlausible(output.weedPayout) &&
                        payoutPlausible(output.acidPayout);
                    const bool completionPlausible = completed == 0 || completed == 1;
                    if (!premiumPlausible || !payoutsPlausible || !completionPlausible)
                        return Finish(false, std::move(state), "Enhanced Street Dealer record validation failed");

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
