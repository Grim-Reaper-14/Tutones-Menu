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
    struct alignas(16) NativeVector3 final
    {
        float x{};
        float y{};
        float z{};
        float pad{};
    };

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
                if (m_VectorRefTargets[i])
                    *m_VectorRefTargets[i] = m_VectorRefSources[i];
            }
            m_NumVectorRefs = 0;
        }

    protected:
        void* m_ReturnValue{};
        std::uint32_t m_ArgCount{};
        void* m_Args{};
        std::int32_t m_NumVectorRefs{};
        NativeVector3* m_VectorRefTargets[4]{};
        NativeVector3 m_VectorRefSources[4]{};
    };

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
