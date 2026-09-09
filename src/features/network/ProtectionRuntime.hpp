#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/memory/PatternScanner.hpp"

#include <MinHook.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
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
        std::string status{"Not installed"};
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
            std::scoped_lock lock(m_LifecycleMutex);
            if (!m_Installed.load(std::memory_order_acquire) && !m_Target)
                m_ShuttingDown.store(false, std::memory_order_release);
        }

        bool Start() noexcept
        {
            std::scoped_lock lifecycleLock(m_LifecycleMutex);

            if (m_Installed.load(std::memory_order_acquire))
                return true;
            if (m_ShuttingDown.load(std::memory_order_acquire))
                return SetStatus(false, "Protection runtime is shutting down");

            const auto& module = GamePointers::Get().Module();
            auto* match = Memory::PatternScanner::FindFirst(module, "48 81 C1 00 03 00 00 4C 89 E2");
            if (!match)
                return SetStatus(false, "Enhanced ReceiveNetMessage pattern not found");

            auto* call = match + 0xD;
            if (static_cast<std::uint8_t>(*call) != 0xE8)
                return SetStatus(false, "ReceiveNetMessage callsite validation failed");

            m_Target = Memory::PatternScanner::ResolveRip(call + 1);
            if (!m_Target)
                return SetStatus(false, "ReceiveNetMessage target resolution failed");

            const MH_STATUS created = ::MH_CreateHook(
                m_Target,
                reinterpret_cast<void*>(&ReceiveNetMessageDetour),
                reinterpret_cast<void**>(&m_Original));
            if (created != MH_OK && created != MH_ERROR_ALREADY_CREATED)
                return SetStatus(false, std::string("Protection hook create failed: ") + MH_StatusToString(created));

            const MH_STATUS enabled = ::MH_EnableHook(m_Target);
            if (enabled != MH_OK && enabled != MH_ERROR_ENABLED)
            {
                if (created == MH_OK)
                    ::MH_RemoveHook(m_Target);
                m_Target = nullptr;
                m_Original = nullptr;
                return SetStatus(false, std::string("Protection hook enable failed: ") + MH_StatusToString(enabled));
            }

            m_Installed.store(true, std::memory_order_release);
            TUTONES_LOG_INFO(
                "protections",
                "Enhanced ReceiveNetMessage hook installed in passive session-safe pass-through mode");
            return SetStatus(
                true,
                "SESSION SAFE PASS-THROUGH: Tutones monitors inbound frames but never drops them; GTA receives every packet unchanged");
        }

        void Stop() noexcept
        {
            std::scoped_lock lifecycleLock(m_LifecycleMutex);
            m_ShuttingDown.store(true, std::memory_order_release);
            m_Installed.store(false, std::memory_order_release);

            if (m_Target)
            {
                const auto disabled = ::MH_DisableHook(m_Target);
                if (disabled != MH_OK && disabled != MH_ERROR_DISABLED && disabled != MH_ERROR_NOT_CREATED)
                    TUTONES_LOG_WARN("protections", "Failed to disable protection hook cleanly during shutdown");

                while (m_ActiveCallbacks.load(std::memory_order_acquire) != 0)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));

                const auto removed = ::MH_RemoveHook(m_Target);
                if (removed != MH_OK && removed != MH_ERROR_NOT_CREATED)
                    TUTONES_LOG_WARN("protections", "Failed to remove protection hook cleanly during shutdown");
            }

            m_Target = nullptr;
            m_Original = nullptr;
            SetStatus(false, "Stopped");
        }

        // These settings remain persisted so the user's protection preferences are not
        // lost. While pass-through mode is active they are policy preferences only and
        // do not cause ReceiveNetMessage to reject traffic.
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
            out.installed = m_Installed.load(std::memory_order_acquire);
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
            out.packetsInspected = m_PacketsInspected.load(std::memory_order_acquire);
            out.packetsBlocked = 0;
            out.eventsInspected = 0;
            out.eventsBlocked = 0;
            out.forcedLeaveAttemptsBlocked = 0;
            out.knownCrashAttemptsBlocked = 0;
            out.lastBlockedEvent = -1;
            out.lastBlockedMessageType = -1;
            out.lastBlockedPeerId = 0;
            std::scoped_lock lock(m_StatusMutex);
            out.status = m_Status;
            return out;
        }

        void ResetCounters() noexcept
        {
            m_PacketsInspected.store(0, std::memory_order_release);
        }

    private:
        enum class NetEventType : int
        {
            FrameReceived = 4,
        };

        class NetEvent
        {
        public:
            virtual ~NetEvent() = default;
            virtual void Destroy() = 0;
            virtual NetEventType GetEventType() = 0;
            virtual std::uint32_t Unknown18() = 0;

            std::uint32_t timestamp{};
            std::byte pad0C[52]{};
            std::uint32_t msgId{};
            std::uint32_t cxnId{};
            NetEvent* self{};
            std::uint32_t peerId{};
            std::byte pad54[4]{};
        };
        static_assert(sizeof(NetEvent) == 0x58);

        class FrameReceivedEvent : public NetEvent
        {
        public:
            int securityId{};
            std::byte pad5C[4]{};
            std::byte address[0x20]{};
            std::uint32_t length{};
            std::byte pad84[4]{};
            void* data{};
        };
        static_assert(sizeof(FrameReceivedEvent) == 0x90);

        using ReceiveNetMessageFn = void(*)(void*, void*, NetEvent*);

        ProtectionRuntime() = default;

        class CallbackGuard final
        {
        public:
            explicit CallbackGuard(ProtectionRuntime& owner) noexcept
                : m_Owner(owner)
            {
                m_Owner.m_ActiveCallbacks.fetch_add(1, std::memory_order_acq_rel);
            }

            ~CallbackGuard()
            {
                m_Owner.m_ActiveCallbacks.fetch_sub(1, std::memory_order_acq_rel);
            }

        private:
            ProtectionRuntime& m_Owner;
        };

        static void ReceiveNetMessageDetour(void* a1, void* manager, NetEvent* event)
        {
            auto& self = Get();
            CallbackGuard callback(self);

            const auto original = self.m_Original;
            if (!original)
                return;

            if (!self.m_ShuttingDown.load(std::memory_order_acquire)
                && self.m_Installed.load(std::memory_order_acquire)
                && event
                && event->GetEventType() == NetEventType::FrameReceived)
            {
                self.m_PacketsInspected.fetch_add(1, std::memory_order_relaxed);
            }

            // Deliberately fail open. Until the Enhanced PackedEvents parser is
            // validated against live b1158.13 traffic, no Tutones protection decision
            // is allowed to suppress session, host-migration, Rockstar or BattlEye-
            // adjacent transport traffic. GTA's original receive path always runs.
            original(a1, manager, event);
        }

        bool SetStatus(bool result, std::string status)
        {
            std::scoped_lock lock(m_StatusMutex);
            m_Status = std::move(status);
            return result;
        }

        std::atomic<bool> m_Installed{false};
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
        std::atomic<std::uint64_t> m_PacketsInspected{0};
        std::atomic<std::uint32_t> m_ActiveCallbacks{0};
        void* m_Target{};
        ReceiveNetMessageFn m_Original{};
        mutable std::mutex m_LifecycleMutex;
        mutable std::mutex m_StatusMutex;
        std::string m_Status{"Not installed"};
    };
}
