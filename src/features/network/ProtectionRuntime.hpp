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
        bool blockMalformed{true};
        bool blockForcedLeave{true};
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
                "Enhanced ReceiveNetMessage protection hook installed with crash and forced-leave filtering");
            return SetStatus(true, "Enhanced crash, kick and packet protections active");
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
            out.packetsBlocked = m_PacketsBlocked.load(std::memory_order_acquire);
            out.eventsInspected = m_EventsInspected.load(std::memory_order_acquire);
            out.eventsBlocked = m_EventsBlocked.load(std::memory_order_acquire);
            out.forcedLeaveAttemptsBlocked = m_ForcedLeaveAttemptsBlocked.load(std::memory_order_acquire);
            out.knownCrashAttemptsBlocked = m_KnownCrashAttemptsBlocked.load(std::memory_order_acquire);
            out.lastBlockedEvent = m_LastBlockedEvent.load(std::memory_order_acquire);
            out.lastBlockedMessageType = m_LastBlockedMessageType.load(std::memory_order_acquire);
            out.lastBlockedPeerId = m_LastBlockedPeerId.load(std::memory_order_acquire);
            std::scoped_lock lock(m_StatusMutex);
            out.status = m_Status;
            return out;
        }

        void ResetCounters() noexcept
        {
            m_PacketsInspected.store(0, std::memory_order_release);
            m_PacketsBlocked.store(0, std::memory_order_release);
            m_EventsInspected.store(0, std::memory_order_release);
            m_EventsBlocked.store(0, std::memory_order_release);
            m_ForcedLeaveAttemptsBlocked.store(0, std::memory_order_release);
            m_KnownCrashAttemptsBlocked.store(0, std::memory_order_release);
            m_LastBlockedEvent.store(-1, std::memory_order_release);
            m_LastBlockedMessageType.store(-1, std::memory_order_release);
            m_LastBlockedPeerId.store(0, std::memory_order_release);
        }

    private:
        enum class NetEventType : int
        {
            FrameReceived = 4,
        };

        enum class PayloadVerdict
        {
            Allow,
            Malformed,
            KnownCrash,
            ForcedLeave,
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

        class BitReader final
        {
        public:
            BitReader(const void* data, std::size_t bytes) noexcept
                : m_Data(static_cast<const std::uint8_t*>(data)), m_Bits(bytes * 8)
            {
            }

            [[nodiscard]] bool Read(std::uint32_t count, std::uint64_t& out) noexcept
            {
                if (!m_Data || count > 64 || m_Pos + count > m_Bits)
                    return false;
                out = 0;
                for (std::uint32_t i = 0; i < count; ++i)
                {
                    const std::size_t bit = m_Pos + i;
                    const std::uint8_t value = (m_Data[bit >> 3] >> (7 - (bit & 7))) & 1;
                    out = (out << 1) | value;
                }
                m_Pos += count;
                return true;
            }

            [[nodiscard]] bool Skip(std::size_t count) noexcept
            {
                if (m_Pos + count > m_Bits)
                    return false;
                m_Pos += count;
                return true;
            }

            [[nodiscard]] bool Peek(std::uint32_t count, std::uint64_t& out) const noexcept
            {
                BitReader copy = *this;
                return copy.Read(count, out);
            }

            [[nodiscard]] BitReader Limited(std::size_t count) const noexcept
            {
                BitReader copy = *this;
                if (count < copy.Remaining())
                    copy.m_Bits = copy.m_Pos + count;
                return copy;
            }

            [[nodiscard]] std::size_t Position() const noexcept { return m_Pos; }
            [[nodiscard]] std::size_t Remaining() const noexcept { return m_Bits >= m_Pos ? m_Bits - m_Pos : 0; }

        private:
            const std::uint8_t* m_Data{};
            std::size_t m_Bits{};
            std::size_t m_Pos{};
        };

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

            if (self.m_ShuttingDown.load(std::memory_order_acquire)
                || !self.m_Installed.load(std::memory_order_acquire)
                || !event
                || event->GetEventType() != NetEventType::FrameReceived)
            {
                original(a1, manager, event);
                return;
            }

            auto* frame = static_cast<FrameReceivedEvent*>(event);
            self.m_PacketsInspected.fetch_add(1, std::memory_order_relaxed);
            if (!frame->data || frame->length == 0 || frame->length > 65535)
            {
                if (self.m_BlockMalformed.load(std::memory_order_acquire))
                {
                    self.BlockPacket(-2, -1, frame->peerId);
                    return;
                }
                original(a1, manager, event);
                return;
            }

            BitReader reader(frame->data, frame->length);
            std::uint64_t magic{};
            if (!reader.Read(14, magic))
            {
                if (self.m_BlockMalformed.load(std::memory_order_acquire))
                {
                    self.BlockPacket(-2, -1, frame->peerId);
                    return;
                }
                original(a1, manager, event);
                return;
            }

            // Match YimMenuV2: an unknown/non-message frame is passed to GTA unchanged.
            if (magic != 0x3246)
            {
                original(a1, manager, event);
                return;
            }

            std::uint64_t extended{};
            std::uint64_t messageType{};
            if (!reader.Read(1, extended) || !reader.Read(extended ? 16u : 8u, messageType))
            {
                if (self.m_BlockMalformed.load(std::memory_order_acquire))
                {
                    self.BlockPacket(-2, -1, frame->peerId);
                    return;
                }
                original(a1, manager, event);
                return;
            }

            // Current Enhanced netMessage::Type values.
            constexpr std::uint64_t RequestKickFromHost = 0x0D;
            constexpr std::uint64_t KickPlayer = 0x34;
            constexpr std::uint64_t PackedEvents = 0x4F;

            if (self.m_BlockForcedLeave.load(std::memory_order_acquire)
                && (messageType == RequestKickFromHost || messageType == KickPlayer))
            {
                self.BlockForcedLeave(static_cast<int>(messageType), frame->peerId);
                return;
            }

            if (messageType == PackedEvents && self.ShouldBlockPackedEvents(reader, frame->peerId))
                return;

            original(a1, manager, event);
        }

        [[nodiscard]] bool ShouldBlockPackedEvents(BitReader& reader, std::uint32_t peerId) noexcept
        {
            std::uint64_t count{};
            std::uint64_t bufferSize{};
            if (!reader.Read(5, count) || !reader.Read(15, bufferSize))
                return BlockMalformedPacket(peerId);

            if (bufferSize > 7296 || bufferSize > reader.Remaining())
                return BlockMalformedPacket(peerId);

            std::size_t remaining = static_cast<std::size_t>(bufferSize);
            std::uint64_t parsed{};

            // Inspect the entire declared PackedEvents payload instead of trusting the
            // 5-bit count as a loop bound. This prevents hidden events after a forged count.
            while (remaining >= 39)
            {
                const std::size_t before = reader.Position();
                std::uint64_t eventId{};
                std::uint64_t eventIndex{};
                std::uint64_t handledBits{};
                std::uint64_t eventDataSize{};
                std::uint64_t hasExtra{};
                if (!reader.Read(7, eventId)
                    || !reader.Read(9, eventIndex)
                    || !reader.Read(8, handledBits)
                    || !reader.Read(15, eventDataSize)
                    || !reader.Read(1, hasExtra))
                    return BlockMalformedPacket(peerId);
                if (hasExtra && !reader.Skip(16))
                    return BlockMalformedPacket(peerId);

                const std::size_t headerBits = reader.Position() - before;
                if (headerBits > remaining)
                    return BlockMalformedPacket(peerId);

                m_EventsInspected.fetch_add(1, std::memory_order_relaxed);
                if (eventDataSize > reader.Remaining()
                    || eventDataSize > remaining - headerBits)
                    return BlockMalformedPacket(peerId);

                const int id = static_cast<int>(eventId);
                if (IsConfiguredEventBlocked(id))
                {
                    m_EventsBlocked.fetch_add(1, std::memory_order_relaxed);
                    BlockPacket(id, -1, peerId);
                    return true;
                }

                const auto payload = reader.Limited(static_cast<std::size_t>(eventDataSize));
                const auto verdict = InspectEventPayload(id, payload);
                if (verdict == PayloadVerdict::Malformed)
                {
                    if (m_BlockMalformed.load(std::memory_order_acquire))
                    {
                        m_EventsBlocked.fetch_add(1, std::memory_order_relaxed);
                        BlockPacket(-2, -1, peerId);
                        return true;
                    }
                }
                else if (verdict == PayloadVerdict::KnownCrash
                    && m_BlockKnownCrashes.load(std::memory_order_acquire))
                {
                    m_EventsBlocked.fetch_add(1, std::memory_order_relaxed);
                    m_KnownCrashAttemptsBlocked.fetch_add(1, std::memory_order_relaxed);
                    BlockPacket(id, -1, peerId);
                    return true;
                }
                else if (verdict == PayloadVerdict::ForcedLeave
                    && m_BlockForcedLeave.load(std::memory_order_acquire))
                {
                    m_EventsBlocked.fetch_add(1, std::memory_order_relaxed);
                    m_ForcedLeaveAttemptsBlocked.fetch_add(1, std::memory_order_relaxed);
                    BlockPacket(id, -1, peerId);
                    return true;
                }

                if (!reader.Skip(static_cast<std::size_t>(eventDataSize)))
                    return BlockMalformedPacket(peerId);

                const std::size_t consumed = reader.Position() - before;
                if (consumed > remaining)
                    return BlockMalformedPacket(peerId);
                remaining -= consumed;
                ++parsed;
            }

            // A forged event count can otherwise hide events from filters. Treat a
            // mismatch as malformed while still permitting normal trailing padding bits.
            if (parsed != count)
                return BlockMalformedPacket(peerId);

            return false;
        }

        [[nodiscard]] PayloadVerdict InspectEventPayload(int id, BitReader payload) const noexcept
        {
            switch (id)
            {
            case 27: // DOOR_BREAK_EVENT; current Enhanced Yim protection treats it as non-legitimate traffic.
                return PayloadVerdict::KnownCrash;

            case 28: // SCRIPTED_GAME_EVENT
            {
                if (!m_BlockMalformedScriptEvents.load(std::memory_order_acquire))
                    return PayloadVerdict::Allow;

                std::uint64_t argsSize{};
                if (!payload.Read(32, argsSize))
                    return PayloadVerdict::Malformed;

                // CScriptedGameEvent stores at most 54 int64 arguments = 432 bytes.
                if (argsSize > 432 || argsSize * 8 > payload.Remaining())
                    return PayloadVerdict::Malformed;
                return PayloadVerdict::Allow;
            }

            case 33: // SCRIPT_WORLD_STATE_EVENT
            {
                std::uint64_t type{};
                std::uint64_t ignored{};
                if (!payload.Read(4, type) || !payload.Read(1, ignored))
                    return PayloadVerdict::Malformed;
                if (!ReadGameScriptId(payload))
                    return PayloadVerdict::Malformed;

                // PopGroupOverride crash: invalid zero population group with the
                // known invalid percentages used by crash payloads.
                if (type == 2)
                {
                    std::uint64_t popSchedule{};
                    std::uint64_t popGroup{};
                    std::uint64_t percentage{};
                    if (!payload.Read(8, popSchedule)
                        || !payload.Read(32, popGroup)
                        || !payload.Read(7, percentage))
                        return PayloadVerdict::Malformed;

                    if (popGroup == 0 && (percentage == 0 || percentage == 103))
                        return PayloadVerdict::KnownCrash;
                }
                return PayloadVerdict::Allow;
            }

            case 50: // SCRIPT_ENTITY_STATE_CHANGE_EVENT
            {
                std::uint64_t entity{};
                std::uint64_t type{};
                std::uint64_t unknown{};
                if (!payload.Read(13, entity)
                    || !payload.Read(4, type)
                    || !payload.Read(32, unknown))
                    return PayloadVerdict::Malformed;

                // Current Enhanced values are 0..9. Values above the last valid type
                // are rejected before the game can dispatch an invalid state change.
                if (type > 9)
                    return PayloadVerdict::KnownCrash;

                // SettingOfTaskVehicleTempAction crash payloads use actions 15..18.
                if (type == 6)
                {
                    std::uint64_t vehicleId{};
                    std::uint64_t action{};
                    if (!payload.Read(13, vehicleId) || !payload.Read(8, action))
                        return PayloadVerdict::Malformed;
                    if (action >= 15 && action <= 18)
                        return PayloadVerdict::KnownCrash;
                }
                return PayloadVerdict::Allow;
            }

            case 64: // KICK_VOTES_EVENT
                return PayloadVerdict::ForcedLeave;

            default:
                return PayloadVerdict::Allow;
            }
        }

        [[nodiscard]] static bool ReadGameScriptId(BitReader& payload) noexcept
        {
            std::uint64_t ignored{};
            std::uint64_t hasPositionHash{};
            std::uint64_t hasInstanceId{};

            if (!payload.Read(32, ignored) || !payload.Read(32, ignored))
                return false;
            if (!payload.Read(1, hasPositionHash))
                return false;
            if (hasPositionHash && !payload.Read(32, ignored))
                return false;
            if (!payload.Read(1, hasInstanceId))
                return false;
            if (hasInstanceId && !payload.Read(8, ignored))
                return false;
            return true;
        }

        [[nodiscard]] bool IsConfiguredEventBlocked(int id) const noexcept
        {
            switch (id)
            {
            case 6: return m_BlockWeaponDamage.load(std::memory_order_acquire);
            case 16: return m_BlockFire.load(std::memory_order_acquire);
            case 17: return m_BlockExplosions.load(std::memory_order_acquire);
            case 24: return m_BlockRagdoll.load(std::memory_order_acquire);
            case 28: return m_BlockScriptEvents.load(std::memory_order_acquire);
            case 43: return m_BlockClearTasks.load(std::memory_order_acquire);
            case 51: return m_BlockSounds.load(std::memory_order_acquire);
            case 74: return m_BlockPtfx.load(std::memory_order_acquire);
            default: return false;
            }
        }

        [[nodiscard]] bool BlockMalformedPacket(std::uint32_t peerId) noexcept
        {
            if (!m_BlockMalformed.load(std::memory_order_acquire))
                return false;
            BlockPacket(-2, -1, peerId);
            return true;
        }

        void BlockForcedLeave(int messageType, std::uint32_t peerId) noexcept
        {
            m_ForcedLeaveAttemptsBlocked.fetch_add(1, std::memory_order_relaxed);
            BlockPacket(-1, messageType, peerId);
        }

        void BlockPacket(int eventId, int messageType, std::uint32_t peerId) noexcept
        {
            m_PacketsBlocked.fetch_add(1, std::memory_order_relaxed);
            m_LastBlockedEvent.store(eventId, std::memory_order_release);
            m_LastBlockedMessageType.store(messageType, std::memory_order_release);
            m_LastBlockedPeerId.store(peerId, std::memory_order_release);
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
        std::atomic<bool> m_BlockForcedLeave{true};
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
        std::atomic<std::uint64_t> m_PacketsBlocked{0};
        std::atomic<std::uint64_t> m_EventsInspected{0};
        std::atomic<std::uint64_t> m_EventsBlocked{0};
        std::atomic<std::uint64_t> m_ForcedLeaveAttemptsBlocked{0};
        std::atomic<std::uint64_t> m_KnownCrashAttemptsBlocked{0};
        std::atomic<std::uint32_t> m_ActiveCallbacks{0};
        std::atomic<int> m_LastBlockedEvent{-1};
        std::atomic<int> m_LastBlockedMessageType{-1};
        std::atomic<std::uint32_t> m_LastBlockedPeerId{0};
        void* m_Target{};
        ReceiveNetMessageFn m_Original{};
        mutable std::mutex m_LifecycleMutex;
        mutable std::mutex m_StatusMutex;
        std::string m_Status{"Not installed"};
    };
}
