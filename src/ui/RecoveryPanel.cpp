#include "RecoveryPanel.hpp"

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/recovery/RecoveryRuntime.hpp"
#include "../features/recovery/UnlockCatalog.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstddef>

namespace Tutones::UI
{
    namespace
    {
        using Game::Recovery::EnhancedRankUnlocks;
        using Game::Recovery::HighestMappedRank;
        using Game::Recovery::RecoveryAction;
        using Game::Recovery::RecoveryRuntime;
        using Game::Recovery::RecoverySnapshot;
        using Game::Recovery::UnlockCategory;

        const ImVec4 Accent = V11Theme::Accent;

        std::uint64_t g_LastRevision{};
        std::array<int, 5> g_WarehouseCrates{};
        int g_BunkerSupplies{};
        int g_BunkerProduct{};
        float g_RpMultiplier{1.0f};
        bool g_RpEnabled{};
        const char* g_Message{"Ready"};

        [[nodiscard]] const char* ActionName(RecoveryAction action) noexcept
        {
            switch (action)
            {
            case RecoveryAction::None: return "None";
            case RecoveryAction::SetWarehouseCrates: return "Special Cargo crates";
            case RecoveryAction::SetBunkerSupplies: return "Bunker supplies";
            case RecoveryAction::SetBunkerProduct: return "Bunker product";
            }
            return "Unknown";
        }

        [[nodiscard]] const char* CategoryName(UnlockCategory category) noexcept
        {
            switch (category)
            {
            case UnlockCategory::Activities: return "Activity";
            case UnlockCategory::Weapons: return "Weapon";
            case UnlockCategory::WeaponUpgrades: return "Weapon upgrade";
            case UnlockCategory::VehiclePaints: return "Vehicle paint";
            }
            return "Unknown";
        }

        void SyncUi(const RecoverySnapshot& snapshot) noexcept
        {
            if (snapshot.revision == g_LastRevision)
                return;

            g_LastRevision = snapshot.revision;
            for (std::size_t index = 0; index < snapshot.warehouses.size(); ++index)
            {
                const auto& warehouse = snapshot.warehouses[index];
                if (warehouse.readable)
                    g_WarehouseCrates[index] = std::max(0, warehouse.crates);
            }

            if (snapshot.bunker.readable)
            {
                g_BunkerSupplies = std::clamp(snapshot.bunker.supplies, 0, 100);
                g_BunkerProduct = std::clamp(snapshot.bunker.product, 0, 100);
            }

            g_RpMultiplier = snapshot.requestedRpMultiplier;
            g_RpEnabled = snapshot.rpMultiplierEnabled;
        }

        void RenderLastAction(const RecoverySnapshot& snapshot) noexcept
        {
            ImGui::Separator();
            if (snapshot.lastAction == RecoveryAction::None)
            {
                ImGui::TextDisabled("No Recovery action has run yet.");
                return;
            }

            ImGui::Text(
                "Last action: %s - %s",
                ActionName(snapshot.lastAction),
                snapshot.lastActionSucceeded ? "success" : "failed");
            ImGui::TextDisabled("Target: %d   Value: %d", snapshot.lastActionTarget, snapshot.lastActionValue);
        }

        void RenderOverview(const RecoverySnapshot& snapshot) noexcept
        {
            ImGui::TextUnformatted("Enhanced Recovery runtime");
            ImGui::Separator();
            ImGui::Text("Native runtime: %s", snapshot.nativeReady ? "ready" : "waiting");
            ImGui::Text("GTA Online session: %s", snapshot.sessionStarted ? "active" : "not active");
            ImGui::Text("Character stats: %s", snapshot.statsReady ? "readable" : "waiting / partial");
            if (snapshot.characterIndex >= 0)
                ImGui::Text("MP character slot: MP%d", snapshot.characterIndex);
            if (snapshot.unlockRankReady)
                ImGui::Text("Online rank: %d", snapshot.onlineRank);

            ImGui::Spacing();
            ImGui::Text("RP multiplier: %s", snapshot.rpMultiplierReady ? "ready" : "waiting");
            if (snapshot.rpMultiplierReady)
                ImGui::Text("Observed XP multiplier: %.2fx", snapshot.observedRpMultiplier);

            int ownedWarehouses{};
            for (const auto& warehouse : snapshot.warehouses)
                ownedWarehouses += warehouse.owned ? 1 : 0;
            ImGui::Text("Special Cargo warehouses: %d / 5", ownedWarehouses);
            ImGui::Text("Bunker: %s", snapshot.bunker.owned ? (snapshot.bunker.setup ? "owned / setup" : "owned / setup pending") : "not owned");

            ImGui::Spacing();
            ImGui::TextDisabled("Business values are read from the active MP character slot.");
            ImGui::TextDisabled("Writes are rejected unless the matching business is owned and readable.");
            RenderLastAction(snapshot);
        }

        void RenderRpMultiplier(RecoveryRuntime& runtime, const RecoverySnapshot& snapshot) noexcept
        {
            ImGui::TextUnformatted("RP Multiplier");
            ImGui::Separator();

            if (!snapshot.rpMultiplierReady)
            {
                ImGui::TextDisabled("Waiting for the Enhanced XP_MULTIPLIER global.");
                return;
            }

            if (ImGui::Checkbox("Override RP multiplier", &g_RpEnabled))
            {
                runtime.SetRpMultiplierEnabled(g_RpEnabled);
                g_Message = g_RpEnabled ? "RP multiplier enabled" : "RP multiplier restored";
            }
            DescribeLastV11Item("Override GTA Enhanced's XP_MULTIPLIER while enabled and restore the prior value when disabled.");

            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::SliderFloat("##rp_multiplier", &g_RpMultiplier, 0.0f, 100.0f, "%.2fx"))
                runtime.SetRpMultiplier(g_RpMultiplier);
            DescribeLastV11Item("Choose the RP multiplier written to the current Enhanced XP_MULTIPLIER tunable global.");

            ImGui::Text("Observed multiplier: %.2fx", snapshot.observedRpMultiplier);
            ImGui::TextDisabled("0x suppresses RP. The control is capped at 100x in the UI.");
            ImGui::TextDisabled("%s", g_Message);
        }

        void RenderSpecialCargo(RecoveryRuntime& runtime, const RecoverySnapshot& snapshot) noexcept
        {
            ImGui::TextColored(Accent, "Special Cargo");
            bool anyOwned{};
            for (std::size_t index = 0; index < snapshot.warehouses.size(); ++index)
            {
                const auto& warehouse = snapshot.warehouses[index];
                if (!warehouse.owned)
                    continue;

                anyOwned = true;
                ImGui::PushID(static_cast<int>(index));
                ImGui::Text("Warehouse %d  |  Property %d", warehouse.slot + 1, warehouse.propertyId);

                if (!warehouse.readable || warehouse.capacity <= 0)
                {
                    ImGui::TextDisabled("Warehouse state is not readable yet.");
                    ImGui::PopID();
                    continue;
                }

                g_WarehouseCrates[index] = std::clamp(g_WarehouseCrates[index], 0, warehouse.capacity);
                ImGui::SetNextItemWidth(300.0f);
                ImGui::SliderInt("Crates", &g_WarehouseCrates[index], 0, warehouse.capacity);
                DescribeLastV11Item("Choose the Special Cargo crate count for this owned warehouse.");
                ImGui::SameLine();

                const bool blocked = snapshot.actionPending || !snapshot.statsReady;
                ImGui::BeginDisabled(blocked);
                if (ImGui::Button("Apply", ImVec2(92.0f, 0.0f)))
                    g_Message = runtime.QueueSetWarehouseCrates(static_cast<int>(index), g_WarehouseCrates[index]) ? "Cargo write queued" : "Cargo write rejected";
                ImGui::EndDisabled();
                DescribeLastV11Item("Write the selected crate count, then read the stat back to verify the operation.");
                ImGui::TextDisabled("Current: %d / %d", warehouse.crates, warehouse.capacity);
                ImGui::Separator();
                ImGui::PopID();
            }

            if (!anyOwned)
                ImGui::TextDisabled("No owned Special Cargo warehouse was detected.");
        }

        void RenderBunker(RecoveryRuntime& runtime, const RecoverySnapshot& snapshot) noexcept
        {
            ImGui::TextColored(Accent, "Bunker");
            if (!snapshot.bunker.owned)
            {
                ImGui::TextDisabled("No owned Bunker was detected.");
                return;
            }
            if (!snapshot.bunker.readable)
            {
                ImGui::TextDisabled("Bunker state is not readable yet.");
                return;
            }
            if (!snapshot.bunker.setup)
            {
                ImGui::TextDisabled("The owned Bunker has not completed setup.");
                return;
            }

            ImGui::Text("Property ID: %d", snapshot.bunker.propertyId);
            g_BunkerSupplies = std::clamp(g_BunkerSupplies, 0, 100);
            ImGui::SetNextItemWidth(300.0f);
            ImGui::SliderInt("Supplies", &g_BunkerSupplies, 0, 100);
            DescribeLastV11Item("Choose the Bunker material/supply total for the owned and setup Bunker.");
            ImGui::SameLine();
            ImGui::BeginDisabled(snapshot.actionPending || !snapshot.statsReady);
            if (ImGui::Button("Apply##bunker_supplies", ImVec2(92.0f, 0.0f)))
                g_Message = runtime.QueueSetBunkerSupplies(g_BunkerSupplies) ? "Bunker supplies queued" : "Bunker supplies rejected";
            ImGui::EndDisabled();
            DescribeLastV11Item("Write the Bunker supply stat and read it back to verify the requested value.");
            ImGui::TextDisabled("Current supplies: %d / 100", snapshot.bunker.supplies);

            g_BunkerProduct = std::clamp(g_BunkerProduct, 0, 100);
            ImGui::SetNextItemWidth(300.0f);
            ImGui::SliderInt("Product", &g_BunkerProduct, 0, 100);
            DescribeLastV11Item("Choose the Bunker product total for the owned and setup Bunker.");
            ImGui::SameLine();
            ImGui::BeginDisabled(snapshot.actionPending || !snapshot.statsReady);
            if (ImGui::Button("Apply##bunker_product", ImVec2(92.0f, 0.0f)))
                g_Message = runtime.QueueSetBunkerProduct(g_BunkerProduct) ? "Bunker product queued" : "Bunker product rejected";
            ImGui::EndDisabled();
            DescribeLastV11Item("Write the Bunker product stat and read it back to verify the requested value.");
            ImGui::TextDisabled("Current product: %d / 100", snapshot.bunker.product);
        }

        void RenderBusinesses(RecoveryRuntime& runtime, const RecoverySnapshot& snapshot) noexcept
        {
            if (!snapshot.sessionStarted)
            {
                ImGui::TextDisabled("Join GTA Online to read business state.");
                return;
            }

            RenderSpecialCargo(runtime, snapshot);
            ImGui::Spacing();
            RenderBunker(runtime, snapshot);
            ImGui::Spacing();
            ImGui::TextDisabled("%s", g_Message);
            RenderLastAction(snapshot);
        }

        void RenderUnlocks(const RecoverySnapshot& snapshot) noexcept
        {
            ImGui::TextUnformatted("Enhanced Unlock Manager");
            ImGui::Separator();

            if (!snapshot.sessionStarted)
            {
                ImGui::TextDisabled("Join GTA Online to read the active character's unlock progression.");
                SetV11Description("The Enhanced unlock manager reads the active MP character and only exposes conditions that have been verified for the current game build.");
                return;
            }

            if (!snapshot.unlockRankReady)
            {
                ImGui::TextDisabled("Waiting for MPX_CHAR_RANK_FM from the active character.");
                SetV11Description("Tutones will not infer unlock state when the active character rank cannot be read safely.");
                return;
            }

            std::size_t unlockedCount{};
            for (const auto& entry : EnhancedRankUnlocks)
                unlockedCount += snapshot.onlineRank >= entry.minimumRank ? 1u : 0u;

            ImGui::Text("Online rank: %d", snapshot.onlineRank);
            ImGui::Text("Verified rank gates satisfied: %zu / %zu", unlockedCount, EnhancedRankUnlocks.size());
            const float progress = HighestMappedRank > 0
                ? std::clamp(static_cast<float>(snapshot.onlineRank) / static_cast<float>(HighestMappedRank), 0.0f, 1.0f)
                : 1.0f;
            ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f));
            DescribeLastV11Item("Progress through the rank gates currently mapped from GTA V Enhanced 1.73 mp_unlocks.c.");

            ImGui::Spacing();
            ImGui::SeparatorText("Verified rank gates");
            if (ImGui::BeginChild("##enhanced_unlock_rank_list", ImVec2(0.0f, 230.0f), true))
            {
                for (const auto& entry : EnhancedRankUnlocks)
                {
                    const bool unlocked = snapshot.onlineRank >= entry.minimumRank;
                    if (unlocked)
                        ImGui::TextColored(Accent, "READY");
                    else
                        ImGui::TextDisabled("LOCKED");
                    ImGui::SameLine(72.0f);
                    ImGui::Text("Rank %d  %s", entry.minimumRank, entry.label);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(%s)", CategoryName(entry.category));
                }
            }
            ImGui::EndChild();

            ImGui::Spacing();
            ImGui::TextWrapped("Write-based unlock packs remain disabled until each Enhanced packed-stat or script condition is mapped and read-back validated. Tutones will not run blind legacy unlock loops.");
            SetV11Description("Enhanced 1.73 unlock progression sourced from verified mp_unlocks rank gates. Packed-stat write packs remain opt-in only after per-group validation is implemented.");
        }
    }

    void RenderRecoveryPanel(std::size_t subtab) noexcept
    {
        auto& runtime = RecoveryRuntime::Get();
        const RecoverySnapshot snapshot = runtime.Snapshot();
        SyncUi(snapshot);

        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##recovery_panel", ImVec2(490.0f, 430.0f), true))
        {
            ImGui::TextColored(Accent, "Recovery");
            ImGui::SameLine();
            const char* title = subtab == 0 ? "Overview" : subtab == 1 ? "RP Multiplier" : subtab == 2 ? "Businesses" : "Unlocks";
            ImGui::TextDisabled("%s", title);
            ImGui::Separator();

            if (!runtime.IsRunning())
                ImGui::TextDisabled("Recovery runtime is offline.");
            else if (subtab == 0)
                RenderOverview(snapshot);
            else if (subtab == 1)
                RenderRpMultiplier(runtime, snapshot);
            else if (subtab == 2)
                RenderBusinesses(runtime, snapshot);
            else
                RenderUnlocks(snapshot);
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
