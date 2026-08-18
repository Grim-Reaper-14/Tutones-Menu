            anim.elementOpacity = Animate(anim.elementOpacity, selected ? 0.04f : hovered ? 0.01f : 0.0f, 0.07f);
            anim.rectOpacity = Animate(anim.rectOpacity, selected ? 1.0f : 0.0f, 0.15f);
            anim.textOpacity = Animate(anim.textOpacity, selected ? 1.0f : hovered ? 0.5f : 0.3f, 0.07f);

            auto* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(min, max, ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, anim.elementOpacity)), 3.0f);

            ImFont* font = FontOrDefault(g_TabIcons);
            const ImVec2 textSize = font->CalcTextSizeA(15.0f, FLT_MAX, 0.0f, icon);
            draw->AddText(font, 15.0f, ImVec2(min.x + (size.x - textSize.x) * 0.5f, min.y + (size.y - textSize.y) * 0.5f), ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, anim.textOpacity)), icon);
            draw->AddRectFilled(ImVec2(max.x + 4.0f, min.y + 6.0f), ImVec2(max.x + 8.0f, max.y - 6.0f), ImGui::GetColorU32(ImVec4(Accent.x, Accent.y, Accent.z, anim.rectOpacity)), 4.0f);
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
            auto& anim = g_SubtabAnimations[itemId];
            anim.elementOpacity = Animate(anim.elementOpacity, selected ? 0.04f : 0.0f, 0.09f);
            anim.rectOpacity = Animate(anim.rectOpacity, selected ? 1.0f : 0.0f, 0.20f);
            anim.textOpacity = Animate(anim.textOpacity, selected ? 1.0f : 0.3f, 0.07f);

            auto* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(min, max, ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, anim.elementOpacity)), 3.0f);
            ImFont* font = FontOrDefault(g_Medium);
            const ImVec2 textSize = font->CalcTextSizeA(15.0f, FLT_MAX, 0.0f, label);
            draw->AddText(font, 15.0f, ImVec2(min.x + 15.0f, min.y + (size.y - textSize.y) * 0.5f), ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, anim.textOpacity)), label);
            draw->AddRectFilled(ImVec2(max.x + 5.0f, min.y + 9.0f), ImVec2(max.x + 8.0f, max.y - 9.0f), ImGui::GetColorU32(ImVec4(Accent.x, Accent.y, Accent.z, anim.rectOpacity)), 4.0f);
            return pressed;
        }

        void DrawPanel(ImDrawList* draw, const ImVec2& windowPos, const ImVec2& localPos, const ImVec2& size, const char* title, std::initializer_list<const char*> lines) noexcept
        {
            const ImVec2 min = Offset(windowPos, localPos.x, localPos.y);
            const ImVec2 max{min.x + size.x, min.y + size.y};
            draw->AddRectFilled(min, max, Color(24, 24, 26), 4.0f);
            draw->AddRect(min, max, Color(255, 255, 255, 8), 4.0f);
            draw->AddLine(Offset(min, 1.0f, 32.0f), ImVec2(max.x - 1.0f, min.y + 32.0f), Color(255, 255, 255, 8));
            draw->AddText(FontOrDefault(g_Bold), 15.0f, Offset(min, 16.0f, 9.0f), ImGui::GetColorU32(Accent), title);

            float y = min.y + 49.0f;
            for (const char* line : lines)
            {
                if (!line || line[0] == '\0')
                {
                    y += 10.0f;
                    continue;
                }
                draw->AddText(FontOrDefault(g_Medium), 14.0f, ImVec2(min.x + 16.0f, y), Color(188, 188, 193), line);
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
        TUTONES_LOG_DEBUG("ui", "Tutones navigation reset to menu layout");
    }

    void TutonesMenu::ProcessInput() noexcept
    {
        const auto actions = Input::Get().ConsumePendingActions();
        if (actions == 0)
            return;

        const std::size_t itemCount = Categories[m_Category].itemCount;
        if ((actions & ToMask(InputAction::Up)) != 0)
            m_Item = (m_Item == 0) ? itemCount - 1 : m_Item - 1;
        if ((actions & ToMask(InputAction::Down)) != 0)
            m_Item = (m_Item + 1) % itemCount;
        if ((actions & ToMask(InputAction::Left)) != 0)
        {
            m_Category = (m_Category == 0) ? Categories.size() - 1 : m_Category - 1;
            m_Item = 0;
        }
        if ((actions & ToMask(InputAction::Right)) != 0)
        {
            m_Category = (m_Category + 1) % Categories.size();
            m_Item = 0;
        }
        if ((actions & ToMask(InputAction::Back)) != 0)
        {
            if (m_Category != 0)
            {
                m_Category = 0;
                m_Item = 0;
            }
            else
                Input::Get().SetMenuOpen(false);
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
            }
            ImGui::PopID();
        }
    }

    void TutonesMenu::RenderCategoryRail() noexcept
    {
        const ImVec2 windowPos = ImGui::GetWindowPos();
        auto* draw = ImGui::GetWindowDrawList();
        draw->AddText(FontOrDefault(g_SubtabTitle), 15.0f, Offset(windowPos, 72.0f, 60.0f), Color(255, 255, 255, 102), "MAIN");

        for (std::size_t i = 0; i < Categories[m_Category].itemCount; ++i)
        {
            ImGui::PushID(static_cast<int>(i));
            const ImVec2 localPos{57.0f, 86.0f + static_cast<float>(i) * 35.0f};
            if (OriginalSubtabButton("##original_subtab", Categories[m_Category].items[i], i == m_Item, localPos))
                m_Item = i;
            ImGui::PopID();
        }

        draw->AddText(FontOrDefault(g_Medium), 12.0f, Offset(windowPos, 64.0f, 397.0f), Color(110, 110, 116), "ARROWS  MOVE");
        draw->AddText(FontOrDefault(g_Medium), 12.0f, Offset(windowPos, 64.0f, 416.0f), Color(110, 110, 116), "ENTER  SELECT");
        draw->AddText(FontOrDefault(g_Medium), 12.0f, Offset(windowPos, 64.0f, 435.0f), Color(110, 110, 116), "ESC  BACK");
