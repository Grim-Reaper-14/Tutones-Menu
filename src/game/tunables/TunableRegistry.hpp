#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../runtime/GameRuntime.hpp"
#include "../script/ScriptGlobal.hpp"
#include "../script/ScriptRuntime.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Tutones::Game::Tunables
{
    [[nodiscard]] constexpr std::uint32_t Joaat(std::string_view text) noexcept
    {
        std::uint32_t hash{};
        for (char c : text)
        {
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

    struct TunableRegistrySnapshot final
    {
        bool running{};
        bool initialized{};
        bool globalsReady{};
        bool idleTunablesResolved{};
        std::size_t registeredCount{};
        std::uint64_t revision{};
        std::string message{"Stopped"};
    };

    class TunableRegistry final
    {
    public:
        static constexpr std::size_t BaseGlobal = 262145;

        static TunableRegistry& Get() noexcept
        {
            static TunableRegistry instance;
            return instance;
        }

        bool Start()
        {
            bool expected = false;
            if (!m_Running.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return true;

            m_Initialized.store(false, std::memory_order_release);
            m_GlobalsReady.store(false, std::memory_order_release);
            m_IdleResolved.store(false, std::memory_order_release);
            m_TickQueued.store(false, std::memory_order_release);
            m_NextResolveAttemptMs.store(0, std::memory_order_release);
            m_Revision.store(0, std::memory_order_release);

            {
                std::scoped_lock lock(m_Mutex);
                m_Entries.clear();
                m_Message = "Starting central tunable registry";
            }

            // XP_MULTIPLIER is a long-standing Rockstar tunable used by the Recovery
            // runtime and YimMenuV2. Register the verified address immediately; live
            // access is still checked every time a Tunable object is read or written.
            RegisterKnown("XP_MULTIPLIER", 1);
            m_Initialized.store(true, std::memory_order_release);

            if (!QueueNextTick())
            {
                m_Running.store(false, std::memory_order_release);
                SetMessage("Tunable registry could not queue discovery tick");
                TUTONES_LOG_ERROR("game.tunables", "Central tunable registry failed to queue its first GTA script-thread tick");
                return false;
            }

            TUTONES_LOG_INFO("game.tunables", "Central tunable registry started; resolving named Enhanced tunables");
            return true;
        }

        void Stop() noexcept
        {
            if (!m_Running.exchange(false, std::memory_order_acq_rel))
                return;

            m_TickQueued.store(false, std::memory_order_release);
            m_Initialized.store(false, std::memory_order_release);
            m_GlobalsReady.store(false, std::memory_order_release);
            m_IdleResolved.store(false, std::memory_order_release);
            m_NextResolveAttemptMs.store(0, std::memory_order_release);

            {
                std::scoped_lock lock(m_Mutex);
                m_Entries.clear();
                m_Message = "Stopped";
            }

            m_Revision.fetch_add(1, std::memory_order_acq_rel);
            TUTONES_LOG_INFO("game.tunables", "Central tunable registry stopped");
        }

        [[nodiscard]] bool IsRunning() const noexcept
        {
            return m_Running.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool Initialized() const noexcept
        {
            return m_Initialized.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool RegisterKnown(std::string_view name, std::size_t offset) noexcept
        {
            if (name.empty())
                return false;
            return RegisterGlobal(Joaat(name), BaseGlobal + offset, name);
        }

        [[nodiscard]] bool RegisterGlobal(
            std::uint32_t hash,
            std::size_t globalIndex,
            std::string_view name = {}) noexcept
        {
            if (hash == 0 || globalIndex < BaseGlobal)
                return false;

            std::scoped_lock lock(m_Mutex);
            const auto found = m_Entries.find(hash);
            if (found != m_Entries.end())
            {
                if (found->second.globalIndex != globalIndex)
                {
                    TUTONES_LOG_WARN(
                        "game.tunables",
                        std::string("Rejected conflicting tunable registration for hash 0x") + Hex(hash));
                    return false;
                }
                return true;
            }

            Entry entry{};
            entry.globalIndex = globalIndex;
            entry.name = name.empty() ? std::string{} : std::string{name};
            m_Entries.emplace(hash, std::move(entry));
            m_Revision.fetch_add(1, std::memory_order_acq_rel);
            return true;
        }

        [[nodiscard]] std::optional<Script::ScriptGlobal> Resolve(std::uint32_t hash) const noexcept
        {
            std::scoped_lock lock(m_Mutex);
            const auto found = m_Entries.find(hash);
            if (found == m_Entries.end())
                return std::nullopt;
            return Script::ScriptGlobal(found->second.globalIndex);
        }

        [[nodiscard]] std::optional<Script::ScriptGlobal> Resolve(std::string_view name) const noexcept
        {
            return Resolve(Joaat(name));
        }

        [[nodiscard]] TunableRegistrySnapshot Snapshot() const
        {
            TunableRegistrySnapshot snapshot;
            snapshot.running = IsRunning();
            snapshot.initialized = Initialized();
            snapshot.globalsReady = m_GlobalsReady.load(std::memory_order_acquire);
            snapshot.idleTunablesResolved = m_IdleResolved.load(std::memory_order_acquire);
            snapshot.revision = m_Revision.load(std::memory_order_acquire);
            std::scoped_lock lock(m_Mutex);
            snapshot.registeredCount = m_Entries.size();
            snapshot.message = m_Message;
            return snapshot;
        }

    private:
        struct Entry final
        {
            std::size_t globalIndex{};
            std::string name;
        };

        static constexpr std::array<std::string_view, 8> IdleNames{{
            "IDLEKICK_WARNING1",
            "IDLEKICK_WARNING2",
            "IDLEKICK_WARNING3",
            "IDLEKICK_KICK",
            "ConstrainedKick_Warning1",
            "ConstrainedKick_Warning2",
            "ConstrainedKick_Warning3",
            "ConstrainedKick_Kick",
        }};

        static constexpr std::array<int, 4> IdleDefaults{{
            120000, 300000, 600000, 900000,
        }};
        static constexpr std::array<int, 4> ConstrainedDefaults{{
            30000, 60000, 90000, 120000,
        }};
        static constexpr std::size_t HistoricalIdleOffset = 87;
        static constexpr std::size_t HistoricalConstrainedOffset = 8420;
        static constexpr std::size_t FallbackScanCount = 20000;
        static constexpr std::int64_t ResolveRetryMs = 750;

        TunableRegistry() = default;

        [[nodiscard]] static std::string Hex(std::uint32_t value)
        {
            constexpr char Digits[] = "0123456789ABCDEF";
            std::string out(8, '0');
            for (int index = 7; index >= 0; --index)
            {
                out[static_cast<std::size_t>(index)] = Digits[value & 0xFu];
                value >>= 4;
            }
            return out;
        }

        void SetMessage(std::string message)
        {
            std::scoped_lock lock(m_Mutex);
            m_Message = std::move(message);
        }

        [[nodiscard]] bool QueueNextTick() noexcept
        {
            if (!IsRunning())
                return false;

            bool expected = false;
            if (!m_TickQueued.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return true;

            if (Runtime::GameRuntime::Get().Enqueue([this] { TickOnGameThread(); }))
                return true;

            m_TickQueued.store(false, std::memory_order_release);
            return false;
        }

        void TickOnGameThread() noexcept
        {
            m_TickQueued.store(false, std::memory_order_release);
            if (!IsRunning())
                return;

            auto& scripts = Script::ScriptRuntime::Get();
            auto** globals = scripts.Globals();
            const bool globalsReady = scripts.IsReady() && globals != nullptr;
            m_GlobalsReady.store(globalsReady, std::memory_order_release);

            if (!globalsReady)
            {
                SetMessage("Waiting for shared script globals");
                if (!QueueNextTick())
                    TUTONES_LOG_ERROR("game.tunables", "Tunable registry lost its GTA script-thread scheduling slot");
                return;
            }

            if (!m_IdleResolved.load(std::memory_order_acquire))
            {
                const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
                const auto nextAttempt = m_NextResolveAttemptMs.load(std::memory_order_acquire);

                if (nextAttempt == 0 || nowMs >= nextAttempt)
                {
                    m_NextResolveAttemptMs.store(nowMs + ResolveRetryMs, std::memory_order_release);
                    if (ResolveIdleTunables(globals))
                    {
                        m_IdleResolved.store(true, std::memory_order_release);
                        m_NextResolveAttemptMs.store(0, std::memory_order_release);
                        const auto snapshot = Snapshot();
                        SetMessage(
                            std::string("Ready - ") + std::to_string(snapshot.registeredCount)
                                + " named tunables registered");
                        TUTONES_LOG_INFO(
                            "game.tunables",
                            std::string("Central tunable registry ready; registered ")
                                + std::to_string(snapshot.registeredCount)
                                + " named tunables");
                        return;
                    }
                    SetMessage("Registry online - resolving idle tunable names");
                }
            }

            if (!QueueNextTick())
                TUTONES_LOG_ERROR("game.tunables", "Tunable registry lost its GTA script-thread scheduling slot");
        }

        template<std::size_t N>
        [[nodiscard]] static bool PatternMatchesAt(
            std::int64_t** globals,
            std::size_t offset,
            const std::array<int, N>& pattern) noexcept
        {
            if (!globals)
                return false;

            for (std::size_t index = 0; index < N; ++index)
            {
                const int* value = Script::ScriptGlobal(BaseGlobal + offset + index).As<int>(globals);
                if (!value || *value != pattern[index])
                    return false;
            }
            return true;
        }

        template<std::size_t N>
        [[nodiscard]] static std::vector<std::size_t> FindPatternMatches(
            std::int64_t** globals,
            const std::array<int, N>& pattern) noexcept
        {
            std::vector<std::size_t> matches;
            for (std::size_t offset = 0; offset + N <= FallbackScanCount; ++offset)
            {
                if (PatternMatchesAt(globals, offset, pattern))
                    matches.push_back(offset);
            }
            return matches;
        }

        [[nodiscard]] static std::optional<std::pair<std::size_t, std::size_t>> SelectBestPair(
            const std::vector<std::size_t>& idleMatches,
            const std::vector<std::size_t>& constrainedMatches) noexcept
        {
            if (idleMatches.empty() || constrainedMatches.empty())
                return std::nullopt;

            std::optional<std::pair<std::size_t, std::size_t>> best;
            std::size_t bestScore = std::numeric_limits<std::size_t>::max();
            for (const auto idle : idleMatches)
            {
                for (const auto constrained : constrainedMatches)
                {
                    if (idle == constrained)
                        continue;
                    const auto idleDelta = idle > HistoricalIdleOffset
                        ? idle - HistoricalIdleOffset
                        : HistoricalIdleOffset - idle;
                    const auto constrainedDelta = constrained > HistoricalConstrainedOffset
                        ? constrained - HistoricalConstrainedOffset
                        : HistoricalConstrainedOffset - constrained;
                    const auto score = idleDelta + constrainedDelta;
                    if (!best || score < bestScore)
                    {
                        best = std::pair<std::size_t, std::size_t>{idle, constrained};
                        bestScore = score;
                    }
                }
            }
            return best;
        }

        [[nodiscard]] bool ResolveIdleTunables(std::int64_t** globals) noexcept
        {
            std::optional<std::pair<std::size_t, std::size_t>> pair;
            if (PatternMatchesAt(globals, HistoricalIdleOffset, IdleDefaults)
                && PatternMatchesAt(globals, HistoricalConstrainedOffset, ConstrainedDefaults))
            {
                pair = std::pair<std::size_t, std::size_t>{
                    HistoricalIdleOffset,
                    HistoricalConstrainedOffset,
                };
            }
            else
            {
                pair = SelectBestPair(
                    FindPatternMatches(globals, IdleDefaults),
                    FindPatternMatches(globals, ConstrainedDefaults));
            }

            if (!pair)
                return false;

            const auto [idleOffset, constrainedOffset] = *pair;
            for (std::size_t index = 0; index < 4; ++index)
            {
                if (!RegisterKnown(IdleNames[index], idleOffset + index))
                    return false;
                if (!RegisterKnown(IdleNames[index + 4], constrainedOffset + index))
                    return false;
            }

            TUTONES_LOG_INFO(
                "game.tunables",
                std::string("Registered idle tunable hashes from live Global_262145: idle +")
                    + std::to_string(idleOffset)
                    + ", constrained +"
                    + std::to_string(constrainedOffset));
            return true;
        }

        std::atomic<bool> m_Running{false};
        std::atomic<bool> m_Initialized{false};
        std::atomic<bool> m_GlobalsReady{false};
        std::atomic<bool> m_IdleResolved{false};
        std::atomic<bool> m_TickQueued{false};
        std::atomic<std::int64_t> m_NextResolveAttemptMs{0};
        std::atomic<std::uint64_t> m_Revision{0};
        mutable std::mutex m_Mutex;
        std::unordered_map<std::uint32_t, Entry> m_Entries;
        std::string m_Message{"Stopped"};
    };

    class Tunable final
    {
    public:
        constexpr explicit Tunable(std::uint32_t hash) noexcept
            : m_Hash(hash)
        {
        }

        constexpr explicit Tunable(std::string_view name) noexcept
            : m_Hash(Joaat(name))
        {
        }

        [[nodiscard]] bool IsReady() const noexcept
        {
            const auto global = TunableRegistry::Get().Resolve(m_Hash);
            auto** globals = Script::ScriptRuntime::Get().Globals();
            return global.has_value() && globals && global->As<std::int64_t>(globals) != nullptr;
        }

        template<typename T>
        [[nodiscard]] std::optional<T> Get() const noexcept
        {
            const auto global = TunableRegistry::Get().Resolve(m_Hash);
            auto** globals = Script::ScriptRuntime::Get().Globals();
            if (!global || !globals)
                return std::nullopt;
            const T* value = global->As<T>(globals);
            if (!value)
                return std::nullopt;
            return *value;
        }

        template<typename T>
        bool Set(T value) const noexcept
        {
            const auto global = TunableRegistry::Get().Resolve(m_Hash);
            auto** globals = Script::ScriptRuntime::Get().Globals();
            if (!global || !globals)
                return false;
            T* target = global->As<T>(globals);
            if (!target)
                return false;
            *target = value;
            return *target == value;
        }

        [[nodiscard]] constexpr std::uint32_t Hash() const noexcept
        {
            return m_Hash;
        }

    private:
        std::uint32_t m_Hash{};
    };
}
