    }

    void TutonesMenu::RenderContent() noexcept
    {
        const ImVec2 windowPos = ImGui::GetWindowPos();
        auto* draw = ImGui::GetWindowDrawList();
        const char* selected = Categories[m_Category].items[m_Item];

        switch (m_Category)
        {
        case 0:
            DrawPanel(draw, windowPos, ImVec2(226.0f, 16.0f), ImVec2(240.0f, 300.0f), "Tutones Menu", {"Player options", "Weapons runtime", "Vehicle customization", "World controls", "Menu settings"});
            DrawPanel(draw, windowPos, ImVec2(476.0f, 16.0f), ImVec2(240.0f, 240.0f), "Controls", {"F4  open / close", "Arrow keys  navigate", "Enter  select", "Escape  back", "Mouse  point / click"});
            DrawPanel(draw, windowPos, ImVec2(226.0f, 332.0f), ImVec2(240.0f, 114.0f), "Selected", {selected});
            DrawPanel(draw, windowPos, ImVec2(476.0f, 272.0f), ImVec2(240.0f, 174.0f), "V11", {"Shared script runtime", "Weapon patch capabilities", "Build-verified hook lifecycle", "More sections follow"});
            break;

        case 1:
            RenderPlayerPanel(m_Item);
            break;

        case 2:
            RenderWeaponPanel(m_Item);
            break;

        case 3:
            if (m_Item == 0)
                RenderVehicleGeneralPanel();
            else if (m_Item == 1)
                RenderVehiclePaintPanel();
            else
                RenderVehicleModificationPanel();
            break;

        case 4:
            DrawPanel(draw, windowPos, ImVec2(226.0f, 16.0f), ImVec2(240.0f, 300.0f), "World", {"Time", "Weather", "Blackout", "Environment", "World options"});
            DrawPanel(draw, windowPos, ImVec2(476.0f, 16.0f), ImVec2(240.0f, 240.0f), "Time & Weather", {"Clock", "Freeze time", "Weather type", "Weather transition"});
            DrawPanel(draw, windowPos, ImVec2(226.0f, 332.0f), ImVec2(240.0f, 114.0f), "Selected", {selected});
            DrawPanel(draw, windowPos, ImVec2(476.0f, 272.0f), ImVec2(240.0f, 174.0f), "Teleport", {"Waypoint", "Saved locations", "Interior locations", "Custom coordinates"});
            break;

        default:
            DrawPanel(draw, windowPos, ImVec2(226.0f, 16.0f), ImVec2(240.0f, 300.0f), "Menu", {"F4 toggle", "Mouse control", "Keyboard control", "Menu preferences", "Window behavior"});
            DrawPanel(draw, windowPos, ImVec2(476.0f, 16.0f), ImVec2(240.0f, 240.0f), "Theme", {"Charcoal layout", "Current accent", "Original icon font", "Original 730 x 460 design"});
            DrawPanel(draw, windowPos, ImVec2(226.0f, 332.0f), ImVec2(240.0f, 114.0f), "Selected", {selected});
            DrawPanel(draw, windowPos, ImVec2(476.0f, 272.0f), ImVec2(240.0f, 174.0f), "Controls", {"Arrow keys", "Enter", "Escape", "Mouse", "F4"});
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
        const ImVec2 center{viewport->Pos.x + viewport->Size.x * 0.5f, viewport->Pos.y + viewport->Size.y * 0.5f};
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(MenuWidth, MenuHeight), ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(18.0f / 255.0f, 18.0f / 255.0f, 20.0f / 255.0f, 1.0f));

        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus;
        if (ImGui::Begin("Tutones Menu##original", nullptr, flags))
        {
            const ImVec2 pos = ImGui::GetWindowPos();
            const ImVec2 size = ImGui::GetWindowSize();
            auto* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(pos, ImVec2(pos.x + LeftPanelWidth, pos.y + size.y), Color(24, 24, 26), 4.0f);
            draw->AddLine(Offset(pos, LeftPanelWidth, 2.0f), Offset(pos, LeftPanelWidth, size.y - 2.0f), Color(255, 255, 255, 8));
            draw->AddLine(Offset(pos, IconRailWidth, 2.0f), Offset(pos, IconRailWidth, size.y - 2.0f), Color(255, 255, 255, 8));
            draw->AddLine(Offset(pos, 2.0f, 47.0f), Offset(pos, IconRailWidth, 47.0f), Color(255, 255, 255, 8));
            draw->AddLine(Offset(pos, 63.0f, 47.0f), Offset(pos, 195.0f, 47.0f), Color(255, 255, 255, 8));
            draw->AddText(FontOrDefault(g_Logo), 21.0f, Offset(pos, 14.0f, 12.0f), ImGui::GetColorU32(Accent), "A");
            draw->AddText(FontOrDefault(g_TabTitleIcon), 18.0f, Offset(pos, 65.0f, 14.0f), ImGui::GetColorU32(Accent), Categories[m_Category].icon);
            draw->AddText(FontOrDefault(g_TabTitle), 19.0f, Offset(pos, 93.0f, 15.0f), Color(255, 255, 255), Categories[m_Category].name);
            draw->AddRect(Offset(pos, 1.0f, 1.0f), ImVec2(pos.x + size.x - 1.0f, pos.y + size.y - 1.0f), Color(255, 255, 255, 8), 4.0f);
            RenderNavigationRail();
            RenderCategoryRail();
            RenderContent();
        }

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
    }
}
