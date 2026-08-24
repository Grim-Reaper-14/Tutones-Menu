#pragma once

#include "../native/NativeCallContext.hpp"

#include <cstddef>
#include <cstdint>

namespace Tutones::Game::Types
{
    struct ScriptProgram final
    {
        std::byte pad00[0x10]{};
        std::uint8_t** codeBlocks{};       // 0x10
        std::uint32_t hash{};              // 0x18
        std::uint32_t codeSize{};          // 0x1C
        std::uint32_t argCount{};           // 0x20
        std::uint32_t localCount{};         // 0x24
        std::uint32_t globalCount{};        // 0x28
        std::uint32_t nativeCount{};        // 0x2C
        void* localData{};                  // 0x30
        void** globalData{};                // 0x38
        Native::NativeHandler* nativeEntrypoints{}; // 0x40
        std::uint32_t procCount{};          // 0x48
        std::byte pad4C[0x4]{};
        const char** procNames{};           // 0x50
        std::uint32_t nameHash{};           // 0x58
        std::uint32_t refCount{};           // 0x5C
        const char* name{};                 // 0x60
        const char** stringsData{};         // 0x68
        std::uint32_t stringsCount{};        // 0x70
        std::byte pad74[0x0C]{};

        [[nodiscard]] std::uint8_t* GetCodeAddress(std::uint32_t index) const noexcept
        {
            if (!codeBlocks || index >= codeSize)
                return nullptr;

            auto* page = codeBlocks[index >> 14];
            return page ? &page[index & 0x3FFF] : nullptr;
        }
    };

    static_assert(offsetof(ScriptProgram, codeBlocks) == 0x10);
    static_assert(offsetof(ScriptProgram, nativeCount) == 0x2C);
    static_assert(offsetof(ScriptProgram, nativeEntrypoints) == 0x40);
    static_assert(sizeof(ScriptProgram) == 0x80);
}
