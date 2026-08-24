#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::Business
{
    enum class InstantResupplyTarget : std::size_t
    {
        Slot0,
        Slot1,
        Slot2,
        Slot3,
        Slot4,
        Bunker,
        AcidLab,
        Count,
    };

    struct InstantResupplyDefinition final
    {
        const char* label;
        std::size_t offset;
    };

    inline constexpr std::size_t InstantResupplyGlobal = 1673820;
    inline constexpr std::array<InstantResupplyDefinition, static_cast<std::size_t>(InstantResupplyTarget::Count)> InstantResupplyCatalog{{
        {"Slot 0", 1},
        {"Slot 1", 2},
        {"Slot 2", 3},
        {"Slot 3", 4},
        {"Slot 4", 5},
        {"Bunker", 6},
        {"Acid Lab", 7},
    }};

    struct InstantResupplySnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        std::string message{"Ready"};
    };

    class InstantResupplyRuntime final
    {
    public:
        static InstantResupplyRuntime& Get() noexcept
        {
            static InstantResupplyRuntime instance;
            return instance;
        }

        bool QueueRequest(InstantResupplyTarget target)
        {
            const auto index = static_cast<std::size_t>(target);
            if (index >= InstantResupplyCatalog.size())
                return false;

            const auto definition = InstantResupplyCatalog[index];
            return Queue(std::string(definition.label) + " resupply queued", [this, definition] {
                bool* sessionStarted = GamePointers::Get().IsSessionStarted();
                if (!sessionStarted || !*sessionStarted)
                    return Finish(false, "Join GTA Online before using Instant Resupply");

                auto* pages = GamePointers::Get().ScriptGlobals();
                if (!pages)
                    return Finish(false, "Enhanced script globals are unavailable");

                int* target = Script::ScriptGlobal(InstantResupplyGlobal).At(definition.offset).As<int>(pages);
                if (!target)
                    return Finish(false, "Instant Resupply global is unavailable");

                *target = 1;
                const bool success = *target == 1;
                if (success)
                {
                    TUTONES_LOG_INFO(
                        "business.resupply",
                        std::string("Applied ") + definition.label + " via Global_1673820 + " + std::to_string(definition.offset));
                }

                Finish(
                    success,
                    success ? std::string(definition.label) + " Instant Resupply applied"
                            : std::string(definition.label) + " Instant Resupply failed read-back verification");
            });
        }

        [[nodiscard]] InstantResupplySnapshot Snapshot() const
        {
            InstantResupplySnapshot snapshot;
            snapshot.pending = m_Pending.load(std::memory_order_acquire);
            std::scoped_lock lock(m_Mutex);
            snapshot.haveResult = m_HaveResult;
            snapshot.lastSucceeded = m_LastSucceeded;
            snapshot.message = m_Message;
            return snapshot;
        }

    private:
        InstantResupplyRuntime() = default;

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
