#include "WeaponPanel.hpp"

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/weapon/WeaponRuntime.hpp"

#include <imgui.h>

#include <cstddef>

namespace Tutones::UI
{
    namespace
    {
        using Game::WeaponFeatures::WeaponRuntime;
        using Game::WeaponFeatures::WeaponSnapshot;

        const ImVec4 Accent = V11Theme::Accent;
        constexpr ImVec4 Supported{43.0f / 255.0f, 231.0f / 255.0f, 111.0f / 255.0f, 1.0f};

        struct ExplosionEntry final
        {
            int value;
            const char* label;
        };

        constexpr ExplosionEntry ExplosionTypes[] = {
            {-1, "Don't Care"},
            {0, "Grenade"},
            {1, "Grenade Launcher"},
            {2, "Sticky Bomb"},
            {3, "Molotov"},
            {4, "Rocket"},
            {5, "Tank Shell"},
            {6, "Hi Octane"},
            {7, "Car"},
            {8, "Plane"},
            {9, "Petrol Pump"},
            {10, "Bike"},
            {11, "Directional Steam"},
            {12, "Directional Flame"},
            {13, "Directional Water Hydrant"},
            {14, "Directional Gas Canister"},
            {15, "Boat"},
            {16, "Ship Destroy"},
            {17, "Truck"},
            {18, "Bullet"},
            {19, "Smoke Grenade Launcher"},
            {20, "Smoke Grenade"},
            {21, "BZ Gas"},
            {22, "Flare"},
            {23, "Gas Canister"},
            {24, "Extinguisher"},
            {25, "Programmable AR"},
            {26, "Train"},
            {27, "Barrel"},
            {28, "Propane"},
            {29, "Blimp"},
            {30, "Directional Flame Explode"},
            {31, "Tanker"},
            {32, "Plane Rocket"},
            {33, "Vehicle Bullet"},
            {34, "Gas Tank"},
            {35, "Bird Crap"},
            {36, "Railgun"},
            {37, "Blimp 2"},
            {38, "Firework"},
            {39, "Snowball"},
            {40, "Proximity Mine"},
            {41, "Valkyrie Cannon"},
            {42, "Air Defence"},
            {43, "Pipe Bomb"},
            {44, "Vehicle Mine"},
            {45, "Explosive Ammo"},
            {46, "APC Shell"},
            {47, "Cluster Bomb"},
            {48, "Gas Bomb"},
            {49, "Incendiary Bomb"},
            {50, "Standard Bomb"},
            {51, "Torpedo"},
            {52, "Underwater Torpedo"},
            {53, "Bombushka Cannon"},
            {54, "Cluster Bomb Secondary"},
            {55, "Hunter Barrage"},
            {56, "Hunter Cannon"},
            {57, "Rogue Cannon"},
            {58, "Underwater Mine"},
            {59, "Orbital Cannon"},
            {60, "Standard Bomb Wide"},
            {61, "Explosive Ammo Shotgun"},
            {62, "Oppressor Mk II Cannon"},
            {63, "Kinetic Mortar"},
            {64, "Vehicle Mine Kinetic"},
            {65, "Vehicle Mine EMP"},
            {66, "Vehicle Mine Spike"},
            {67, "Vehicle Mine Slick"},
            {68, "Vehicle Mine Tar"},
            {69, "Script Drone"},
            {70, "Ray Gun"},
            {71, "Buried Mine"},
            {72, "Script Missile"},
            {73, "RC Tank Rocket"},
            {74, "Water Bomb"},
            {75, "Water Bomb Secondary"},
            {76, "Unknown F728C4A9"},
            {77, "Unknown BAEC056F"},
            {78, "Flash Grenade"},
            {79, "Stun Grenade"},
            {80, "Unknown 763D3B3B"},
            {81, "Script Missile Large"},
            {82, "Submarine Big"},
            {83, "EMP Launcher"},
        };

        const char* ExplosionLabel(int value) noexcept
        {
            for (const auto& entry : ExplosionTypes)
                if (entry.value == value)
                    return entry.label;
            return "Unknown";
        }

        void SupportBadge(bool supported) noexcept
        {
            ImGui::SameLine(330.0f);
            if (supported)
                ImGui::TextColored(Supported, "SUPPORTED");
            else
                ImGui::TextDisabled("UNAVAILABLE");
        }

        void RenderGeneral(const WeaponSnapshot& snapshot) noexcept
        {
            ImGui::TextUnformatted("Runtime status");
            ImGui::Separator();
            ImGui::Text("Weapon runtime: %s", snapshot.running ? "online" : "offline");
            ImGui::Text("Pointer foundation: %s", snapshot.pointersResolved ? "ready" : "waiting");
            ImGui::Text("Native backend: %s", snapshot.nativeReady ? "ready" : "waiting");
            ImGui::Text("Aimbot patches: %s", snapshot.aimbotSupported ? "ready" : "unavailable");
            ImGui::Text("Head lock patch: %s", snapshot.aimForHeadSupported ? "ready" : "unavailable");
            ImGui::Text("Driver lock patch: %s", snapshot.targetDriversSupported ? "ready" : "unavailable");
            ImGui::Text("Dead-target hook: %s", snapshot.releaseDeadTargetSupported ? "ready" : "unavailable");

            ImGui::Spacing();
            ImGui::TextDisabled("Patch availability reflects this loaded DLL instance; startup logs include a one-line patch self-check.");
        }

        void RenderAmmo(WeaponRuntime& runtime, const WeaponSnapshot& snapshot) noexcept
        {
            bool infiniteAmmo = snapshot.settings.infiniteAmmo;
            bool infiniteClip = snapshot.settings.infiniteClip;

            ImGui::BeginDisabled(!snapshot.nativeReady);
            if (ImGui::Checkbox("Infinite Ammo", &infiniteAmmo))
                runtime.SetInfiniteAmmo(infiniteAmmo);
            ImGui::EndDisabled();
            DescribeLastV11Item("Maintain infinite reserve ammunition for the local player's current weapons while the GTA native backend is ready.");
            SupportBadge(snapshot.nativeReady);

            ImGui::BeginDisabled(!snapshot.nativeReady);
            if (ImGui::Checkbox("Infinite Clip", &infiniteClip))
                runtime.SetInfiniteClip(infiniteClip);
            ImGui::EndDisabled();
            DescribeLastV11Item("Maintain the current weapon clip without normal reload depletion while the GTA native backend is ready.");
            SupportBadge(snapshot.nativeReady);

            ImGui::Separator();
            static const char* utilityMessage = "Ready";
            ImGui::BeginDisabled(!snapshot.nativeReady);
            if (ImGui::Button("Give All Weapons", ImVec2(220.0f, 0.0f)))
                utilityMessage = runtime.QueueGiveAllWeapons() ? "Give All Weapons queued" : "Give All Weapons rejected";
            DescribeLastV11Item("Give the local player the current Yim weapon catalog with a large initial ammo count.");
            ImGui::SameLine();
            if (ImGui::Button("Give Max Ammo", ImVec2(-1.0f, 0.0f)))
                utilityMessage = runtime.QueueGiveMaxAmmo() ? "Give Max Ammo queued" : "Give Max Ammo rejected";
            DescribeLastV11Item("Query each available weapon's GTA max-ammo value and refill the local player's reserve ammo to that limit.");
            ImGui::EndDisabled();

            if (!snapshot.nativeReady)
                ImGui::TextDisabled("Waiting for the GTA native table.");
            else
                ImGui::TextDisabled("%s", utilityMessage);
        }

        void RenderAimbot(WeaponRuntime& runtime, const WeaponSnapshot& snapshot) noexcept
        {
            bool aimbot = snapshot.settings.aimbot;
            bool aimForHead = snapshot.settings.aimForHead;
            bool targetDrivers = snapshot.settings.targetDrivers;
            bool releaseDeadPed = snapshot.settings.releaseDeadPed;

            ImGui::BeginDisabled(!snapshot.aimbotSupported);
            if (ImGui::Checkbox("Enable Aimbot", &aimbot))
                runtime.SetAimbot(aimbot);
            ImGui::EndDisabled();
            DescribeLastV11Item("Enable the assisted-aim patch set when the live patch configuration in this loaded DLL instance is ready.");
            SupportBadge(snapshot.aimbotSupported);
            if (!snapshot.aimbotSupported)
            {
                ImGui::TextDisabled(snapshot.pointersResolved
                    ? "Aimbot patch configuration is missing in this DLL instance. Check the current build ID and startup log."
                    : "GTA pointer foundation is not ready in this DLL instance.");
            }

            ImGui::Separator();
            ImGui::BeginDisabled(!aimbot || !snapshot.aimForHeadSupported);
            if (ImGui::Checkbox("Aim For Head", &aimForHead))
                runtime.SetAimForHead(aimForHead);
            ImGui::EndDisabled();
            DescribeLastV11Item("Use the verified lock-on position patch so assisted aim targets the head position when aimbot is enabled.");
            SupportBadge(snapshot.aimForHeadSupported);

            ImGui::BeginDisabled(!aimbot || !snapshot.targetDriversSupported);
            if (ImGui::Checkbox("Target Drivers", &targetDrivers))
                runtime.SetTargetDrivers(targetDrivers);
            ImGui::EndDisabled();
            DescribeLastV11Item("Use the verified driver lock-on patch so assisted aim can target drivers inside vehicles when aimbot is enabled.");
            SupportBadge(snapshot.targetDriversSupported);

            ImGui::BeginDisabled(!snapshot.releaseDeadTargetSupported);
            if (ImGui::Checkbox("Release Dead Target", &releaseDeadPed))
                runtime.SetReleaseDeadPed(releaseDeadPed);
            ImGui::EndDisabled();
            DescribeLastV11Item("When assisted aim is holding a dead ped, clear the target and try GTA's new-target search; release the dead target only if retargeting fails.");
            SupportBadge(snapshot.releaseDeadTargetSupported);

            ImGui::Spacing();
            ImGui::TextDisabled("Release Dead Target keeps Yim-style retarget behavior in a separate detour.");
        }

        void RenderBulletEffects(WeaponRuntime& runtime, const WeaponSnapshot& snapshot) noexcept
        {
            bool explosiveAmmo = snapshot.settings.explosiveAmmo;
            int explosionType = snapshot.settings.explosionType;
            float damage = snapshot.settings.explosionDamage;
            float shake = snapshot.settings.explosionCameraShake;

            ImGui::BeginDisabled(!snapshot.nativeReady);
            if (ImGui::Checkbox("Explosive Ammo", &explosiveAmmo))
                runtime.SetExplosiveAmmo(explosiveAmmo);
            ImGui::EndDisabled();
            DescribeLastV11Item("Create an explosion at the last valid firearm impact coordinate when the explosive-ammo effect is enabled and the native backend is ready.");
            SupportBadge(snapshot.nativeReady);

            ImGui::BeginDisabled(!snapshot.nativeReady);
            if (ImGui::BeginCombo("Explosion Type", ExplosionLabel(explosionType)))
            {
                for (const auto& entry : ExplosionTypes)
                {
                    const bool selected = entry.value == explosionType;
                    if (ImGui::Selectable(entry.label, selected))
                    {
                        explosionType = entry.value;
                        runtime.SetExplosionType(explosionType);
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            DescribeLastV11Item("Choose the GTA explosion type created at valid firearm impact coordinates while Explosive Ammo is enabled.");

            if (ImGui::SliderFloat("Explosion Damage", &damage, 0.0f, 1000.0f, "%.1f"))
                runtime.SetExplosionDamage(damage);
            DescribeLastV11Item("Adjust the damage scale passed to the explosion effect created by Explosive Ammo.");
            if (ImGui::SliderFloat("Camera Shake", &shake, 0.0f, 10.0f, "%.2f"))
                runtime.SetExplosionCameraShake(shake);
            DescribeLastV11Item("Adjust the camera-shake value passed to explosions created by the Explosive Ammo effect.");
            ImGui::EndDisabled();

            if (!snapshot.nativeReady)
                ImGui::TextDisabled("Waiting for the GTA native table.");
            else
                ImGui::TextDisabled("Explosions are created from the last valid firearm impact coordinate.");
        }
    }

    void RenderWeaponPanel(std::size_t subtab) noexcept
    {
        auto& runtime = WeaponRuntime::Get();
        const WeaponSnapshot snapshot = runtime.Snapshot();

        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##weapon_panel", ImVec2(490.0f, 430.0f), true))
        {
            constexpr const char* SubtabNames[] = {"General", "Ammo", "Aimbot", "Bullet Effects"};
            const auto index = subtab < 4 ? subtab : std::size_t{0};
            ImGui::TextColored(Accent, "Weapons");
            ImGui::SameLine();
            ImGui::TextDisabled("%s", SubtabNames[index]);
            ImGui::Separator();

            if (!snapshot.running)
                ImGui::TextDisabled("Weapon runtime is offline.");
            else if (index == 0)
                RenderGeneral(snapshot);
            else if (index == 1)
                RenderAmmo(runtime, snapshot);
            else if (index == 2)
                RenderAimbot(runtime, snapshot);
            else
                RenderBulletEffects(runtime, snapshot);
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
