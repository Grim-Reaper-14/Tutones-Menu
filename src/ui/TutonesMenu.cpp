#include "BusinessPanel.hpp"
#include "CasinoLuckyWheelPanel.hpp"
#include "GoodBehaviorBonusPanel.hpp"
#include "MiscPanel.hpp"
#include "NativeToolsPanel.hpp"
#include "NightclubPanel.hpp"
#include "PersistentMenuState.hpp"
#include "ProtectionPanel.hpp"
#include "SelfV2Panel.hpp"
#include "WorldPanel.hpp"
#include "../features/recovery/RecoveryRuntime.hpp"
#include "../features/vehicle/VehicleModificationRuntime.hpp"
#include "../features/world/WorldRuntime.hpp"

#include <cstring>

#define Categories BaseCategories
#include "TutonesMenu.part00.inc"
#undef Categories

namespace ImGui
{
    namespace
    {
        bool g_TutonesV2OverlayItemHovered{};
        ImVec2 g_TutonesV2OverlayCursor{};
    }

    // V2 navigation is drawn in a NoMouseInputs overlay child. Keep a virtual
    // cursor for its manually hit-tested controls rather than mutating ImGui's
    // real cursor. ImGui 1.92 asserts when SetCursorPos extends window bounds
    // without a submitted item afterwards.
    inline void TutonesV2OverlaySetCursorPos(const ImVec2& localPos) noexcept
    {
        const ImVec2 windowPos = GetWindowPos();
        g_TutonesV2OverlayCursor = ImVec2(
            windowPos.x + localPos.x - GetScrollX(),
            windowPos.y + localPos.y - GetScrollY());
    }

    inline ImVec2 TutonesV2OverlayGetCursorScreenPos() noexcept
    {
        return g_TutonesV2OverlayCursor;
    }

    // The V2 rail is rendered inside a transparent overlay child so it can sit
    // above the dashboard visually. The child itself must not own mouse input,
    // otherwise it blocks every real feature control underneath it. Navigation
    // uses direct mouse hit-testing instead of normal ImGui item ownership.
    inline bool TutonesV2OverlayInvisibleButton(
        const char*,
        const ImVec2& size,
        ImGuiButtonFlags = 0) noexcept
    {
        const ImVec2 min = g_TutonesV2OverlayCursor;
        const ImVec2 max{min.x + size.x, min.y + size.y};
        g_TutonesV2OverlayItemHovered = IsMouseHoveringRect(min, max, false);
        return g_TutonesV2OverlayItemHovered && IsMouseClicked(ImGuiMouseButton_Left);
    }

    inline bool TutonesV2OverlayIsItemHovered(ImGuiHoveredFlags = 0) noexcept
    {
        return g_TutonesV2OverlayItemHovered;
    }

    inline bool TutonesV2BeginChild(
        const char* strId,
        const ImVec2& size,
        bool border,
        ImGuiWindowFlags windowFlags) noexcept
    {
        if (strId && std::strcmp(strId, "##v11_rail_input") == 0)
            windowFlags |= ImGuiWindowFlags_NoMouseInputs;
        return BeginChild(strId, size, border, windowFlags);
    }
}

namespace Tutones::UI
{
    namespace
    {
        // Navigation is organized by user intent instead of backend ownership.
        constexpr std::array<CategoryEntry, 11> Categories{{
            {"P", "SELF",
                {{"General", "Online", "Movement", "Appearance"}},
                {{"X", "O", "M", "A"}},
                {{"Health, wanted level, invincibility and local-player state.", "Online-only local-player controls backed by verified Freemode globals.", "Run, swim, stamina and movement modifiers.", "Model and component appearance controls."}}, 4, true},
            {"W", "WEAPONS",
                {{"General", "Ammo", "Aimbot", "Bullet Effects"}},
                {{"X", "B", "X", "E"}},
                {{"Weapon runtime capabilities and general weapon controls.", "Infinite ammo and infinite clip controls.", "Assisted-aim controls and target handling.", "Impact-driven bullet-effect configuration."}}, 4, true},
            {"V", "VEHICLES",
                {{"General", "Customize", "Garage", nullptr}},
                {{"V", "R", "G", nullptr}},
                {{"Spawner, current vehicle actions and clone tools.", "Paint, custom RGB, LSC modifications, wheels, lighting, neon and tire settings in one customization hub.", "Rockstar personal vehicles and Tutones local saved vehicles kept together in one garage hub.", nullptr}}, 3, true},
            {"O", "ONLINE",
                {{"Session", "Players", "Services", "Player State"}},
                {{"G", "P", "N", "S"}},
                {{"Join or leave GTA Online sessions from one place.", "Live 32-slot Online roster with vitals, position and connection telemetry.", "Enhanced decompile-backed Online services.", "Verified local-player network state and Off Radar controls."}}, 4, true},
            {"O", "WORLD",
                {{"Population", "Time & Weather", "Teleport", "Entities"}},
                {{"O", "Q", "L", "E"}},
                {{"Pedestrian, scenario, traffic and parked-vehicle population controls.", "Local clock, freeze-time, weather and blackout controls.", "Waypoint and map-destination teleport tools with ground-height resolution.", "Local entity cleanup and live crosshair entity inspection."}}, 4, true},
            {"C", "BUSINESSES",
                {{"Business Hub", nullptr, nullptr, nullptr}},
                {{"C", nullptr, nullptr, nullptr}},
                {{"All business tools in one hub: Nightclub, Special Cargo, Bunker, Motorcycle Club, Acid Lab, Hangar and Vehicle Cargo."}}, 1, true},
            {"D", "RECOVERY",
                {{"Overview", "Casino", "Unlocks", "RP"}},
                {{"D", "C", "U", "H"}},
                {{"Recovery status and reward controls, including Good Behavior Bonus.", "Lucky Wheel prize selection and direct read/write slot-machine controls.", "Enhanced DLC clothing unlock groups with packed-stat read-back verification.", "Override the current Enhanced XP multiplier while enabled and restore it when disabled."}}, 4, true},
            {"S", "PROTECTIONS",
                {{"Overview", "Network Events", "Script Events", nullptr}},
                {{"S", "N", "K", nullptr}},
                {{"Live protection runtime and packet-hook status.", "Packed-event filters for malformed or unwanted network traffic.", "Malformed scripted-event validation and scripted-event blocking controls.", nullptr}}, 3, true},
            {"C", "SETTINGS",
                {{"General", "Theme", "Controls", nullptr}},
                {{"C", "T", "C", nullptr}},
                {{"Save, load and reload persistent menu settings.", "Named themes, banner/background images and Windows font selection.", "Menu input and resource refresh controls.", nullptr}}, 3, true},
            {"M", "UTILITIES",
                {{"General", "HUD", "Camera & Player", nullptr}},
                {{"M", "H", "C", nullptr}},
                {{"Gameplay convenience actions and shared quality-of-life controls.", "Coordinates, heading, FPS and Online-session overlays.", "Camera-shake, player cleanup, parachute and underwater utility controls."}}, 3, true},
            {"T", "TOOLS",
                {{"Workshop", "Vehicle & Camera", "World Tools", "Diagnostics"}},
                {{"W", "C", "O", "K"}},
                {{"Weapon components/tints, player props, outfits and animation playback.", "Scripted freecam plus advanced vehicle native controls.", "Custom blips, particle effects, bodyguards, IPL streaming and interior tools.", "Resolve current Enhanced native hashes to executable handler addresses without invoking them."}}, 4, true},
        }};

        void RenderTutonesRuntimeOverlays() noexcept
        {
            Input::Get().PollFallbackHotkeys();
            static_cast<void>(Game::Protections::ProtectionRuntime::Get().Start());
            Game::Recovery::CasinoSlotMachineRuntime::Get().Tick();
            RenderMiscOverlay();
        }
    }
}

#include "TutonesMenu.part01.inc"
#include "TutonesMenu.v12style.inc"
#define SetCursorPos TutonesV2OverlaySetCursorPos
#define GetCursorScreenPos TutonesV2OverlayGetCursorScreenPos
#define InvisibleButton TutonesV2OverlayInvisibleButton
#define IsItemHovered TutonesV2OverlayIsItemHovered
#include "TutonesMenu.v12.inc"
#undef IsItemHovered
#undef InvisibleButton
#undef GetCursorScreenPos
#undef SetCursorPos
#include "TutonesMenu.v12pages.inc"
#include "TutonesMenu.v12tools.inc"
#include "VehicleMenuV2Panel.hpp"

namespace Tutones::UI
{
    namespace
    {
        void DrawV2FooterBrand() noexcept
        {
            const ImVec2 pos = ImGui::GetWindowPos();
            auto* draw = ImGui::GetWindowDrawList();
            const ImVec2 footerMin{pos.x + 28.0f, pos.y + V11Theme::MenuHeight - 66.0f};

            // Cover the old redesign working-label and expose the real product version.
            draw->AddRectFilled(
                ImVec2(footerMin.x + 10.0f, footerMin.y + 20.0f),
                ImVec2(footerMin.x + 150.0f, footerMin.y + 38.0f),
                IM_COL32(8, 11, 16, 255));
            draw->AddText(
                FontOrDefault(g_Bold),
                11.0f,
                ImVec2(footerMin.x + 14.0f, footerMin.y + 23.0f),
                ImGui::GetColorU32(Accent),
                "V2 Dashboard");
        }

        void RenderOnlineNavigationPanel(std::size_t index) noexcept
        {
            switch (index)
            {
            case 0:
                RenderV12GamePanel(0);
                break;
            case 1:
                RenderV12NetworkPanel(2);
                break;
            case 2:
                RenderV12NetworkPanel(0);
                break;
            default:
                RenderV12NetworkPanel(3);
                break;
            }
        }

        void RenderBusinessNavigationPanel(std::size_t) noexcept
        {
            RenderV12BusinessPanel();
        }

        void RenderRecoveryNavigationPanel(std::size_t index) noexcept
        {
            switch (index)
            {
            case 0:
                RenderV12RecoveryPanel(0);
                RenderGoodBehaviorBonusControl();
                break;
            case 1:
                RenderV12CasinoPanel();
                break;
            case 2:
                RenderV12RecoveryPanel(3);
                break;
            default:
                RenderV12RecoveryPanel(1);
                break;
            }
        }

        void RenderUtilitiesNavigationPanel(std::size_t index) noexcept
        {
            RenderV12MiscPanel(index);
        }
    }
}

#define DrawPanel(...) RenderV12ProtectionPanel(m_Item)
#define RenderMiscOverlay(...) RenderTutonesRuntimeOverlays()
#define RenderPlayerPanel(index) ((index) == 0 ? RenderSelfV2Panel() : RenderV12PlayerPanel(index))
#define RenderPlayerOnlinePanel() RenderV12PlayerOnlinePanel()
#define RenderWeaponPanel(index) RenderV12WeaponPanel(index)
#define RenderVehicleGeneralPanel() RenderV12VehicleGeneralPanel()
#define RenderVehiclePaintPanel() RenderV12VehiclePaintPanel()
#define RenderVehicleModificationPanel() RenderV12VehicleModificationPanel()
#define RenderPersonalVehiclePanel() RenderV12PersonalVehiclePanel()
#define RenderGamePanel(index) RenderOnlineNavigationPanel(index)
#define RenderWorldPanel(index) RenderV12WorldPanel(index)
#define RenderRecoveryPanel(index) RenderBusinessNavigationPanel(index)
#define RenderNetworkPanel(index) RenderRecoveryNavigationPanel(index)
#define RenderSettingsPanel(index) RenderV12SettingsPanel(index)
#define RenderMiscPanel(index) RenderUtilitiesNavigationPanel(index)
#define RenderNativeToolsPanel(index) RenderV12NativeToolsDashboard(index)
#define RenderNavigationRail() do { RenderV12NavigationRail(); DrawV2FooterBrand(); } while (false)
#define RenderCategoryRail() RenderV12CategoryRail()
#define RenderStatusPanel(category, item) RenderV12StatusPanel(category, item)
#define ApplyV11Style() ApplyV12Style()
#define BeginChild TutonesV2BeginChild
#include "TutonesMenu.part03.inc"
#undef BeginChild
#undef ApplyV11Style
#undef RenderStatusPanel
#undef RenderCategoryRail
#undef RenderNavigationRail
#undef RenderNativeToolsPanel
#undef RenderMiscPanel
#undef RenderSettingsPanel
#undef RenderNetworkPanel
#undef RenderRecoveryPanel
#undef RenderWorldPanel
#undef RenderGamePanel
#undef RenderPersonalVehiclePanel
#undef RenderVehicleModificationPanel
#undef RenderVehiclePaintPanel
#undef RenderVehicleGeneralPanel
#undef RenderWeaponPanel
#undef RenderPlayerOnlinePanel
#undef RenderPlayerPanel
#undef RenderMiscOverlay
#undef DrawPanel
