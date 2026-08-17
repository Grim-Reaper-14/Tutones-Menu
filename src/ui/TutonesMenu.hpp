#pragma once

#include <cstddef>

namespace Tutones::UI
{
    class TutonesMenu final
    {
    public:
        static TutonesMenu& Get() noexcept;

        void Render() noexcept;
        void Reset() noexcept;

    private:
        TutonesMenu() = default;
        ~TutonesMenu() = default;
        TutonesMenu(const TutonesMenu&) = delete;
        TutonesMenu& operator=(const TutonesMenu&) = delete;

        void ProcessInput() noexcept;
        void RenderNavigationRail() noexcept;
        void RenderCategoryRail() noexcept;
        void RenderContent() noexcept;

        std::size_t m_Category{};
        std::size_t m_Item{};
    };
}
