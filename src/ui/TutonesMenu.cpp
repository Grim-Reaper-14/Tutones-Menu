#include "TutonesMenu.hpp"

#include "Input.hpp"
#include "../core/logging/Logger.hpp"

#include <imgui.h>

#include <array>
#include <algorithm>

namespace Tutones::UI
{
    namespace
    {
        constexpr std::array<const char*, 5> Categories{
            "Home",
            "Player",
            "Vehicle",
            "World",
            "Settings",
        };

        constexpr std::array<const char*, 4> DiagnosticItems{
            "Hook Status",
            "Renderer Status",
            "Input Status",
            "Logging Status",
        };
    }

    TutonesMenu& TutonesMenu::Get() noexcept
    {
        static TutonesMenu instance;
        return instance;
    }

    void TutonesMenu::Reset() noexcept
    {
        m_Category = 0;
        m_Item = 0;
        TUTONES_LOG_DEBUG("ui", "Tutones menu navigation state reset");
    }

    void TutonesMenu::ProcessInput() noexcept
    {
        const auto actions = Input::Get().ConsumePendingActions();
        if (actions == 0)
            return;

        if ((actions & ToMask(InputAction::Up)) != 0)
        {
            m_Item = (m_Item == 0) ? DiagnosticItems.size() - 1 : m_Item - 1;
            TUTONES_LOG_DEBUG("ui", "Menu selection moved up");
        }
        if ((actions & ToMask(InputAction::Down)) != 0)
        {
            m_Item = (m_Item + 1) % DiagnosticItems.size();
            TUTONES_LOG_DEBUG("ui", "Menu selection moved down");
        }
        if ((actions & ToMask(InputAction::Left)) != 0)
        {
            m_Category = (m_Category == 0) ? Categories.size() - 1 : m_Category - 1;
            m_Item = 0;
            TUTONES_LOG_DEBUG("ui", "Menu category moved left");
        }
        if ((actions & ToMask(InputAction::Right)) != 0)
        {
            m_Category = (m_Category + 1) % Categories.size();
            m_Item = 0;
            TUTONES_LOG_DEBUG("ui", "Menu category moved right");
        }
        if ((actions & ToMask(InputAction::Back)) != 0)
        {
            if (m_Category != 0)
            {
                m_Category = 0;
                m_Item = 0;
                TUTONES_LOG_DEBUG("ui", "Menu Back returned to Home category");
            }
            else
            {
                Input::Get().SetMenuOpen(false);
                TUTONES_LOG_DEBUG("ui", "Menu Back closed Tutones Menu from Home");
            }
        }
        if ((actions & ToMask(InputAction::Select)) != 0)
            TUTONES_LOG_DEBUG("ui", "Menu Select received on diagnostic shell");
    }

    void TutonesMenu::RenderNavigationRail() noexcept
    {
        ImGui::BeginChild("##tutones_nav", ImVec2(64.0f, 0.0f), true);
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::TextUnformatted("TT");
        ImGui::Separator();
        for (std::size_t i = 0; i < Categories.size(); ++i)
        {
            const bool selected = i == m_Category;
            if (ImGui::Selectable(Categories[i], selected, 0, ImVec2(0.0f, 36.0f)))
            {
                m_Category = i;
                m_Item = 0;
            }
        }
        ImGui::EndChild();
    }

    void TutonesMenu::RenderCategoryRail() noexcept
    {
        ImGui::SameLine();
        ImGui::BeginChild("##tutones_categories", ImVec2(190.0f, 0.0f), true);
        ImGui::TextUnformatted(Categories[m_Category]);
        ImGui::Separator();

        for (std::size_t i = 0; i < DiagnosticItems.size(); ++i)
        {
            const bool selected = i == m_Item;
            if (ImGui::Selectable(DiagnosticItems[i], selected))
                m_Item = i;
        }

        ImGui::EndChild();
    }

    void TutonesMenu::RenderContent() noexcept
    {
        ImGui::SameLine();
        ImGui::BeginChild("##tutones_content", ImVec2(0.0f, 0.0f), true);

        ImGui::Text("Tutones Menu / %s", Categories[m_Category]);
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextUnformatted("D3D12 diagnostic shell is active.");
        ImGui::TextUnformatted("Present hook: active");
        ImGui::TextUnformatted("Primary swap chain: pinned");
        ImGui::TextUnformatted("DIRECT queue: captured");
        ImGui::TextUnformatted("WndProc routing: active");
        ImGui::TextUnformatted("Input: F4 + numpad navigation");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Selected: %s", DiagnosticItems[m_Item]);
        ImGui::TextUnformatted("This first shell is intentionally feature-light for hook/render validation.");

        ImGui::EndChild();
    }

    void TutonesMenu::Render() noexcept
    {
        if (!Input::Get().IsMenuOpen())
            return;

        ProcessInput();

        auto& style = ImGui::GetStyle();
        style.WindowRounding = 5.0f;
        style.ChildRounding = 4.0f;
        style.FrameRounding = 3.0f;

        ImGui::SetNextWindowSize(ImVec2(980.0f, 620.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.97f);

        constexpr ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoSavedSettings;

        if (ImGui::Begin("Tutones Menu", nullptr, flags))
        {
            RenderNavigationRail();
            RenderCategoryRail();
            RenderContent();
        }
        ImGui::End();
    }
}
