#include "RecoveryPanel.hpp"

#include "EnhancedHashCatalogWidget.hpp"
#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/recovery/ClothingUnlockRuntime.hpp"
#include "../features/recovery/RecoveryRuntime.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstddef>

namespace Tutones::UI
{
    namespace
    {
        using Game::Recovery::ClothingUnlockRuntime;
        using Game::Recovery::RecoveryAction;
        using Game::Recovery::RecoveryRuntime;
        using Game::Recovery::RecoverySnapshot;
        namespace ClothingUnlockData = Game::Recovery::ClothingUnlockData;

        const ImVec4 Accent = V11Theme::Accent;

        std::uint64_t g_LastRevision{};
        std::array<int, 5> g_WarehouseCrates{};
        int g_BunkerSupplies{};
        int g_BunkerProduct{};
        int g_PickupAmount{1000};
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
            case RecoveryAction::EarnFromPickup: return "Pickup earnings";
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

            const char* status = snapshot.lastAction == RecoveryAction::EarnFromPickup
                ? (snapshot.lastActionSucceeded ? "dispatched" : "failed")
                : (snapshot.lastActionSucceeded ? "success" : "failed");
            ImGui::Text("Last action: %s - %s", ActionName(snapshot.lastAction), status);

            if (snapshot.lastAction == RecoveryAction::EarnFromPickup)
                ImGui::TextDisabled("Amount: %d", snapshot.lastActionValue);
            else
                ImGui::TextDisabled("Target: %d   Value: %d", snapshot.lastActionTarget, snapshot.lastActionValue);
        }

        void RenderOverview(RecoveryRuntime& runtime, const RecoverySnapshot& snapshot) noexcept
        {
            ImGui::TextUnformatted("Enhanced Recovery runtime");
            ImGui::Separator();
            ImGui::Text("Native runtime: %s", snapshot.nativeReady ? "ready" : "waiting");
            ImGui::Text("GTA Online session: %s", snapshot.sessionStarted ? "active" : "not active");
            ImGui::Text("Character stats: %s", snapshot.statsReady ? "readable" : "waiting / partial");
            if (snapshot.characterIndex >= 0)
                ImGui::Text("MP character slot: MP%d", snapshot.characterIndex);

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
            ImGui::TextColored(Accent, "Money");
            g_PickupAmount = std::max(1, g_PickupAmount);
            ImGui::SetNextItemWidth(280.0f);
            ImGui::InputInt("Pickup amount", &g_PickupAmount, 1000, 10000);
            g_PickupAmount = std::max(1, g_PickupAmount);
            DescribeLastV11Item("Choose the amount passed to GTA Online's NETWORK_EARN_FROM_PICKUP transaction native.");

            ImGui::BeginDisabled(snapshot.actionPending || !snapshot.nativeReady || !snapshot.sessionStarted);
            if (ImGui::Button("Earn from pickup", ImVec2(-1.0f, 0.0f)))
                g_Message = runtime.QueueEarnFromPickup(g_PickupAmount) ? "Pickup earnings queued" : "Pickup earnings rejected";
            ImGui::EndDisabled();
            DescribeLastV11Item("Queue one NETWORK_EARN_FROM_PICKUP call on the GTA script thread using the selected amount.");
            ImGui::TextDisabled("One-shot transaction. GTA Online may accept, reject, or cap amounts according to its current transaction rules.");
            ImGui::TextDisabled("%s", g_Message);

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
            ImGui::Spacing();
            RenderAdditionalTransactionLists(true);
        }

        void RenderUnlocks(const RecoverySnapshot& snapshot) noexcept
        {
            auto& clothing = ClothingUnlockRuntime::Get();
            const auto state = clothing.Snapshot();
            const auto& groups = ClothingUnlockData::Groups();
            const bool blocked = state.pending || !snapshot.nativeReady || !snapshot.sessionStarted;

            ImGui::TextColored(Accent, "Clothing by DLC");
            ImGui::TextWrapped("Enhanced clothing-only packed-stat map with per-DLC unlocks, mixed-range filtering, and read-back verification.");
            ImGui::Separator();

            ImGui::BeginDisabled(blocked);
            if (ImGui::Button("Unlock All Verified Clothing", ImVec2(-1.0f, 0.0f)))
                g_Message = clothing.QueueAll() ? "All clothing unlocks queued" : "Clothing unlock queue rejected";
            ImGui::EndDisabled();
            DescribeLastV11Item("Unlock every mapped clothing, mask, outfit and accessory flag across the verified DLC groups below.");

            if (!snapshot.sessionStarted)
                ImGui::TextDisabled("Join GTA Online before applying clothing unlocks.");
            else if (!snapshot.nativeReady)
                ImGui::TextDisabled("Waiting for the Enhanced native runtime.");

            if (state.pending)
            {
                const float progress = state.total == 0
                    ? 0.0f
                    : static_cast<float>(state.completed) / static_cast<float>(state.total);
                ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f));
                ImGui::TextDisabled("%zu / %zu packed flags checked | failures: %zu",
                    state.completed, state.total, state.failed);
            }
            else if (state.haveResult)
            {
                ImGui::Text("Last clothing pass: %s", state.lastSucceeded ? "verified" : "completed with failures");
                ImGui::TextDisabled("Checked: %zu | failed read-backs: %zu", state.total, state.failed);
            }

            ImGui::TextDisabled("%s", g_Message);
            ImGui::Spacing();
            ImGui::SeparatorText("DLC groups");

            if (ImGui::BeginChild("##clothing_dlc_groups", ImVec2(0.0f, 235.0f), true))
            {
                for (std::size_t index = 0; index < groups.size(); ++index)
                {
                    const auto& group = groups[index];
                    ImGui::PushID(static_cast<int>(index));
                    ImGui::TextWrapped("%s", group.name);
                    ImGui::TextDisabled("%s | %zu flags", group.packedFamily, ClothingUnlockData::Count(group));
                    ImGui::BeginDisabled(blocked);
                    if (ImGui::Button("Unlock this DLC", ImVec2(-1.0f, 0.0f)))
                        g_Message = clothing.QueueGroup(index) ? "DLC clothing unlock queued" : "DLC clothing unlock rejected";
                    ImGui::EndDisabled();
                    DescribeLastV11Item("Set this DLC group's mapped clothing packed-bools on the GTA script thread and verify each one by reading it back.");
                    ImGui::Separator();
                    ImGui::PopID();
                }
            }
            ImGui::EndChild();

            ImGui::TextDisabled("Tattoo-only flags, weapon/vehicle/gameplay unlocks, and unresolved 60000+ packed ranges are intentionally excluded.");
            SetV11Description("Unlock Enhanced clothing by DLC using mapped packed-bool IDs, batched script-thread writes, and read-back verification.");
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
                RenderOverview(runtime, snapshot);
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
