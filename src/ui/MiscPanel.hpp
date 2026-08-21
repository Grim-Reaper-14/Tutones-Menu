#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/network/NetworkRuntime.hpp"
#include "../features/player/PlayerRuntime.hpp"
#include "../game/MiscNatives.hpp"
#include "../game/PlayerNatives.hpp"
#include "../game/native/NativeInvoker.hpp"
#include "../runtime/GameRuntime.hpp"

#include <imgui.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>

namespace Tutones::UI
{
    namespace MiscPanelDetail
    {
        enum class ActionState : int
        {
            None,
            Queued,
            Succeeded,
            Failed,
        };

        inline std::atomic<bool> g_ShowCoordinates{false};
        inline std::atomic<bool> g_ShowHeading{false};
        inline std::atomic<bool> g_ShowFps{false};
        inline std::atomic<bool> g_ShowSessionInfo{false};
        inline std::atomic<bool> g_DisableCameraShake{false};

        inline std::atomic<bool> g_PlayerSampleValid{false};
        inline std::atomic<float> g_PlayerX{};
        inline std::atomic<float> g_PlayerY{};
        inline std::atomic<float> g_PlayerZ{};
        inline std::atomic<float> g_PlayerHeading{};
        inline std::atomic<float> g_GameplayFov{};
        inline std::atomic<int> g_ClockHours{-1};
        inline std::atomic<int> g_ClockMinutes{-1};

        inline std::atomic<ActionState> g_ActionState{ActionState::None};
        inline const char* g_ActionLabel{"None"};
        inline std::chrono::steady_clock::time_point g_NextSample{};

        inline int g_SetHour{12};
        inline int g_SetMinute{};
        inline int g_WeatherIndex{};
        inline bool g_FreezeClock{};
        inline bool g_Blackout{};
        inline bool g_AquaLungs{};
        inline bool g_InfiniteOxygen{};

        constexpr std::array<const char*, 15> WeatherNames{{
            "EXTRASUNNY", "CLEAR", "CLOUDS", "SMOG", "FOGGY",
            "OVERCAST", "RAIN", "THUNDER", "CLEARING", "NEUTRAL",
            "SNOW", "BLIZZARD", "SNOWLIGHT", "XMAS", "HALLOWEEN",
        }};

        template<typename Fn>
        bool QueueAction(const char* label, Fn&& action)
        {
            g_ActionLabel = label ? label : "Misc action";
            g_ActionState.store(ActionState::Queued, std::memory_order_release);
            if (!Runtime::GameRuntime::Get().Enqueue([fn = std::forward<Fn>(action)]() mutable {
                    const bool success = fn();
                    g_ActionState.store(
                        success ? ActionState::Succeeded : ActionState::Failed,
                        std::memory_order_release);
                }))
            {
                g_ActionState.store(ActionState::Failed, std::memory_order_release);
                return false;
            }
            return true;
        }

        inline void QueueSample(bool force = false) noexcept
        {
            const auto now = std::chrono::steady_clock::now();
            if (!force && g_NextSample != std::chrono::steady_clock::time_point{} && now < g_NextSample)
                return;
            g_NextSample = now + std::chrono::milliseconds(250);

            static_cast<void>(Runtime::GameRuntime::Get().Enqueue([] {
                const auto ped = Game::PlayerNatives::PlayerPedId();
                if (ped && *ped != 0)
                {
                    const auto coords = Game::Native::NativeInvoker::Invoke<Game::Native::NativeVector3>(
                        Game::Native::NativeId::GetEntityCoords,
                        *ped,
                        std::int32_t{0});
                    const auto heading = Game::Native::NativeInvoker::Invoke<float>(
                        Game::Native::NativeId::GetEntityHeading,
                        *ped);
                    if (coords && heading
                        && std::isfinite(coords->x) && std::isfinite(coords->y) && std::isfinite(coords->z)
                        && std::isfinite(*heading))
                    {
                        g_PlayerX.store(coords->x, std::memory_order_release);
                        g_PlayerY.store(coords->y, std::memory_order_release);
                        g_PlayerZ.store(coords->z, std::memory_order_release);
                        g_PlayerHeading.store(*heading, std::memory_order_release);
                        g_PlayerSampleValid.store(true, std::memory_order_release);
                    }
                    else
                    {
                        g_PlayerSampleValid.store(false, std::memory_order_release);
                    }
                }
                else
                {
                    g_PlayerSampleValid.store(false, std::memory_order_release);
                }

                if (const auto fov = Game::MiscNatives::GetGameplayCamFov(); fov && std::isfinite(*fov))
                    g_GameplayFov.store(*fov, std::memory_order_release);
                if (const auto hours = Game::MiscNatives::GetClockHours())
                    g_ClockHours.store(*hours, std::memory_order_release);
                if (const auto minutes = Game::MiscNatives::GetClockMinutes())
                    g_ClockMinutes.store(*minutes, std::memory_order_release);

                if (g_DisableCameraShake.load(std::memory_order_acquire))
                    static_cast<void>(Game::MiscNatives::StopGameplayCamShaking(true));
            }));
        }

        inline void RenderActionStatus() noexcept
        {
            const auto state = g_ActionState.load(std::memory_order_acquire);
            const char* text = "No Misc action has run yet.";
            switch (state)
            {
            case ActionState::Queued: text = "queued"; break;
            case ActionState::Succeeded: text = "success"; break;
            case ActionState::Failed: text = "failed"; break;
            default: break;
            }

            if (state == ActionState::None)
                ImGui::TextDisabled("%s", text);
            else
                ImGui::TextDisabled("%s: %s", g_ActionLabel, text);
        }

        inline void RenderGeneral() noexcept
        {
            auto& network = Game::NetworkFeatures::NetworkRuntime::Get();
            const auto networkSnapshot = network.Snapshot();
            auto& playerRuntime = Game::PlayerFeatures::PlayerRuntime::Get();
            const auto player = playerRuntime.Snapshot();

            ImGui::TextColored(V11Theme::Accent, "Gameplay conveniences");
            bool silenceCalls = networkSnapshot.silencePhoneCalls;
            if (ImGui::Checkbox("Disable incoming phone calls", &silenceCalls))
                network.SetSilencePhoneCalls(silenceCalls);
            DescribeLastV11Item("Suppress incoming GTA Online phone calls using the existing reversible network quality-of-life runtime.");

            bool mobileRadio = player.mobileRadio;
            if (ImGui::Checkbox("Mobile radio", &mobileRadio))
                playerRuntime.SetMobileRadio(mobileRadio);
            DescribeLastV11Item("Keep GTA's mobile radio available during gameplay while on foot.");

            ImGui::Spacing();
            if (ImGui::Button("Skip active cutscene", ImVec2(-1.0f, 0.0f)))
            {
                static_cast<void>(QueueAction("Skip cutscene", [] {
                    return Game::Native::NativeInvoker::InvokeVoid(
                        Game::Native::NativeId::StopCutsceneImmediately);
                }));
            }
            DescribeLastV11Item("Immediately stop the currently active cutscene through the verified Enhanced native registry.");

            if (ImGui::Button("Skip conversation line", ImVec2(-1.0f, 0.0f)))
            {
                static_cast<void>(QueueAction("Skip conversation", [] {
                    return Game::Native::NativeInvoker::InvokeVoid(
                        Game::Native::NativeId::SkipToNextScriptedConversationLine);
                }));
            }
            DescribeLastV11Item("Advance the active scripted conversation to its next line.");

            ImGui::Spacing();
            RenderActionStatus();
        }

        inline void RenderHud() noexcept
        {
            ImGui::TextColored(V11Theme::Accent, "Tutones HUD overlay");
            ImGui::TextWrapped("These read-only overlays remain visible while the main menu is closed.");
            ImGui::Separator();

            bool coordinates = g_ShowCoordinates.load(std::memory_order_acquire);
            if (ImGui::Checkbox("Coordinates", &coordinates))
                g_ShowCoordinates.store(coordinates, std::memory_order_release);
            DescribeLastV11Item("Show the local player's live world coordinates in the Tutones overlay.");

            bool heading = g_ShowHeading.load(std::memory_order_acquire);
            if (ImGui::Checkbox("Heading", &heading))
                g_ShowHeading.store(heading, std::memory_order_release);
            DescribeLastV11Item("Show the local player's current world heading in degrees.");

            bool fps = g_ShowFps.load(std::memory_order_acquire);
            if (ImGui::Checkbox("FPS", &fps))
                g_ShowFps.store(fps, std::memory_order_release);
            DescribeLastV11Item("Show Dear ImGui's rolling renderer frame-rate estimate.");

            bool session = g_ShowSessionInfo.load(std::memory_order_acquire);
            if (ImGui::Checkbox("Session info", &session))
                g_ShowSessionInfo.store(session, std::memory_order_release);
            DescribeLastV11Item("Show Online session state, player count and the current Freemode host slot when available.");

            QueueSample(true);
            ImGui::Spacing();
            ImGui::TextDisabled("Overlay player telemetry refreshes at 250 ms.");
        }

        inline void RenderWorld() noexcept
        {
            QueueSample();
            const int currentHour = g_ClockHours.load(std::memory_order_acquire);
            const int currentMinute = g_ClockMinutes.load(std::memory_order_acquire);

            ImGui::TextColored(V11Theme::Accent, "Time");
            if (currentHour >= 0 && currentMinute >= 0)
                ImGui::Text("Current local clock: %02d:%02d", currentHour, currentMinute);
            else
                ImGui::TextDisabled("Current local clock: unavailable");

            ImGui::SliderInt("Hour", &g_SetHour, 0, 23);
            ImGui::SliderInt("Minute", &g_SetMinute, 0, 59);
            if (ImGui::Button("Apply time", ImVec2(-1.0f, 0.0f)))
            {
                const int hour = g_SetHour;
                const int minute = g_SetMinute;
                static_cast<void>(QueueAction("Set clock time", [hour, minute] {
                    return Game::MiscNatives::SetClockTime(hour, minute, 0);
                }));
            }

            if (ImGui::Checkbox("Freeze time", &g_FreezeClock))
            {
                const bool enabled = g_FreezeClock;
                static_cast<void>(QueueAction(enabled ? "Freeze clock" : "Resume clock", [enabled] {
                    return Game::MiscNatives::PauseClock(enabled);
                }));
            }
            DescribeLastV11Item("Pause or resume the local GTA clock. Disable this before unloading the menu to restore normal clock progression.");

            ImGui::Separator();
            ImGui::TextColored(V11Theme::Accent, "Weather");
            ImGui::Combo("Weather type", &g_WeatherIndex, WeatherNames.data(), static_cast<int>(WeatherNames.size()));
            if (ImGui::Button("Apply weather", ImVec2(-1.0f, 0.0f)))
            {
                const char* weather = WeatherNames[static_cast<std::size_t>(g_WeatherIndex)];
                static_cast<void>(QueueAction("Set weather", [weather] {
                    return Game::MiscNatives::SetWeatherTypeNowPersist(weather);
                }));
            }

            if (ImGui::Checkbox("Blackout", &g_Blackout))
            {
                const bool enabled = g_Blackout;
                static_cast<void>(QueueAction(enabled ? "Enable blackout" : "Disable blackout", [enabled] {
                    return Game::MiscNatives::SetArtificialLightsState(enabled);
                }));
            }
            DescribeLastV11Item("Toggle the local artificial-light blackout state using the current Enhanced native mapping.");

            ImGui::Spacing();
            RenderActionStatus();
        }

        inline void RenderCameraUtilities() noexcept
        {
            QueueSample();
            const float fov = g_GameplayFov.load(std::memory_order_acquire);

            ImGui::TextColored(V11Theme::Accent, "Camera");
            if (fov > 0.0f && std::isfinite(fov))
                ImGui::Text("Gameplay camera FOV: %.2f", fov);
            else
                ImGui::TextDisabled("Gameplay camera FOV: unavailable");
            ImGui::TextDisabled("FOV is read-only until the scripted-camera lifecycle backend is added.");

            bool disableShake = g_DisableCameraShake.load(std::memory_order_acquire);
            if (ImGui::Checkbox("Disable camera shake", &disableShake))
                g_DisableCameraShake.store(disableShake, std::memory_order_release);
            DescribeLastV11Item("Continuously stop gameplay-camera shake while enabled; no camera object is created or retained.");

            if (ImGui::Button("Stop camera shake now", ImVec2(-1.0f, 0.0f)))
            {
                static_cast<void>(QueueAction("Stop camera shake", [] {
                    return Game::MiscNatives::StopGameplayCamShaking(true);
                }));
            }

            ImGui::BeginDisabled();
            ImGui::Button("Freecam - backend pending", ImVec2(-1.0f, 0.0f));
            ImGui::EndDisabled();
            DescribeLastV11Item("Freecam stays disabled until a dedicated scripted-camera create/activate/destroy lifecycle is implemented.");

            ImGui::Separator();
            ImGui::TextColored(V11Theme::Accent, "Utilities");
            auto& playerRuntime = Game::PlayerFeatures::PlayerRuntime::Get();
            const auto player = playerRuntime.Snapshot();

            bool keepClean = player.keepPlayerClean;
            if (ImGui::Checkbox("Keep player clean", &keepClean))
                playerRuntime.SetKeepPlayerClean(keepClean);
            DescribeLastV11Item("Continuously clear blood, wetness, environmental dirt and visible player damage.");

            bool criticalHits = player.disableCriticalHits;
            if (ImGui::Checkbox("Disable critical hits", &criticalHits))
                playerRuntime.SetDisableCriticalHits(criticalHits);
            DescribeLastV11Item("Disable critical-hit damage on the local player while preserving normal damage types.");

            bool parachutes = player.infiniteParachutes;
            if (ImGui::Checkbox("Infinite parachutes", &parachutes))
                playerRuntime.SetInfiniteParachutes(parachutes);
            DescribeLastV11Item("Maintain a reserve parachute and restore the parachute gadget when needed.");

            if (ImGui::Checkbox("Aqua lungs", &g_AquaLungs))
                playerRuntime.SetAquaLungs(g_AquaLungs);
            DescribeLastV11Item("Continuously refill the local player's underwater breath bar.");

            if (ImGui::Checkbox("Infinite oxygen", &g_InfiniteOxygen))
                playerRuntime.SetInfiniteOxygen(g_InfiniteOxygen);
            DescribeLastV11Item("Extend the local player's underwater time while enabled; disabling restores GTA's normal underwater limit.");

            if (ImGui::Button("Clear blood / dirt / wetness", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(playerRuntime.QueueClearDamage());
            DescribeLastV11Item("Run the existing verified player cleanup action once.");

            ImGui::Spacing();
            RenderActionStatus();
        }
    }

    inline void RenderMiscOverlay() noexcept
    {
        using namespace MiscPanelDetail;
        const bool coordinates = g_ShowCoordinates.load(std::memory_order_acquire);
        const bool heading = g_ShowHeading.load(std::memory_order_acquire);
        const bool fps = g_ShowFps.load(std::memory_order_acquire);
        const bool session = g_ShowSessionInfo.load(std::memory_order_acquire);
        const bool noShake = g_DisableCameraShake.load(std::memory_order_acquire);

        if (!coordinates && !heading && !fps && !session && !noShake)
            return;

        QueueSample();
        if (!coordinates && !heading && !fps && !session)
            return;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        if (!viewport)
            return;

        ImGui::SetNextWindowPos(
            ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 12.0f, viewport->WorkPos.y + 12.0f),
            ImGuiCond_Always,
            ImVec2(1.0f, 0.0f));
        ImGui::SetNextWindowBgAlpha(0.62f);
        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
            | ImGuiWindowFlags_AlwaysAutoResize
            | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoFocusOnAppearing
            | ImGuiWindowFlags_NoNav
            | ImGuiWindowFlags_NoInputs;

        if (ImGui::Begin("##tutones_misc_overlay", nullptr, flags))
        {
            ImGui::TextColored(V11Theme::Accent, "TUTONES");
            if (coordinates || heading)
            {
                if (g_PlayerSampleValid.load(std::memory_order_acquire))
                {
                    if (coordinates)
                    {
                        ImGui::Text("XYZ  %.1f  %.1f  %.1f",
                            g_PlayerX.load(std::memory_order_acquire),
                            g_PlayerY.load(std::memory_order_acquire),
                            g_PlayerZ.load(std::memory_order_acquire));
                    }
                    if (heading)
                        ImGui::Text("Heading  %.1f deg", g_PlayerHeading.load(std::memory_order_acquire));
                }
                else
                {
                    ImGui::TextDisabled("Player telemetry unavailable");
                }
            }

            if (fps)
                ImGui::Text("FPS  %.1f", ImGui::GetIO().Framerate);

            if (session)
            {
                const auto network = Game::NetworkFeatures::NetworkRuntime::Get().Snapshot();
                if (network.sessionStarted)
                {
                    ImGui::Text("Online  %d / 32", network.playerRoster.activeCount);
                    if (network.playerRoster.freemodeHost >= 0)
                        ImGui::Text("Freemode host  %d", network.playerRoster.freemodeHost);
                }
                else
                {
                    ImGui::TextDisabled("Online session inactive");
                }
            }
        }
        ImGui::End();
    }

    inline void RenderMiscPanel(std::size_t subtab) noexcept
    {
        using namespace MiscPanelDetail;
        const std::size_t index = subtab < 4 ? subtab : 0;
        constexpr const char* names[] = {"General", "HUD", "World", "Camera & Utilities"};

        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);
        if (ImGui::BeginChild("##misc_panel", ImVec2(490.0f, 430.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Misc");
            ImGui::SameLine();
            ImGui::TextDisabled("%s", names[index]);
            ImGui::Separator();

            if (index == 0)
                RenderGeneral();
            else if (index == 1)
                RenderHud();
            else if (index == 2)
                RenderWorld();
            else
                RenderCameraUtilities();
        }
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
