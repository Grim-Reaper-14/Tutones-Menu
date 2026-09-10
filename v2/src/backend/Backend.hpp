#pragma once

namespace TutonesV2::Backend
{
    class Backend final
    {
    public:
        static Backend& Get() noexcept;

        bool Initialize() noexcept;
        void Shutdown() noexcept;
        [[nodiscard]] bool IsReady() const noexcept;

    private:
        Backend() = default;
        bool m_Ready{};
    };
}
