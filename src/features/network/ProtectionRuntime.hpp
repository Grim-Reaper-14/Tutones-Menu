#pragma once

#include "../../core/logging/Logger.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::Protections
{
    struct ProtectionSnapshot final
    {
        bool installed{};
        bool passThroughOnly{true};
        bool blockMalformed{true};
        bool blockForcedLeave{};
        bool blockKnownCrashes{true};
        bool blockSounds{};
        bool blockExplosions{};
        bool blockFire{};
        bool blockWeaponDamage{};
        bool blockRagdoll{};
        bool blockClearTasks{};
        bool blockPtfx{};
        bool blockScriptEvents{};
        bool blockMalformedScriptEvents{true};
        std::uint64_t packetsInspected{};
        std::uint64_t packetsBlocked{};
        std::uint64_t eventsInspected{};
        std::uint64_t eventsBlocked{};
        std::uint64_t forcedLeaveAttemptsBlocked{};
        std::uint64_t knownCrashAttemptsBlocked{};
        int lastBlockedEvent{-1};
        int lastBlockedMessageType{-1};
        std::uint32_t lastBlockedPeerId{};
        std::string status{"Network receive hook disabled"};
    };

    class ProtectionRuntime final
    {
    public:
        static ProtectionRuntime& Get() noexcept
        {
            static ProtectionRuntime instance;
            return instance;
        }

        void PrepareForStart() noexcept
        {
            m_ShuttingDown.store(false, std::memory_order_release);
        }

        bool Start() noexcept
        {
            bool expected = false;
            if (m_Started.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            {
                TUTONES_LOG_INFO(
                    "protections",
                    "Network receive detour disabled for session-isolation diagnostics; Tutones is not intercepting inbound GTA traffic");
                SetStatus(
                    "SESSION DIAGNOSTIC MODE: ReceiveNetMessage is not hooked; inbound GTA, host-migration and session traffic bypass Tutones completely");
            }
            return true;
        }

        void Stop() noexcept
        {
            m_ShuttingDown.store(true, std::memory_order_release);
            if (m_Started.exchange(false, std::memory_order_acq_rel))
            {
                SetStatus("Stopped");
                TUTONES_LOG_INFO("protections", "Session diagnostic protection runtime stopped");
            }
        }

        // Preferences remain persisted so the user's selections are not lost while
        // receive-side filtering is disabled. No setter below installs a network hook
        // or suppresses GTA traffic in this diagnostic build.
        void SetBlockMalformed(bool value) noexcept { m_BlockMalformed.store(value, std::memory_order_release); }
        void SetBlockForcedLeave(bool value) noexcept { m_BlockForcedLeave.store(value, std::memory_order_release); }
        void SetBlockKnownCrashes(bool value) noexcept { m_BlockKnownCrashes.store(value, std::memory_order_release); }
        void SetBlockSounds(bool value) noexcept { m_BlockSounds.store(value, std::memory_order_release); }
        void SetBlockExplosions(bool value) noexcept { m_BlockExplosions.store(value, std::memory_order_release); }
        void SetBlockFire(bool value) noexcept { m_BlockFire.store(value, std::memory_order_release); }
        void SetBlockWeaponDamage(bool value) noexcept { m_BlockWeaponDamage.store(value, std::memory_order_release); }
        void SetBlockRagdoll(bool value) noexcept { m_BlockRagdoll.store(value, std::memory_order_release); }
        void SetBlockClearTasks(bool value) noexcept { m_BlockClearTasks.store(value, std::memory_order_release); }
        void SetBlockPtfx(bool value) noexcept { m_BlockPtfx.store(value, std::memory_order_release); }
        void SetBlockScriptEvents(bool value) noexcept { m_BlockScriptEvents.store(value, std::memory_order_release); }
        void SetBlockMalformedScriptEvents(bool value) noexcept { m_BlockMalformedScriptEvents.store(value, std::memory_order_release); }

        [[nodiscard]] ProtectionSnapshot Snapshot() const
        {
            ProtectionSnapshot out;
            out.installed = m_Started.load(std::memory_order_acquire);
            out.passThroughOnly = true;
            out.blockMalformed = m_BlockMalformed.load(std::memory_order_acquire);
            out.blockForcedLeave = m_BlockForcedLeave.load(std::memory_order_acquire);
            out.blockKnownCrashes = m_BlockKnownCrashes.load(std::memory_order_acquire);
            out.blockSounds = m_BlockSounds.load(std::memory_order_acquire);
            out.blockExplosions = m_BlockExplosions.load(std::memory_order_acquire);
            out.blockFire = m_BlockFire.load(std::memory_order_acquire);
            out.blockWeaponDamage = m_BlockWeaponDamage.load(std::memory_order_acquire);
            out.blockRagdoll = m_BlockRagdoll.load(std::memory_order_acquire);
            out.blockClearTasks = m_BlockClearTasks.load(std::memory_order_acquire);
            out.blockPtfx = m_BlockPtfx.load(std::memory_order_acquire);
            out.blockScriptEvents = m_BlockScriptEvents.load(std::memory_order_acquire);
            out.blockMalformedScriptEvents = m_BlockMalformedScriptEvents.load(std::memory_order_acquire);
            out.packetsInspected = 0;
            out.packetsBlocked = 0;
            out.eventsInspected = 0;
            out.eventsBlocked = 0;
            out.forcedLeaveAttemptsBlocked = 0;
            out.knownCrashAttemptsBlocked = 0;
            out.lastBlockedEvent = -1;
            out.lastBlockedMessageType = -1;
            out.lastBlockedPeerId = 0;
            {
                std::scoped_lock lock(m_StatusMutex);
                out.status = m_Status;
            }
            return out;
        }

        void ResetCounters() noexcept
        {
            // No receive-side counters exist while the network detour is disabled.
        }

    private:
        ProtectionRuntime() = default;

        void SetStatus(std::string status)
        {
            std::scoped_lock lock(m_StatusMutex);
            m_Status = std::move(status);
        }

        std::atomic<bool> m_Started{false};
        std::atomic<bool> m_ShuttingDown{false};
        std::atomic<bool> m_BlockMalformed{true};
        std::atomic<bool> m_BlockForcedLeave{false};
        std::atomic<bool> m_BlockKnownCrashes{true};
        std::atomic<bool> m_BlockSounds{false};
        std::atomic<bool> m_BlockExplosions{false};
        std::atomic<bool> m_BlockFire{false};
        std::atomic<bool> m_BlockWeaponDamage{false};
        std::atomic<bool> m_BlockRagdoll{false};
        std::atomic<bool> m_BlockClearTasks{false};
        std::atomic<bool> m_BlockPtfx{false};
        std::atomic<bool> m_BlockScriptEvents{false};
        std::atomic<bool> m_BlockMalformedScriptEvents{true};
        mutable std::mutex m_StatusMutex;
        std::string m_Status{"Network receive hook disabled"};
    };
}
