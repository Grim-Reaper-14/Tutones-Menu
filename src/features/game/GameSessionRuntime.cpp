#include "GameSessionRuntime.hpp"

#include "../../core/logging/Logger.hpp"
#include "../../game/PlayerNatives.hpp"
#include "../../game/VehicleNatives.hpp"
#include "../../game/native/NativeInvoker.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../game/script/ScriptFunction.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../game/script/ScriptPointer.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>

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
        constexpr std::uint32_t TunablesRegistrationHash = Joaat("tunables_registration");
        constexpr std::size_t JoinTypeGlobal = 1575048;
        constexpr std::size_t TunableBaseAddress = 0x40001;
        constexpr std::array<int, 4> IdleKickDefaults{{120000, 300000, 600000, 900000}};
        constexpr std::array<int, 4> ConstrainedKickDefaults{{30000, 60000, 90000, 120000}};

        template<std::size_t N>
        [[nodiscard]] std::optional<std::size_t> FindUniqueTunablePattern(
            std::int64_t** globals,
            std::size_t tunableCount,
            const std::array<int, N>& pattern) noexcept
        {
            if (!globals || tunableCount < N)
                return std::nullopt;

            std::optional<std::size_t> match;
            for (std::size_t offset = 0; offset + N <= tunableCount; ++offset)
            {
                bool equal = true;
                for (std::size_t index = 0; index < N; ++index)
                {
                    const int* value = Script::ScriptGlobal(TunableBaseAddress + offset + index).As<int>(globals);
                    if (!value || *value != pattern[index])
                    {
                        equal = false;
                        break;
                    }
                }

                if (!equal)
                    continue;

                if (match)
                    return std::nullopt;
                match = TunableBaseAddress + offset;
            }
            return match;
        }
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

    bool GameSessionRuntime::QueueSkipCutscene()
    {
        if (!Native::NativeRegistry::Get().IsReady())
            return false;

        return Runtime::GameRuntime::Get().Enqueue([] {
            const bool success = Native::NativeInvoker::InvokeVoid(Native::NativeId::StopCutsceneImmediately);
            if (success)
                TUTONES_LOG_INFO("game.session", "Stopped the active cutscene immediately");
            else
                TUTONES_LOG_WARN("game.session", "STOP_CUTSCENE_IMMEDIATELY could not be invoked");
        });
    }

    bool GameSessionRuntime::QueueSkipConversation()
    {
        if (!Native::NativeRegistry::Get().IsReady())
            return false;

        return Runtime::GameRuntime::Get().Enqueue([] {
            const bool success = Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SkipToNextScriptedConversationLine);
            if (success)
                TUTONES_LOG_INFO("game.session", "Skipped to the next scripted conversation line");
            else
                TUTONES_LOG_WARN("game.session", "SKIP_TO_NEXT_SCRIPTED_CONVERSATION_LINE could not be invoked");
        });
    }

    bool GameSessionRuntime::QueueAirstrikeAhead(int damage)
    {
        if (!Native::NativeRegistry::Get().IsReady())
            return false;

        bool expected = false;
        if (!m_ServicePending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            return false;
        {
            std::scoped_lock lock(m_Mutex);
            m_State.servicePending = true;
        }

        const int requestedDamage = std::clamp(damage, 1, 1000);
        if (Runtime::GameRuntime::Get().Enqueue([this, requestedDamage] {
                RecordServiceResult(
                    GameServiceAction::AirstrikeAhead,
                    ExecuteAirstrikeOnGameThread(requestedDamage));
            }))
            return true;

        RecordServiceResult(GameServiceAction::AirstrikeAhead, false);
        return false;
    }

    bool GameSessionRuntime::QueueAmmoDrop()
    {
        return QueuePickupDrop(GameServiceAction::AmmoDrop);
    }

    bool GameSessionRuntime::QueueMinigunDrop()
    {
        return QueuePickupDrop(GameServiceAction::MinigunDrop);
    }

    bool GameSessionRuntime::QueuePickupDrop(GameServiceAction action)
    {
        if (action != GameServiceAction::AmmoDrop && action != GameServiceAction::MinigunDrop)
            return false;
        if (!Native::NativeRegistry::Get().IsReady())
            return false;

        bool expected = false;
        if (!m_ServicePending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            return false;
        {
            std::scoped_lock lock(m_Mutex);
            m_State.servicePending = true;
        }

        if (Runtime::GameRuntime::Get().Enqueue([this, action] {
                if (!BeginPickupDropOnGameThread(action))
                    RecordServiceResult(action, false);
            }))
            return true;

        RecordServiceResult(action, false);
        return false;
    }

    void GameSessionRuntime::SetNoIdle(bool enabled)
    {
        const bool previous = m_NoIdle.exchange(enabled, std::memory_order_acq_rel);
        if (previous == enabled)
            return;

        {
            std::scoped_lock lock(m_Mutex);
            m_State.noIdleEnabled = enabled;
            if (!enabled)
                m_State.noIdleReady = false;
        }

        if (enabled)
        {
            m_NextNoIdleResolve = {};
            static_cast<void>(EnsureUtilityTick());
            return;
        }

        static_cast<void>(Runtime::GameRuntime::Get().Enqueue([this] {
            static_cast<void>(RestoreNoIdleOnGameThread());
        }));
    }

    void GameSessionRuntime::Shutdown() noexcept
    {
        m_NoIdle.store(false, std::memory_order_release);
        m_Pending.store(false, std::memory_order_release);
        m_ServicePending.store(false, std::memory_order_release);
        m_UtilityTickQueued.store(false, std::memory_order_release);

        const auto cleanup = [this] {
            static_cast<void>(RestoreNoIdleOnGameThread());

            if (m_PendingPickupModel != 0)
            {
                static_cast<void>(PlayerNatives::SetModelAsNoLongerNeeded(m_PendingPickupModel));
                m_PendingPickupModel = 0;
            }
            m_PendingServiceAction = GameServiceAction::None;
            m_ServiceDeadline = {};
        };

        auto& runtime = Runtime::GameRuntime::Get();
        if (runtime.IsOnGameThread())
        {
            cleanup();
        }
        else if (runtime.IsInitialized())
        {
            const auto cleaned = std::make_shared<std::atomic<bool>>(false);
            if (runtime.Enqueue([cleanup, cleaned] {
                    cleanup();
                    cleaned->store(true, std::memory_order_release);
                }))
            {
                const auto deadline = Clock::now() + std::chrono::milliseconds(250);
                while (!cleaned->load(std::memory_order_acquire) && Clock::now() < deadline)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        std::scoped_lock lock(m_Mutex);
        m_State.actionPending = false;
        m_State.noIdleEnabled = false;
        m_State.noIdleReady = false;
        m_State.servicePending = false;
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
        snapshot.noIdleEnabled = m_NoIdle.load(std::memory_order_acquire);
        snapshot.servicePending = m_ServicePending.load(std::memory_order_acquire);
        return snapshot;
    }

    bool GameSessionRuntime::EnsureUtilityTick()
    {
        if (!m_NoIdle.load(std::memory_order_acquire)
            && !m_ServicePending.load(std::memory_order_acquire))
            return false;

        bool expected = false;
        if (!m_UtilityTickQueued.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            return true;

        if (Runtime::GameRuntime::Get().Enqueue([this] { UtilityTickOnGameThread(); }))
            return true;

        m_UtilityTickQueued.store(false, std::memory_order_release);
        return false;
    }

    void GameSessionRuntime::UtilityTickOnGameThread() noexcept
    {
        m_UtilityTickQueued.store(false, std::memory_order_release);

        if (m_NoIdle.load(std::memory_order_acquire))
        {
            const bool applied = ApplyNoIdleOnGameThread();
            std::scoped_lock lock(m_Mutex);
            m_State.noIdleEnabled = true;
            m_State.noIdleReady = applied;
        }

        if (m_ServicePending.load(std::memory_order_acquire))
            ProcessPendingServiceOnGameThread();

        if ((m_NoIdle.load(std::memory_order_acquire)
                || m_ServicePending.load(std::memory_order_acquire))
            && !EnsureUtilityTick())
            TUTONES_LOG_WARN("game.session", "Game utility tick could not be re-queued");
    }

    bool GameSessionRuntime::ResolveNoIdleTunablesOnGameThread() noexcept
    {
        if (m_NoIdleResolved.load(std::memory_order_acquire))
            return true;

        const auto now = Clock::now();
        if (m_NextNoIdleResolve != Clock::time_point{} && now < m_NextNoIdleResolve)
            return false;
        m_NextNoIdleResolve = now + std::chrono::seconds(1);

        auto& scriptRuntime = Script::ScriptRuntime::Get();
        auto** globals = scriptRuntime.Globals();
        auto* program = scriptRuntime.FindProgram(TunablesRegistrationHash);
        if (!scriptRuntime.IsReady() || !globals || !program || program->globalCount <= TunableBaseAddress)
            return false;

        const std::size_t tunableCount = static_cast<std::size_t>(program->globalCount) - TunableBaseAddress;
        const auto idle = FindUniqueTunablePattern(globals, tunableCount, IdleKickDefaults);
        const auto constrained = FindUniqueTunablePattern(globals, tunableCount, ConstrainedKickDefaults);
        if (!idle || !constrained || *idle == *constrained)
            return false;

        for (std::size_t index = 0; index < 4; ++index)
        {
            m_NoIdleGlobals[index] = *idle + index;
            m_NoIdleGlobals[index + 4] = *constrained + index;
        }

        for (std::size_t index = 0; index < m_NoIdleGlobals.size(); ++index)
        {
            const int* value = Script::ScriptGlobal(m_NoIdleGlobals[index]).As<int>(globals);
            if (!value)
            {
                m_NoIdleGlobals = {};
                return false;
            }
            m_NoIdleOriginals[index] = *value;
        }

        m_NoIdleResolved.store(true, std::memory_order_release);
        TUTONES_LOG_INFO("game.session", "Resolved current-build idle-kick tunables from verified default timer sequences");
        return true;
    }

    bool GameSessionRuntime::ApplyNoIdleOnGameThread() noexcept
    {
        if (!ResolveNoIdleTunablesOnGameThread())
            return false;

        auto** globals = Script::ScriptRuntime::Get().Globals();
        if (!globals)
            return false;

        for (const std::size_t global : m_NoIdleGlobals)
        {
            int* value = Script::ScriptGlobal(global).As<int>(globals);
            if (!value)
                return false;
            *value = INT_MAX;
        }
        return true;
    }

    bool GameSessionRuntime::RestoreNoIdleOnGameThread() noexcept
    {
        if (!m_NoIdleResolved.exchange(false, std::memory_order_acq_rel))
            return true;

        auto** globals = Script::ScriptRuntime::Get().Globals();
        bool success = globals != nullptr;
        if (globals)
        {
            for (std::size_t index = 0; index < m_NoIdleGlobals.size(); ++index)
            {
                int* value = Script::ScriptGlobal(m_NoIdleGlobals[index]).As<int>(globals);
                if (!value)
                {
                    success = false;
                    continue;
                }
                *value = m_NoIdleOriginals[index];
            }
        }

        m_NoIdleGlobals = {};
        m_NoIdleOriginals = {};
        m_NextNoIdleResolve = {};
        if (success)
            TUTONES_LOG_INFO("game.session", "Restored original idle-kick tunable timers");
        else
            TUTONES_LOG_WARN("game.session", "Could not fully restore idle-kick tunables");
        return success;
    }

    bool GameSessionRuntime::BeginPickupDropOnGameThread(GameServiceAction action) noexcept
    {
        const char* modelName = action == GameServiceAction::MinigunDrop
            ? "ex_prop_adv_case_sm"
            : "prop_box_ammo02a";
        m_PendingPickupModel = Joaat(modelName);
        m_PendingServiceAction = action;
        m_ServiceDeadline = Clock::now() + std::chrono::seconds(5);

        if (!PlayerNatives::RequestModel(m_PendingPickupModel))
        {
            m_PendingPickupModel = 0;
            m_PendingServiceAction = GameServiceAction::None;
            return false;
        }

        if (!EnsureUtilityTick())
        {
            static_cast<void>(PlayerNatives::SetModelAsNoLongerNeeded(m_PendingPickupModel));
            m_PendingPickupModel = 0;
            m_PendingServiceAction = GameServiceAction::None;
            return false;
        }
        return true;
    }

    void GameSessionRuntime::ProcessPendingServiceOnGameThread() noexcept
    {
        const GameServiceAction action = m_PendingServiceAction;
        if (action != GameServiceAction::AmmoDrop && action != GameServiceAction::MinigunDrop)
            return;
        if (m_PendingPickupModel == 0)
        {
            RecordServiceResult(action, false);
            return;
        }

        const auto loaded = PlayerNatives::HasModelLoaded(m_PendingPickupModel);
        if (loaded && *loaded)
        {
            bool success = false;
            const auto ped = PlayerNatives::PlayerPedId();
            if (ped && *ped != 0)
            {
                const auto coords = VehicleNatives::GetEntityCoords(*ped, true);
                const auto heading = VehicleNatives::GetEntityHeading(*ped);
                if (coords && heading)
                {
                    constexpr float DegreesToRadians = 0.01745329251994329577f;
                    const float radians = *heading * DegreesToRadians;
                    const float x = coords->x - std::sin(radians) * 3.0f;
                    const float y = coords->y + std::cos(radians) * 3.0f;
                    const float z = coords->z + 0.35f;
                    const std::uint32_t pickup = Joaat(action == GameServiceAction::MinigunDrop
                        ? "PICKUP_WEAPON_MINIGUN"
                        : "PICKUP_AMMO_BULLET_MP");
                    const auto created = Native::NativeInvoker::Invoke<Entity>(
                        Native::NativeId::CreateAmbientPickup,
                        pickup,
                        x,
                        y,
                        z,
                        std::int32_t{1 << 12},
                        200,
                        m_PendingPickupModel,
                        std::int32_t{1},
                        std::int32_t{1});
                    success = created && *created != 0;
                }
            }

            static_cast<void>(PlayerNatives::SetModelAsNoLongerNeeded(m_PendingPickupModel));
            m_PendingPickupModel = 0;
            m_PendingServiceAction = GameServiceAction::None;
            m_ServiceDeadline = {};
            RecordServiceResult(action, success);
            return;
        }

        if (Clock::now() >= m_ServiceDeadline)
        {
            static_cast<void>(PlayerNatives::SetModelAsNoLongerNeeded(m_PendingPickupModel));
            m_PendingPickupModel = 0;
            m_PendingServiceAction = GameServiceAction::None;
            m_ServiceDeadline = {};
            RecordServiceResult(action, false);
            return;
        }

        static_cast<void>(PlayerNatives::RequestModel(m_PendingPickupModel));
    }

    bool GameSessionRuntime::ExecuteAirstrikeOnGameThread(int damage) noexcept
    {
        const auto ped = PlayerNatives::PlayerPedId();
        if (!ped || *ped == 0)
            return false;

        const auto coords = VehicleNatives::GetEntityCoords(*ped, true);
        const auto heading = VehicleNatives::GetEntityHeading(*ped);
        if (!coords || !heading)
            return false;

        constexpr float DegreesToRadians = 0.01745329251994329577f;
        const float radians = *heading * DegreesToRadians;
        Vector3 target = *coords;
        target.x -= std::sin(radians) * 45.0f;
        target.y += std::cos(radians) * 45.0f;
        target.z -= 1.0f;

        constexpr std::array<Vector3, 5> Offsets{{
            Vector3{0.0f, 0.0f, 0.0f, 0.0f},
            Vector3{5.0f, 5.0f, 0.0f, 0.0f},
            Vector3{-5.0f, 5.0f, 0.0f, 0.0f},
            Vector3{5.0f, -5.0f, 0.0f, 0.0f},
            Vector3{-5.0f, -5.0f, 0.0f, 0.0f},
        }};

        const std::uint32_t weapon = Joaat("WEAPON_AIRSTRIKE_ROCKET");
        bool success = true;
        for (const auto& offset : Offsets)
        {
            const float endX = target.x + offset.x;
            const float endY = target.y + offset.y;
            const float endZ = target.z;
            success = Native::NativeInvoker::InvokeVoid(
                Native::NativeId::ShootSingleBulletBetweenCoordsIgnoreEntityNew,
                endX,
                endY,
                endZ + 45.0f,
                endX,
                endY,
                endZ,
                damage,
                std::int32_t{1},
                weapon,
                *ped,
                std::int32_t{1},
                std::int32_t{0},
                -1.0f,
                *ped,
                std::int32_t{1},
                std::int32_t{1},
                Entity{0},
                std::int32_t{0},
                std::int32_t{0},
                std::int32_t{0},
                std::int32_t{0}) && success;
        }
        return success;
    }

    void GameSessionRuntime::RecordServiceResult(GameServiceAction action, bool success) noexcept
    {
        m_ServicePending.store(false, std::memory_order_release);
        std::scoped_lock lock(m_Mutex);
        m_State.servicePending = false;
        m_State.lastServiceAction = action;
        m_State.lastServiceSucceeded = success;
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
