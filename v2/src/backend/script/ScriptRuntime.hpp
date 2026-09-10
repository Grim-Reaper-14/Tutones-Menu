#pragma once

namespace TutonesV2::Backend::Script
{
    class ScriptRuntime final
    {
    public:
        static ScriptRuntime& Get() noexcept;
        bool Initialize() noexcept;
        void Shutdown() noexcept;
        [[nodiscard]] bool IsReady() const noexcept;

    private:
        ScriptRuntime() = default;
        bool m_Ready{};
    };
}
