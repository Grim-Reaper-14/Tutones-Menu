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

        constexpr auto handlesAndPtrsPattern = "0F 1F 84 00 00 00 00 00 89 F8 0F 28 FE 41";
        if (auto* match = Memory::PatternScanner::FindFirst(m_Module, handlesAndPtrsPattern))
        {
            auto* address = Memory::PatternScanner::ResolveRip(match - 0x0A);
            if (address && m_Module.Contains(address))
            {
                m_PtrToHandle = reinterpret_cast<PtrToHandleFn>(address);
                TUTONES_LOG_INFO("game.ptr", std::string("Resolved PtrToHandle at ") + AddressString(address));
            }
            else
                TUTONES_LOG_WARN("game.ptr", "PtrToHandle resolved outside GTA module image");
        }
        else
            TUTONES_LOG_WARN("game.ptr", "PtrToHandle pattern was not found; pointer-backed entity features will be unavailable");

        constexpr auto scriptGlobalsPattern = "48 8B 8E B8 00 00 00 48 8D 15 ? ? ? ? 49 89 D8";
        if (auto* match = Memory::PatternScanner::FindFirst(m_Module, scriptGlobalsPattern))
        {
            auto* address = Memory::PatternScanner::ResolveRip(match + 0x0A);
            if (address && m_Module.Contains(address))
            {
                m_ScriptGlobals = reinterpret_cast<std::int64_t**>(address);
                TUTONES_LOG_INFO("game.ptr", std::string("Resolved ScriptGlobals at ") + AddressString(address));
            }
            else
                TUTONES_LOG_WARN("game.ptr", "ScriptGlobals resolved outside GTA module image");
        }
        else
            TUTONES_LOG_WARN("game.ptr", "ScriptGlobals pattern was not found; V11 script-global features will be unavailable");

        constexpr auto scriptProgramsPattern = "48 C7 84 C8 D8 00 00 00 00 00 00 00";
        if (auto* match = Memory::PatternScanner::FindFirst(m_Module, scriptProgramsPattern))
        {
            auto* base = Memory::PatternScanner::ResolveRip(match + 0x16);
            auto* address = base ? base + 0xD8 : nullptr;
            if (address && m_Module.Contains(address))
            {
                m_ScriptPrograms = reinterpret_cast<Types::ScriptProgram**>(address);
                TUTONES_LOG_INFO("game.ptr", std::string("Resolved ScriptPrograms at ") + AddressString(address));
            }
            else
                TUTONES_LOG_WARN("game.ptr", "ScriptPrograms resolved outside GTA module image");
        }
        else
            TUTONES_LOG_WARN("game.ptr", "ScriptPrograms pattern was not found; V11 script-function features will be unavailable");

        constexpr auto scriptVmPattern = "49 63 41 1C";
        if (auto* match = Memory::PatternScanner::FindFirst(m_Module, scriptVmPattern))
        {
            auto* address = match - 0x24;
            if (m_Module.Contains(address))
            {
                m_ScriptVm = reinterpret_cast<ScriptVmFn>(address);
                TUTONES_LOG_INFO("game.ptr", std::string("Resolved ScriptVM at ") + AddressString(address));
            }
            else
                TUTONES_LOG_WARN("game.ptr", "ScriptVM resolved outside GTA module image");
        }
        else
            TUTONES_LOG_WARN("game.ptr", "ScriptVM pattern was not found; V11 script-function features will be unavailable");

        constexpr auto assistedAimShouldReleaseEntityPattern = "80 7F 28 04 75 6A";
        if (auto* match = Memory::PatternScanner::FindFirst(m_Module, assistedAimShouldReleaseEntityPattern))
        {
            auto* address = match - 0x0F;
            if (m_Module.Contains(address))
            {
                m_AssistedAimShouldReleaseEntity = address;
                TUTONES_LOG_INFO("game.ptr", std::string("Resolved AssistedAimShouldReleaseEntity at ") + AddressString(address));
            }
            else
                TUTONES_LOG_WARN("game.ptr", "AssistedAimShouldReleaseEntity resolved outside GTA module image");
        }
        else
            TUTONES_LOG_WARN("game.ptr", "AssistedAimShouldReleaseEntity pattern was not found; Release Dead Target will be unavailable");

        constexpr auto assistedAimFindNewTargetPattern = "0F 84 C9 00 00 00 48 89 CE 48 89 F9";
        if (auto* match = Memory::PatternScanner::FindFirst(m_Module, assistedAimFindNewTargetPattern))
        {
            auto* address = match - 0x33;
            if (m_Module.Contains(address))
            {
                m_AssistedAimFindNewTarget = reinterpret_cast<AssistedAimFindNewTargetFn>(address);
                TUTONES_LOG_INFO("game.ptr", std::string("Resolved AssistedAimFindNewTarget at ") + AddressString(address));
            }
            else
                TUTONES_LOG_WARN("game.ptr", "AssistedAimFindNewTarget resolved outside GTA module image");
        }
        else
            TUTONES_LOG_WARN("game.ptr", "AssistedAimFindNewTarget pattern was not found; Release Dead Target will be unavailable");

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
        m_ScriptPrograms = nullptr;
        m_ScriptGlobals = nullptr;
        m_ScriptVm = nullptr;
        m_PtrToHandle = nullptr;
        m_AssistedAimShouldReleaseEntity = nullptr;
        m_AssistedAimFindNewTarget = nullptr;
        m_Module.Reset();
    }

    bool GamePointers::IsResolved() const noexcept { return m_Resolved.load(std::memory_order_acquire); }
    InitNativeTablesFn GamePointers::InitNativeTables() const noexcept { return m_InitNativeTables; }
    RunScriptThreadsFn GamePointers::RunScriptThreads() const noexcept { return m_RunScriptThreads; }
    Types::AtArray<Types::ScriptThread*>* GamePointers::ScriptThreads() const noexcept { return m_ScriptThreads; }
    Types::ScriptProgram** GamePointers::ScriptPrograms() const noexcept { return m_ScriptPrograms; }
    std::int64_t** GamePointers::ScriptGlobals() const noexcept { return m_ScriptGlobals; }
    ScriptVmFn GamePointers::ScriptVm() const noexcept { return m_ScriptVm; }
    PtrToHandleFn GamePointers::PtrToHandle() const noexcept { return m_PtrToHandle; }
    void* GamePointers::AssistedAimShouldReleaseEntity() const noexcept { return m_AssistedAimShouldReleaseEntity; }
    AssistedAimFindNewTargetFn GamePointers::AssistedAimFindNewTarget() const noexcept { return m_AssistedAimFindNewTarget; }
    const Memory::ModuleView& GamePointers::Module() const noexcept { return m_Module; }
}
