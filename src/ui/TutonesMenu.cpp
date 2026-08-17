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
        struct CategoryEntry final
        {
            const char* glyph;
            const char* name;
        };

        constexpr std::array<CategoryEntry, 5> Categories{{
            {"H", "Home"},
            {"P", "Player"},
            {"V", "Vehicle"},
            {"W", "World"},
            {"S", "Settings"},
        }};

        constexpr std::array<std::array<const char*, 4>, 5> CategoryItems{{
            {{"Overview", "Quick Actions", "Favorites", "Diagnostics"}},
            {{"Player Options", "Health & Armor", "Movement", "Appearance"}},
            {{"Vehicle Options", "Spawner", "Paint", "Modifications"}},
            {{"World Options", "Time & Weather", "Teleport", "Environment"}},
            {{"Menu", "Theme", "Input", "Logging"}},
        }};

        constexpr std::array<const char*, 5> CategoryDescriptions{{
            "Runtime overview and quick access to Tutones systems.",
            "Player feature foundation. Native-backed options connect next.",
            "Vehicle control and customization, including the paint pipeline.",
            "World, time, weather, teleport, and environment controls.",
            "Tutones appearance, controls, diagnostics, and logging.",
        }};

        constexpr ImVec4 Accent{147.0f / 255.0f, 190.0f / 255.0f, 66.0f / 255.0f, 1.0f};
        constexpr float HeaderHeight = 48.0f;
        constexpr float NavWidth = 70.0f;
        constexpr float CategoryWidth = 188.0f;

        ImU32 Color(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255) noexcept
        {
            return IM_COL32(r, g, b, a);
        }

        bool RailButton(const char* id, const char* glyph, const char* tooltip, bool selected) noexcept
        {
            const ImVec2 size{46.0f, 42.0f};
            const ImVec2 min = ImGui::GetCursorScreenPos();
            const ImVec2 max{min.x + size.x, min.y + size.y};

            const bool pressed = ImGui::InvisibleButton(id, size);
            const bool hovered = ImGui::IsItemHovered();
            auto* draw = ImGui::GetWindowDrawList();

            if (selected || hovered)
                draw->AddRectFilled(min, max, Color(255, 255, 255, selected ? 12 : 6), 4.0f);

            if (selected)
            {
                draw->AddRectFilled(
                    ImVec2(max.x - 3.0f, min.y + 9.0f),
                    ImVec2(max.x, max.y - 9.0f),
                    ImGui::GetColorU32(Accent),
                    3.0f);
            }

            const auto textSize = ImGui::CalcTextSize(glyph);
            draw->AddText(
                ImVec2(min.x + (size.x - textSize.x) * 0.5f, min.y + (size.y - textSize.y) * 0.5f),
                selected ? Color(245, 245, 245) : Color(155, 155, 160),
                glyph);

            if (hovered)
            {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(tooltip);
                ImGui::EndTooltip();
            }

            return pressed;
        }

        bool Subtab(const char* id, const char* label, bool selected) noexcept
        {
            const float width = std::max(40.0f, ImGui::GetContentRegionAvail().x - 16.0f);
            const ImVec2 size{width, 34.0f};
            const ImVec2 min = ImGui::GetCursorScreenPos();
            const ImVec2 max{min.x + size.x, min.y + size.y};

            const bool pressed = ImGui::InvisibleButton(id, size);
            const bool hovered = ImGui::IsItemHovered();
            auto* draw = ImGui::GetWindowDrawList();

            if (selected || hovered)
                draw->AddRectFilled(min, max, Color(255, 255, 255, selected ? 11 : 5), 4.0f);

            if (selected)
            {
                draw->AddRectFilled(
                    ImVec2(max.x - 3.0f, min.y + 8.0f),
                    ImVec2(max.x, max.y - 8.0f),
                    ImGui::GetColorU32(Accent),
                    3.0f);
            }

            const auto textSize = ImGui::CalcTextSize(label);
            draw->AddText(
                ImVec2(min.x + 12.0f, min.y + (size.y - textSize.y) * 0.5f),
                selected ? Color(240, 240, 242) : Color(148, 148, 154),
                label);

            return pressed;
        }

        void StatusCard(const char* id, const char* title, const char* value, const char* detail, bool active = true) noexcept
        {
            const ImVec2 min = ImGui::GetCursorScreenPos();
            const ImVec2 size{ImGui::GetContentRegionAvail().x, 66.0f};
            const ImVec2 max{min.x + size.x, min.y + size.y};

            ImGui::InvisibleButton(id, size);
            auto* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(min, max, Color(29, 29, 32), 5.0f);
            draw->AddRect(min, max, Color(255, 255, 255, 10), 5.0f);
            draw->AddRectFilled(
                ImVec2(min.x, min.y + 12.0f),
                ImVec2(min.x + 3.0f, max.y - 12.0f),
                active ? ImGui::GetColorU32(Accent) : Color(105, 105, 110),
                3.0f);

            draw->AddText(ImVec2(min.x + 14.0f, min.y + 11.0f), Color(228, 228, 230), title);
            const auto valueSize = ImGui::CalcTextSize(value);
            draw->AddText(
                ImVec2(max.x - valueSize.x - 14.0f, min.y + 11.0f),
                active ? ImGui::GetColorU32(Accent) : Color(135, 135, 140),
                value);
            draw->AddText(ImVec2(min.x + 14.0f, min.y + 38.0f), Color(125, 125, 132), detail);
        }
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

        constexpr std::size_t itemCount = CategoryItems.front().size();

        if ((actions & ToMask(InputAction::Up)) != 0)
        {
            m_Item = (m_Item == 0) ? itemCount - 1 : m_Item - 1;
            TUTONES_LOG_DEBUG("ui", "Menu selection moved up");
        }
        if ((actions & ToMask(InputAction::Down)) != 0)
        {
            m_Item = (m_Item + 1) % itemCount;
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
                TUTONES_LOG_DEBUG("ui", "Menu Back returned to Home");
            }
            else
            {
                Input::Get().SetMenuOpen(false);
                TUTONES_LOG_DEBUG("ui", "Menu Back closed Tutones Menu from Home");
            }
        }
        if ((actions & ToMask(InputAction::Select)) != 0)
        {
            TUTONES_LOG_DEBUG("ui", "Menu Select received");
        }
    }

    void TutonesMenu::RenderNavigationRail() noexcept
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(16.0f / 255.0f, 16.0f / 255.0f, 18.0f / 255.0f, 1.0f));
        ImGui::BeginChild("##tutones_nav", ImVec2(NavWidth, 0.0f), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        ImGui::SetCursorPosY(17.0f);
        for (std::size_t i = 0; i < Categories.size(); ++i)
        {
            ImGui::SetCursorPosX(12.0f);
            ImGui::PushID(static_cast<int>(i));
            if (RailButton("##category", Categories[i].glyph, Categories[i].name, i == m_Category))
            {
                m_Category = i;
                m_Item = 0;
                TUTONES_LOG_DEBUG("ui.mouse", "Menu category selected with mouse");
            }
            ImGui::PopID();
            ImGui::Dummy(ImVec2(0.0f, 5.0f));
        }

        const auto pos = ImGui::GetWindowPos();
        const auto size = ImGui::GetWindowSize();
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(pos.x + size.x - 1.0f, pos.y),
            ImVec2(pos.x + size.x - 1.0f, pos.y + size.y),
            Color(255, 255, 255, 9));

        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    void TutonesMenu::RenderCategoryRail() noexcept
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(20.0f / 255.0f, 20.0f / 255.0f, 22.0f / 255.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 17.0f));
        ImGui::BeginChild("##tutones_categories", ImVec2(CategoryWidth, 0.0f), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        ImGui::TextColored(Accent, "%s", Categories[m_Category].name);
        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        for (std::size_t i = 0; i < CategoryItems[m_Category].size(); ++i)
        {
            ImGui::PushID(static_cast<int>(i));
            if (Subtab("##subtab", CategoryItems[m_Category][i], i == m_Item))
            {
                m_Item = i;
                TUTONES_LOG_DEBUG("ui.mouse", "Menu subtab selected with mouse");
            }
            ImGui::PopID();
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
        }

        const auto pos = ImGui::GetWindowPos();
        const auto size = ImGui::GetWindowSize();
        auto* draw = ImGui::GetWindowDrawList();
        draw->AddLine(
            ImVec2(pos.x + size.x - 1.0f, pos.y),
            ImVec2(pos.x + size.x - 1.0f, pos.y + size.y),
            Color(255, 255, 255, 9));
        draw->AddText(ImVec2(pos.x + 14.0f, pos.y + size.y - 48.0f), Color(95, 95, 101), "NUM8/2  MOVE");
        draw->AddText(ImVec2(pos.x + 14.0f, pos.y + size.y - 30.0f), Color(95, 95, 101), "NUM5 SELECT   NUM0 BACK");

        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    void TutonesMenu::RenderContent() noexcept
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(24.0f / 255.0f, 24.0f / 255.0f, 26.0f / 255.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22.0f, 19.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 10.0f));
        ImGui::BeginChild("##tutones_content", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_NoScrollbar);

        ImGui::TextDisabled("%s  /  %s", Categories[m_Category].name, CategoryItems[m_Category][m_Item]);
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.96f, 1.0f), "%s", CategoryItems[m_Category][m_Item]);
        ImGui::TextDisabled("%s", CategoryDescriptions[m_Category]);
        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        if (m_Category == 0 || m_Category == 4)
        {
            StatusCard("##hook", "Hook Layer", "ONLINE", "Present + ResizeBuffers + ExecuteCommandLists");
            ImGui::Dummy(ImVec2(0.0f, 6.0f));
            StatusCard("##renderer", "Renderer", "D3D12", "Primary swap chain + DIRECT queue captured");
            ImGui::Dummy(ImVec2(0.0f, 6.0f));
            StatusCard("##input", "Input", "CAPTURED", "F4 + numpad + mouse routed only to Tutones while open");
            ImGui::Dummy(ImVec2(0.0f, 6.0f));
            StatusCard("##logging", "Logging", "ACTIVE", "Lifecycle, hook, render, input, and UI diagnostics");
        }
        else if (m_Category == 2)
        {
            StatusCard("##vehicle-runtime", "Vehicle Runtime", "NEXT", "Vehicle detection and native runtime are the next feature layer", false);
            ImGui::Dummy(ImVec2(0.0f, 6.0f));
            StatusCard("##vehicle-paint", "Paint Pipeline", "PLANNED", "Normal, Metallic, Pearlescent, Matte, Metal, Chrome, Worn, Chameleon", false);
            ImGui::Dummy(ImVec2(0.0f, 6.0f));
            StatusCard("##vehicle-wheels", "Wheel Paint", "PLANNED", "Normal + Chameleon wheel color support", false);
        }
        else
        {
            StatusCard("##feature-foundation", "Feature Foundation", "READY", "UI, renderer, input capture, config, filesystem, and logging are live");
            ImGui::Dummy(ImVec2(0.0f, 6.0f));
            StatusCard("##runtime-next", "Game Runtime", "NEXT", "Native/game infrastructure will populate this category", false);
        }

        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
    }

    void TutonesMenu::Render() noexcept
    {
        if (!Input::Get().IsMenuOpen())
            return;

        ProcessInput();
        if (!Input::Get().IsMenuOpen())
            return;

        auto& io = ImGui::GetIO();
        io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
        io.MouseDrawCursor = true;

        auto& style = ImGui::GetStyle();
        style.WindowRounding = 7.0f;
        style.ChildRounding = 0.0f;
        style.FrameRounding = 4.0f;
        style.WindowBorderSize = 1.0f;

        const auto* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(900.0f, 560.0f), ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(15.0f / 255.0f, 15.0f / 255.0f, 17.0f / 255.0f, 0.99f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(52.0f / 255.0f, 52.0f / 255.0f, 57.0f / 255.0f, 1.0f));

        constexpr ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse;

        if (ImGui::Begin("##TutonesMenuRoot", nullptr, flags))
        {
            const auto pos = ImGui::GetWindowPos();
            const auto size = ImGui::GetWindowSize();
            auto* draw = ImGui::GetWindowDrawList();

            draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + HeaderHeight), Color(18, 18, 20), 7.0f, ImDrawFlags_RoundCornersTop);
            draw->AddLine(
                ImVec2(pos.x, pos.y + HeaderHeight),
                ImVec2(pos.x + size.x, pos.y + HeaderHeight),
                Color(255, 255, 255, 10));
            draw->AddText(ImVec2(pos.x + 18.0f, pos.y + 16.0f), ImGui::GetColorU32(Accent), "TUTONES");
            draw->AddText(ImVec2(pos.x + 79.0f, pos.y + 16.0f), Color(228, 228, 231), "MENU");
            draw->AddText(ImVec2(pos.x + size.x - 83.0f, pos.y + 16.0f), Color(100, 100, 106), "F4  CLOSE");

            ImGui::SetCursorPos(ImVec2(0.0f, HeaderHeight));
            RenderNavigationRail();
            ImGui::SameLine(0.0f, 0.0f);
            RenderCategoryRail();
            ImGui::SameLine(0.0f, 0.0f);
            RenderContent();
        }
        ImGui::End();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();
    }
}
