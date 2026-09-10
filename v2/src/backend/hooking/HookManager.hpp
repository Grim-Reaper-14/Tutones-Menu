#pragma once

namespace TutonesV2::Backend::Hooking
{
    class HookManager final
    {
    public:
        static HookManager& Get() noexcept;
        bool Initialize() noexcept;
        void Shutdown() noexcept;
        [[nodiscard]] bool IsReady() const noexcept;

    private:
        HookManager() = default;
        bool m_Ready{};
    };
}
