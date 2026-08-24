#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
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
    namespace SpecialCargoToolsDetail
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

    struct SpecialCargoToolsSnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        std::string message{"Ready"};
    };

    class SpecialCargoToolsRuntime final
    {
    public:
        static constexpr std::size_t SourcingBaseGlobal = 1882762;
        static constexpr std::size_t SourcingAmountOffset = 13;
        static constexpr std::size_t LupeSpecialItemOffset = 14;
        static constexpr std::size_t LupeSpecialAvailableOffset = 15;
        static constexpr std::size_t LupeCargoTypeOffset = 16;

        static constexpr std::size_t TunablesGlobal = 262145;
        static constexpr std::size_t BuyCooldownOffset = 15592;
        static constexpr std::size_t SellCooldownOffset = 15593;
        static constexpr std::size_t FirstCratePriceOffset = 15825;
        static constexpr int CratePriceTierCount = 21;

        static constexpr std::size_t UniqueSpecialAvailableGlobal = 1951074;
        static constexpr std::size_t UniqueSpecialItemGlobal = 1950921;

        static constexpr std::uint32_t ContrabandBuyHash = SpecialCargoToolsDetail::Joaat("gb_contraband_buy");
        static constexpr std::uint32_t ContrabandSellHash = SpecialCargoToolsDetail::Joaat("gb_contraband_sell");

        static SpecialCargoToolsRuntime& Get() noexcept
        {
            static SpecialCargoToolsRuntime instance;
            return instance;
        }

        bool QueueSourcingSettings(int amount, int cargoType, int specialItem, bool specialAvailable)
        {
            if (amount < 1 || amount > 111 || cargoType < -1 || cargoType > 10 || specialItem < 0 || specialItem > 5)
                return false;

            return Queue("Special Cargo sourcing settings queued", [this, amount, cargoType, specialItem, specialAvailable] {
                auto* pages = RequireGlobals();
                if (!pages)
                    return;

                int* amountGlobal = Script::ScriptGlobal(SourcingBaseGlobal).At(SourcingAmountOffset).As<int>(pages);
                int* typeGlobal = Script::ScriptGlobal(SourcingBaseGlobal).At(LupeCargoTypeOffset).As<int>(pages);
                int* specialGlobal = Script::ScriptGlobal(SourcingBaseGlobal).At(LupeSpecialItemOffset).As<int>(pages);
                int* availableGlobal = Script::ScriptGlobal(SourcingBaseGlobal).At(LupeSpecialAvailableOffset).As<int>(pages);
                if (!amountGlobal || !typeGlobal || !specialGlobal || !availableGlobal)
                    return Finish(false, "Special Cargo sourcing globals are unavailable");

                *amountGlobal = amount;
                *typeGlobal = cargoType;
                *specialGlobal = specialItem;
                *availableGlobal = specialAvailable ? 1 : 0;

                const bool success = *amountGlobal == amount
                    && *typeGlobal == cargoType
                    && *specialGlobal == specialItem
                    && *availableGlobal == (specialAvailable ? 1 : 0);

                if (success)
                {
                    TUTONES_LOG_INFO(
                        "recovery.special_cargo",
                        std::string("Applied sourcing settings amount=") + std::to_string(amount)
                            + " type=" + std::to_string(cargoType)
                            + " special=" + std::to_string(specialItem)
                            + " available=" + std::to_string(specialAvailable ? 1 : 0));
                }
                Finish(success, success ? "Special Cargo sourcing settings applied" : "Sourcing settings failed read-back verification");
            });
        }

        bool QueueCooldowns(int buyMilliseconds, int sellMilliseconds)
        {
            if (buyMilliseconds < 0 || sellMilliseconds < 0)
                return false;

            return Queue("Special Cargo cooldown globals queued", [this, buyMilliseconds, sellMilliseconds] {
                auto* pages = RequireGlobals();
                if (!pages)
                    return;

                int* buy = Script::ScriptGlobal(TunablesGlobal).At(BuyCooldownOffset).As<int>(pages);
                int* sell = Script::ScriptGlobal(TunablesGlobal).At(SellCooldownOffset).As<int>(pages);
                if (!buy || !sell)
                    return Finish(false, "Special Cargo cooldown globals are unavailable");

                *buy = buyMilliseconds;
                *sell = sellMilliseconds;
                const bool success = *buy == buyMilliseconds && *sell == sellMilliseconds;

                if (success)
                {
                    TUTONES_LOG_INFO(
                        "recovery.special_cargo",
                        std::string("Applied cooldowns buy=") + std::to_string(buyMilliseconds)
                            + " sell=" + std::to_string(sellMilliseconds));
                }
                Finish(success, success ? "Special Cargo cooldowns applied" : "Cooldown globals failed read-back verification");
            });
        }

        bool QueueCratePrice(int tierIndex, int value)
        {
            if (tierIndex < 0 || tierIndex >= CratePriceTierCount || value < 0)
                return false;

            return Queue("Special Cargo crate price queued", [this, tierIndex, value] {
                auto* pages = RequireGlobals();
                if (!pages)
                    return;

                int* price = Script::ScriptGlobal(TunablesGlobal)
                    .At(FirstCratePriceOffset + static_cast<std::size_t>(tierIndex))
                    .As<int>(pages);
                if (!price)
                    return Finish(false, "Selected Special Cargo price global is unavailable");

                *price = value;
                const bool success = *price == value;
                if (success)
                {
                    TUTONES_LOG_INFO(
                        "recovery.special_cargo",
                        std::string("Applied crate price tier=") + std::to_string(tierIndex)
                            + " value=" + std::to_string(value));
                }
                Finish(success, success ? "Special Cargo crate price applied" : "Crate price failed read-back verification");
            });
        }

        bool QueueUniqueSpecialItem(int uniqueItemValue)
        {
            if (!IsKnownUniqueSpecialItem(uniqueItemValue))
                return false;

            return Queue("Unique Special Cargo item queued", [this, uniqueItemValue] {
                auto* pages = RequireGlobals();
                if (!pages)
                    return;

                int* unique = Script::ScriptGlobal(UniqueSpecialItemGlobal).As<int>(pages);
                int* available = Script::ScriptGlobal(UniqueSpecialAvailableGlobal).As<int>(pages);
                if (!unique || !available)
                    return Finish(false, "Unique Special Cargo globals are unavailable");

                *unique = uniqueItemValue;
                *available = 1;
                const bool success = *unique == uniqueItemValue && *available == 1;

                if (success)
                {
                    TUTONES_LOG_INFO(
                        "recovery.special_cargo",
                        std::string("Forced unique Special Cargo item value=") + std::to_string(uniqueItemValue));
                }
                Finish(success, success ? "Unique Special Cargo item enabled" : "Unique Special Cargo globals failed read-back verification");
            });
        }

        bool QueueInstantBuy()
        {
            return Queue("Instant Special Cargo buy locals queued", [this] {
                auto* thread = RequireScriptThread(ContrabandBuyHash, "gb_contraband_buy");
                if (!thread)
                    return;

                int* stage = Script::ScriptLocal(thread, 634 + 5).As<int>();
                int* result = Script::ScriptLocal(thread, 634 + 191).As<int>();
                int* state = Script::ScriptLocal(thread, 634 + 192).As<int>();
                if (!stage || !result || !state)
                    return Finish(false, "gb_contraband_buy locals are unavailable");

                *stage = 1;
                *result = 6;
                *state = 4;
                const bool success = *stage == 1 && *result == 6 && *state == 4;
                if (success)
                    TUTONES_LOG_INFO("recovery.special_cargo", "Applied gb_contraband_buy locals 639=1, 825=6, 826=4");
                Finish(success, success ? "Instant Special Cargo buy locals applied" : "Instant buy locals failed read-back verification");
            });
        }

        bool QueueInstantSell()
        {
            return Queue("Instant Special Cargo sell local queued", [this] {
                auto* thread = RequireScriptThread(ContrabandSellHash, "gb_contraband_sell");
                if (!thread)
                    return;

                int* state = Script::ScriptLocal(thread, 576 + 1).As<int>();
                if (!state)
                    return Finish(false, "gb_contraband_sell local 577 is unavailable");

                *state = 67230;
                const bool success = *state == 67230;
                if (success)
                    TUTONES_LOG_INFO("recovery.special_cargo", "Applied gb_contraband_sell local 577=67230");
                Finish(success, success ? "Instant Special Cargo sell local applied" : "Instant sell local failed read-back verification");
            });
        }

        [[nodiscard]] SpecialCargoToolsSnapshot Snapshot() const
        {
            SpecialCargoToolsSnapshot snapshot;
            snapshot.pending = m_Pending.load(std::memory_order_acquire);
            std::scoped_lock lock(m_Mutex);
            snapshot.haveResult = m_HaveResult;
            snapshot.lastSucceeded = m_LastSucceeded;
            snapshot.message = m_Message;
            return snapshot;
        }

    private:
        SpecialCargoToolsRuntime() = default;

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
                Finish(false, "Join GTA Online before using Special Cargo tools");
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

        [[nodiscard]] Types::ScriptThread* RequireScriptThread(std::uint32_t scriptHash, const char* scriptName)
        {
            bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            if (!sessionStarted || !*sessionStarted)
            {
                Finish(false, "Join GTA Online before using Special Cargo mission locals");
                return nullptr;
            }

            auto& scripts = Script::ScriptRuntime::Get();
            if (!scripts.IsReady())
            {
                Finish(false, "Shared script runtime is unavailable");
                return nullptr;
            }

            auto* thread = scripts.FindThread(scriptHash);
            if (!thread || !thread->stack)
            {
                Finish(false, std::string(scriptName) + " is not active");
                return nullptr;
            }
            return thread;
        }

        [[nodiscard]] static bool IsKnownUniqueSpecialItem(int value) noexcept
        {
            switch (value)
            {
            case 2:
            case 4:
            case 6:
            case 7:
            case 8:
            case 9:
                return true;
            default:
                return false;
            }
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
        std::string m_Message{"Ready"};
    };
}
