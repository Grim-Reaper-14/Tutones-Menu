#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/memory/PatternScanner.hpp"

#include <MinHook.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::Protections
{
    struct ProtectionSnapshot final
    {
        bool installed{};
        bool blockMalformed{true};
        bool blockSounds{true};
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
        int lastBlockedEvent{-1};
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

        bool Start() noexcept
        {
            if (m_Installed.load(std::memory_order_acquire))
                return true;

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
            TUTONES_LOG_INFO("protections", "Yim-style Enhanced ReceiveNetMessage protection hook installed");
            return SetStatus(true, "Enhanced packet protections active");
        }

        void Stop() noexcept
        {
            m_Installed.store(false, std::memory_order_release);
            if (m_Target)
            {
                ::MH_DisableHook(m_Target);
                ::MH_RemoveHook(m_Target);
            }
            m_Target = nullptr;
            m_Original = nullptr;
            SetStatus(false, "Stopped");
        }

        void SetBlockMalformed(bool value) noexcept { m_BlockMalformed.store(value, std::memory_order_release); }
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
            out.lastBlockedEvent = m_LastBlockedEvent.load(std::memory_order_acquire);
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
            m_LastBlockedEvent.store(-1, std::memory_order_release);
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

            [[nodiscard]] std::size_t Position() const noexcept { return m_Pos; }
            [[nodiscard]] std::size_t Remaining() const noexcept { return m_Bits >= m_Pos ? m_Bits - m_Pos : 0; }

        private:
            const std::uint8_t* m_Data{};
            std::size_t m_Bits{};
            std::size_t m_Pos{};
        };

        ProtectionRuntime() = default;

        static void ReceiveNetMessageDetour(void* a1, void* manager, NetEvent* event)
        {
            auto& self = Get();
            const auto original = self.m_Original;
            if (!original)
                return;

            if (!self.m_Installed.load(std::memory_order_acquire)
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
                    self.BlockPacket(-2);
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
                    self.BlockPacket(-2);
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
                    self.BlockPacket(-2);
                    return;
                }
                original(a1, manager, event);
                return;
            }

            // PackedEvents = 0x4F in current YimMenuV2 Enhanced netMessage::Type.
            if (messageType == 0x4F && self.ShouldBlockPackedEvents(reader))
                return;

            original(a1, manager, event);
        }

        [[nodiscard]] bool ShouldBlockPackedEvents(BitReader& reader) noexcept
        {
            std::uint64_t count{};
            std::uint64_t bufferSize{};
            if (!reader.Read(5, count) || !reader.Read(15, bufferSize))
                return BlockMalformedPacket();

            if (bufferSize > 7296 || bufferSize > reader.Remaining())
                return BlockMalformedPacket();

            std::size_t remaining = static_cast<std::size_t>(bufferSize);
            std::uint64_t parsed{};
            while (remaining >= 39 && parsed < count)
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
                    return BlockMalformedPacket();
                if (hasExtra && !reader.Skip(16))
                    return BlockMalformedPacket();

                m_EventsInspected.fetch_add(1, std::memory_order_relaxed);
                if (eventDataSize > reader.Remaining() || eventDataSize > 4096)
                    return BlockMalformedPacket();

                const int id = static_cast<int>(eventId);
                bool block = IsConfiguredEventBlocked(id);

                if (id == 28 && !block && m_BlockMalformedScriptEvents.load(std::memory_order_acquire))
                {
                    // CScriptedGameEvent::Deserialize begins with a 32-bit byte count;
                    // YimMenuV2 stores at most 54 int64 arguments (432 bytes).
                    std::uint64_t argsSize{};
                    if (eventDataSize < 32 || !reader.Peek(32, argsSize)
                        || argsSize > 432
                        || (32 + argsSize * 8) > eventDataSize)
                        block = true;
                }

                if (block)
                {
                    m_EventsBlocked.fetch_add(1, std::memory_order_relaxed);
                    BlockPacket(id);
                    return true;
                }

                if (!reader.Skip(static_cast<std::size_t>(eventDataSize)))
                    return BlockMalformedPacket();

                const std::size_t consumed = reader.Position() - before;
                if (consumed > remaining)
                    return BlockMalformedPacket();
                remaining -= consumed;
                ++parsed;
            }
            return false;
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

        [[nodiscard]] bool BlockMalformedPacket() noexcept
        {
            if (!m_BlockMalformed.load(std::memory_order_acquire))
                return false;
            BlockPacket(-2);
            return true;
        }

        void BlockPacket(int eventId) noexcept
        {
            m_PacketsBlocked.fetch_add(1, std::memory_order_relaxed);
            m_LastBlockedEvent.store(eventId, std::memory_order_release);
        }

        bool SetStatus(bool result, std::string status)
        {
            std::scoped_lock lock(m_StatusMutex);
            m_Status = std::move(status);
            return result;
        }

        std::atomic<bool> m_Installed{false};
        std::atomic<bool> m_BlockMalformed{true};
        std::atomic<bool> m_BlockSounds{true};
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
        std::atomic<int> m_LastBlockedEvent{-1};
        void* m_Target{};
        ReceiveNetMessageFn m_Original{};
        mutable std::mutex m_StatusMutex;
        std::string m_Status{"Not installed"};
    };
}
