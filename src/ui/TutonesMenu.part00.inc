#include "TutonesMenu.hpp"

#include "GamePanel.hpp"
#include "Input.hpp"
#include "PlayerOnlinePanel.hpp"
#include "PlayerPanel.hpp"
#include "VehicleGeneralPanel.hpp"
#include "VehicleModificationPanel.hpp"
#include "VehiclePaintPanel.hpp"
#include "WeaponPanel.hpp"
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
            std::array<const char*, 4> items;
            std::size_t itemCount;
        };

        constexpr std::array<CategoryEntry, 7> Categories{{
            {"B", "Home",     {{"Overview", "Quick Actions", "Favorites", nullptr}}, 3},
            {"C", "Player",   {{"General", "Online", "Movement", "Appearance"}}, 4},
            {"D", "Weapons",  {{"General", "Ammo", "Aimbot", "Bullet Effects"}}, 4},
            {"E", "Vehicle",  {{"General", "Paint", "Modifications", nullptr}}, 3},
            {"F", "Game",     {{"Session", "Creator", nullptr, nullptr}}, 2},
            {"G", "World",    {{"General", "Time & Weather", "Teleport", nullptr}}, 3},
            {"H", "Settings", {{"General", "Theme", "Controls", nullptr}}, 3},
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

            g_Medium = io.Fonts->AddFontFromMemoryTTF(PTRootUIMedium, static_cast<int>(sizeof(PTRootUIMedium)), 15.0f, &config);
            g_Bold = io.Fonts->AddFontFromMemoryTTF(PTRootUIBold, static_cast<int>(sizeof(PTRootUIBold)), 15.0f, &config);
            g_TabIcons = io.Fonts->AddFontFromMemoryTTF(clarityfont, static_cast<int>(sizeof(clarityfont)), 15.0f, &config);
            g_Logo = io.Fonts->AddFontFromMemoryTTF(clarityfont, static_cast<int>(sizeof(clarityfont)), 21.0f, &config);
            g_TabTitle = io.Fonts->AddFontFromMemoryTTF(PTRootUIBold, static_cast<int>(sizeof(PTRootUIBold)), 19.0f, &config);
            g_TabTitleIcon = io.Fonts->AddFontFromMemoryTTF(clarityfont, static_cast<int>(sizeof(clarityfont)), 18.0f, &config);
            g_SubtabTitle = io.Fonts->AddFontFromMemoryTTF(PTRootUIBold, static_cast<int>(sizeof(PTRootUIBold)), 15.0f, &config);

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
