#pragma once

namespace TutonesV2::Frontend::Render
{
    class Renderer final
    {
    public:
        static Renderer& Get() noexcept;
        bool Initialize() noexcept;
        void Shutdown() noexcept;
        [[nodiscard]] bool IsReady() const noexcept;

    private:
        Renderer() = default;
        bool m_Ready{};
    };
}
