#include "WorldPanel.hpp"
#include "../core/config/MenuSettings.hpp"

namespace Tutones::UI
{
    void TutonesMenu::RenderContent() noexcept
    {
        const ImVec2 rawWindowPos = ImGui::GetWindowPos();
        const ImVec2 windowPos{rawWindowPos.x, rawWindowPos.y - ImGui::GetScrollY()};
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
            RenderWorldPanel(m_Item);
            break;

        case 5:
            RenderRecoveryPanel(m_Item);
            break;

        case 6:
            RenderNetworkPanel(m_Item);
            break;

        case 7:
            DrawPanel(draw, windowPos, ImVec2(226.0f, 16.0f), ImVec2(490.0f, 430.0f), "PROTECTIONS", {selected, "Protection UI target only.", "Network/script event protection backends are pending."});
            break;

        case 8:
            RenderSettingsPanel(m_Item);
            break;

        case 9:
            RenderMiscPanel(m_Item == 2 ? 3 : m_Item);
            break;

        case 10:
            RenderNativeToolsPanel(m_Item);
            break;

        default:
            break;
        }
    }

    bool TutonesMenu::PrepareFrameResources() noexcept
    {
        if (!ImGui::GetCurrentContext())
            return false;

        auto& themes = ThemeManager::Get();
        if (!themes.EnsureInitialized())
            return false;
        if (!EnsureOriginalFonts())
            return false;

        // ImGui 1.92 has a dynamic font atlas and DX12 texture-update support, but
        // the atlas is still locked between NewFrame() and EndFrame(). All font/image
        // mutations therefore happen here before Renderer starts the ImGui frame.
        themes.ApplyPendingResources();
        return true;
    }

    void TutonesMenu::Render() noexcept
    {
        SyncPersistentMenuState();
        RenderMiscOverlay();

        if (!Input::Get().IsMenuOpen())
            return;

        auto& themes = ThemeManager::Get();
        auto& uiSettings = Core::Config::MenuSettingsService::Get().Current().ui;
        ApplyV11Style();
        ProcessInput();

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const ImVec2 center{viewport->WorkPos.x + viewport->WorkSize.x * 0.5f, viewport->WorkPos.y + viewport->WorkSize.y * 0.5f};
        const ImVec2 topLeft{viewport->WorkPos.x + 12.0f, viewport->WorkPos.y + 12.0f};
        const ImVec2 minSize{V11Theme::MenuWidth, V11Theme::MenuHeight};
        const ImVec2 maxSize{
            std::max(V11Theme::MenuWidth, viewport->WorkSize.x - 24.0f),
            std::max(V11Theme::MenuHeight, viewport->WorkSize.y - 24.0f)};
        const ImVec2 requestedSize{
            std::clamp(uiSettings.menuWidth, minSize.x, maxSize.x),
            std::clamp(uiSettings.menuHeight, minSize.y, maxSize.y)};

        if (uiSettings.anchorTopLeft)
            ImGui::SetNextWindowPos(topLeft, ImGuiCond_Always);
        else
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (uiSettings.resizable)
        {
            ImGui::SetNextWindowSizeConstraints(minSize, maxSize);
            ImGui::SetNextWindowSize(requestedSize, ImGuiCond_Once);
        }
        else
        {
            ImGui::SetNextWindowSize(requestedSize, ImGuiCond_Always);
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, V11Theme::WindowBg);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
            | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoScrollWithMouse
            | ImGuiWindowFlags_NoNavInputs
            | ImGuiWindowFlags_NoNavFocus;
        if (!uiSettings.resizable)
            flags |= ImGuiWindowFlags_NoResize;
        if (uiSettings.anchorTopLeft)
            flags |= ImGuiWindowFlags_NoMove;

        if (ImGui::Begin("Tutone Menu##v11", nullptr, flags))
        {
            const ImVec2 pos = ImGui::GetWindowPos();
            const ImVec2 windowSize = ImGui::GetWindowSize();
            auto* draw = ImGui::GetWindowDrawList();

            if (uiSettings.resizable)
            {
                uiSettings.menuWidth = windowSize.x;
                uiSettings.menuHeight = windowSize.y;
            }

            const float menuWidth = windowSize.x;
            const float bodyHeight = std::max(1.0f, windowSize.y - V11Theme::HeaderHeight - V11Theme::FooterHeight);
            const float footerLocalY = V11Theme::HeaderHeight + bodyHeight;
            const float footerY = pos.y + footerLocalY;

            draw->AddRectFilled(pos, ImVec2(pos.x + menuWidth, pos.y + windowSize.y), ImGui::GetColorU32(V11Theme::WindowBg));

            const ImTextureRef headerTexture = themes.HeaderTexture();
            if (headerTexture.GetTexID() != ImTextureID_Invalid)
            {
                const ImVec2 headerMax{pos.x + menuWidth, pos.y + V11Theme::HeaderHeight};
                if (themes.HeaderIsCustom())
                    draw->AddImage(headerTexture, pos, headerMax);
                else
                    draw->AddImage(headerTexture, pos, headerMax, ImVec2(0.0f, 0.045f), ImVec2(1.0f, 0.605f));
            }
            else
            {
                DrawTopBanner(draw, pos, menuWidth);
            }

            const ImVec2 bodyMin{pos.x, pos.y + V11Theme::HeaderHeight};
            const ImVec2 bodyMax{pos.x + menuWidth, footerY};
            draw->AddRectFilled(bodyMin, bodyMax, ImGui::GetColorU32(V11Theme::BodyBg));

            const ImTextureRef backgroundTexture = themes.BackgroundTexture();
            if (backgroundTexture.GetTexID() != ImTextureID_Invalid)
            {
                const float alpha = std::clamp(themes.Current().backgroundOpacity, 0.0f, 1.0f);
                draw->AddImage(
                    backgroundTexture,
                    bodyMin,
                    bodyMax,
                    ImVec2(0.0f, 0.0f),
                    ImVec2(1.0f, 1.0f),
                    IM_COL32(255, 255, 255, static_cast<int>(alpha * 255.0f)));
            }

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
            ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(V11Theme::ControlBg.x, V11Theme::ControlBg.y, V11Theme::ControlBg.z, 0.72f));
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, V11Theme::AccentDark);
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, V11Theme::Accent);
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, V11Theme::AccentHover);
            ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 10.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 5.0f);
            constexpr ImGuiWindowFlags bodyFlags = ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus;
            if (ImGui::BeginChild("##v11_body", ImVec2(V11Theme::ContentHostWidth, bodyHeight), false, bodyFlags))
                RenderContent();
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(5);

            ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            constexpr ImGuiWindowFlags railFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus;
            if (ImGui::BeginChild(
                    "##v11_rail_input",
                    ImVec2(V11Theme::SubtabRailX + V11Theme::SubtabRailWidth, footerLocalY),
                    false,
                    railFlags))
            {
                RenderNavigationRail();
                RenderCategoryRail();
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();

            RenderStatusPanel(m_Category, m_Item);
            draw->AddText(
                FontOrDefault(g_Bold),
                12.0f,
                ImVec2(pos.x + V11Theme::StatusX + 46.0f, pos.y + V11Theme::HeaderHeight + 158.0f),
                ImGui::GetColorU32(V11Theme::MutedText),
                ::Tutones::App::BuildInfo::Label);

            const ImVec2 footerMin{pos.x, footerY};
            const ImVec2 footerMax{pos.x + menuWidth, footerY + V11Theme::FooterHeight};
            const char* description = CurrentV11Description();
            if (!description)
                description = Categories[m_Category].descriptions[m_Item];
            if (!description)
                description = "No description is available for this option yet.";

            const ImTextureRef footerTexture = themes.FooterTexture();
            if (footerTexture.GetTexID() != ImTextureID_Invalid)
            {
                if (themes.FooterIsCustom())
                    draw->AddImage(footerTexture, footerMin, footerMax);
                else
                    draw->AddImage(footerTexture, footerMin, footerMax, ImVec2(0.0f, 0.635f), ImVec2(1.0f, 0.965f));

                const ImVec2 textMin{footerMin.x + 172.0f, footerMin.y + 24.0f};
                const ImVec2 textMax{footerMax.x - 150.0f, footerMax.y - 18.0f};
                draw->AddRectFilled(
                    ImVec2(textMin.x - 12.0f, textMin.y - 8.0f),
                    ImVec2(textMax.x + 12.0f, textMax.y + 8.0f),
                    IM_COL32(2, 5, 10, 205),
                    3.0f);
                draw->AddText(FontOrDefault(g_Bold), 22.0f, textMin, ImGui::GetColorU32(Accent), "DESCRIPTION");
                draw->AddText(
                    FontOrDefault(g_Medium),
                    15.0f,
                    ImVec2(textMin.x, textMin.y + 34.0f),
                    IM_COL32(235, 239, 247, 255),
                    description,
                    nullptr,
                    textMax.x - textMin.x);
            }
            else
            {
                draw->AddRectFilled(footerMin, footerMax, ImGui::GetColorU32(V11Theme::FooterBg));
                draw->AddText(FontOrDefault(g_Bold), 24.0f, ImVec2(footerMin.x + 172.0f, footerMin.y + 24.0f), ImGui::GetColorU32(Accent), "DESCRIPTION");
                draw->AddText(
                    FontOrDefault(g_Medium),
                    15.0f,
                    ImVec2(footerMin.x + 174.0f, footerMin.y + 63.0f),
                    IM_COL32(226, 232, 242, 255),
                    description,
                    nullptr,
                    std::max(160.0f, menuWidth - 340.0f));
            }
        }

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
        SyncPersistentMenuState();
    }
}
