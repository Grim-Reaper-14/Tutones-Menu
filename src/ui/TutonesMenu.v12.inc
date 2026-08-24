#include "../features/network/NetworkRuntime.hpp"
#include "../features/network/ProtectionRuntime.hpp"
#include "../features/player/PlayerRuntime.hpp"
#include "../game/script/ScriptRuntime.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>

namespace Tutones::UI
{
    namespace
    {
        constexpr float V12RailRight = 306.0f;
        constexpr float V12CenterLeft = 326.0f;
        constexpr float V12PageLeft = 354.0f;

        [[nodiscard]] ImU32 V12Text(float alpha = 1.0f) noexcept
        {
            return ImGui::GetColorU32(ImVec4(0.94f, 0.95f, 0.97f, alpha));
        }

        [[nodiscard]] ImU32 V12Muted(float alpha = 1.0f) noexcept
        {
            return ImGui::GetColorU32(ImVec4(
                V11Theme::MutedText.x,
                V11Theme::MutedText.y,
                V11Theme::MutedText.z,
                alpha));
        }

        [[nodiscard]] ImU32 V12Green(float alpha = 1.0f) noexcept
        {
            return IM_COL32(46, 224, 105, static_cast<int>(255.0f * alpha));
        }

        void V12Card(ImDrawList* draw, const ImVec2& min, const ImVec2& max, float rounding = 6.0f) noexcept
        {
            draw->AddRectFilled(min, max, IM_COL32(8, 11, 16, 248), rounding);
            draw->AddRect(min, max, IM_COL32(78, 83, 91, 82), rounding, 0, 1.0f);
        }

        void V12SectionTitle(
            ImDrawList* draw,
            const ImVec2& pos,
            const char* title,
            char icon = 'X') noexcept
        {
            DrawSubtabPictogram(draw, icon, ImVec2(pos.x + 9.0f, pos.y + 9.0f), 18.0f, ImGui::GetColorU32(Accent));
            draw->AddText(FontOrDefault(g_Bold), 14.0f, ImVec2(pos.x + 26.0f, pos.y), V12Text(), title);
        }

        void V12StatusTile(
            ImDrawList* draw,
            const ImVec2& min,
            float width,
            const char* label,
            const char* value,
            bool good) noexcept
        {
            const ImVec2 max{min.x + width, min.y + 66.0f};
            V12Card(draw, min, max, 6.0f);
            draw->AddText(FontOrDefault(g_Medium), 12.0f, ImVec2(min.x + 13.0f, min.y + 11.0f), V12Muted(), label);
            draw->AddText(
                FontOrDefault(g_Bold),
                13.0f,
                ImVec2(min.x + 13.0f, min.y + 36.0f),
                good ? V12Green() : ImGui::GetColorU32(Accent),
                value);
        }

        void V12ProgressBar(
            ImDrawList* draw,
            const ImVec2& min,
            float width,
            float fraction,
            const char* rightText) noexcept
        {
            fraction = std::clamp(fraction, 0.0f, 1.0f);
            const ImVec2 max{min.x + width, min.y + 5.0f};
            draw->AddRectFilled(min, max, IM_COL32(42, 46, 53, 255), 3.0f);
            if (fraction > 0.0f)
            {
                draw->AddRectFilled(
                    min,
                    ImVec2(min.x + width * fraction, max.y),
                    ImGui::GetColorU32(Accent),
                    3.0f);
            }
            if (rightText)
                draw->AddText(FontOrDefault(g_Medium), 12.0f, ImVec2(max.x + 10.0f, min.y - 6.0f), V12Text(), rightText);
        }

        bool V12CategoryButton(
            const char* id,
            std::size_t category,
            const char* label,
            bool selected,
            const ImVec2& localPos) noexcept
        {
            ImGui::SetCursorPos(localPos);
            const ImVec2 min = ImGui::GetCursorScreenPos();
            const ImVec2 size{250.0f, 46.0f};
            const ImVec2 max{min.x + size.x, min.y + size.y};
            const ImGuiID itemId = ImGui::GetID(id);
            const bool pressed = ImGui::InvisibleButton(id, size);
            const bool hovered = ImGui::IsItemHovered();

            if (hovered)
                SetV11Description(Categories[category].descriptions[0]);

            auto& anim = g_TabAnimations[itemId];
            anim.elementOpacity = Animate(anim.elementOpacity, selected ? 0.31f : hovered ? 0.13f : 0.0f, 0.12f);
            anim.rectOpacity = Animate(anim.rectOpacity, selected ? 1.0f : hovered ? 0.50f : 0.0f, 0.14f);
            anim.textOpacity = Animate(anim.textOpacity, selected ? 1.0f : hovered ? 0.94f : 0.76f, 0.11f);

            auto* draw = ImGui::GetWindowDrawList();
            if (selected || hovered)
            {
                draw->AddRectFilled(
                    min,
                    max,
                    ImGui::GetColorU32(ImVec4(Accent.x, Accent.y, Accent.z, anim.elementOpacity)),
                    5.0f);
                draw->AddRect(
                    min,
                    max,
                    ImGui::GetColorU32(ImVec4(Accent.x, Accent.y, Accent.z, anim.rectOpacity * 0.72f)),
                    5.0f,
                    0,
                    1.0f);
            }

            if (selected)
                draw->AddRectFilled(min, ImVec2(min.x + 3.0f, max.y), ImGui::GetColorU32(Accent), 3.0f);

            const char code = Categories[category].icon && Categories[category].icon[0]
                ? Categories[category].icon[0]
                : 'X';
            DrawSubtabPictogram(
                draw,
                code,
                ImVec2(min.x + 25.0f, min.y + 23.0f),
                20.0f,
                selected ? ImGui::GetColorU32(Accent) : V12Muted(anim.textOpacity));

            draw->AddText(
                FontOrDefault(g_Bold),
                14.0f,
                ImVec2(min.x + 52.0f, min.y + 15.0f),
                V12Text(anim.textOpacity),
                label);
            return pressed;
        }

        bool V12SubtabButton(
            const char* id,
            const char* label,
            bool selected,
            const ImVec2& localPos,
            float width) noexcept
        {
            ImGui::SetCursorPos(localPos);
            const ImVec2 min = ImGui::GetCursorScreenPos();
            const ImVec2 size{width, 38.0f};
            const ImVec2 max{min.x + size.x, min.y + size.y};
            const ImGuiID itemId = ImGui::GetID(id);
            const bool pressed = ImGui::InvisibleButton(id, size);
            const bool hovered = ImGui::IsItemHovered();

            auto& anim = g_SubtabAnimations[itemId];
            anim.elementOpacity = Animate(anim.elementOpacity, selected ? 0.30f : hovered ? 0.10f : 0.0f, 0.12f);
            anim.rectOpacity = Animate(anim.rectOpacity, selected ? 1.0f : hovered ? 0.45f : 0.18f, 0.14f);
            anim.textOpacity = Animate(anim.textOpacity, selected ? 1.0f : hovered ? 0.95f : 0.74f, 0.11f);

            auto* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(
                min,
                max,
                selected
                    ? ImGui::GetColorU32(ImVec4(Accent.x, Accent.y, Accent.z, anim.elementOpacity))
                    : IM_COL32(9, 12, 17, 230),
                4.0f);
            draw->AddRect(
                min,
                max,
                selected
                    ? ImGui::GetColorU32(ImVec4(Accent.x, Accent.y, Accent.z, 0.84f))
                    : IM_COL32(73, 78, 87, hovered ? 105 : 62),
                4.0f,
                0,
                1.0f);

            if (selected)
                draw->AddRectFilled(ImVec2(min.x, max.y - 3.0f), max, ImGui::GetColorU32(Accent), 2.0f);

            const ImVec2 textSize = ImGui::CalcTextSize(label);
            draw->AddText(
                FontOrDefault(g_Bold),
                13.0f,
                ImVec2(min.x + (width - textSize.x) * 0.5f, min.y + 11.0f),
                V12Text(anim.textOpacity),
                label);
            return pressed;
        }

        void V12DrawTopChrome(ImDrawList* draw, const ImVec2& pos) noexcept
        {
            const auto network = Game::NetworkFeatures::NetworkRuntime::Get().Snapshot();
            const auto protections = Game::Protections::ProtectionRuntime::Get().Snapshot();
            const bool scriptsReady = Game::Script::ScriptRuntime::Get().IsReady();

            // Cover the legacy full-width banner with the V12 dashboard chrome.
            draw->AddRectFilled(pos, ImVec2(pos.x + V11Theme::MenuWidth, pos.y + 108.0f), IM_COL32(5, 8, 12, 255));
            draw->AddLine(
                ImVec2(pos.x + V12RailRight, pos.y + 108.0f),
                ImVec2(pos.x + V11Theme::MenuWidth - 16.0f, pos.y + 108.0f),
                IM_COL32(74, 79, 87, 75),
                1.0f);

            // Logo block.
            draw->AddText(FontOrDefault(g_Bold), 38.0f, ImVec2(pos.x + 34.0f, pos.y + 24.0f), ImGui::GetColorU32(Accent), "TUTONES");
            draw->AddText(FontOrDefault(g_Bold), 17.0f, ImVec2(pos.x + 94.0f, pos.y + 69.0f), V12Text(0.90f), "M E N U");

            draw->AddText(FontOrDefault(g_Medium), 13.0f, ImVec2(pos.x + V12CenterLeft, pos.y + 26.0f), V12Muted(), "Welcome back,");
            draw->AddText(FontOrDefault(g_Bold), 13.0f, ImVec2(pos.x + V12CenterLeft + 91.0f, pos.y + 26.0f), ImGui::GetColorU32(Accent), "Tutones");
            draw->AddText(
                FontOrDefault(g_Medium),
                12.0f,
                ImVec2(pos.x + V12CenterLeft, pos.y + 55.0f),
                V12Muted(),
                network.sessionStarted ? "Enhanced 1.73   /   Online   /   F4 to Close" : "Enhanced 1.73   /   Story Mode   /   F4 to Close");

            V12StatusTile(
                draw,
                ImVec2(pos.x + 760.0f, pos.y + 20.0f),
                112.0f,
                "Session",
                network.sessionStarted ? "Online" : "Offline",
                network.sessionStarted);
            V12StatusTile(
                draw,
                ImVec2(pos.x + 884.0f, pos.y + 20.0f),
                122.0f,
                "Protections",
                protections.installed ? "Active" : "Offline",
                protections.installed);
            V12StatusTile(
                draw,
                ImVec2(pos.x + 1018.0f, pos.y + 20.0f),
                142.0f,
                "Script Runtime",
                scriptsReady ? "Ready" : "Unavailable",
                scriptsReady);

            const ImVec2 clockMin{pos.x + V11Theme::StatusX, pos.y + 16.0f};
            const ImVec2 clockMax{clockMin.x + V11Theme::StatusWidth, clockMin.y + 78.0f};
            V12Card(draw, clockMin, clockMax, 6.0f);

            std::time_t now = std::time(nullptr);
            std::tm local{};
            localtime_s(&local, &now);
            char timeText[16]{};
            char dateText[32]{};
            std::snprintf(timeText, sizeof(timeText), "%02d:%02d:%02d", local.tm_hour, local.tm_min, local.tm_sec);
            std::snprintf(dateText, sizeof(dateText), "%04d-%02d-%02d", local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
            draw->AddText(FontOrDefault(g_Bold), 18.0f, ImVec2(clockMin.x + 18.0f, clockMin.y + 15.0f), V12Text(), timeText);
            draw->AddText(FontOrDefault(g_Medium), 12.0f, ImVec2(clockMin.x + 18.0f, clockMin.y + 47.0f), V12Muted(), dateText);
        }

        void RenderV12StatusPanel(std::size_t category, std::size_t item) noexcept
        {
            const ImVec2 pos = ImGui::GetWindowPos();
            auto* draw = ImGui::GetWindowDrawList();
            const float x = pos.x + V11Theme::StatusX;
            const float w = V11Theme::StatusWidth;

            const auto player = Game::PlayerFeatures::PlayerRuntime::Get().Snapshot();
            const auto network = Game::NetworkFeatures::NetworkRuntime::Get().Snapshot();
            const bool scriptsReady = Game::Script::ScriptRuntime::Get().IsReady();

            const char* description = CurrentV11Description();
            if (!description)
                description = Categories[category].descriptions[item];
            if (!description)
                description = "No information is available for this page yet.";

            // INFORMATION
            ImVec2 min{x, pos.y + 118.0f};
            ImVec2 max{x + w, pos.y + 248.0f};
            V12Card(draw, min, max);
            V12SectionTitle(draw, ImVec2(min.x + 14.0f, min.y + 13.0f), "INFORMATION", 'X');
            draw->AddText(
                FontOrDefault(g_Medium),
                12.0f,
                ImVec2(min.x + 14.0f, min.y + 45.0f),
                V12Muted(),
                description,
                nullptr,
                w - 28.0f);
            draw->AddText(FontOrDefault(g_Bold), 11.0f, ImVec2(min.x + 14.0f, max.y - 22.0f), ImGui::GetColorU32(Accent), Categories[category].items[item]);

            // PLAYER STATUS
            min = {x, pos.y + 258.0f};
            max = {x + w, pos.y + 334.0f};
            V12Card(draw, min, max);
            V12SectionTitle(draw, ImVec2(min.x + 14.0f, min.y + 11.0f), "PLAYER STATUS", 'P');

            const float healthFraction = player.valid && player.maxHealth > 0
                ? static_cast<float>(player.health) / static_cast<float>(player.maxHealth)
                : 0.0f;
            const float armorFraction = player.valid ? static_cast<float>(player.armor) / 100.0f : 0.0f;
            char healthText[24]{};
            char armorText[24]{};
            std::snprintf(healthText, sizeof(healthText), "%d", player.valid ? player.health : 0);
            std::snprintf(armorText, sizeof(armorText), "%d", player.valid ? player.armor : 0);
            draw->AddText(FontOrDefault(g_Medium), 11.0f, ImVec2(min.x + 14.0f, min.y + 40.0f), V12Muted(), "Health");
            V12ProgressBar(draw, ImVec2(min.x + 78.0f, min.y + 45.0f), 104.0f, healthFraction, healthText);
            draw->AddText(FontOrDefault(g_Medium), 11.0f, ImVec2(min.x + 14.0f, min.y + 59.0f), V12Muted(), "Armor");
            V12ProgressBar(draw, ImVec2(min.x + 78.0f, min.y + 64.0f), 104.0f, armorFraction, armorText);

            // Reserved V12 build strip. part03 also renders BuildInfo::Label here,
            // so the old draw call becomes part of the new design instead of noise.
            min = {x, pos.y + 344.0f};
            max = {x + w, pos.y + 372.0f};
            V12Card(draw, min, max, 5.0f);
            draw->AddText(FontOrDefault(g_Bold), 10.0f, ImVec2(min.x + 12.0f, min.y + 9.0f), ImGui::GetColorU32(Accent), "BUILD");

            // SESSION INFO
            min = {x, pos.y + 382.0f};
            max = {x + w, pos.y + 548.0f};
            V12Card(draw, min, max);
            V12SectionTitle(draw, ImVec2(min.x + 14.0f, min.y + 13.0f), "SESSION INFO", 'O');

            const auto& roster = network.playerRoster;
            const char* sessionValue = network.sessionStarted ? "Online" : "Offline";
            char playersText[16]{};
            char localText[16]{};
            char hostText[16]{};
            std::snprintf(playersText, sizeof(playersText), "%d", roster.activeCount);
            std::snprintf(localText, sizeof(localText), "%d", roster.localPlayer);
            std::snprintf(hostText, sizeof(hostText), "%d", roster.freemodeHost);

            const char* labels[] = {"Players", "Session", "Local ID", "Freemode Host", "Script Globals"};
            const char* values[] = {
                playersText,
                sessionValue,
                localText,
                hostText,
                network.scriptGlobalsReady ? "Ready" : "Unavailable",
            };
            float y = min.y + 47.0f;
            for (int i = 0; i < 5; ++i)
            {
                draw->AddText(FontOrDefault(g_Medium), 11.0f, ImVec2(min.x + 14.0f, y), V12Muted(), labels[i]);
                const bool good = (i == 4 && network.scriptGlobalsReady) || (i == 1 && network.sessionStarted);
                const ImU32 valueColor = good ? V12Green() : V12Text();
                const float valueWidth = ImGui::CalcTextSize(values[i]).x;
                draw->AddText(FontOrDefault(g_Medium), 11.0f, ImVec2(max.x - 14.0f - valueWidth, y), valueColor, values[i]);
                y += 23.0f;
            }

            // SYSTEM
            min = {x, pos.y + 558.0f};
            max = {x + w, pos.y + 744.0f};
            V12Card(draw, min, max);
            V12SectionTitle(draw, ImVec2(min.x + 14.0f, min.y + 13.0f), "SYSTEM", 'C');

            float pingMs = -1.0f;
            if (roster.localPlayer >= 0 && roster.localPlayer < static_cast<int>(roster.players.size()))
            {
                const auto& localPlayer = roster.players[static_cast<std::size_t>(roster.localPlayer)];
                if (localPlayer.latencyReadable && std::isfinite(localPlayer.averageLatency))
                    pingMs = localPlayer.averageLatency < 10.0f ? localPlayer.averageLatency * 1000.0f : localPlayer.averageLatency;
            }

            char fpsText[24]{};
            char frameText[24]{};
            char pingText[24]{};
            std::snprintf(fpsText, sizeof(fpsText), "%.0f", ImGui::GetIO().Framerate);
            std::snprintf(frameText, sizeof(frameText), "%.2f ms", ImGui::GetIO().DeltaTime * 1000.0f);
            if (pingMs >= 0.0f)
                std::snprintf(pingText, sizeof(pingText), "%.0f ms", pingMs);
            else
                std::snprintf(pingText, sizeof(pingText), "N/A");

            const char* systemLabels[] = {"FPS", "Frame", "Ping", "Renderer", "Script Runtime"};
            const char* systemValues[] = {fpsText, frameText, pingText, "D3D12", scriptsReady ? "Ready" : "Unavailable"};
            y = min.y + 48.0f;
            for (int i = 0; i < 5; ++i)
            {
                draw->AddText(FontOrDefault(g_Medium), 11.0f, ImVec2(min.x + 14.0f, y), V12Muted(), systemLabels[i]);
                const float valueWidth = ImGui::CalcTextSize(systemValues[i]).x;
                const bool good = (i == 4 && scriptsReady) || (i == 2 && pingMs >= 0.0f);
                draw->AddText(
                    FontOrDefault(g_Medium),
                    11.0f,
                    ImVec2(max.x - 14.0f - valueWidth, y),
                    good ? V12Green() : V12Text(),
                    systemValues[i]);
                y += 24.0f;
            }
        }
    }

    void TutonesMenu::RenderV12NavigationRail() noexcept
    {
        const ImVec2 pos = ImGui::GetWindowPos();
        auto* draw = ImGui::GetWindowDrawList();

        V12DrawTopChrome(draw, pos);

        // Full-height left navigation panel from the approved mockup.
        const ImVec2 railMin{pos.x + 16.0f, pos.y + 14.0f};
        const ImVec2 railMax{pos.x + 294.0f, pos.y + V11Theme::MenuHeight - 14.0f};
        draw->AddRectFilled(railMin, railMax, IM_COL32(6, 9, 13, 252), 7.0f);
        draw->AddRect(railMin, railMax, IM_COL32(76, 81, 90, 88), 7.0f, 0, 1.0f);
        draw->AddLine(
            ImVec2(pos.x + V12RailRight, pos.y + 16.0f),
            ImVec2(pos.x + V12RailRight, pos.y + V11Theme::MenuHeight - 16.0f),
            IM_COL32(74, 79, 87, 72),
            1.0f);

        // The top chrome is redrawn after the rail so the logo stays crisp.
        draw->AddText(FontOrDefault(g_Bold), 38.0f, ImVec2(pos.x + 34.0f, pos.y + 24.0f), ImGui::GetColorU32(Accent), "TUTONES");
        draw->AddText(FontOrDefault(g_Bold), 17.0f, ImVec2(pos.x + 94.0f, pos.y + 69.0f), V12Text(0.90f), "M E N U");

        for (std::size_t i = 0; i < Categories.size(); ++i)
        {
            ImGui::PushID(static_cast<int>(i));
            const ImVec2 localPos{28.0f, 126.0f + static_cast<float>(i) * 52.0f};
            if (V12CategoryButton("##v12_category", i, Categories[i].name, i == m_Category, localPos))
            {
                m_Category = i;
                m_Item = 0;
            }
            ImGui::PopID();
        }

        const ImVec2 footerMin{pos.x + 28.0f, pos.y + V11Theme::MenuHeight - 66.0f};
        const ImVec2 footerMax{pos.x + 282.0f, pos.y + V11Theme::MenuHeight - 26.0f};
        V12Card(draw, footerMin, footerMax, 5.0f);
        draw->AddText(FontOrDefault(g_Bold), 12.0f, ImVec2(footerMin.x + 14.0f, footerMin.y + 8.0f), V12Text(), "Tutones Menu");
        draw->AddText(FontOrDefault(g_Bold), 11.0f, ImVec2(footerMin.x + 14.0f, footerMin.y + 23.0f), ImGui::GetColorU32(Accent), "V12 Dashboard");
    }

    void TutonesMenu::RenderV12CategoryRail() noexcept
    {
        const ImVec2 pos = ImGui::GetWindowPos();
        auto* draw = ImGui::GetWindowDrawList();

        // Page title + description area.
        draw->AddRectFilled(
            ImVec2(pos.x + V12RailRight + 1.0f, pos.y + 108.0f),
            ImVec2(pos.x + V11Theme::StatusX - 12.0f, pos.y + 199.0f),
            IM_COL32(5, 8, 12, 246));

        const char categoryCode = Categories[m_Category].icon && Categories[m_Category].icon[0]
            ? Categories[m_Category].icon[0]
            : 'X';
        draw->AddCircleFilled(ImVec2(pos.x + V12PageLeft + 17.0f, pos.y + 135.0f), 22.0f, IM_COL32(229, 42, 48, 34), 32);
        DrawSubtabPictogram(
            draw,
            categoryCode,
            ImVec2(pos.x + V12PageLeft + 17.0f, pos.y + 135.0f),
            22.0f,
            ImGui::GetColorU32(Accent));
        draw->AddText(FontOrDefault(g_TabTitle), 23.0f, ImVec2(pos.x + V12PageLeft + 51.0f, pos.y + 118.0f), V12Text(), Categories[m_Category].name);
        draw->AddText(
            FontOrDefault(g_Medium),
            12.0f,
            ImVec2(pos.x + V12PageLeft + 51.0f, pos.y + 151.0f),
            V12Muted(),
            Categories[m_Category].descriptions[m_Item],
            nullptr,
            720.0f);

        float x = V12PageLeft;
        for (std::size_t i = 0; i < Categories[m_Category].itemCount; ++i)
        {
            const char* label = Categories[m_Category].items[i];
            const float width = std::clamp(ImGui::CalcTextSize(label).x + 52.0f, 116.0f, 188.0f);
            ImGui::PushID(static_cast<int>(i));
            if (V12SubtabButton("##v12_subtab", label, i == m_Item, ImVec2(x, 160.0f), width))
                m_Item = i;
            if (ImGui::IsItemHovered())
                SetV11Description(Categories[m_Category].descriptions[i]);
            ImGui::PopID();
            x += width + 8.0f;
        }
    }
}
