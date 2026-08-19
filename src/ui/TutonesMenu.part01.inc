namespace Tutones::UI
{
    namespace
    {
        ImTextureID V11BannerTexture() noexcept
        {
            const auto handle = Render::Renderer::Get().V11BannerTextureId();
            return handle != 0
                ? static_cast<ImTextureID>(handle)
                : static_cast<ImTextureID>(0);
        }

        void DrawCategoryPictogram(ImDrawList* draw, std::size_t category, const ImVec2& center, ImU32 color) noexcept
        {
            constexpr float r = 7.0f;
            switch (category)
            {
            case 0: // Player
                draw->AddCircleFilled(ImVec2(center.x, center.y - 5.0f), 3.8f, color, 20);
                draw->AddLine(ImVec2(center.x - 7.0f, center.y + 7.0f), ImVec2(center.x - 5.0f, center.y + 1.0f), color, 2.0f);
                draw->AddLine(ImVec2(center.x - 5.0f, center.y + 1.0f), ImVec2(center.x + 5.0f, center.y + 1.0f), color, 2.0f);
                draw->AddLine(ImVec2(center.x + 5.0f, center.y + 1.0f), ImVec2(center.x + 7.0f, center.y + 7.0f), color, 2.0f);
                draw->AddLine(ImVec2(center.x - 7.0f, center.y + 7.0f), ImVec2(center.x + 7.0f, center.y + 7.0f), color, 2.0f);
                break;
            case 1: // Weapons / crosshair
                draw->AddCircle(center, r, color, 24, 1.8f);
                draw->AddLine(ImVec2(center.x - 11.0f, center.y), ImVec2(center.x - 3.0f, center.y), color, 1.8f);
                draw->AddLine(ImVec2(center.x + 3.0f, center.y), ImVec2(center.x + 11.0f, center.y), color, 1.8f);
                draw->AddLine(ImVec2(center.x, center.y - 11.0f), ImVec2(center.x, center.y - 3.0f), color, 1.8f);
                draw->AddLine(ImVec2(center.x, center.y + 3.0f), ImVec2(center.x, center.y + 11.0f), color, 1.8f);
                draw->AddCircleFilled(center, 1.6f, color, 12);
                break;
            case 2: // Vehicle
                draw->AddRect(ImVec2(center.x - 9.0f, center.y - 3.0f), ImVec2(center.x + 9.0f, center.y + 5.0f), color, 2.0f, 0, 1.8f);
                draw->AddLine(ImVec2(center.x - 6.0f, center.y - 3.0f), ImVec2(center.x - 3.0f, center.y - 7.0f), color, 1.8f);
                draw->AddLine(ImVec2(center.x - 3.0f, center.y - 7.0f), ImVec2(center.x + 4.0f, center.y - 7.0f), color, 1.8f);
                draw->AddLine(ImVec2(center.x + 4.0f, center.y - 7.0f), ImVec2(center.x + 7.0f, center.y - 3.0f), color, 1.8f);
                draw->AddCircleFilled(ImVec2(center.x - 5.0f, center.y + 6.0f), 2.2f, color, 12);
                draw->AddCircleFilled(ImVec2(center.x + 5.0f, center.y + 6.0f), 2.2f, color, 12);
                break;
            case 3: // Game
                draw->AddRect(ImVec2(center.x - 9.0f, center.y - 5.0f), ImVec2(center.x + 9.0f, center.y + 5.0f), color, 4.0f, 0, 1.8f);
                draw->AddLine(ImVec2(center.x - 5.0f, center.y - 1.0f), ImVec2(center.x - 5.0f, center.y + 3.0f), color, 1.8f);
                draw->AddLine(ImVec2(center.x - 7.0f, center.y + 1.0f), ImVec2(center.x - 3.0f, center.y + 1.0f), color, 1.8f);
                draw->AddCircleFilled(ImVec2(center.x + 4.0f, center.y), 1.6f, color, 10);
                draw->AddCircleFilled(ImVec2(center.x + 7.0f, center.y - 2.5f), 1.4f, color, 10);
                break;
            case 4: // World
                draw->AddCircle(center, 8.5f, color, 28, 1.8f);
                draw->AddLine(ImVec2(center.x - 8.0f, center.y), ImVec2(center.x + 8.0f, center.y), color, 1.4f);
                draw->AddLine(ImVec2(center.x, center.y - 8.0f), ImVec2(center.x, center.y + 8.0f), color, 1.4f);
                draw->AddCircle(center, 4.0f, color, 20, 1.0f);
                break;
            case 5: // Recovery / diamond
            {
                const ImVec2 points[4] = {
                    ImVec2(center.x, center.y - 9.0f),
                    ImVec2(center.x + 8.0f, center.y),
                    ImVec2(center.x, center.y + 9.0f),
                    ImVec2(center.x - 8.0f, center.y),
                };
                draw->AddPolyline(points, 4, color, ImDrawFlags_Closed, 1.8f);
                draw->AddLine(ImVec2(center.x - 8.0f, center.y), ImVec2(center.x + 8.0f, center.y), color, 1.0f);
                draw->AddLine(ImVec2(center.x, center.y - 9.0f), ImVec2(center.x, center.y + 9.0f), color, 1.0f);
                break;
            }
            case 6: // Network / Wi-Fi
                for (int arc = 0; arc < 3; ++arc)
                {
                    draw->PathArcTo(center, 4.0f + static_cast<float>(arc) * 4.0f, -2.75f, -0.39f, 16);
                    draw->PathStroke(color, 0, 1.8f);
                }
                draw->AddCircleFilled(ImVec2(center.x, center.y + 5.5f), 1.8f, color, 10);
                break;
            case 7: // Protections / shield
            {
                const ImVec2 points[5] = {
                    ImVec2(center.x, center.y - 9.0f),
                    ImVec2(center.x + 8.0f, center.y - 5.0f),
                    ImVec2(center.x + 6.0f, center.y + 4.0f),
                    ImVec2(center.x, center.y + 9.0f),
                    ImVec2(center.x - 6.0f, center.y + 4.0f),
                };
                draw->AddPolyline(points, 5, color, ImDrawFlags_Closed, 1.8f);
                draw->AddLine(ImVec2(center.x - 8.0f, center.y - 5.0f), ImVec2(center.x, center.y - 9.0f), color, 1.8f);
                break;
            }
            default: // Menu settings / gear
                draw->AddCircle(center, 6.0f, color, 24, 1.8f);
                draw->AddCircle(center, 2.0f, color, 16, 1.6f);
                draw->AddLine(ImVec2(center.x, center.y - 11.0f), ImVec2(center.x, center.y - 7.0f), color, 2.0f);
                draw->AddLine(ImVec2(center.x, center.y + 7.0f), ImVec2(center.x, center.y + 11.0f), color, 2.0f);
                draw->AddLine(ImVec2(center.x - 11.0f, center.y), ImVec2(center.x - 7.0f, center.y), color, 2.0f);
                draw->AddLine(ImVec2(center.x + 7.0f, center.y), ImVec2(center.x + 11.0f, center.y), color, 2.0f);
                break;
            }
        }

        bool CategoryButton(const char* id, std::size_t category, const char* label, bool selected, const ImVec2& localPos) noexcept
        {
            ImGui::SetCursorPos(localPos);
            const ImVec2 min = ImGui::GetCursorScreenPos();
            const ImVec2 size{V11Theme::CategoryRailWidth - 20.0f, 38.0f};
            const ImVec2 max{min.x + size.x, min.y + size.y};
            const ImGuiID itemId = ImGui::GetID(id);

            const bool pressed = ImGui::InvisibleButton(id, size);
            const bool hovered = ImGui::IsItemHovered();
            auto& anim = g_TabAnimations[itemId];
            anim.elementOpacity = Animate(anim.elementOpacity, selected ? 0.34f : hovered ? 0.14f : 0.02f, 0.10f);
            anim.rectOpacity = Animate(anim.rectOpacity, selected ? 1.0f : hovered ? 0.50f : 0.18f, 0.12f);
            anim.textOpacity = Animate(anim.textOpacity, selected ? 1.0f : hovered ? 0.92f : 0.70f, 0.10f);

            auto* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(min, max, ImGui::GetColorU32(ImVec4(Accent.x, Accent.y, Accent.z, anim.elementOpacity)), 2.0f);
            if (selected || hovered)
                draw->AddRect(min, max, ImGui::GetColorU32(ImVec4(Accent.x, Accent.y, Accent.z, anim.rectOpacity * 0.80f)), 2.0f, 0, 1.0f);

            const ImU32 iconColor = ImGui::GetColorU32(ImVec4(Accent.x, Accent.y, Accent.z, anim.textOpacity));
            DrawCategoryPictogram(draw, category, ImVec2(min.x + 22.0f, (min.y + max.y) * 0.5f), iconColor);
            draw->AddText(FontOrDefault(g_Bold), 14.0f, ImVec2(min.x + 42.0f, min.y + 10.0f), ImGui::GetColorU32(ImVec4(0.95f, 0.97f, 1.0f, anim.textOpacity)), label);
            draw->AddRectFilled(ImVec2(min.x, min.y + 3.0f), ImVec2(min.x + 3.0f, max.y - 3.0f), ImGui::GetColorU32(ImVec4(Accent.x, Accent.y, Accent.z, anim.rectOpacity)), 2.0f);
            if (selected)
            {
                const ImU32 chevron = ImGui::GetColorU32(ImVec4(Accent.x, Accent.y, Accent.z, 0.86f));
                draw->AddTriangleFilled(
                    ImVec2(max.x - 1.0f, min.y + 6.0f),
                    ImVec2(max.x + 8.0f, (min.y + max.y) * 0.5f),
                    ImVec2(max.x - 1.0f, max.y - 6.0f),
                    chevron);
            }
            return pressed;
        }

        bool SubtabButton(const char* id, const char* icon, const char* label, bool selected, const ImVec2& localPos) noexcept
        {
            ImGui::SetCursorPos(localPos);
            const ImVec2 min = ImGui::GetCursorScreenPos();
            const ImVec2 size{V11Theme::SubtabRailWidth - 18.0f, 36.0f};
            const ImVec2 max{min.x + size.x, min.y + size.y};
            const ImGuiID itemId = ImGui::GetID(id);

            const bool pressed = ImGui::InvisibleButton(id, size);
            const bool hovered = ImGui::IsItemHovered();
            auto& anim = g_SubtabAnimations[itemId];
            anim.elementOpacity = Animate(anim.elementOpacity, selected ? 0.28f : hovered ? 0.10f : 0.0f, 0.10f);
            anim.rectOpacity = Animate(anim.rectOpacity, selected ? 1.0f : hovered ? 0.42f : 0.0f, 0.14f);
            anim.textOpacity = Animate(anim.textOpacity, selected ? 1.0f : hovered ? 0.90f : 0.62f, 0.10f);

            auto* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(min, max, ImGui::GetColorU32(ImVec4(Accent.x, Accent.y, Accent.z, anim.elementOpacity)), 2.0f);
            if (selected)
                draw->AddRect(min, max, ImGui::GetColorU32(ImVec4(Accent.x, Accent.y, Accent.z, 0.72f)), 2.0f, 0, 1.0f);
            draw->AddText(FontOrDefault(g_TabIcons), 16.0f, ImVec2(min.x + 12.0f, min.y + 9.0f), ImGui::GetColorU32(ImVec4(Accent.x, Accent.y, Accent.z, anim.textOpacity)), icon ? icon : "I");
            draw->AddText(FontOrDefault(g_Medium), 14.0f, ImVec2(min.x + 40.0f, min.y + 9.0f), ImGui::GetColorU32(ImVec4(0.95f, 0.97f, 1.0f, anim.textOpacity)), label);
            draw->AddRectFilled(ImVec2(max.x - 3.0f, min.y + 6.0f), ImVec2(max.x, max.y - 6.0f), ImGui::GetColorU32(ImVec4(Accent.x, Accent.y, Accent.z, anim.rectOpacity)), 2.0f);
            if (selected)
            {
                const ImU32 chevron = ImGui::GetColorU32(ImVec4(Accent.x, Accent.y, Accent.z, 0.82f));
                draw->AddTriangleFilled(
                    ImVec2(max.x - 1.0f, min.y + 7.0f),
                    ImVec2(max.x + 7.0f, (min.y + max.y) * 0.5f),
                    ImVec2(max.x - 1.0f, max.y - 7.0f),
                    chevron);
            }
            return pressed;
        }

        void DrawPanel(ImDrawList* draw, const ImVec2& windowPos, const ImVec2& localPos, const ImVec2& size, const char* title, std::initializer_list<const char*> lines) noexcept
        {
            const ImVec2 min = Offset(windowPos, localPos.x, localPos.y);
            const ImVec2 max{min.x + size.x, min.y + size.y};
            draw->AddRectFilled(min, max, ImGui::GetColorU32(V11Theme::PanelBg), 3.0f);
            draw->AddRect(min, max, ImGui::GetColorU32(V11Theme::PanelBorder), 3.0f);
            draw->AddLine(Offset(min, 1.0f, 32.0f), ImVec2(max.x - 1.0f, min.y + 32.0f), ImGui::GetColorU32(ImVec4(Accent.x, Accent.y, Accent.z, 0.20f)));
            draw->AddText(FontOrDefault(g_Bold), 15.0f, Offset(min, 16.0f, 9.0f), ImGui::GetColorU32(Accent), title);

            float y = min.y + 49.0f;
            for (const char* line : lines)
            {
                if (!line || line[0] == '\0')
                {
                    y += 10.0f;
                    continue;
                }
                draw->AddText(FontOrDefault(g_Medium), 14.0f, ImVec2(min.x + 16.0f, y), Color(191, 199, 214), line);
                y += 22.0f;
                if (y > max.y - 18.0f)
                    break;
            }
        }

        void DrawAngularFrame(ImDrawList* draw, const ImVec2& min, const ImVec2& max, float glow) noexcept
        {
            const ImU32 border = ImGui::GetColorU32(ImVec4(Accent.x, Accent.y, Accent.z, glow));
            const ImU32 dim = ImGui::GetColorU32(ImVec4(0.22f, 0.28f, 0.36f, 0.70f));
            draw->AddRect(min, max, dim, 0.0f, 0, 2.0f);
            draw->AddLine(ImVec2(min.x + 18.0f, min.y), ImVec2(max.x - 18.0f, min.y), border, 2.0f);
            draw->AddLine(ImVec2(min.x + 18.0f, max.y), ImVec2(max.x - 18.0f, max.y), border, 2.0f);
            draw->AddLine(ImVec2(min.x, min.y + 18.0f), ImVec2(min.x, max.y - 18.0f), border, 2.0f);
            draw->AddLine(ImVec2(max.x, min.y + 18.0f), ImVec2(max.x, max.y - 18.0f), border, 2.0f);
        }

        bool CurrentFeatureBacked(std::size_t category, std::size_t item) noexcept
        {
            if (category == 3 && item == 1)
                return false;
            return Categories[category].backendImplemented;
        }

        void RenderStatusPanel(std::size_t category, std::size_t item) noexcept
        {
            const ImVec2 pos = ImGui::GetWindowPos();
            const ImVec2 min = Offset(pos, V11Theme::StatusX, V11Theme::HeaderHeight + 12.0f);
            const ImVec2 max = Offset(min, V11Theme::StatusWidth, V11Theme::BodyHeight - 24.0f);
            auto* draw = ImGui::GetWindowDrawList();

            draw->AddRectFilled(min, max, ImGui::GetColorU32(V11Theme::PanelBg), 3.0f);
            DrawAngularFrame(draw, min, max, 0.48f);
            draw->AddText(FontOrDefault(g_Bold), 16.0f, Offset(min, 18.0f, 16.0f), ImGui::GetColorU32(Accent), "TUTONES V11");
            draw->AddLine(Offset(min, 16.0f, 45.0f), ImVec2(max.x - 16.0f, min.y + 45.0f), ImGui::GetColorU32(V11Theme::Separator));

            const char* labels[] = {"GTA V ENHANCED", "RENDERER  D3D12", "BUILD  V11", "CATEGORY", "OPTION", "STATUS"};
            const char* values[] = {"", "", "", Categories[category].name, Categories[category].items[item], CurrentFeatureBacked(category, item) ? "BACKEND WIRED" : "UI / PENDING"};

            float y = min.y + 64.0f;
            for (int i = 0; i < 6; ++i)
            {
                draw->AddText(FontOrDefault(g_TabIcons), 15.0f, ImVec2(min.x + 17.0f, y), ImGui::GetColorU32(Accent), i == 0 ? "D" : i == 1 ? "E" : i == 2 ? "G" : i == 3 ? Categories[category].icon : i == 4 ? "I" : "H");
                draw->AddText(FontOrDefault(g_Medium), 13.0f, ImVec2(min.x + 46.0f, y), Color(214, 222, 236), labels[i]);
                if (values[i][0] != '\0')
                    draw->AddText(FontOrDefault(g_Bold), 12.0f, ImVec2(min.x + 46.0f, y + 18.0f), i == 5 && CurrentFeatureBacked(category, item) ? Color(43, 231, 111) : ImGui::GetColorU32(V11Theme::MutedText), values[i]);
                y += values[i][0] != '\0' ? 49.0f : 32.0f;
            }
        }

        void DrawTopBanner(ImDrawList* draw, const ImVec2& pos, float width) noexcept
        {
            const ImVec2 min = pos;
            const ImVec2 max{pos.x + width, pos.y + V11Theme::HeaderHeight};
            if (const ImTextureID texture = V11BannerTexture())
            {
                draw->AddImage(texture, min, max, ImVec2(0.0f, 0.045f), ImVec2(1.0f, 0.605f));
                return;
            }

            draw->AddRectFilledMultiColor(min, max,
                Color(1, 4, 9), Color(1, 4, 9), Color(2, 16, 36), Color(2, 16, 36));
            DrawAngularFrame(draw, Offset(min, 8.0f, 8.0f), Offset(max, -8.0f, -8.0f), 0.78f);
            draw->AddText(FontOrDefault(g_Bold), 46.0f, ImVec2(pos.x + width * 0.36f, pos.y + 62.0f), ImGui::GetColorU32(Accent), "TUTONES");
            draw->AddText(FontOrDefault(g_Bold), 46.0f, ImVec2(pos.x + width * 0.54f, pos.y + 62.0f), Color(242, 245, 250), "MENU");
            draw->AddText(FontOrDefault(g_Medium), 13.0f, ImVec2(pos.x + width * 0.455f, pos.y + 122.0f), Color(125, 149, 183), "GTA V ENHANCED  //  V11");
        }

        void DrawDescriptionBanner(ImDrawList* draw, const ImVec2& pos, float width, std::size_t category, std::size_t item) noexcept
        {
            const float footerY = pos.y + V11Theme::FooterY;
            const ImVec2 min{pos.x, footerY};
            const ImVec2 max{pos.x + width, footerY + V11Theme::FooterHeight};
            const char* description = Categories[category].descriptions[item];
            if (!description)
                description = "No description is available for this option yet.";

            if (const ImTextureID texture = V11BannerTexture())
            {
                draw->AddImage(texture, min, max, ImVec2(0.0f, 0.635f), ImVec2(1.0f, 0.965f));

                // The approved artwork contains placeholder copy. Keep its frame, info badge,
                // DESCRIPTION heading, and TT emblem, but replace only the body copy at runtime.
                const ImVec2 textMaskMin{min.x + 220.0f, min.y + 54.0f};
                const ImVec2 textMaskMax{max.x - 205.0f, max.y - 20.0f};
                draw->AddRectFilled(textMaskMin, textMaskMax, Color(2, 5, 10, 242), 2.0f);
                draw->AddText(
                    FontOrDefault(g_Medium),
                    15.0f,
                    ImVec2(textMaskMin.x + 8.0f, textMaskMin.y + 7.0f),
                    Color(235, 239, 247),
                    description,
                    nullptr,
                    textMaskMax.x - textMaskMin.x - 16.0f);
                return;
            }

            draw->AddRectFilled(min, max, ImGui::GetColorU32(V11Theme::FooterBg));
            DrawAngularFrame(draw, Offset(min, 8.0f, 8.0f), Offset(max, -8.0f, -8.0f), 0.66f);

            const ImVec2 iconCenter{min.x + 85.0f, min.y + V11Theme::FooterHeight * 0.50f};
            draw->AddCircle(iconCenter, 45.0f, Color(2, 108, 249, 145), 64, 4.0f);
            draw->AddCircle(iconCenter, 34.0f, Color(2, 108, 249, 85), 64, 2.0f);
            draw->AddText(FontOrDefault(g_Bold), 44.0f, ImVec2(iconCenter.x - 10.0f, iconCenter.y - 29.0f), ImGui::GetColorU32(Accent), "i");
            draw->AddLine(ImVec2(min.x + 145.0f, min.y + 25.0f), ImVec2(min.x + 145.0f, max.y - 25.0f), Color(2, 108, 249, 180), 2.0f);
            draw->AddText(FontOrDefault(g_Bold), 24.0f, ImVec2(min.x + 172.0f, min.y + 24.0f), ImGui::GetColorU32(Accent), "DESCRIPTION");
            draw->AddText(FontOrDefault(g_Medium), 15.0f, ImVec2(min.x + 174.0f, min.y + 63.0f), Color(226, 232, 242), description);

            const ImVec2 emblemCenter{max.x - 80.0f, min.y + V11Theme::FooterHeight * 0.50f};
            draw->AddText(FontOrDefault(g_Logo), 40.0f, ImVec2(emblemCenter.x - 16.0f, emblemCenter.y - 27.0f), ImGui::GetColorU32(Accent), "A");
            draw->AddText(FontOrDefault(g_Bold), 11.0f, ImVec2(emblemCenter.x - 24.0f, emblemCenter.y + 24.0f), Color(137, 162, 195), "TUTONES");
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
        TUTONES_LOG_DEBUG("ui", "Tutones V11 visual-shell navigation reset");
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
            Input::Get().SetMenuOpen(false);

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
            const ImVec2 localPos{V11Theme::CategoryRailX + 10.0f, V11Theme::HeaderHeight + 12.0f + static_cast<float>(i) * 42.0f};
            if (CategoryButton("##v11_category", i, Categories[i].name, i == m_Category, localPos))
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
        draw->AddText(FontOrDefault(g_SubtabTitle), 14.0f, Offset(windowPos, V11Theme::SubtabRailX + 12.0f, V11Theme::HeaderHeight + 16.0f), ImGui::GetColorU32(ImVec4(V11Theme::MutedText.x, V11Theme::MutedText.y, V11Theme::MutedText.z, 0.82f)), "OPTIONS");

        for (std::size_t i = 0; i < Categories[m_Category].itemCount; ++i)
        {
            ImGui::PushID(static_cast<int>(i));
            const ImVec2 localPos{V11Theme::SubtabRailX + 8.0f, V11Theme::HeaderHeight + 44.0f + static_cast<float>(i) * 40.0f};
            if (SubtabButton("##v11_subtab", Categories[m_Category].itemIcons[i], Categories[m_Category].items[i], i == m_Item, localPos))
                m_Item = i;
            ImGui::PopID();
        }
    }
}
