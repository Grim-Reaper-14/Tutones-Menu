#pragma once

#include <intrin.h>

#include <cstddef>
#include <cstdint>

namespace Tutones::Game::Types
{
    template<typename T>
    struct AtArray final
    {
        T* data{};
        std::uint16_t size{};
        std::uint16_t capacity{};
        std::uint32_t reserved{};
    };

    static_assert(sizeof(AtArray<std::uint32_t>) == 0x10);

    enum class ScriptThreadState : std::int32_t
    {
        Idle,
        Running,
        Killed,
        Paused,
        Unknown4,
    };

    struct ScriptThreadContext final
    {
        std::uint32_t threadId{};
        std::uint32_t pad04{};
        std::uint64_t scriptHash{};
        ScriptThreadState state{};
        std::uint32_t programCounter{};
        std::uint32_t framePointer{};
        std::uint32_t stackPointer{};
        float timerA{};
        float timerB{};
        float waitTimer{};
        std::byte pad2C[0x2C]{};
        std::uint32_t stackSize{};
        std::byte pad5C[0x54]{};
    };

    static_assert(sizeof(ScriptThreadContext) == 0xB0);

    struct ScriptThread final
    {
        void* vtable{};
        ScriptThreadContext context{};
        void* stack{};
        std::byte padC0[0x4]{};
        std::uint32_t parameterSize{};
        std::uint32_t parameterLocation{};
        std::byte padCC[0x4]{};
        char errorMessage[128]{};
        std::uint32_t scriptHash{};
        char scriptName[64]{};
        std::byte pad194[0x4]{};
    };

    static_assert(offsetof(ScriptThread, context) == 0x08);
    static_assert(offsetof(ScriptThread, scriptHash) == 0x150);
    static_assert(sizeof(ScriptThread) == 0x198);

    struct TlsContext final
    {
        std::byte pad000[0x7A0]{};
        ScriptThread* currentScriptThread{};
        bool scriptThreadActive{};
        std::byte pad7A9[0x7]{};

        [[nodiscard]] static TlsContext* Get() noexcept
        {
#if defined(_M_X64)
            const auto tlsArray = static_cast<std::uintptr_t>(__readgsqword(0x58));
            if (!tlsArray)
                return nullptr;
            return *reinterpret_cast<TlsContext**>(tlsArray);
#else
            return nullptr;
#endif
        }
    };

    static_assert(offsetof(TlsContext, currentScriptThread) == 0x7A0);
    static_assert(sizeof(TlsContext) == 0x7B0);
}
