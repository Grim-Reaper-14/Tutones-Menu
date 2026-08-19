#include "PlayerPanel.hpp"

#include "V11Theme.hpp"
#include "../features/player/PlayerRuntime.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

namespace Tutones::UI
{
    namespace
    {
        using Game::PlayerFeatures::PlayerAction;
        using Game::PlayerFeatures::PlayerRuntime;
        using Game::PlayerFeatures::PlayerSnapshot;

        const ImVec4 Accent = V11Theme::Accent;
        constexpr std::array<const char*, 12> ComponentNames{{
            "Head", "Mask / Beard", "Hair", "Torso", "Legs", "Bags / Hands",
            "Shoes", "Accessories", "Undershirt", "Body Armor", "Decals", "Top / Jacket",
        }};

        int g_LastPed{};
        int g_Health{200};
        int g_Armor{};
        int g_Wanted{};
        bool g_Invincible{};
        bool g_Invisible{};
        bool g_PoliceIgnore{};
        bool g_EveryoneIgnore{};
        bool g_NeverWanted{};
        bool g_NoRagdoll{};
        bool g_SuperJump{};
        bool g_InfiniteStamina{};
        float g_RunMultiplier{1.0f};
        float g_SwimMultiplier{1.0f};

        char g_ModelName[64] = "mp_m_freemode_01";
        int g_Component{};
        int g_Drawable{};
        int g_Texture{};
        int g_Palette{};
        int g_LastAppearanceComponent{-1};
        bool g_AppearanceSyncPending{true};
        const char* g_Message{"Ready"};

        [[nodiscard]] const char* ActionName(PlayerAction action) noexcept
        {
            switch (action)
            {
            case PlayerAction::None: return "None";
            case PlayerAction::SetHealth: return "Set health";
            case PlayerAction::Heal: return "Heal";
            case PlayerAction::SetArmor: return "Set armor";
            case PlayerAction::SetWanted: return "Set wanted";
            case PlayerAction::ClearWanted: return "Clear wanted";
            case PlayerAction::ApplyPersistent: return "Apply player settings";
            case PlayerAction::ModelRequest: return "Request model";
            case PlayerAction::ModelSwap: return "Swap model";
            case PlayerAction::SetComponent: return "Set outfit component";
            case PlayerAction::DefaultComponents: return "Default outfit";
            case PlayerAction::RandomComponents: return "Random outfit";
            }
            return "Unknown";
        }

        void SyncPlayerUi(const PlayerSnapshot& snapshot) noexcept
        {
            if (!snapshot.valid)
            {
                g_LastPed = 0;
                return;
            }
            if (snapshot.ped == g_LastPed)
                return;

            g_LastPed = snapshot.ped;
            g_Health = snapshot.health;
            g_Armor = snapshot.armor;
            g_Wanted = snapshot.wantedLevel;
            g_Invincible = snapshot.invincible;
            g_Invisible = snapshot.invisible;
            g_PoliceIgnore = snapshot.policeIgnore;
            g_EveryoneIgnore = snapshot.everyoneIgnore;
            g_NeverWanted = snapshot.neverWanted;
            g_NoRagdoll = snapshot.noRagdoll;
            g_SuperJump = snapshot.superJump;
            g_InfiniteStamina = snapshot.infiniteStamina;
            g_RunMultiplier = snapshot.runMultiplier;
            g_SwimMultiplier = snapshot.swimMultiplier;
            g_Component = snapshot.observedComponent;
            g_Drawable = std::max(0, snapshot.currentDrawable);
            g_Texture = std::max(0, snapshot.currentTexture);
            g_Palette = std::clamp(snapshot.currentPalette, 0, 3);
            g_LastAppearanceComponent = snapshot.observedComponent;
            g_AppearanceSyncPending = false;
        }

        void RenderOperationStatus(const PlayerSnapshot& snapshot) noexcept
        {
            ImGui::Separator();
            if (snapshot.lastAction == PlayerAction::None)
                ImGui::TextDisabled("No Player action has run yet.");
            else
                ImGui::Text("Last action: %s - %s", ActionName(snapshot.lastAction), snapshot.lastActionSucceeded ? "success" : "failed");
            if (snapshot.modelLoadPending)
                ImGui::TextDisabled("Model 0x%08X is loading...", snapshot.pendingModel);
        }

        void RenderGeneral(PlayerRuntime& runtime, const PlayerSnapshot& snapshot) noexcept
        {
            const int maxHealth = std::max(1, snapshot.maxHealth);
            g_Health = std::clamp(g_Health, 0, maxHealth);
            ImGui::Text("Ped: %d   Model: 0x%08X", snapshot.ped, snapshot.model);
            ImGui::Text("Current health: %d / %d", snapshot.health, snapshot.maxHealth);
            ImGui::SliderInt("Health", &g_Health, 0, maxHealth);
            if (ImGui::Button("Set health", ImVec2(180.0f, 0.0f)))
                g_Message = runtime.QueueSetHealth(g_Health) ? "Health queued" : "Health rejected";
            ImGui::SameLine();
            if (ImGui::Button("Full heal", ImVec2(-1.0f, 0.0f)))
            {
                g_Health = maxHealth;
                g_Message = runtime.QueueHeal() ? "Heal queued" : "Heal rejected";
            }

            g_Armor = std::clamp(g_Armor, 0, 100);
            ImGui::SliderInt("Armor", &g_Armor, 0, 100);
            if (ImGui::Button("Set armor", ImVec2(-1.0f, 0.0f)))
                g_Message = runtime.QueueSetArmor(g_Armor) ? "Armor queued" : "Armor rejected";

            ImGui::Separator();
            g_Wanted = std::clamp(g_Wanted, 0, 5);
            ImGui::SliderInt("Wanted level", &g_Wanted, 0, 5);
            if (ImGui::Button("Apply wanted", ImVec2(180.0f, 0.0f)))
                g_Message = runtime.QueueSetWantedLevel(g_Wanted) ? "Wanted queued" : "Wanted rejected";
            ImGui::SameLine();
            if (ImGui::Button("Clear wanted", ImVec2(-1.0f, 0.0f)))
            {
                g_Wanted = 0;
                g_Message = runtime.QueueClearWanted() ? "Wanted clear queued" : "Wanted clear rejected";
            }

            if (ImGui::Checkbox("Never wanted", &g_NeverWanted)) runtime.SetNeverWanted(g_NeverWanted);
            if (ImGui::Checkbox("Invincible", &g_Invincible)) runtime.SetInvincible(g_Invincible);
            if (ImGui::Checkbox("Invisible", &g_Invisible)) runtime.SetInvisible(g_Invisible);
            if (ImGui::Checkbox("Police ignore", &g_PoliceIgnore)) runtime.SetPoliceIgnore(g_PoliceIgnore);
            if (ImGui::Checkbox("Everyone ignore", &g_EveryoneIgnore)) runtime.SetEveryoneIgnore(g_EveryoneIgnore);

            ImGui::TextDisabled("%s", g_Message);
            RenderOperationStatus(snapshot);
        }

        void RenderMovement(PlayerRuntime& runtime, const PlayerSnapshot& snapshot) noexcept
        {
            ImGui::TextUnformatted("Movement multipliers");
            if (ImGui::SliderFloat("Run / sprint", &g_RunMultiplier, 1.0f, 1.49f, "%.2fx"))
                runtime.SetRunMultiplier(g_RunMultiplier);
            if (ImGui::SliderFloat("Swim", &g_SwimMultiplier, 1.0f, 1.49f, "%.2fx"))
                runtime.SetSwimMultiplier(g_SwimMultiplier);

            ImGui::Separator();
            if (ImGui::Checkbox("Super jump", &g_SuperJump)) runtime.SetSuperJump(g_SuperJump);
            if (ImGui::Checkbox("Infinite stamina", &g_InfiniteStamina)) runtime.SetInfiniteStamina(g_InfiniteStamina);
            if (ImGui::Checkbox("No ragdoll", &g_NoRagdoll)) runtime.SetNoRagdoll(g_NoRagdoll);

            ImGui::Spacing();
            ImGui::TextDisabled("Super jump and infinite stamina are maintained on the GTA script tick.");
            RenderOperationStatus(snapshot);
        }

        void RenderAppearance(PlayerRuntime& runtime, const PlayerSnapshot& snapshot) noexcept
        {
            ImGui::TextUnformatted("Player model");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputText("##player_model_name", g_ModelName, sizeof(g_ModelName));
            if (ImGui::Button("Load ped model", ImVec2(-1.0f, 0.0f)))
                g_Message = runtime.QueueModelByName(std::string(g_ModelName)) ? "Model request queued" : "Model request rejected";
            ImGui::TextDisabled("Examples: mp_m_freemode_01, mp_f_freemode_01, player_zero");

            ImGui::Separator();
            ImGui::TextUnformatted("Outfit / component editor");
            if (ImGui::Combo("Component", &g_Component, ComponentNames.data(), static_cast<int>(ComponentNames.size())))
            {
                g_Drawable = -1;
                g_Texture = 0;
                g_Palette = 0;
                g_AppearanceSyncPending = true;
                g_LastAppearanceComponent = -1;
                runtime.SetObservedComponent(g_Component, -1);
            }

            if (snapshot.observedComponent == g_Component && g_AppearanceSyncPending)
            {
                g_Drawable = std::max(0, snapshot.currentDrawable);
                g_Texture = std::max(0, snapshot.currentTexture);
                g_Palette = std::clamp(snapshot.currentPalette, 0, 3);
                g_AppearanceSyncPending = false;
                g_LastAppearanceComponent = g_Component;
            }

            if (g_Drawable < 0 || snapshot.observedComponent != g_Component)
            {
                ImGui::TextDisabled("Reading component options...");
                runtime.SetObservedComponent(g_Component, -1);
            }
            else
            {
                const int drawableMax = std::max(0, snapshot.drawableCount - 1);
                g_Drawable = std::clamp(g_Drawable, 0, drawableMax);
                if (ImGui::SliderInt("Drawable", &g_Drawable, 0, drawableMax))
                {
                    g_Texture = 0;
                    runtime.SetObservedComponent(g_Component, g_Drawable);
                }
                else
                    runtime.SetObservedComponent(g_Component, g_Drawable);

                const bool textureReady = snapshot.textureQueryDrawable == g_Drawable;
                if (textureReady)
                {
                    const int textureMax = std::max(0, snapshot.textureCount - 1);
                    g_Texture = std::clamp(g_Texture, 0, textureMax);
                    ImGui::SliderInt("Texture", &g_Texture, 0, textureMax);
                }
                else
                {
                    g_Texture = 0;
                    ImGui::TextDisabled("Reading textures for drawable %d...", g_Drawable);
                }

                ImGui::SliderInt("Palette", &g_Palette, 0, 3);
                if (ImGui::Button("Apply component", ImVec2(-1.0f, 0.0f)))
                    g_Message = runtime.QueueSetComponent(g_Component, g_Drawable, g_Texture, g_Palette) ? "Component queued" : "Component rejected";
            }

            if (ImGui::Button("Default outfit", ImVec2(180.0f, 0.0f)))
                g_Message = runtime.QueueDefaultComponents() ? "Default outfit queued" : "Default outfit rejected";
            ImGui::SameLine();
            if (ImGui::Button("Random outfit", ImVec2(-1.0f, 0.0f)))
                g_Message = runtime.QueueRandomComponents() ? "Random outfit queued" : "Random outfit rejected";

            ImGui::TextDisabled("%s", g_Message);
            RenderOperationStatus(snapshot);
        }
    }

    void RenderPlayerPanel(std::size_t subtab) noexcept
    {
        auto& runtime = PlayerRuntime::Get();
        const PlayerSnapshot snapshot = runtime.Snapshot();
        SyncPlayerUi(snapshot);

        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##player_panel", ImVec2(490.0f, 430.0f), true))
        {
            ImGui::TextColored(Accent, "Player");
            ImGui::SameLine();
            ImGui::TextDisabled("%s", subtab == 0 ? "General" : subtab == 1 ? "Movement" : "Appearance");
            ImGui::Separator();

            if (!runtime.IsRunning())
                ImGui::TextDisabled("Player runtime is offline.");
            else if (!snapshot.valid)
                ImGui::TextDisabled("Waiting for a valid local player snapshot.");
            else if (subtab == 0)
                RenderGeneral(runtime, snapshot);
            else if (subtab == 1)
                RenderMovement(runtime, snapshot);
            else
                RenderAppearance(runtime, snapshot);
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
