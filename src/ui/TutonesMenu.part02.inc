#include "WorldPanel.hpp"

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

        default:
            break;
        }
    }

    void TutonesMenu::Render() noexcept
    {
        if (!Input::Get().IsMenuOpen())
            return;

        auto& themes = ThemeManager::Get();
        if (!themes.EnsureInitialized())
            return;
        if (!EnsureOriginalFonts())
            return;
        themes.ApplyPendingResources();

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

            const ImTextureRef headerTexture = themes.HeaderTexture();
            if (headerTexture.GetTexID() != ImTextureID_Invalid)
            {
                const ImVec2 headerMax{pos.x + V11Theme::MenuWidth, pos.y + V11Theme::HeaderHeight};
                if (themes.HeaderIsCustom())
                    draw->AddImage(headerTexture, pos, headerMax);
                else
                    draw->AddImage(headerTexture, pos, headerMax, ImVec2(0.0f, 0.045f), ImVec2(1.0f, 0.605f));
            }

            const ImVec2 bodyMin{pos.x, pos.y + V11Theme::HeaderHeight};
            const ImVec2 bodyMax{pos.x + V11Theme::MenuWidth, footerY};
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
            constexpr ImGuiWindowFlags bodyFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus;
            if (ImGui::BeginChild("##v11_body", ImVec2(V11Theme::ContentHostWidth, V11Theme::BodyHeight), false, bodyFlags))
                RenderContent();
            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            constexpr ImGuiWindowFlags railFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus;
            if (ImGui::BeginChild(
                    "##v11_rail_input",
                    ImVec2(V11Theme::SubtabRailX + V11Theme::SubtabRailWidth, V11Theme::FooterY),
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

            DrawDescriptionBanner(draw, pos, V11Theme::MenuWidth, m_Category, m_Item);
            const ImTextureRef footerTexture = themes.FooterTexture();
            if (footerTexture.GetTexID() != ImTextureID_Invalid)
            {
                const ImVec2 footerMin{pos.x, footerY};
                const ImVec2 footerMax{pos.x + V11Theme::MenuWidth, footerY + V11Theme::FooterHeight};
                if (themes.FooterIsCustom())
                    draw->AddImage(footerTexture, footerMin, footerMax);
                else
                    draw->AddImage(footerTexture, footerMin, footerMax, ImVec2(0.0f, 0.635f), ImVec2(1.0f, 0.965f));

                const char* description = CurrentV11Description();
                if (!description)
                    description = Categories[m_Category].descriptions[m_Item];
                if (!description)
                    description = "No description is available for this option yet.";

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
        }

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
    }
}
