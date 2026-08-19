#include "TutonesMenu.hpp"

#include "../app/BuildInfo.hpp"
#include "GamePanel.hpp"
#include "Input.hpp"
#include "PersonalVehiclePanel.hpp"
#include "PlayerOnlinePanel.hpp"
#include "PlayerPanel.hpp"
#include "RecoveryPanel.hpp"
#include "V11Theme.hpp"
#include "VehicleGeneralPanel.hpp"
#include "VehicleModificationPanel.hpp"
#include "VehiclePaintPanel.hpp"
#include "WeaponPanel.hpp"
#include "../core/logging/Logger.hpp"
#include "../render/Renderer.hpp"
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
            std::array<const char*, 4> itemIcons;
            std::array<const char*, 4> descriptions;
            std::size_t itemCount;
            bool backendImplemented;
        };

        constexpr std::array<CategoryEntry, 9> Categories{{
            {"P", "PLAYER",
                {{"General", "Online", "Movement", "Appearance"}},
                {{"X", "O", "M", "A"}},
                {{"Health, wanted level, invincibility and local player state.", "Online-only player state backed by verified Freemode globals.", "Run, swim, stamina and movement modifiers.", "Model and component appearance controls."}}, 4, true},
            {"W", "WEAPONS",
                {{"General", "Ammo", "Aimbot", "Bullet Effects"}},
                {{"X", "B", "X", "E"}},
                {{"Weapon runtime capabilities and backend status.", "Infinite ammo and infinite clip state.", "Assisted-aim patches with dead-target retarget support.", "Impact-driven explosive ammunition configuration."}}, 4, true},
            {"V", "VEHICLE",
                {{"General", "Paint", "Modifications", "Personal Vehicles"}},
                {{"V", "T", "R", "V"}},
                {{"Spawner, current vehicle, clone and saved vehicle tools.", "Vehicle color, custom RGB and finish controls.", "LSC mods, wheels, lights, neon and tire settings.", "Rockstar personal vehicles, garage slots, Request and Repair state."}}, 4, true},
            {"G", "GAME",
                {{"Session", "Creator", nullptr, nullptr}},
                {{"O", "K", nullptr, nullptr}},
                {{"Verified GTA Online join types and Leave Online transitions.", "Creator stays disabled until its exact Enhanced transition path is verified.", nullptr, nullptr}}, 2, true},
            {"O", "WORLD",
                {{"General", "Time & Weather", "Teleport", nullptr}},
                {{"O", "Q", "L", nullptr}},
                {{"World feature workspace.", "Clock and weather workspace.", "Teleport workspace.", nullptr}}, 3, false},
            {"D", "RECOVERY",
                {{"Overview", "RP Multiplier", "Businesses", "Unlocks"}},
                {{"D", "H", "C", "U"}},
                {{"Live Enhanced Recovery capability and business-state overview.", "Override the current Enhanced XP multiplier while enabled, restoring the prior value when disabled.", "Special Cargo and Bunker state with ownership-gated, read-back-verified controls.", "Enhanced unlock groups remain pending until their packed-stat and script conditions are verified."}}, 4, false},
            {"N", "NETWORK",
                {{"Overview", "Session", "Players", nullptr}},
                {{"N", "O", "P", nullptr}},
                {{"Network workspace and verified online capabilities.", "Session transitions live under Game in the current backend.", "Player-network tools are not implemented in the current build.", nullptr}}, 3, false},
            {"S", "PROTECTIONS",
                {{"Overview", "Network Events", "Script Events", nullptr}},
                {{"S", "N", "K", nullptr}},
                {{"Protection workspace and capability status.", "Network-event protections are not implemented in the current build.", "Script-event protections are not implemented in the current build.", nullptr}}, 3, false},
            {"C", "MENU SETTINGS",
                {{"General", "Theme", "Controls", nullptr}},
                {{"C", "T", "C", nullptr}},
                {{"Tutone Menu runtime and menu preferences.", "Blue visual shell and banner styling.", "Keyboard and mouse input preferences.", nullptr}}, 3, false},
        }};

        const ImVec4 Accent = V11Theme::Accent;
        const ImVec4 AccentHover = V11Theme::AccentHover;

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

        void ApplyV11Style() noexcept
        {
            auto& style = ImGui::GetStyle();
            style.Colors[ImGuiCol_CheckMark] = Accent;
            style.Colors[ImGuiCol_SliderGrab] = Accent;
            style.Colors[ImGuiCol_SliderGrabActive] = AccentHover;
            style.Colors[ImGuiCol_Header] = ImVec4(Accent.x, Accent.y, Accent.z, 0.24f);
            style.Colors[ImGuiCol_HeaderHovered] = ImVec4(AccentHover.x, AccentHover.y, AccentHover.z, 0.42f);
            style.Colors[ImGuiCol_HeaderActive] = ImVec4(Accent.x, Accent.y, Accent.z, 0.58f);
            style.Colors[ImGuiCol_ButtonHovered] = ImVec4(Accent.x, Accent.y, Accent.z, 0.30f);
            style.Colors[ImGuiCol_ButtonActive] = ImVec4(Accent.x, Accent.y, Accent.z, 0.46f);
            style.Colors[ImGuiCol_FrameBg] = V11Theme::ControlBg;
            style.Colors[ImGuiCol_FrameBgHovered] = V11Theme::ControlHover;
            style.Colors[ImGuiCol_FrameBgActive] = ImVec4(Accent.x, Accent.y, Accent.z, 0.18f);
            style.Colors[ImGuiCol_Border] = V11Theme::PanelBorder;
            style.Colors[ImGuiCol_Separator] = V11Theme::Separator;
            style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(Accent.x, Accent.y, Accent.z, 0.55f);
            style.Colors[ImGuiCol_SeparatorActive] = Accent;
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
            g_TabIcons = io.Fonts->AddFontFromMemoryTTF(clarityfont, static_cast<int>(sizeof(clarityfont)), 18.0f, &config);
            g_Logo = io.Fonts->AddFontFromMemoryTTF(clarityfont, static_cast<int>(sizeof(clarityfont)), 28.0f, &config);
            g_TabTitle = io.Fonts->AddFontFromMemoryTTF(PTRootUIBold, static_cast<int>(sizeof(PTRootUIBold)), 21.0f, &config);
            g_TabTitleIcon = io.Fonts->AddFontFromMemoryTTF(clarityfont, static_cast<int>(sizeof(clarityfont)), 20.0f, &config);
            g_SubtabTitle = io.Fonts->AddFontFromMemoryTTF(PTRootUIBold, static_cast<int>(sizeof(PTRootUIBold)), 16.0f, &config);

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
                TUTONES_LOG_INFO("ui.fonts", "Loaded Tutones embedded fonts for V11 visual shell");
                return true;
            }

            TUTONES_LOG_WARN("ui.fonts", "Tutones embedded fonts were incomplete; using ImGui fallback fonts");
            return g_Medium != nullptr;
        }
    }
}
