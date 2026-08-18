#include "GameSessionRuntime.hpp"

#include "../../core/logging/Logger.hpp"
#include "../../game/script/ScriptFunction.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../game/script/ScriptPointer.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <cstdint>

namespace Tutones::Game::SessionFeatures
{
    namespace
    {
        constexpr std::uint32_t Joaat(const char* text) noexcept
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

        constexpr std::uint32_t ShopControllerHash = Joaat("shop_controller");
        constexpr std::size_t JoinTypeGlobal = 1575048;
    }

    GameSessionRuntime& GameSessionRuntime::Get() noexcept
    {
        static GameSessionRuntime instance;
        return instance;
    }

    bool GameSessionRuntime::QueueJoin(JoinType type)
    {
        bool expected = false;
        if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            return false;

        {
            std::scoped_lock lock(m_Mutex);
            m_State.actionPending = true;
            m_State.lastRequested = type;
        }

        if (Runtime::GameRuntime::Get().Enqueue([this, type] { ExecuteOnGameThread(type); }))
        {
            TUTONES_LOG_INFO("game.session", "Queued GTA Online session transition");
            return true;
        }

        m_Pending.store(false, std::memory_order_release);
        RecordResult(type, false, false);
        TUTONES_LOG_ERROR("game.session", "Failed to queue GTA Online session transition on the game thread");
        return false;
    }

    bool GameSessionRuntime::QueueLeaveOnline()
    {
        return QueueJoin(JoinType::LeaveOnline);
    }

    GameSessionSnapshot GameSessionRuntime::Snapshot() const noexcept
    {
        GameSessionSnapshot snapshot;
        {
            std::scoped_lock lock(m_Mutex);
            snapshot = m_State;
        }
        snapshot.scriptRuntimeReady = Script::ScriptRuntime::Get().IsReady();
        snapshot.actionPending = m_Pending.load(std::memory_order_acquire);
        return snapshot;
    }

    void GameSessionRuntime::ExecuteOnGameThread(JoinType type) noexcept
    {
        auto& scriptRuntime = Script::ScriptRuntime::Get();
        auto** globals = scriptRuntime.Globals();
        auto* thread = scriptRuntime.FindThread(ShopControllerHash);
        auto* program = scriptRuntime.FindProgram(ShopControllerHash);
        auto* joinType = globals ? Script::ScriptGlobal(JoinTypeGlobal).As<std::int32_t>(globals) : nullptr;

        if (!scriptRuntime.IsReady() || !thread || !program || !joinType)
        {
            RecordResult(type, false, false);
            TUTONES_LOG_WARN("game.session", "shop_controller transition prerequisites are unavailable");
            return;
        }

        static Script::ScriptFunction sendToClouds(
            ShopControllerHash,
            Script::ScriptPointer("SendToClouds", "2D 00 02 00 00 72 5D ? ? ? 72"));

        if (!sendToClouds.CallVoid())
        {
            RecordResult(type, false, false);
            TUTONES_LOG_WARN("game.session", "SendToClouds script function could not be resolved or executed");
            return;
        }

        *joinType = static_cast<std::int32_t>(type);
        RecordResult(type, true, true);
        TUTONES_LOG_INFO("game.session", type == JoinType::LeaveOnline
            ? "Leave Online transition launched"
            : "GTA Online session join transition launched");
    }

    void GameSessionRuntime::RecordResult(JoinType type, bool shopControllerReady, bool success) noexcept
    {
        m_Pending.store(false, std::memory_order_release);
        std::scoped_lock lock(m_Mutex);
        m_State.scriptRuntimeReady = Script::ScriptRuntime::Get().IsReady();
        m_State.capabilityProbed = true;
        m_State.shopControllerReady = shopControllerReady;
        m_State.actionPending = false;
        m_State.hasLastAction = true;
        m_State.lastRequested = type;
        m_State.lastActionSucceeded = success;
    }
}
