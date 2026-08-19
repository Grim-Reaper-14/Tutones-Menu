namespace Tutones::UI
{
    void TutonesMenu::RenderContent() noexcept
    {
        const ImVec2 windowPos = ImGui::GetWindowPos();
        auto* draw = ImGui::GetWindowDrawList();
        const char* selected = Categories[m_Category].items[m_Item];

        switch (m_Category)
        {
        case 0:
            if (m_Item == 0)
                RenderPlayerPanel(0);
            else if (m_Item == 1)
                RenderPlayerOnlinePanel();
            else
                RenderPlayerPanel(m_Item - 1);
            break;

        case 1:
            RenderWeaponPanel(m_Item);
            break;

        case 2:
            if (m_Item == 0)
                RenderVehicleGeneralPanel();
            else if (m_Item == 1)
                RenderVehiclePaintPanel();
            else if (m_Item == 2)
                RenderVehicleModificationPanel();
            else
                RenderPersonalVehiclePanel();
            break;

        case 3:
            RenderGamePanel(m_Item);
            break;

        case 4:
            DrawPanel(draw, windowPos, ImVec2(226.0f, 16.0f), ImVec2(490.0f, 430.0f), "WORLD", {selected, "Visual shell is wired.", "World backend work is still pending."});
            break;

        case 5:
            RenderRecoveryPanel(m_Item);
            break;

        case 6:
            DrawPanel(draw, windowPos, ImVec2(226.0f, 16.0f), ImVec2(490.0f, 430.0f), "NETWORK", {selected, "Session transitions stay under Game.", "Additional network tooling is pending."});
            break;

        case 7:
            DrawPanel(draw, windowPos, ImVec2(226.0f, 16.0f), ImVec2(490.0f, 430.0f), "PROTECTIONS", {selected, "Protection UI target only.", "Network/script event protection backends are pending."});
            break;

        default:
            DrawPanel(draw, windowPos, ImVec2(226.0f, 16.0f), ImVec2(490.0f, 430.0f), "MENU SETTINGS", {selected, "Tutone Menu blue visual shell", "Banner-style header + description frame", "Keyboard / mouse input", "Theme persistence wiring remains separate."});
            break;
        }
    }

    void TutonesMenu::Render() noexcept
    {
        if (!Input::Get().IsMenuOpen())
            return;
        if (!EnsureOriginalFonts())
            return;

        ApplyV11Style();
        ProcessInput();

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const ImVec2 center{viewport->Pos.x + viewport->Size.x * 0.5f, viewport->Pos.y + viewport->Size.y * 0.5f};
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(V11Theme::MenuWidth, V11Theme::MenuHeight), ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, V11Theme::WindowBg);

        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus;
        if (ImGui::Begin("Tutone Menu##v11", nullptr, flags))
        {
            const ImVec2 pos = ImGui::GetWindowPos();
            auto* draw = ImGui::GetWindowDrawList();
            const float footerY = pos.y + V11Theme::FooterY;

            draw->AddRectFilled(pos, ImVec2(pos.x + V11Theme::MenuWidth, pos.y + V11Theme::MenuHeight), ImGui::GetColorU32(V11Theme::WindowBg));
            DrawTopBanner(draw, pos, V11Theme::MenuWidth);
            draw->AddRectFilled(ImVec2(pos.x, pos.y + V11Theme::HeaderHeight), ImVec2(pos.x + V11Theme::MenuWidth, footerY), ImGui::GetColorU32(V11Theme::BodyBg));

            draw->AddRectFilled(
                ImVec2(pos.x + V11Theme::CategoryRailX, pos.y + V11Theme::HeaderHeight + 8.0f),
                ImVec2(pos.x + V11Theme::CategoryRailX + V11Theme::CategoryRailWidth, footerY - 8.0f),
                ImGui::GetColorU32(V11Theme::RailBg), 3.0f);
            draw->AddRectFilled(
                ImVec2(pos.x + V11Theme::SubtabRailX, pos.y + V11Theme::HeaderHeight + 8.0f),
                ImVec2(pos.x + V11Theme::SubtabRailX + V11Theme::SubtabRailWidth, footerY - 8.0f),
                ImGui::GetColorU32(V11Theme::RailBg), 3.0f);

            BeginV11DescriptionFrame(Categories[m_Category].descriptions[m_Item]);

            ImGui::SetCursorPos(ImVec2(V11Theme::ContentHostX, V11Theme::HeaderHeight));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            constexpr ImGuiWindowFlags bodyFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus;
            if (ImGui::BeginChild("##v11_body", ImVec2(V11Theme::ContentHostWidth, V11Theme::BodyHeight), false, bodyFlags))
                RenderContent();
            ImGui::EndChild();
            ImGui::PopStyleColor();

            RenderNavigationRail();
            RenderCategoryRail();
            RenderStatusPanel(m_Category, m_Item);
            DrawDescriptionBanner(draw, pos, V11Theme::MenuWidth, m_Category, m_Item);
        }

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
    }
}
