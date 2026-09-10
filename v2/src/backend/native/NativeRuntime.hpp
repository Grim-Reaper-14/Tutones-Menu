#pragma once

namespace TutonesV2::Backend::Native
{
    class NativeRuntime final
    {
    public:
        static NativeRuntime& Get() noexcept;
        bool Initialize() noexcept;
        void Shutdown() noexcept;
        [[nodiscard]] bool IsReady() const noexcept;

    private:
        NativeRuntime() = default;
        bool m_Ready{};
    };
}
