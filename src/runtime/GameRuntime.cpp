#include "GameRuntime.hpp"

#include "../core/logging/Logger.hpp"
#include "../game/GameState.hpp"
#include "../game/native/NativeInvoker.hpp"
#include "../game/native/NativeRegistry.hpp"
#include "../game/script/ScriptRuntime.hpp"

#include <MinHook.h>
#include <Windows.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <string>
#include <thread>

namespace Tutones::Runtime
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

        constexpr std::array<std::uint32_t, 3> PreferredScriptHashes{
            Joaat("freemode"),
            Joaat("main_persistent"),
            Joaat("startup"),
        };

        [[nodiscard]] bool IsPreferredScript(std::uint32_t programHash, std::uint32_t nameHash) noexcept
        {
            for (const auto hash : PreferredScriptHashes)
                if (programHash == hash || nameHash == hash)
                    return true;
            return false;
        }

        constexpr std::uint8_t EntityTypePed = 4;
        constexpr std::ptrdiff_t EntityTypeOffset = 0x28;
        constexpr std::ptrdiff_t AssistedAimTargetOffset = 0x38;

        std::string MinHookMessage(const char* prefix, MH_STATUS status)
        {
            std::string message(prefix);
            message += ": ";
            message += MH_StatusToString(status);
            return message;
        }
    }

    GameRuntime& GameRuntime::Get() noexcept
    {
        static GameRuntime instance;
        return instance;
    }

    bool GameRuntime::Initialize()
    {
        bool expected = false;
        if (!m_Initialized.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        {
            TUTONES_LOG_TRACE("runtime.game", "Game runtime initialize requested while already active");
            return true;
        }

        m_ShuttingDown.store(false, std::memory_order_release);
        m_ActiveCallbacks.store(0, std::memory_order_release);
        m_GameThreadId.store(0, std::memory_order_release);
        m_ReleaseDeadTargetSupported.store(false, std::memory_order_release);
        m_NativeInitAttempted = false;
        m_NativeExecutionArmed = false;
        m_LoggedNoScriptThread = false;
        m_LoggedNoTls = false;

        TUTONES_LOG_INFO("runtime.game", "Initializing GTA Enhanced game-thread runtime");

        auto& pointers = Game::GamePointers::Get();
        if (!pointers.Resolve())
        {
            TUTONES_LOG_ERROR("runtime.game", "GTA Enhanced pointer resolution failed");
            m_Initialized.store(false, std::memory_order_release);
            return false;
        }

        Game::Script::ScriptRuntime::Get().Configure(
            pointers.ScriptThreads(),
            pointers.ScriptPrograms(),
            pointers.ScriptGlobals(),
            pointers.ScriptVm());
        if (Game::Script::ScriptRuntime::Get().IsReady())
            TUTONES_LOG_INFO("runtime.game", "V11 shared script runtime pointers are ready");
        else
            TUTONES_LOG_WARN("runtime.game", "V11 shared script runtime is incomplete; script-backed features will report unavailable");

        const auto runScriptThreads = pointers.RunScriptThreads();
        if (!runScriptThreads)
        {
            TUTONES_LOG_ERROR("runtime.game", "RunScriptThreads target is unavailable");
            Game::Script::ScriptRuntime::Get().Reset();
            pointers.Reset();
            m_Initialized.store(false, std::memory_order_release);
            return false;
        }

        m_RunScriptThreadsTarget = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(runScriptThreads));
        const auto detour = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(&GameRuntime::RunScriptThreadsDetour));

        auto status = MH_CreateHook(
            m_RunScriptThreadsTarget,
            detour,
            reinterpret_cast<void**>(&m_OriginalRunScriptThreads));
        if (status != MH_OK)
        {
            TUTONES_LOG_ERROR("runtime.game", MinHookMessage("Failed to create RunScriptThreads hook", status));
            Game::Script::ScriptRuntime::Get().Reset();
            pointers.Reset();
            m_RunScriptThreadsTarget = nullptr;
            m_Initialized.store(false, std::memory_order_release);
            return false;
        }

        status = MH_EnableHook(m_RunScriptThreadsTarget);
        if (status != MH_OK)
        {
            TUTONES_LOG_ERROR("runtime.game", MinHookMessage("Failed to enable RunScriptThreads hook", status));
            MH_RemoveHook(m_RunScriptThreadsTarget);
            m_OriginalRunScriptThreads = nullptr;
            m_RunScriptThreadsTarget = nullptr;
            Game::Script::ScriptRuntime::Get().Reset();
            pointers.Reset();
            m_Initialized.store(false, std::memory_order_release);
            return false;
        }

        m_AssistedAimShouldReleaseEntityTarget = pointers.AssistedAimShouldReleaseEntity();
        if (m_AssistedAimShouldReleaseEntityTarget && pointers.AssistedAimFindNewTarget() && pointers.PtrToHandle())
        {
            const auto assistedDetour = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(&GameRuntime::AssistedAimShouldReleaseEntityDetour));
            status = MH_CreateHook(
                m_AssistedAimShouldReleaseEntityTarget,
                assistedDetour,
                reinterpret_cast<void**>(&m_OriginalAssistedAimShouldReleaseEntity));
            if (status == MH_OK)
            {
                status = MH_EnableHook(m_AssistedAimShouldReleaseEntityTarget);
                if (status == MH_OK)
                {
                    m_ReleaseDeadTargetSupported.store(true, std::memory_order_release);
                    TUTONES_LOG_INFO("runtime.game", "AssistedAimShouldReleaseEntity hook installed; Release Dead Target is supported");
                }
                else
                {
                    TUTONES_LOG_WARN("runtime.game", MinHookMessage("Failed to enable AssistedAimShouldReleaseEntity hook", status));
                    MH_RemoveHook(m_AssistedAimShouldReleaseEntityTarget);
                    m_OriginalAssistedAimShouldReleaseEntity = nullptr;
                    m_AssistedAimShouldReleaseEntityTarget = nullptr;
                }
            }
            else
            {
                TUTONES_LOG_WARN("runtime.game", MinHookMessage("Failed to create AssistedAimShouldReleaseEntity hook", status));
                m_OriginalAssistedAimShouldReleaseEntity = nullptr;
                m_AssistedAimShouldReleaseEntityTarget = nullptr;
            }
        }
        else
        {
            m_AssistedAimShouldReleaseEntityTarget = nullptr;
            TUTONES_LOG_WARN("runtime.game", "Release Dead Target dependencies are incomplete; feature will remain unavailable");
        }

        TUTONES_LOG_INFO("runtime.game", "RunScriptThreads hook installed; native work will wait for a live ScriptVM context");
        return true;
    }

    void GameRuntime::Shutdown() noexcept
    {
        if (!m_Initialized.exchange(false, std::memory_order_acq_rel))
        {
            TUTONES_LOG_TRACE("runtime.game", "Game runtime shutdown requested while inactive");
            return;
        }

        m_ShuttingDown.store(true, std::memory_order_release);
        m_ReleaseDeadTargetSupported.store(false, std::memory_order_release);
        TUTONES_LOG_INFO("runtime.game", "Shutting down GTA game-thread runtime");

        if (m_AssistedAimShouldReleaseEntityTarget)
        {
            const auto status = MH_DisableHook(m_AssistedAimShouldReleaseEntityTarget);
            if (status != MH_OK && status != MH_ERROR_DISABLED)
                TUTONES_LOG_WARN("runtime.game", MinHookMessage("AssistedAimShouldReleaseEntity hook disable returned", status));
        }

        if (m_RunScriptThreadsTarget)
        {
            const auto status = MH_DisableHook(m_RunScriptThreadsTarget);
            if (status != MH_OK && status != MH_ERROR_DISABLED)
                TUTONES_LOG_WARN("runtime.game", MinHookMessage("RunScriptThreads hook disable returned", status));
        }

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (m_ActiveCallbacks.load(std::memory_order_acquire) != 0 && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

        if (m_ActiveCallbacks.load(std::memory_order_acquire) != 0)
            TUTONES_LOG_WARN("runtime.game", "Timed out waiting for GTA hook callback drain");
        else
            TUTONES_LOG_DEBUG("runtime.game", "GTA hook callbacks drained");

        if (m_AssistedAimShouldReleaseEntityTarget)
        {
            const auto status = MH_RemoveHook(m_AssistedAimShouldReleaseEntityTarget);
            if (status != MH_OK && status != MH_ERROR_NOT_CREATED)
                TUTONES_LOG_WARN("runtime.game", MinHookMessage("AssistedAimShouldReleaseEntity hook removal returned", status));
        }

        if (m_RunScriptThreadsTarget)
        {
            const auto status = MH_RemoveHook(m_RunScriptThreadsTarget);
            if (status != MH_OK && status != MH_ERROR_NOT_CREATED)
                TUTONES_LOG_WARN("runtime.game", MinHookMessage("RunScriptThreads hook removal returned", status));
        }

        {
            std::scoped_lock lock(m_TaskMutex);
            m_Tasks.clear();
        }

        Game::Script::ScriptRuntime::Get().Reset();
        Game::GameState::Get().Reset();
        Game::Native::NativeRegistry::Get().Shutdown();
        Game::GamePointers::Get().Reset();

        m_OriginalAssistedAimShouldReleaseEntity = nullptr;
        m_AssistedAimShouldReleaseEntityTarget = nullptr;
        m_OriginalRunScriptThreads = nullptr;
        m_RunScriptThreadsTarget = nullptr;
        m_GameThreadId.store(0, std::memory_order_release);
        m_NativeInitAttempted = false;
        m_NativeExecutionArmed = false;
        m_LoggedNoScriptThread = false;
        m_LoggedNoTls = false;
        m_ShuttingDown.store(false, std::memory_order_release);

        TUTONES_LOG_INFO("runtime.game", "GTA game-thread runtime stopped");
    }

    bool GameRuntime::IsInitialized() const noexcept
    {
        return m_Initialized.load(std::memory_order_acquire);
    }

    bool GameRuntime::IsOnGameThread() const noexcept
    {
        const auto threadId = m_GameThreadId.load(std::memory_order_acquire);
        return threadId != 0 && threadId == ::GetCurrentThreadId();
    }

    std::uint32_t GameRuntime::GameThreadId() const noexcept
    {
        return m_GameThreadId.load(std::memory_order_acquire);
    }

    bool GameRuntime::Enqueue(std::function<void()> task)
    {
        if (!task || !IsInitialized() || m_ShuttingDown.load(std::memory_order_acquire))
            return false;

        std::scoped_lock lock(m_TaskMutex);
        if (m_Tasks.size() >= 256)
        {
            TUTONES_LOG_WARN("runtime.game", "Game-thread task queue is full; rejected task");
            return false;
        }

        m_Tasks.emplace_back(std::move(task));
        TUTONES_LOG_TRACE("runtime.game", "Queued task for GTA script thread");
        return true;
    }

    void GameRuntime::SetReleaseDeadTargetEnabled(bool enabled) noexcept
    {
        m_ReleaseDeadTargetEnabled.store(enabled, std::memory_order_release);
    }

    bool GameRuntime::ReleaseDeadTargetEnabled() const noexcept
    {
        return m_ReleaseDeadTargetEnabled.load(std::memory_order_acquire);
    }

    bool GameRuntime::ReleaseDeadTargetSupported() const noexcept
    {
        return m_ReleaseDeadTargetSupported.load(std::memory_order_acquire);
    }

    bool GameRuntime::RunScriptThreadsDetour(int operationsToExecute) noexcept
    {
        auto& runtime = Get();
        runtime.m_ActiveCallbacks.fetch_add(1, std::memory_order_acq_rel);

        bool result{};
        if (runtime.m_OriginalRunScriptThreads)
            result = runtime.m_OriginalRunScriptThreads(operationsToExecute);

        // Do not synthesize a script TLS context here. At this point Rockstar's
        // scheduler has already returned. Native work is dispatched instead by
        // TickLiveScriptContext from the ScriptVM detour while a real script is active.
        runtime.m_ActiveCallbacks.fetch_sub(1, std::memory_order_acq_rel);
        return result;
    }

    bool GameRuntime::AssistedAimShouldReleaseEntityDetour(std::int64_t context) noexcept
    {
        auto& runtime = Get();
        runtime.m_ActiveCallbacks.fetch_add(1, std::memory_order_acq_rel);

        const auto finish = [&runtime](bool value) noexcept {
            runtime.m_ActiveCallbacks.fetch_sub(1, std::memory_order_acq_rel);
            return value;
        };

        const auto original = runtime.m_OriginalAssistedAimShouldReleaseEntity;
        if (!original)
            return finish(false);
        if (context == 0 || runtime.m_ShuttingDown.load(std::memory_order_acquire))
            return finish(original(context));

        auto** targetSlot = reinterpret_cast<void**>(context + AssistedAimTargetOffset);
        void* entity = targetSlot ? *targetSlot : nullptr;
        if (!entity)
            return finish(original(context));

        const auto entityType = *(reinterpret_cast<const std::uint8_t*>(entity) + EntityTypeOffset);
        if (entityType != EntityTypePed)
            return finish(original(context));

        const auto ptrToHandle = Game::GamePointers::Get().PtrToHandle();
        if (!ptrToHandle)
            return finish(original(context));

        const int handle = ptrToHandle(entity);
        if (handle == 0)
            return finish(original(context));

        const auto dead = Game::Native::NativeInvoker::Invoke<std::int32_t>(
            Game::Native::NativeId::IsEntityDead,
            handle,
            std::int32_t{1});
        if (!dead || *dead == 0)
            return finish(original(context));

        *targetSlot = nullptr;
        const auto findNewTarget = Game::GamePointers::Get().AssistedAimFindNewTarget();
        if (!findNewTarget)
        {
            *targetSlot = entity;
            return finish(original(context));
        }

        if (!findNewTarget(context))
        {
            *targetSlot = entity;
            if (runtime.m_ReleaseDeadTargetEnabled.load(std::memory_order_acquire))
                return finish(true);
        }

        return finish(original(context));
    }

    void GameRuntime::TickLiveScriptContext(std::uint32_t programHash, std::uint32_t nameHash) noexcept
    {
        if (!IsInitialized() || m_ShuttingDown.load(std::memory_order_acquire))
            return;
        if (!IsPreferredScript(programHash, nameHash))
            return;

        auto* tls = Game::Types::TlsContext::Get();
        if (!tls || !tls->scriptThreadActive || !tls->currentScriptThread)
        {
            if (!m_LoggedNoTls)
            {
                m_LoggedNoTls = true;
                TUTONES_LOG_WARN("runtime.game", "Live ScriptVM callback did not expose an active GTA script TLS context");
            }
            return;
        }
        m_LoggedNoTls = false;

        const auto activeHash = tls->currentScriptThread->scriptHash;
        if (!IsPreferredScript(activeHash, activeHash))
        {
            if (!m_LoggedNoScriptThread)
            {
                m_LoggedNoScriptThread = true;
                TUTONES_LOG_DEBUG("runtime.game", "Ignoring ScriptVM callback whose active TLS script is not a preferred host");
            }
            return;
        }
        m_LoggedNoScriptThread = false;

        const auto currentThread = ::GetCurrentThreadId();
        const auto previousThread = m_GameThreadId.exchange(currentThread, std::memory_order_acq_rel);
        if (previousThread == 0)
            TUTONES_LOG_INFO("runtime.game", "First live GTA ScriptVM context observed");
        else if (previousThread != currentThread)
            TUTONES_LOG_WARN("runtime.game", "Live ScriptVM execution moved to a different OS thread");

        auto& registry = Game::Native::NativeRegistry::Get();
        registry.MarkGameThread(currentThread);

        if (!registry.IsReady())
        {
            if (m_NativeInitAttempted)
                return;

            m_NativeInitAttempted = true;
            if (!registry.Initialize(Game::GamePointers::Get().InitNativeTables()))
            {
                TUTONES_LOG_ERROR("runtime.game", "Focused GTA native table failed to initialize inside live ScriptVM context");
                Core::Logging::Logger::Get().Flush();
                return;
            }

            m_NativeExecutionArmed = true;
            TUTONES_LOG_INFO("runtime.game", "Native table ready inside authentic GTA ScriptVM context; first invocation deferred");
            Core::Logging::Logger::Get().Flush();
            return;
        }

        if (m_NativeExecutionArmed)
        {
            m_NativeExecutionArmed = false;
            TUTONES_LOG_INFO("runtime.game", "Native execution armed on a subsequent live GTA ScriptVM tick");
            Core::Logging::Logger::Get().Flush();
        }

        Game::GameState::Get().Tick();
        DrainTasks();
    }

    void GameRuntime::DrainTasks() noexcept
    {
        std::function<void()> task;
        std::size_t remaining{};
        {
            std::scoped_lock lock(m_TaskMutex);
            if (m_Tasks.empty())
                return;
            task = std::move(m_Tasks.front());
            m_Tasks.pop_front();
            remaining = m_Tasks.size();
        }

        TUTONES_LOG_DEBUG("runtime.game", std::string("Executing one GTA script-thread task; remaining=") + std::to_string(remaining));
        Core::Logging::Logger::Get().Flush();

        try
        {
            task();
        }
        catch (const std::exception& exception)
        {
            TUTONES_LOG_ERROR("runtime.game", std::string("Game-thread task threw exception: ") + exception.what());
        }
        catch (...)
        {
            TUTONES_LOG_ERROR("runtime.game", "Game-thread task threw an unknown exception");
        }
    }
}
