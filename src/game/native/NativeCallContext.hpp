#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

namespace Tutones::Game::Native
{
    // GTA script-native vectors use one 64-bit VM slot per float component.
    // This matches YimMenuV2's rage::scrVector layout (x @ 0, y @ 8, z @ 16).
    struct NativeVector3 final
    {
        alignas(8) float x{};
        alignas(8) float y{};
        alignas(8) float z{};
    };

    static_assert(sizeof(NativeVector3) == 0x18);

    // Native handlers keep temporary vector-ref sources as the engine's packed
    // 16-byte fvector3 representation. Keep this separate from NativeVector3
    // or FixVectors will copy y/z into the wrong script slots.
    struct alignas(16) NativeVectorRefSource final
    {
        float x{};
        float y{};
        float z{};
        float pad{};
    };

    static_assert(sizeof(NativeVectorRefSource) == 0x10);

    class NativeCallContext
    {
    public:
        void Reset() noexcept
        {
            m_ArgCount = 0;
            m_NumVectorRefs = 0;
        }

        template<typename T>
        bool PushArg(T value) noexcept
        {
            using Value = std::remove_cv_t<std::remove_reference_t<T>>;
            static_assert(std::is_trivially_copyable_v<Value>);
            static_assert(sizeof(Value) <= sizeof(std::uint64_t));

            if (!m_Args || m_ArgCount >= 40)
                return false;

            std::uint64_t slot{};
            std::memcpy(&slot, &value, sizeof(Value));
            reinterpret_cast<std::uint64_t*>(m_Args)[m_ArgCount++] = slot;
            return true;
        }

        template<typename T>
        [[nodiscard]] T GetReturnValue() const noexcept
        {
            using Value = std::remove_cv_t<std::remove_reference_t<T>>;
            static_assert(std::is_trivially_copyable_v<Value>);
            static_assert(sizeof(Value) <= sizeof(NativeVector3));

            Value value{};
            if (m_ReturnValue)
                std::memcpy(&value, m_ReturnValue, sizeof(Value));
            return value;
        }

        void FixVectors() noexcept
        {
            const auto count = std::clamp(m_NumVectorRefs, 0, 4);
            for (int i = 0; i < count; ++i)
            {
                if (!m_VectorRefTargets[i])
                    continue;

                m_VectorRefTargets[i]->x = m_VectorRefSources[i].x;
                m_VectorRefTargets[i]->y = m_VectorRefSources[i].y;
                m_VectorRefTargets[i]->z = m_VectorRefSources[i].z;
            }
            m_NumVectorRefs = 0;
        }

    protected:
        void* m_ReturnValue{};                         // 0x00
        std::uint32_t m_ArgCount{};                    // 0x08
        void* m_Args{};                                // 0x10
        std::int32_t m_NumVectorRefs{};                // 0x18
        NativeVector3* m_VectorRefTargets[4]{};        // 0x20
        NativeVectorRefSource m_VectorRefSources[4]{}; // 0x40
    };

    static_assert(offsetof(NativeCallContext, m_ReturnValue) == 0x00);
    static_assert(offsetof(NativeCallContext, m_ArgCount) == 0x08);
    static_assert(offsetof(NativeCallContext, m_Args) == 0x10);
    static_assert(offsetof(NativeCallContext, m_NumVectorRefs) == 0x18);
    static_assert(offsetof(NativeCallContext, m_VectorRefTargets) == 0x20);
    static_assert(offsetof(NativeCallContext, m_VectorRefSources) == 0x40);
    static_assert(sizeof(NativeCallContext) == 0x80);

    using NativeHash = std::uint64_t;
    using NativeHandler = void(*)(NativeCallContext* context);

    class CallContext final : public NativeCallContext
    {
    public:
        CallContext() noexcept
        {
            m_ReturnValue = m_ReturnStack.data();
            m_Args = m_ArgStack.data();
            Reset();
        }

    private:
        std::array<std::uint64_t, 10> m_ReturnStack{};
        std::array<std::uint64_t, 40> m_ArgStack{};
    };
}
