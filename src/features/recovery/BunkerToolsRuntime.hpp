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
    namespace BunkerToolsDetail
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

    struct BunkerToolsSnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool fastProductionEnabled{};
        std::string message{"Ready"};
    };

    struct BunkerTuningProfile final
    {
        int productValue{5000};
        float nearSaleMultiplier{1.0f};
        float farSaleMultiplier{1.5f};
        float highDemandBonus{2.5f};
        float highDemandMaxBonus{20.0f};
        int manufacturingProductionMs{600000};
        int researchProductionMs{300000};
    };

    class BunkerToolsRuntime final
    {
    public:
        static constexpr std::size_t TunablesGlobal = 262145;
        static constexpr std::size_t ProductValueOffset = 21347;
        static constexpr std::size_t NearSaleMultiplierOffset = 21319;
        static constexpr std::size_t FarSaleMultiplierOffset = 21320;
        static constexpr std::size_t HighDemandBonusOffset = 21232;
        static constexpr std::size_t HighDemandMaxBonusOffset = 21233;
        static constexpr std::size_t ManufacturingProductionOffset = 21342;
        static constexpr std::size_t ResearchProductionOffset = 21358;
        static constexpr int FastManufacturingProductionMs = 1000;

        static constexpr std::uint32_t GunrunningHash = BunkerToolsDetail::Joaat("gb_gunrunning");
        static constexpr std::size_t GunrunningInstantSellLocal = 1275 + 774;

        static BunkerToolsRuntime& Get() noexcept
        {
            static BunkerToolsRuntime instance;
            return instance;
        }

        bool QueueSetFastProduction(bool enabled)
        {
            return Queue(enabled ? "Bunker fast production queued" : "Bunker production restore queued", [this, enabled] {
                auto* pages = RequireGlobals();
                if (!pages)
                    return;

                int* manufacturing = Script::ScriptGlobal(TunablesGlobal).At(ManufacturingProductionOffset).As<int>(pages);
                if (!manufacturing)
                    return Finish(false, "Bunker manufacturing production global is unavailable");

                const bool wasEnabled = m_FastProductionEnabled.load(std::memory_order_acquire);
                if (enabled)
                {
                    if (!wasEnabled)
                        m_SavedManufacturingMs = *manufacturing > 0 ? *manufacturing : BunkerTuningProfile{}.manufacturingProductionMs;

                    *manufacturing = FastManufacturingProductionMs;
                    const bool success = *manufacturing == FastManufacturingProductionMs;
                    if (success)
                    {
                        m_FastProductionEnabled.store(true, std::memory_order_release);
                        TUTONES_LOG_INFO(
                            "recovery.bunker",
                            std::string("Enabled Bunker fast production manufactureMs=")
                                + std::to_string(FastManufacturingProductionMs)
                                + " restoreMs=" + std::to_string(m_SavedManufacturingMs));
                    }
                    return Finish(success,
                        success
                            ? "Fast Bunker stock production enabled (1 second per manufacturing cycle)"
                            : "Fast Bunker stock production failed read-back verification");
                }

                if (!wasEnabled)
                    return Finish(true, "Fast Bunker stock production is already disabled");

                const int restoreMs = m_SavedManufacturingMs > 0
                    ? m_SavedManufacturingMs
                    : BunkerTuningProfile{}.manufacturingProductionMs;
                *manufacturing = restoreMs;
                const bool success = *manufacturing == restoreMs;
                if (success)
                {
                    m_FastProductionEnabled.store(false, std::memory_order_release);
                    m_SavedManufacturingMs = 0;
                    TUTONES_LOG_INFO(
                        "recovery.bunker",
                        std::string("Restored Bunker manufacturing production time to ")
                            + std::to_string(restoreMs) + " ms");
                }
                Finish(success,
                    success
                        ? "Normal Bunker stock production restored"
                        : "Bunker production restore failed read-back verification");
            });
        }

        bool QueueApplyProfile(BunkerTuningProfile profile)
        {
            if (profile.productValue < 0
                || profile.nearSaleMultiplier < 0.0f
                || profile.farSaleMultiplier < 0.0f
                || profile.highDemandBonus < 0.0f
                || profile.highDemandMaxBonus < 0.0f
                || profile.manufacturingProductionMs < 0
                || profile.researchProductionMs < 0)
            {
                return false;
            }

            return Queue("Bunker tuning globals queued", [this, profile] {
                auto* pages = RequireGlobals();
                if (!pages)
                    return;

                int* productValue = Script::ScriptGlobal(TunablesGlobal).At(ProductValueOffset).As<int>(pages);
                float* nearSale = Script::ScriptGlobal(TunablesGlobal).At(NearSaleMultiplierOffset).As<float>(pages);
                float* farSale = Script::ScriptGlobal(TunablesGlobal).At(FarSaleMultiplierOffset).As<float>(pages);
                float* demandBonus = Script::ScriptGlobal(TunablesGlobal).At(HighDemandBonusOffset).As<float>(pages);
                float* maxDemandBonus = Script::ScriptGlobal(TunablesGlobal).At(HighDemandMaxBonusOffset).As<float>(pages);
                int* manufacturing = Script::ScriptGlobal(TunablesGlobal).At(ManufacturingProductionOffset).As<int>(pages);
                int* research = Script::ScriptGlobal(TunablesGlobal).At(ResearchProductionOffset).As<int>(pages);

                if (!productValue || !nearSale || !farSale || !demandBonus || !maxDemandBonus || !manufacturing || !research)
                    return Finish(false, "One or more Bunker tuning globals are unavailable");

                const bool fastProduction = m_FastProductionEnabled.load(std::memory_order_acquire);
                const int manufacturingTarget = fastProduction
                    ? FastManufacturingProductionMs
                    : profile.manufacturingProductionMs;
                if (fastProduction)
                    m_SavedManufacturingMs = profile.manufacturingProductionMs;

                *productValue = profile.productValue;
                *nearSale = profile.nearSaleMultiplier;
                *farSale = profile.farSaleMultiplier;
                *demandBonus = profile.highDemandBonus;
                *maxDemandBonus = profile.highDemandMaxBonus;
                *manufacturing = manufacturingTarget;
                *research = profile.researchProductionMs;

                const bool success = *productValue == profile.productValue
                    && *nearSale == profile.nearSaleMultiplier
                    && *farSale == profile.farSaleMultiplier
                    && *demandBonus == profile.highDemandBonus
                    && *maxDemandBonus == profile.highDemandMaxBonus
                    && *manufacturing == manufacturingTarget
                    && *research == profile.researchProductionMs;

                if (success)
                {
                    TUTONES_LOG_INFO(
                        "recovery.bunker",
                        std::string("Applied Bunker tuning profile product=") + std::to_string(profile.productValue)
                            + " near=" + std::to_string(profile.nearSaleMultiplier)
                            + " far=" + std::to_string(profile.farSaleMultiplier)
                            + " demand=" + std::to_string(profile.highDemandBonus)
                            + " maxDemand=" + std::to_string(profile.highDemandMaxBonus)
                            + " manufactureMs=" + std::to_string(manufacturingTarget)
                            + " researchMs=" + std::to_string(profile.researchProductionMs));
                }

                Finish(success, success ? "Bunker tuning globals applied" : "Bunker tuning globals failed read-back verification");
            });
        }

        bool QueueInstantSell()
        {
            return Queue("Bunker instant-sell local queued", [this] {
                bool* sessionStarted = GamePointers::Get().IsSessionStarted();
                if (!sessionStarted || !*sessionStarted)
                    return Finish(false, "Join GTA Online before using Bunker Instant Sell");

                auto& scripts = Script::ScriptRuntime::Get();
                if (!scripts.IsReady())
                    return Finish(false, "Shared script runtime is unavailable");

                auto* thread = scripts.FindThread(GunrunningHash);
                if (!thread || !thread->stack)
                    return Finish(false, "gb_gunrunning is not active");

                int* missionState = Script::ScriptLocal(thread, GunrunningInstantSellLocal).As<int>();
                if (!missionState)
                    return Finish(false, "gb_gunrunning instant-sell local is unavailable");

                *missionState = 0;
                const bool success = *missionState == 0;
                if (success)
                    TUTONES_LOG_INFO("recovery.bunker", "Applied gb_gunrunning local 2049=0 for Bunker Instant Sell");

                Finish(success, success ? "Bunker Instant Sell local applied" : "Bunker Instant Sell local failed read-back verification");
            });
        }

        [[nodiscard]] BunkerToolsSnapshot Snapshot() const
        {
            BunkerToolsSnapshot snapshot;
            snapshot.pending = m_Pending.load(std::memory_order_acquire);
            snapshot.fastProductionEnabled = m_FastProductionEnabled.load(std::memory_order_acquire);
            std::scoped_lock lock(m_Mutex);
            snapshot.haveResult = m_HaveResult;
            snapshot.lastSucceeded = m_LastSucceeded;
            snapshot.message = m_Message;
            return snapshot;
        }

    private:
        BunkerToolsRuntime() = default;

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
                Finish(false, "Join GTA Online before using Bunker tuning tools");
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
        std::atomic<bool> m_FastProductionEnabled{false};
        int m_SavedManufacturingMs{};
        mutable std::mutex m_Mutex;
        bool m_HaveResult{};
        bool m_LastSucceeded{};
        std::string m_Message{"Ready"};
    };
}
