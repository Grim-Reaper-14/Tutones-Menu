#include "GamePointers.hpp"

#include "../core/logging/Logger.hpp"
#include "memory/PatternScanner.hpp"

#include <iomanip>
#include <sstream>
#include <string>

namespace Tutones::Game
{
    namespace
    {
        std::string AddressString(const void* address)
        {
            std::ostringstream stream;
            stream << "0x" << std::hex << std::uppercase << reinterpret_cast<std::uintptr_t>(address);
            return stream.str();
        }
    }

    GamePointers& GamePointers::Get() noexcept
    {
        static GamePointers instance;
        return instance;
    }

    bool GamePointers::Resolve()
    {
        if (m_Resolved.load(std::memory_order_acquire))
        {
            TUTONES_LOG_TRACE("game.ptr", "GTA Enhanced pointers already resolved");
            return true;
        }

        Reset();
        TUTONES_LOG_INFO("game.ptr", "Resolving GTA5_Enhanced.exe runtime pointers");

        if (!m_Module.Initialize(L"GTA5_Enhanced.exe"))
        {
            TUTONES_LOG_ERROR("game.ptr", "Could not open GTA5_Enhanced.exe module image");
            return false;
        }

        {
            std::string message("GTA Enhanced module loaded at ");
            message += AddressString(reinterpret_cast<void*>(m_Module.Base()));
            message += "; image size=";
            message += std::to_string(m_Module.Size());
            TUTONES_LOG_INFO("game.ptr", message);
        }

        constexpr auto initNativeTablesPattern = "EB 2A 0F 1F 40 00 48 8B 54 17 10";
        auto* initNativeMatch = Memory::PatternScanner::FindFirst(m_Module, initNativeTablesPattern);
        if (!initNativeMatch)
        {
            TUTONES_LOG_ERROR("game.ptr", "InitNativeTables pattern was not found");
            Reset();
            return false;
        }

        auto* initNativeAddress = initNativeMatch - 0x2A;
        if (!m_Module.Contains(initNativeAddress))
        {
            TUTONES_LOG_ERROR("game.ptr", "InitNativeTables resolved outside GTA module image");
            Reset();
            return false;
        }
        m_InitNativeTables = reinterpret_cast<InitNativeTablesFn>(initNativeAddress);
        TUTONES_LOG_INFO("game.ptr", std::string("Resolved InitNativeTables at ") + AddressString(initNativeAddress));

        constexpr auto runScriptThreadsPattern = "BE 40 5D C6 00";
        auto* runScriptThreadsMatch = Memory::PatternScanner::FindFirst(m_Module, runScriptThreadsPattern);
        if (!runScriptThreadsMatch)
        {
            TUTONES_LOG_ERROR("game.ptr", "RunScriptThreads pattern was not found");
            Reset();
            return false;
        }

        auto* runScriptThreadsAddress = runScriptThreadsMatch - 0x0A;
        if (!m_Module.Contains(runScriptThreadsAddress))
        {
            TUTONES_LOG_ERROR("game.ptr", "RunScriptThreads resolved outside GTA module image");
            Reset();
            return false;
        }
        m_RunScriptThreads = reinterpret_cast<RunScriptThreadsFn>(runScriptThreadsAddress);
        TUTONES_LOG_INFO("game.ptr", std::string("Resolved RunScriptThreads at ") + AddressString(runScriptThreadsAddress));

        constexpr auto scriptThreadsPattern = "48 8B 05 ? ? ? ? 48 89 34 F8 48 FF C7 48 39 FB 75 97";
        auto* scriptThreadsMatch = Memory::PatternScanner::FindFirst(m_Module, scriptThreadsPattern);
        if (!scriptThreadsMatch)
        {
            TUTONES_LOG_ERROR("game.ptr", "ScriptThreads pattern was not found");
            Reset();
            return false;
        }

        auto* scriptThreadsAddress = Memory::PatternScanner::ResolveRip(scriptThreadsMatch + 3);
        if (!scriptThreadsAddress || !m_Module.Contains(scriptThreadsAddress))
        {
            TUTONES_LOG_ERROR("game.ptr", "ScriptThreads pointer resolved outside GTA module image");
            Reset();
            return false;
        }
        m_ScriptThreads = reinterpret_cast<Types::AtArray<Types::ScriptThread*>*>(scriptThreadsAddress);
        TUTONES_LOG_INFO("game.ptr", std::string("Resolved ScriptThreads at ") + AddressString(scriptThreadsAddress));

        m_Resolved.store(true, std::memory_order_release);
        TUTONES_LOG_INFO("game.ptr", "GTA Enhanced pointer foundation resolved successfully");
        return true;
    }

    void GamePointers::Reset() noexcept
    {
        m_Resolved.store(false, std::memory_order_release);
        m_InitNativeTables = nullptr;
        m_RunScriptThreads = nullptr;
        m_ScriptThreads = nullptr;
        m_Module.Reset();
    }

    bool GamePointers::IsResolved() const noexcept { return m_Resolved.load(std::memory_order_acquire); }
    InitNativeTablesFn GamePointers::InitNativeTables() const noexcept { return m_InitNativeTables; }
    RunScriptThreadsFn GamePointers::RunScriptThreads() const noexcept { return m_RunScriptThreads; }
    Types::AtArray<Types::ScriptThread*>* GamePointers::ScriptThreads() const noexcept { return m_ScriptThreads; }
    const Memory::ModuleView& GamePointers::Module() const noexcept { return m_Module; }
}
