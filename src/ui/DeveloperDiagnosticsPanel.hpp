#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../backend/BackendHub.hpp"
#include "../features/game/NoIdleRuntime.hpp"
#include "../features/native/EnhancedNativeToolkit.hpp"
#include "../features/recovery/CasinoSlotMachineRuntime.hpp"
#include "../game/NetworkPlayerNatives.hpp"
#include "../game/native/NativeRegistry.hpp"
#include "../game/script/ScriptRuntime.hpp"
#include "../game/tunables/TunableRegistry.hpp"
#include "../runtime/GameRuntime.hpp"

#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace Tutones::UI
{
    namespace DeveloperDiagnosticsDetail
    {
        inline char g_NativeProbeHash[32] = "0xB0D77D90171EC35F";

        inline char g_TunableFilter[96]{};
        inline bool g_TunableFavoritesOnly{};
        inline std::unordered_set<std::uint32_t> g_TunableFavorites{};
        inline std::uint32_t g_SelectedTunableHash{};
        inline std::int32_t g_TunableEditInt{};
        inline bool g_TunableEditBool{};
        inline float g_TunableEditFloat{};
        inline std::int64_t g_TunableEditRaw{};
        inline char g_TunableAction[160] = "Select a registered tunable";

        inline char g_ScriptFilter[96]{};
        inline std::uint32_t g_SelectedScriptHash{};
        inline std::uint32_t g_SelectedScriptThreadId{};
        inline int g_ScriptLocalIndex{1675};
        inline std::atomic<bool> g_ScriptHostPending{false};
        inline std::atomic<int> g_ScriptHost{-2};
        inline std::mutex g_ScriptHostMutex;
        inline std::string g_ScriptHostMessage{"Host not queried"};

        [[nodiscard]] inline std::uint64_t ParseHash64(const char* text) noexcept
        {
            if (!text || text[0] == '\0')
                return 0;
            char* end{};
            const auto value = std::strtoull(text, &end, 0);
            return end != text ? static_cast<std::uint64_t>(value) : 0;
        }

        [[nodiscard]] inline bool ContainsInsensitive(std::string_view haystack, std::string_view needle)
        {
            if (needle.empty())
                return true;
            if (needle.size() > haystack.size())
                return false;

            for (std::size_t offset = 0; offset + needle.size() <= haystack.size(); ++offset)
            {
                bool match = true;
                for (std::size_t index = 0; index < needle.size(); ++index)
                {
                    const auto left = static_cast<unsigned char>(haystack[offset + index]);
                    const auto right = static_cast<unsigned char>(needle[index]);
                    if (std::tolower(left) != std::tolower(right))
                    {
                        match = false;
                        break;
                    }
                }
                if (match)
                    return true;
            }
            return false;
        }

        [[nodiscard]] inline std::int32_t RawInt(std::int64_t raw) noexcept
        {
            std::int32_t value{};
            std::memcpy(&value, &raw, sizeof(value));
            return value;
        }

        [[nodiscard]] inline float RawFloat(std::int64_t raw) noexcept
        {
            float value{};
            std::memcpy(&value, &raw, sizeof(value));
            return value;
        }

        inline void LoadTunableEditor(const Game::Tunables::TunableEntrySnapshot& entry) noexcept
        {
            g_SelectedTunableHash = entry.hash;
            g_TunableEditRaw = entry.currentRawValue;
            g_TunableEditInt = RawInt(entry.currentRawValue);
            g_TunableEditBool = RawInt(entry.currentRawValue) != 0;
            g_TunableEditFloat = RawFloat(entry.currentRawValue);
            std::snprintf(g_TunableAction, sizeof(g_TunableAction), "Loaded 0x%08X", entry.hash);
        }

        [[nodiscard]] inline const char* ScriptStateName(Game::Types::ScriptThreadState state) noexcept
        {
            switch (state)
            {
            case Game::Types::ScriptThreadState::Idle: return "IDLE";
            case Game::Types::ScriptThreadState::Running: return "RUNNING";
            case Game::Types::ScriptThreadState::Killed: return "KILLED";
            case Game::Types::ScriptThreadState::Paused: return "PAUSED";
            case Game::Types::ScriptThreadState::Unknown4: return "UNKNOWN4";
            }
            return "UNKNOWN";
        }

        inline void ResetScriptHostState()
        {
            g_ScriptHost.store(-2, std::memory_order_release);
            std::scoped_lock lock(g_ScriptHostMutex);
            g_ScriptHostMessage = "Host not queried";
        }

        inline bool QueueScriptHostLookup(std::string scriptName)
        {
            if (scriptName.empty())
                return false;

            bool expected = false;
            if (!g_ScriptHostPending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            {
                std::scoped_lock lock(g_ScriptHostMutex);
                g_ScriptHostMessage = "Querying script host on GTA thread";
            }

            if (Runtime::GameRuntime::Get().Enqueue([scriptName = std::move(scriptName)] {
                    const auto host = Game::NetworkPlayerNatives::GetHostOfScript(scriptName.c_str(), -1, 0);
                    g_ScriptHost.store(host ? *host : -1, std::memory_order_release);
                    {
                        std::scoped_lock lock(g_ScriptHostMutex);
                        g_ScriptHostMessage = host
                            ? std::string("Current host player: ") + std::to_string(*host)
                            : "Host lookup unavailable for this script";
                    }
                    g_ScriptHostPending.store(false, std::memory_order_release);
                }))
            {
                return true;
            }

            g_ScriptHostPending.store(false, std::memory_order_release);
            std::scoped_lock lock(g_ScriptHostMutex);
            g_ScriptHostMessage = "Game-thread queue unavailable";
            return false;
        }

        inline void RenderNativeDiagnostics(
            Game::NativeTools::EnhancedNativeToolkit& toolkit,
            const Game::NativeTools::ToolkitSnapshot& snapshot)
        {
            ImGui::TextColored(V11Theme::Accent, "Enhanced Native Probe");
            ImGui::TextWrapped("Resolve a current Enhanced native hash without invoking the handler.");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##developer_native_hash", "0x0123456789ABCDEF", g_NativeProbeHash, sizeof(g_NativeProbeHash));
            const auto hash = ParseHash64(g_NativeProbeHash);
            ImGui::TextDisabled("Parsed: 0x%016llX", static_cast<unsigned long long>(hash));
            ImGui::BeginDisabled(snapshot.pending || hash == 0 || !snapshot.nativeReady);
            if (ImGui::Button("Resolve Handler", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(toolkit.QueueProbe(hash));
            ImGui::EndDisabled();

            ImGui::SeparatorText("Last Probe");
            ImGui::Text("Hash: 0x%016llX", static_cast<unsigned long long>(snapshot.lastProbeHash));
            ImGui::Text("Resolved: %s", snapshot.lastProbeResolved ? "YES" : "NO");
            ImGui::Text("Address: 0x%llX", static_cast<unsigned long long>(snapshot.lastProbeAddress));

            ImGui::SeparatorText("Native Runtime");
            ImGui::Text("Registry: %s", Game::Native::NativeRegistry::Get().IsReady() ? "READY" : "WAITING");
            ImGui::Text("Toolkit: %s", snapshot.nativeReady ? "READY" : "WAITING");
            ImGui::TextWrapped("%s", snapshot.message.c_str());
        }

        inline void RenderTunableExplorer()
        {
            auto& registry = Game::Tunables::TunableRegistry::Get();
            const auto state = registry.Snapshot();
            const auto entries = registry.EntriesSnapshot();

            ImGui::TextColored(V11Theme::Accent, "Tunable Explorer");
            ImGui::Text("Registry: %s | %zu mappings", state.initialized ? "READY" : state.caching ? "CACHING" : "WAITING", state.registeredCount);
            ImGui::TextDisabled("%s", state.message.c_str());

            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##tunable_filter", "Name, hash, global or offset", g_TunableFilter, sizeof(g_TunableFilter));
            ImGui::Checkbox("Favorites only", &g_TunableFavoritesOnly);

            std::vector<std::size_t> matches;
            matches.reserve(entries.size());
            const std::string_view filter{g_TunableFilter};
            const auto filterHash = filter.empty() ? 0u : Game::Tunables::Joaat(filter);
            for (std::size_t index = 0; index < entries.size(); ++index)
            {
                const auto& entry = entries[index];
                if (g_TunableFavoritesOnly && !g_TunableFavorites.contains(entry.hash))
                    continue;

                char identity[128]{};
                std::snprintf(
                    identity,
                    sizeof(identity),
                    "0x%08X %zu %zu %s %s",
                    entry.hash,
                    entry.globalIndex,
                    entry.offset,
                    entry.name.c_str(),
                    Game::Tunables::TunableValueTypeName(entry.type));
                if (!filter.empty()
                    && entry.hash != filterHash
                    && !ContainsInsensitive(identity, filter))
                {
                    continue;
                }
                matches.push_back(index);
            }

            ImGui::TextDisabled("Showing %zu / %zu", matches.size(), entries.size());
            if (ImGui::BeginChild("##tunable_registry_list", ImVec2(0.0f, 150.0f), true))
            {
                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(matches.size()));
                while (clipper.Step())
                {
                    for (int visible = clipper.DisplayStart; visible < clipper.DisplayEnd; ++visible)
                    {
                        const auto& entry = entries[matches[static_cast<std::size_t>(visible)]];
                        char label[192]{};
                        const bool favorite = g_TunableFavorites.contains(entry.hash);
                        if (entry.name.empty())
                        {
                            std::snprintf(
                                label,
                                sizeof(label),
                                "%c 0x%08X  +%zu  %s",
                                favorite ? '*' : ' ',
                                entry.hash,
                                entry.offset,
                                Game::Tunables::TunableValueTypeName(entry.type));
                        }
                        else
                        {
                            std::snprintf(
                                label,
                                sizeof(label),
                                "%c %s  0x%08X  +%zu",
                                favorite ? '*' : ' ',
                                entry.name.c_str(),
                                entry.hash,
                                entry.offset);
                        }

                        ImGui::PushID(static_cast<int>(entry.hash));
                        if (ImGui::Selectable(label, g_SelectedTunableHash == entry.hash))
                            LoadTunableEditor(entry);
                        ImGui::PopID();
                    }
                }
            }
            ImGui::EndChild();

            const Game::Tunables::TunableEntrySnapshot* selected{};
            for (const auto& entry : entries)
            {
                if (entry.hash == g_SelectedTunableHash)
                {
                    selected = &entry;
                    break;
                }
            }

            if (!selected)
            {
                ImGui::TextDisabled("Select a tunable to inspect or edit it.");
                return;
            }

            ImGui::SeparatorText("Selected Tunable");
            ImGui::Text("Hash 0x%08X | Global_%zu | Global_262145.f_%zu", selected->hash, selected->globalIndex, selected->offset);
            ImGui::Text("Type: %s | Readable: %s", Game::Tunables::TunableValueTypeName(selected->type), selected->readable ? "YES" : "NO");
            ImGui::Text("Current raw: 0x%016llX", static_cast<unsigned long long>(selected->currentRawValue));
            ImGui::Text("Original raw: 0x%016llX", static_cast<unsigned long long>(selected->originalRawValue));

            bool favorite = g_TunableFavorites.contains(selected->hash);
            if (ImGui::Checkbox("Favorite", &favorite))
            {
                if (favorite)
                    g_TunableFavorites.insert(selected->hash);
                else
                    g_TunableFavorites.erase(selected->hash);
            }

            switch (selected->type)
            {
            case Game::Tunables::TunableValueType::Int:
                ImGui::InputInt("Edit INT", &g_TunableEditInt, 0, 0);
                break;
            case Game::Tunables::TunableValueType::Bool:
                ImGui::Checkbox("Edit BOOL", &g_TunableEditBool);
                break;
            case Game::Tunables::TunableValueType::Float:
                ImGui::InputFloat("Edit FLOAT", &g_TunableEditFloat, 0.0f, 0.0f, "%.6f");
                break;
            case Game::Tunables::TunableValueType::Unknown:
                ImGui::InputScalar("Edit RAW", ImGuiDataType_S64, &g_TunableEditRaw);
                break;
            }

            if (ImGui::Button("Apply", ImVec2(142.0f, 0.0f)))
            {
                bool queued{};
                switch (selected->type)
                {
                case Game::Tunables::TunableValueType::Int:
                    queued = registry.QueueSetInt(selected->hash, g_TunableEditInt);
                    break;
                case Game::Tunables::TunableValueType::Bool:
                    queued = registry.QueueSetBool(selected->hash, g_TunableEditBool);
                    break;
                case Game::Tunables::TunableValueType::Float:
                    queued = registry.QueueSetFloat(selected->hash, g_TunableEditFloat);
                    break;
                case Game::Tunables::TunableValueType::Unknown:
                    queued = registry.QueueSetRaw(selected->hash, g_TunableEditRaw);
                    break;
                }
                std::snprintf(g_TunableAction, sizeof(g_TunableAction), queued ? "Edit queued on GTA script thread" : "Edit queue rejected");
            }
            ImGui::SameLine();
            if (ImGui::Button("Restore", ImVec2(142.0f, 0.0f)))
            {
                const bool queued = registry.QueueRestore(selected->hash);
                std::snprintf(g_TunableAction, sizeof(g_TunableAction), queued ? "Original value restore queued" : "Restore queue rejected");
            }
            ImGui::SameLine();
            if (ImGui::Button("Reload", ImVec2(-1.0f, 0.0f)))
                LoadTunableEditor(*selected);
            ImGui::TextDisabled("%s", g_TunableAction);
        }

        inline void RenderScriptInspector()
        {
            auto& runtime = Game::Script::ScriptRuntime::Get();
            const auto threads = runtime.ThreadsSnapshot();

            ImGui::TextColored(V11Theme::Accent, "Script Inspector");
            ImGui::Text("Runtime: %s | Active threads: %zu", runtime.IsReady() ? "READY" : "WAITING", threads.size());
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##script_filter", "Script name or hash", g_ScriptFilter, sizeof(g_ScriptFilter));

            std::vector<std::size_t> matches;
            matches.reserve(threads.size());
            const std::string_view filter{g_ScriptFilter};
            for (std::size_t index = 0; index < threads.size(); ++index)
            {
                const auto& thread = threads[index];
                char identity[128]{};
                std::snprintf(identity, sizeof(identity), "%s 0x%08X %u", thread.scriptName.c_str(), thread.scriptHash, thread.threadId);
                if (filter.empty() || ContainsInsensitive(identity, filter))
                    matches.push_back(index);
            }

            if (ImGui::BeginChild("##script_thread_list", ImVec2(0.0f, 145.0f), true))
            {
                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(matches.size()));
                while (clipper.Step())
                {
                    for (int visible = clipper.DisplayStart; visible < clipper.DisplayEnd; ++visible)
                    {
                        const auto& thread = threads[matches[static_cast<std::size_t>(visible)]];
                        char label[192]{};
                        std::snprintf(
                            label,
                            sizeof(label),
                            "%s%s  [0x%08X]  T%u",
                            thread.scriptName.empty() ? "<unnamed>" : thread.scriptName.c_str(),
                            thread.state == Game::Types::ScriptThreadState::Running ? " *" : "",
                            thread.scriptHash,
                            thread.threadId);
                        const bool selected = g_SelectedScriptHash == thread.scriptHash
                            && g_SelectedScriptThreadId == thread.threadId;
                        ImGui::PushID(static_cast<int>(thread.threadId));
                        if (ImGui::Selectable(label, selected))
                        {
                            g_SelectedScriptHash = thread.scriptHash;
                            g_SelectedScriptThreadId = thread.threadId;
                            ResetScriptHostState();
                        }
                        ImGui::PopID();
                    }
                }
            }
            ImGui::EndChild();

            const Game::Script::ScriptThreadSnapshot* selected{};
            for (const auto& thread : threads)
            {
                if (thread.scriptHash == g_SelectedScriptHash && thread.threadId == g_SelectedScriptThreadId)
                {
                    selected = &thread;
                    break;
                }
            }
            if (!selected)
            {
                ImGui::TextDisabled("Select a running script to inspect its thread and locals.");
                return;
            }

            ImGui::SeparatorText("Selected Script");
            ImGui::Text("%s | hash 0x%08X | thread %u", selected->scriptName.empty() ? "<unnamed>" : selected->scriptName.c_str(), selected->scriptHash, selected->threadId);
            ImGui::Text("State %s | PC %u | FP %u | SP %u / %u", ScriptStateName(selected->state), selected->programCounter, selected->framePointer, selected->stackPointer, selected->stackSize);
            ImGui::Text("Program: %s | code %u | locals %u | globals %u | natives %u",
                selected->programLoaded ? "LOADED" : "MISSING",
                selected->codeSize,
                selected->localCount,
                selected->globalCount,
                selected->nativeCount);

            ImGui::BeginDisabled(selected->scriptName.empty() || g_ScriptHostPending.load(std::memory_order_acquire));
            if (ImGui::Button("Refresh Script Host", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(QueueScriptHostLookup(selected->scriptName));
            ImGui::EndDisabled();
            {
                std::scoped_lock lock(g_ScriptHostMutex);
                ImGui::TextDisabled("%s", g_ScriptHostMessage.c_str());
            }

            ImGui::SeparatorText("Local Viewer");
            ImGui::InputInt("Local index", &g_ScriptLocalIndex, 1, 10);
            g_ScriptLocalIndex = std::max(0, g_ScriptLocalIndex);
            const auto raw = runtime.ReadLocalRaw(selected->scriptHash, static_cast<std::size_t>(g_ScriptLocalIndex));
            if (!raw)
            {
                ImGui::TextDisabled("Local is unavailable or outside this thread's stack.");
            }
            else
            {
                std::int32_t intValue{};
                float floatValue{};
                std::memcpy(&intValue, &*raw, sizeof(intValue));
                std::memcpy(&floatValue, &*raw, sizeof(floatValue));
                ImGui::Text("Raw: 0x%016llX", static_cast<unsigned long long>(*raw));
                ImGui::Text("INT: %d | BOOL: %s | FLOAT: %.6f", intValue, intValue != 0 ? "true" : "false", floatValue);
            }
        }

        inline void RenderBackendHealth()
        {
            const auto backend = Backend::BackendHub::Get().Snapshot();
            const auto tunables = Game::Tunables::TunableRegistry::Get().Snapshot();
            const auto slots = Game::Recovery::CasinoSlotMachineRuntime::Get().Snapshot();
            const auto idle = Game::SessionFeatures::NoIdleRuntime::Get().Snapshot();

            ImGui::TextColored(V11Theme::Accent, "Backend Health Dashboard");
            ImGui::Text("BackendHub: %s | heartbeat %llu", backend.initialized ? "READY" : "OFFLINE", static_cast<unsigned long long>(backend.tickSequence));

            if (ImGui::BeginTable("##backend_core_health", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
            {
                const auto row = [](const char* label, bool ready, const char* detail) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(label);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextColored(ready ? ImVec4(0.30f, 0.90f, 0.45f, 1.0f) : V11Theme::MutedText, "%s%s%s", ready ? "READY" : "WAITING", detail && *detail ? " - " : "", detail && *detail ? detail : "");
                };

                row("Game Runtime", backend.context.gameRuntimeReady, "");
                row("Native Registry", Game::Native::NativeRegistry::Get().IsReady(), "");
                row("Script Runtime", backend.context.scriptRuntimeReady, "");
                row("Script Globals", backend.context.scriptGlobalsReady, "");
                row("Script VM", backend.context.scriptVmReady, "");
                row("Online Session", backend.context.sessionStarted, "");
                row("Freemode", backend.context.freemodeRunning, "");
                row("Tunable Registry", tunables.initialized, tunables.message.c_str());
                row("No Idle", idle.ready, idle.message.c_str());
                row("Casino Slots", slots.scriptActive, slots.message.c_str());
                ImGui::EndTable();
            }

            ImGui::SeparatorText("Capabilities");
            for (std::size_t index = 0; index < static_cast<std::size_t>(Backend::Capability::Count); ++index)
            {
                const auto capability = static_cast<Backend::Capability>(index);
                const bool available = backend.capabilities.Has(capability);
                ImGui::TextColored(
                    available ? ImVec4(0.30f, 0.90f, 0.45f, 1.0f) : V11Theme::MutedText,
                    "%s: %s",
                    Backend::CapabilityName(capability),
                    available ? "READY" : "WAITING");
                if (!backend.capabilities.detail[index].empty())
                {
                    ImGui::SameLine();
                    ImGui::TextDisabled("%s", backend.capabilities.detail[index].c_str());
                }
            }

            ImGui::SeparatorText("Registered Features");
            if (ImGui::BeginChild("##backend_feature_health", ImVec2(0.0f, 160.0f), true))
            {
                for (const auto& feature : backend.features)
                {
                    const bool healthy = feature.state == Backend::RuntimeState::Healthy;
                    ImGui::PushID(feature.id.c_str());
                    ImGui::TextColored(
                        healthy ? ImVec4(0.30f, 0.90f, 0.45f, 1.0f) : V11Theme::MutedText,
                        "%s | %s",
                        feature.displayName.c_str(),
                        Backend::RuntimeStateName(feature.state));
                    ImGui::TextDisabled("%s | attempts %llu", feature.detail.c_str(), static_cast<unsigned long long>(feature.startAttempts));
                    ImGui::Separator();
                    ImGui::PopID();
                }
            }
            ImGui::EndChild();

            ImGui::SeparatorText("Casino Rig Telemetry");
            ImGui::Text("Script: %s | spin state %d | valid state %s", slots.scriptActive ? "ACTIVE" : "OFF", slots.spinState, slots.safeSpinState ? "YES" : "NO");
            ImGui::Text("Result 6: %zu / %zu | last writes %zu", slots.forcedWinCount, slots.tableEntryCount, slots.lastWriteCount);
        }
    }

    inline void RenderDeveloperDiagnosticsPanel() noexcept
    {
        using namespace DeveloperDiagnosticsDetail;
        auto& toolkit = Game::NativeTools::EnhancedNativeToolkit::Get();
        toolkit.RequestSample();
        const auto nativeState = toolkit.Snapshot();

        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##developer_diagnostics_panel", ImVec2(490.0f, 430.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Developer Diagnostics");
            ImGui::SameLine();
            ImGui::TextDisabled("V2 live backend console");
            ImGui::Separator();

            if (ImGui::BeginTabBar("##developer_diagnostics_tabs"))
            {
                if (ImGui::BeginTabItem("Native"))
                {
                    RenderNativeDiagnostics(toolkit, nativeState);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Tunables"))
                {
                    RenderTunableExplorer();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Scripts"))
                {
                    RenderScriptInspector();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Backend"))
                {
                    RenderBackendHealth();
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);

        SetV11Description("Tutones Menu V2 developer console: native probe, editable central tunables, live script/thread/local inspection and BackendHub health telemetry.");
    }
}
