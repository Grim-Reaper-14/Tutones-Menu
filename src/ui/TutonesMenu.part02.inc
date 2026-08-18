
    void TutonesMenu::RenderContent() noexcept
    {
        const ImVec2 windowPos = ImGui::GetWindowPos();
        auto* draw = ImGui::GetWindowDrawList();
        const char* selected = Categories[m_Category].items[m_Item];

        switch (m_Category)
        {
        case 0:
            DrawPanel(draw, windowPos, ImVec2(226.0f, 16.0f), ImVec2(240.0f, 300.0f), "Tutones Menu", {"Player + Online options", "Weapons runtime", "Vehicle customization", "Game session transitions", "World controls"});
            DrawPanel(draw, windowPos, ImVec2(476.0f, 16.0f), ImVec2(240.0f, 240.0f), "Controls", {"F4  open / close", "Arrow keys  navigate", "Enter  select", "Escape  back", "Mouse  point / click"});
            DrawPanel(draw, windowPos, ImVec2(226.0f, 332.0f), ImVec2(240.0f, 114.0f), "Selected", {selected});
            DrawPanel(draw, windowPos, ImVec2(476.0f, 272.0f), ImVec2(240.0f, 174.0f), "V11", {"Shared script runtime", "Off Radar broadcast globals", "Session transition backend", "Creator still unimplemented"});
            break;

        case 1:
            if (m_Item == 0)
                RenderPlayerPanel(0);
            else if (m_Item == 1)
                RenderPlayerOnlinePanel();
            else
                RenderPlayerPanel(m_Item - 1);
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
            RenderGamePanel(m_Item);
            break;

        case 5:
            DrawPanel(draw, windowPos, ImVec2(226.0f, 16.0f), ImVec2(240.0f, 300.0f), "World", {"Time", "Weather", "Blackout", "Environment", "World options"});
            DrawPanel(draw, windowPos, ImVec2(476.0f, 16.0f), ImVec2(240.0f, 240.0f), "Time & Weather", {"Clock", "Freeze time", "Weather type", "Weather transition"});
            DrawPanel(draw, windowPos, ImVec2(226.0f, 332.0f), ImVec2(240.0f, 114.0f), "Selected", {selected});
            DrawPanel(draw, windowPos, ImVec2(476.0f, 272.0f), ImVec2(240.0f, 174.0f), "Teleport", {"Waypoint", "Saved locations", "Interior locations", "Custom coordinates"});
            break;

        default:
            DrawPanel(draw, windowPos, ImVec2(226.0f, 16.0f), ImVec2(240.0f, 300.0f), "Menu", {"F4 toggle", "Mouse control", "Keyboard control", "Menu preferences", "Window behavior"});
            DrawPanel(draw, windowPos, ImVec2(476.0f, 16.0f), ImVec2(240.0f, 240.0f), "Theme", {"V11 blue shell", "Embedded Tutones fonts", "Header + footer chrome", "730 x 540 layout"});
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

        ApplyV11Style();
        ProcessInput();

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const ImVec2 center{viewport->Pos.x + viewport->Size.x * 0.5f, viewport->Pos.y + viewport->Size.y * 0.5f};
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(MenuWidth, MenuHeight), ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 7.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, V11Theme::WindowBg);

        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus;
        if (ImGui::Begin("Tutones Menu##v11", nullptr, flags))
        {
            const ImVec2 pos = ImGui::GetWindowPos();
            const ImVec2 size = ImGui::GetWindowSize();
            const float footerY = pos.y + V11Theme::FooterY;
            auto* draw = ImGui::GetWindowDrawList();

            draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), ImGui::GetColorU32(V11Theme::WindowBg), 7.0f);
            draw->AddRectFilledMultiColor(
                pos,
                ImVec2(pos.x + size.x, pos.y + V11Theme::HeaderHeight),
                ImGui::GetColorU32(V11Theme::HeaderTop),
                ImGui::GetColorU32(V11Theme::HeaderTop),
                ImGui::GetColorU32(V11Theme::HeaderBottom),
                ImGui::GetColorU32(V11Theme::HeaderBottom));
            draw->AddRectFilled(
                ImVec2(pos.x, pos.y + V11Theme::HeaderHeight),
                ImVec2(pos.x + LeftPanelWidth, footerY),
                ImGui::GetColorU32(V11Theme::SidebarBg));
            draw->AddRectFilled(
                ImVec2(pos.x, pos.y + V11Theme::HeaderHeight),
                ImVec2(pos.x + IconRailWidth, footerY),
                ImGui::GetColorU32(V11Theme::IconRailBg));
            draw->AddRectFilled(
                ImVec2(pos.x, footerY),
                ImVec2(pos.x + size.x, pos.y + size.y),
                ImGui::GetColorU32(V11Theme::FooterBg));

            draw->AddLine(
                ImVec2(pos.x, pos.y + V11Theme::HeaderHeight),
                ImVec2(pos.x + size.x, pos.y + V11Theme::HeaderHeight),
                ImGui::GetColorU32(ImVec4(Accent.x, Accent.y, Accent.z, 0.45f)));
            draw->AddLine(
                ImVec2(pos.x, footerY),
                ImVec2(pos.x + size.x, footerY),
                ImGui::GetColorU32(ImVec4(Accent.x, Accent.y, Accent.z, 0.22f)));
            draw->AddLine(
                ImVec2(pos.x + LeftPanelWidth, pos.y + V11Theme::HeaderHeight),
                ImVec2(pos.x + LeftPanelWidth, footerY),
                Color(255, 255, 255, 10));
            draw->AddLine(
                ImVec2(pos.x + IconRailWidth, pos.y + V11Theme::HeaderHeight),
                ImVec2(pos.x + IconRailWidth, footerY),
                Color(255, 255, 255, 8));

            draw->AddText(FontOrDefault(g_Logo), 22.0f, Offset(pos, 14.0f, 17.0f), ImGui::GetColorU32(Accent), "A");
            draw->AddText(FontOrDefault(g_Bold), 17.0f, Offset(pos, 48.0f, 11.0f), Color(245, 248, 255), "TUTONES");
            draw->AddText(FontOrDefault(g_Medium), 11.0f, Offset(pos, 49.0f, 33.0f), ImGui::GetColorU32(V11Theme::MutedText), "MENU  V11");

            draw->AddText(FontOrDefault(g_TabTitleIcon), 18.0f, Offset(pos, 229.0f, 12.0f), ImGui::GetColorU32(Accent), Categories[m_Category].icon);
            draw->AddText(FontOrDefault(g_TabTitle), 19.0f, Offset(pos, 257.0f, 12.0f), Color(255, 255, 255), Categories[m_Category].name);
            draw->AddText(FontOrDefault(g_Medium), 12.0f, Offset(pos, 229.0f, 35.0f), ImGui::GetColorU32(V11Theme::MutedText), Categories[m_Category].descriptions[m_Item]);

            const char* platform = "GTA V ENHANCED";
            const ImVec2 platformSize = FontOrDefault(g_Medium)->CalcTextSizeA(11.0f, FLT_MAX, 0.0f, platform);
            draw->AddText(FontOrDefault(g_Medium), 11.0f, ImVec2(pos.x + size.x - platformSize.x - 16.0f, pos.y + 20.0f), ImGui::GetColorU32(ImVec4(V11Theme::MutedText.x, V11Theme::MutedText.y, V11Theme::MutedText.z, 0.78f)), platform);

            draw->AddText(FontOrDefault(g_Medium), 11.0f, ImVec2(pos.x + 14.0f, footerY + 12.0f), ImGui::GetColorU32(Accent), "V11 RECOVERY");
            draw->AddText(FontOrDefault(g_Medium), 11.0f, ImVec2(pos.x + 116.0f, footerY + 12.0f), ImGui::GetColorU32(V11Theme::MutedText), "LEFT/RIGHT SECTIONS   UP/DOWN OPTIONS   ENTER SELECT   ESC BACK   F4 CLOSE");

            draw->AddRect(
                Offset(pos, 0.5f, 0.5f),
                ImVec2(pos.x + size.x - 0.5f, pos.y + size.y - 0.5f),
                ImGui::GetColorU32(ImVec4(Accent.x, Accent.y, Accent.z, 0.28f)),
                7.0f);

            RenderNavigationRail();
            RenderCategoryRail();

            ImGui::SetCursorPos(ImVec2(0.0f, V11Theme::HeaderHeight));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            constexpr ImGuiWindowFlags bodyFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoMouseInputs;
            if (ImGui::BeginChild("##v11_body", ImVec2(MenuWidth, V11Theme::BodyHeight), false, bodyFlags))
                RenderContent();
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
    }
}
