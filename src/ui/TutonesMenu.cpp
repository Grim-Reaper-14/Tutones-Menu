#include "TutonesMenu.hpp"

#include "Input.hpp"
#include "../core/logging/Logger.hpp"
#include "../../byte_array.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <initializer_list>
#include <string>
#include <unordered_map>

namespace Tutones::UI
{
    namespace
    {
        struct CategoryEntry final
        {
            const char* icon;
            const char* name;
            std::array<const char*, 3> items;
        };

        constexpr std::array<CategoryEntry, 5> Categories{{
            {"B", "Home",     {{"Overview", "Quick Actions", "Diagnostics"}}},
            {"C", "Player",   {{"General", "Movement", "Appearance"}}},
            {"D", "Vehicle",  {{"General", "Paint", "Modifications"}}},
            {"E", "World",    {{"General", "Time & Weather", "Teleport"}}},
            {"F", "Settings", {{"General", "Theme", "Logging"}}},
        }};

        constexpr float MenuWidth = 730.0f;
        constexpr float MenuHeight = 460.0f;
        constexpr float LeftPanelWidth = 210.0f;
        constexpr float IconRailWidth = 47.0f;
        constexpr ImVec4 Accent{147.0f / 255.0f, 190.0f / 255.0f, 66.0f / 255.0f, 1.0f};

        ImFont* g_Medium{};
        ImFont* g_Bold{};
        ImFont* g_TabIcons{};
        ImFont* g_Logo{};
        ImFont* g_TabTitle{};
        ImFont* g_TabTitleIcon{};
        ImFont* g_SubtabTitle{};
        ImGuiContext* g_FontContext{};

        struct TabAnimation final
        {
            float elementOpacity{};
            float rectOpacity{};
            float textOpacity{};
        };

        std::unordered_map<ImGuiID, TabAnimation> g_TabAnimations;
        std::unordered_map<ImGuiID, TabAnimation> g_SubtabAnimations;

        ImVec2 Offset(const ImVec2& point, float x, float y) noexcept
        {
            return ImVec2(point.x + x, point.y + y);
        }

        ImU32 Color(int r, int g, int b, int a = 255) noexcept
        {
            return IM_COL32(r, g, b, a);
        }

        float Animate(float current, float target, float speed) noexcept
        {
            const float delta = ImGui::GetIO().DeltaTime;
            const float amount = std::clamp(speed * (1.0f - delta), 0.0f, 1.0f);
            return current + ((target - current) * amount);
        }

        ImFont* FontOrDefault(ImFont* font) noexcept
        {
            return font ? font : ImGui::GetFont();
        }

        bool EnsureOriginalFonts() noexcept
        {
            auto* context = ImGui::GetCurrentContext();
            if (!context)
                return false;

            if (g_FontContext == context && g_Medium && g_Bold && g_TabIcons)
                return true;

            g_FontContext = context;
            g_Medium = nullptr;
            g_Bold = nullptr;
            g_TabIcons = nullptr;
            g_Logo = nullptr;
            g_TabTitle = nullptr;
            g_TabTitleIcon = nullptr;
            g_SubtabTitle = nullptr;
            g_TabAnimations.clear();
            g_SubtabAnimations.clear();

            auto& io = ImGui::GetIO();
            ImFontConfig config{};
            config.FontDataOwnedByAtlas = false;

            g_Medium = io.Fonts->AddFontFromMemoryTTF(
                PTRootUIMedium,
                static_cast<int>(sizeof(PTRootUIMedium)),
                15.0f,
                &config);
            g_Bold = io.Fonts->AddFontFromMemoryTTF(
                PTRootUIBold,
                static_cast<int>(sizeof(PTRootUIBold)),
                15.0f,
                &config);
            g_TabIcons = io.Fonts->AddFontFromMemoryTTF(
                clarityfont,
                static_cast<int>(sizeof(clarityfont)),
                15.0f,
                &config);
            g_Logo = io.Fonts->AddFontFromMemoryTTF(
                clarityfont,
                static_cast<int>(sizeof(clarityfont)),
                21.0f,
                &config);
            g_TabTitle = io.Fonts->AddFontFromMemoryTTF(
                PTRootUIBold,
                static_cast<int>(sizeof(PTRootUIBold)),
                19.0f,
                &config);
            g_TabTitleIcon = io.Fonts->AddFontFromMemoryTTF(
                clarityfont,
                static_cast<int>(sizeof(clarityfont)),
                18.0f,
                &config);
            g_SubtabTitle = io.Fonts->AddFontFromMemoryTTF(
                PTRootUIBold,
                static_cast<int>(sizeof(PTRootUIBold)),
                15.0f,
                &config);

            if (!g_Medium)
                g_Medium = io.Fonts->AddFontDefault();
            if (!g_Bold) g_Bold = g_Medium;
            if (!g_TabIcons) g_TabIcons = g_Medium;
            if (!g_Logo) g_Logo = g_TabIcons;
            if (!g_TabTitle) g_TabTitle = g_Bold;
            if (!g_TabTitleIcon) g_TabTitleIcon = g_TabIcons;
            if (!g_SubtabTitle) g_SubtabTitle = g_Bold;

            io.FontDefault = g_Medium;

            if (g_Medium && g_Bold && g_TabIcons)
            {
                TUTONES_LOG_INFO("ui.fonts", "Loaded original Tutones embedded menu fonts and icon font");
                return true;
            }

            TUTONES_LOG_WARN("ui.fonts", "Original Tutones embedded fonts were incomplete; using ImGui fallback fonts");
            return g_Medium != nullptr;
        }

        bool OriginalTabButton(const char* id, const char* icon, bool selected, const ImVec2& localPos) noexcept
        {
            ImGui::SetCursorPos(localPos);
            const ImVec2 min = ImGui::GetCursorScreenPos();
            const ImVec2 size{31.0f, 31.0f};
            const ImVec2 max{min.x + size.x, min.y + size.y};
            const ImGuiID itemId = ImGui::GetID(id);

            const bool pressed = ImGui::InvisibleButton(id, size);
            const bool hovered = ImGui::IsItemHovered();

            auto& anim = g_TabAnimations[itemId];
            anim.elementOpacity = Animate(anim.elementOpacity, selected ? 0.04f : hovered ? 0.01f : 0.0f, 0.07f);
            anim.rectOpacity = Animate(anim.rectOpacity, selected ? 1.0f : 0.0f, 0.15f);
            anim.textOpacity = Animate(anim.textOpacity, selected ? 1.0f : hovered ? 0.5f : 0.3f, 0.07f);

            auto* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(min, max, ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, anim.elementOpacity)), 3.0f);

            ImFont* font = FontOrDefault(g_TabIcons);
            const ImVec2 textSize = font->CalcTextSizeA(15.0f, FLT_MAX, 0.0f, icon);
            draw->AddText(
                font,
                15.0f,
                ImVec2(min.x + (size.x - textSize.x) * 0.5f, min.y + (size.y - textSize.y) * 0.5f),
                ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, anim.textOpacity)),
                icon);

            draw->AddRectFilled(
                ImVec2(max.x + 4.0f, min.y + 6.0f),
                ImVec2(max.x + 8.0f, max.y - 6.0f),
                ImGui::GetColorU32(ImVec4(Accent.x, Accent.y, Accent.z, anim.rectOpacity)),
                4.0f);

            return pressed;
        }

        bool OriginalSubtabButton(const char* id, const char* label, bool selected, const ImVec2& localPos) noexcept
        {
            ImGui::SetCursorPos(localPos);
            const ImVec2 min = ImGui::GetCursorScreenPos();
            const ImVec2 size{145.0f, 32.0f};
            const ImVec2 max{min.x + size.x, min.y + size.y};
            const ImGuiID itemId = ImGui::GetID(id);

            const bool pressed = ImGui::InvisibleButton(id, size);
            const bool hovered = ImGui::IsItemHovered();

            auto& anim = g_SubtabAnimations[itemId];
            anim.elementOpacity = Animate(anim.elementOpacity, selected ? 0.04f : 0.0f, 0.09f);
            anim.rectOpacity = Animate(anim.rectOpacity, selected ? 1.0f : 0.0f, 0.20f);
            anim.textOpacity = Animate(anim.textOpacity, selected ? 1.0f : 0.3f, 0.07f);

            auto* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(min, max, ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, anim.elementOpacity)), 3.0f);

            ImFont* font = FontOrDefault(g_Medium);
            const ImVec2 textSize = font->CalcTextSizeA(15.0f, FLT_MAX, 0.0f, label);
            draw->AddText(
                font,
                15.0f,
                ImVec2(min.x + 15.0f, min.y + (size.y - textSize.y) * 0.5f),
                ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, anim.textOpacity)),
                label);

            draw->AddRectFilled(
                ImVec2(max.x + 5.0f, min.y + 9.0f),
                ImVec2(max.x + 8.0f, max.y - 9.0f),
                ImGui::GetColorU32(ImVec4(Accent.x, Accent.y, Accent.z, anim.rectOpacity)),
                4.0f);

            return pressed;
        }

        void DrawPanel(
            ImDrawList* draw,
            const ImVec2& windowPos,
            const ImVec2& localPos,
            const ImVec2& size,
            const char* title,
            std::initializer_list<const char*> lines) noexcept
        {
            const ImVec2 min = Offset(windowPos, localPos.x, localPos.y);
            const ImVec2 max{min.x + size.x, min.y + size.y};

            draw->AddRectFilled(min, max, Color(24, 24, 26), 4.0f);
            draw->AddRect(min, max, Color(255, 255, 255, 8), 4.0f);
            draw->AddLine(Offset(min, 1.0f, 32.0f), ImVec2(max.x - 1.0f, min.y + 32.0f), Color(255, 255, 255, 8));

            draw->AddText(
                FontOrDefault(g_Bold),
                15.0f,
                Offset(min, 16.0f, 9.0f),
                ImGui::GetColorU32(Accent),
                title);

            float y = min.y + 49.0f;
            for (const char* line : lines)
            {
                if (!line || line[0] == '\0')
                {
                    y += 10.0f;
                    continue;
                }

                draw->AddText(
                    FontOrDefault(g_Medium),
                    14.0f,
                    ImVec2(min.x + 16.0f, y),
                    Color(188, 188, 193),
                    line);
                y += 22.0f;

                if (y > max.y - 18.0f)
                    break;
            }
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
        static_cast<void>(EnsureOriginalFonts());
        TUTONES_LOG_DEBUG("ui", "Tutones navigation reset to original menu layout");
    }

    void TutonesMenu::ProcessInput() noexcept
    {
        const auto actions = Input::Get().ConsumePendingActions();
        if (actions == 0)
            return;

        constexpr std::size_t itemCount = 3;

        if ((actions & ToMask(InputAction::Up)) != 0)
        {
            m_Item = (m_Item == 0) ? itemCount - 1 : m_Item - 1;
            TUTONES_LOG_DEBUG("ui", "Original menu subtab moved up");
        }
        if ((actions & ToMask(InputAction::Down)) != 0)
        {
            m_Item = (m_Item + 1) % itemCount;
            TUTONES_LOG_DEBUG("ui", "Original menu subtab moved down");
        }
        if ((actions & ToMask(InputAction::Left)) != 0)
        {
            m_Category = (m_Category == 0) ? Categories.size() - 1 : m_Category - 1;
            m_Item = 0;
            TUTONES_LOG_DEBUG("ui", "Original menu category moved left");
        }
        if ((actions & ToMask(InputAction::Right)) != 0)
        {
            m_Category = (m_Category + 1) % Categories.size();
            m_Item = 0;
            TUTONES_LOG_DEBUG("ui", "Original menu category moved right");
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
            std::string message("Menu Select: ");
            message += Categories[m_Category].name;
            message += " / ";
            message += Categories[m_Category].items[m_Item];
            TUTONES_LOG_DEBUG("ui", message);
        }
    }

    void TutonesMenu::RenderNavigationRail() noexcept
    {
        for (std::size_t i = 0; i < Categories.size(); ++i)
        {
            ImGui::PushID(static_cast<int>(i));
            const ImVec2 localPos{8.0f, 56.0f + static_cast<float>(i) * 36.0f};
            if (OriginalTabButton("##original_tab", Categories[i].icon, i == m_Category, localPos))
            {
                m_Category = i;
                m_Item = 0;
                TUTONES_LOG_DEBUG("ui.mouse", "Original menu category selected with mouse");
            }
            ImGui::PopID();
        }
    }

    void TutonesMenu::RenderCategoryRail() noexcept
    {
        const ImVec2 windowPos = ImGui::GetWindowPos();
        auto* draw = ImGui::GetWindowDrawList();

        draw->AddText(
            FontOrDefault(g_SubtabTitle),
            15.0f,
            Offset(windowPos, 72.0f, 60.0f),
            Color(255, 255, 255, 102),
            "MAIN");

        for (std::size_t i = 0; i < Categories[m_Category].items.size(); ++i)
        {
            ImGui::PushID(static_cast<int>(i));
            const ImVec2 localPos{57.0f, 86.0f + static_cast<float>(i) * 35.0f};
            if (OriginalSubtabButton("##original_subtab", Categories[m_Category].items[i], i == m_Item, localPos))
            {
                m_Item = i;
                TUTONES_LOG_DEBUG("ui.mouse", "Original menu subtab selected with mouse");
            }
            ImGui::PopID();
        }

        draw->AddText(
            FontOrDefault(g_Medium),
            12.0f,
            Offset(windowPos, 64.0f, 397.0f),
            Color(110, 110, 116),
            "NUM8/2  MOVE");
        draw->AddText(
            FontOrDefault(g_Medium),
            12.0f,
            Offset(windowPos, 64.0f, 416.0f),
            Color(110, 110, 116),
            "NUM5 SELECT");
        draw->AddText(
            FontOrDefault(g_Medium),
            12.0f,
            Offset(windowPos, 64.0f, 435.0f),
            Color(110, 110, 116),
            "NUM0 BACK");
    }

    void TutonesMenu::RenderContent() noexcept
    {
        const ImVec2 windowPos = ImGui::GetWindowPos();
        auto* draw = ImGui::GetWindowDrawList();
        const char* selected = Categories[m_Category].items[m_Item];

        switch (m_Category)
        {
        case 0:
            DrawPanel(draw, windowPos, ImVec2(226.0f, 16.0f), ImVec2(240.0f, 300.0f), "Runtime",
                {"Tutones Menu", "D3D12 renderer online", "Primary swap chain pinned", "DIRECT queue captured", "WndProc routing active", "Mouse capture active"});
            DrawPanel(draw, windowPos, ImVec2(476.0f, 16.0f), ImVec2(240.0f, 240.0f), "Navigation",
                {"F4  open / close", "NUM8 / NUM2  up / down", "NUM4 / NUM6  categories", "NUM5  select", "NUM0  back"});
            DrawPanel(draw, windowPos, ImVec2(226.0f, 332.0f), ImVec2(240.0f, 114.0f), "Selected",
                {selected});
            DrawPanel(draw, windowPos, ImVec2(476.0f, 272.0f), ImVec2(240.0f, 174.0f), "Logging",
                {"Hook logging active", "Render logging active", "Input logging active", "UI logging active"});
            break;

        case 1:
            DrawPanel(draw, windowPos, ImVec2(226.0f, 16.0f), ImVec2(240.0f, 300.0f), "Player",
                {"Player runtime placeholder", "Native runtime comes next", "Feature state will live here", "No fake game writes enabled"});
            DrawPanel(draw, windowPos, ImVec2(476.0f, 16.0f), ImVec2(240.0f, 240.0f), "Current",
                {selected, "Input routing validated", "D3D12 overlay validated"});
            DrawPanel(draw, windowPos, ImVec2(226.0f, 332.0f), ImVec2(240.0f, 114.0f), "Status",
                {"Foundation ready"});
            DrawPanel(draw, windowPos, ImVec2(476.0f, 272.0f), ImVec2(240.0f, 174.0f), "Next",
                {"Game runtime", "Native invoker", "Player state access"});
            break;

        case 2:
            DrawPanel(draw, windowPos, ImVec2(226.0f, 16.0f), ImVec2(240.0f, 300.0f), "Vehicle",
                {"Vehicle detection next", "Current vehicle state", "Model / handle tracking", "Safe ownership checks"});
            DrawPanel(draw, windowPos, ImVec2(476.0f, 16.0f), ImVec2(240.0f, 240.0f), "Paint",
                {"Normal", "Metallic / Pearlescent", "Matte / Metal / Chrome", "Worn", "Chameleon"});
            DrawPanel(draw, windowPos, ImVec2(226.0f, 332.0f), ImVec2(240.0f, 114.0f), "Selected",
                {selected});
            DrawPanel(draw, windowPos, ImVec2(476.0f, 272.0f), ImVec2(240.0f, 174.0f), "Wheels",
                {"Normal wheel paint", "Chameleon wheel paint", "Runtime integration pending"});
            break;

        case 3:
            DrawPanel(draw, windowPos, ImVec2(226.0f, 16.0f), ImVec2(240.0f, 300.0f), "World",
                {"World runtime placeholder", "Time and weather", "Teleport foundation", "Environment controls"});
            DrawPanel(draw, windowPos, ImVec2(476.0f, 16.0f), ImVec2(240.0f, 240.0f), "Current",
                {selected, "UI only until native runtime"});
            DrawPanel(draw, windowPos, ImVec2(226.0f, 332.0f), ImVec2(240.0f, 114.0f), "Status",
                {"Foundation ready"});
            DrawPanel(draw, windowPos, ImVec2(476.0f, 272.0f), ImVec2(240.0f, 174.0f), "Next",
                {"Native world access", "Safe state reads", "Feature implementation"});
            break;

        default:
            DrawPanel(draw, windowPos, ImVec2(226.0f, 16.0f), ImVec2(240.0f, 300.0f), "Settings",
                {"Tutones Menu", "Original UI restored", "D3D12 backend injected", "F4 toggle", "NUM0 is Back"});
            DrawPanel(draw, windowPos, ImVec2(476.0f, 16.0f), ImVec2(240.0f, 240.0f), "Theme",
                {"Charcoal panels", "Green 147 / 190 / 66", "Original icon font", "730 x 460 layout"});
            DrawPanel(draw, windowPos, ImVec2(226.0f, 332.0f), ImVec2(240.0f, 114.0f), "Selected",
                {selected});
            DrawPanel(draw, windowPos, ImVec2(476.0f, 272.0f), ImVec2(240.0f, 174.0f), "Logging",
                {"Lifecycle", "Hook / renderer", "Input / UI", "File logging"});
            break;
        }
    }

    void TutonesMenu::Render() noexcept
    {
        if (!Input::Get().IsMenuOpen())
            return;

        if (!EnsureOriginalFonts())
            return;

        ProcessInput();

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const ImVec2 center{
            viewport->Pos.x + viewport->Size.x * 0.5f,
            viewport->Pos.y + viewport->Size.y * 0.5f,
        };

        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(MenuWidth, MenuHeight), ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(18.0f / 255.0f, 18.0f / 255.0f, 20.0f / 255.0f, 1.0f));

        constexpr ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoNavInputs |
            ImGuiWindowFlags_NoNavFocus;

        if (ImGui::Begin("Tutones Menu##original", nullptr, flags))
        {
            const ImVec2 pos = ImGui::GetWindowPos();
            const ImVec2 size = ImGui::GetWindowSize();
            auto* draw = ImGui::GetWindowDrawList();

            draw->AddRectFilled(
                pos,
                ImVec2(pos.x + LeftPanelWidth, pos.y + size.y),
                Color(24, 24, 26),
                4.0f);

            draw->AddLine(Offset(pos, LeftPanelWidth, 2.0f), Offset(pos, LeftPanelWidth, size.y - 2.0f), Color(255, 255, 255, 8));
            draw->AddLine(Offset(pos, IconRailWidth, 2.0f), Offset(pos, IconRailWidth, size.y - 2.0f), Color(255, 255, 255, 8));
            draw->AddLine(Offset(pos, 2.0f, 47.0f), Offset(pos, IconRailWidth, 47.0f), Color(255, 255, 255, 8));
            draw->AddLine(Offset(pos, 63.0f, 47.0f), Offset(pos, 195.0f, 47.0f), Color(255, 255, 255, 8));

            draw->AddText(
                FontOrDefault(g_Logo),
                21.0f,
                Offset(pos, 14.0f, 12.0f),
                ImGui::GetColorU32(Accent),
                "A");

            draw->AddText(
                FontOrDefault(g_TabTitleIcon),
                18.0f,
                Offset(pos, 65.0f, 14.0f),
                ImGui::GetColorU32(Accent),
                Categories[m_Category].icon);

            draw->AddText(
                FontOrDefault(g_TabTitle),
                19.0f,
                Offset(pos, 93.0f, 15.0f),
                Color(255, 255, 255),
                Categories[m_Category].name);

            draw->AddRect(
                Offset(pos, 1.0f, 1.0f),
                ImVec2(pos.x + size.x - 1.0f, pos.y + size.y - 1.0f),
                Color(255, 255, 255, 8),
                4.0f);

            RenderNavigationRail();
            RenderCategoryRail();
            RenderContent();
        }

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
    }
}
