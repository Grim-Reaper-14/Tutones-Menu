namespace Tutones::UI
{
    namespace
    {
        struct V12GuideRect final
        {
            ImVec2 min{};
            ImVec2 max{};
        };

        [[nodiscard]] V12GuideRect BeginV12GuideCard(const char* title, const char* subtitle) noexcept
        {
            const ImVec2 windowPos = ImGui::GetWindowPos();
            const V12GuideRect rect{
                ImVec2(windowPos.x + 724.0f, windowPos.y + 18.0f),
                ImVec2(windowPos.x + 1008.0f, windowPos.y + 315.0f)};
            auto* draw = ImGui::GetWindowDrawList();

            draw->AddRectFilled(rect.min, rect.max, IM_COL32(10, 13, 18, 248), 7.0f);
            draw->AddRect(rect.min, rect.max, IM_COL32(77, 82, 91, 92), 7.0f, 0, 1.0f);
            draw->AddRectFilled(
                rect.min,
                ImVec2(rect.min.x + 3.0f, rect.max.y),
                ImGui::GetColorU32(ImVec4(V11Theme::Accent.x, V11Theme::Accent.y, V11Theme::Accent.z, 0.80f)),
                3.0f);
            draw->AddText(
                FontOrDefault(g_Bold),
                13.0f,
                ImVec2(rect.min.x + 16.0f, rect.min.y + 15.0f),
                ImGui::GetColorU32(V11Theme::Accent),
                title ? title : "PAGE STATUS");
            if (subtitle && *subtitle)
            {
                draw->AddText(
                    FontOrDefault(g_Medium),
                    11.0f,
                    ImVec2(rect.min.x + 16.0f, rect.min.y + 36.0f),
                    ImGui::GetColorU32(V11Theme::MutedText),
                    subtitle);
            }
            draw->AddLine(
                ImVec2(rect.min.x + 16.0f, rect.min.y + 58.0f),
                ImVec2(rect.max.x - 16.0f, rect.min.y + 58.0f),
                ImGui::GetColorU32(V11Theme::Separator),
                1.0f);
            return rect;
        }

        void DrawV12GuideRow(const V12GuideRect& rect, float& y, const char* label, const char* value, bool good = false) noexcept
        {
            auto* draw = ImGui::GetWindowDrawList();
            draw->AddText(FontOrDefault(g_Medium), 11.0f, ImVec2(rect.min.x + 16.0f, y), ImGui::GetColorU32(V11Theme::MutedText), label);
            const char* safe = value ? value : "N/A";
            const float width = ImGui::CalcTextSize(safe).x;
            draw->AddText(
                FontOrDefault(g_Medium),
                11.0f,
                ImVec2(rect.max.x - 16.0f - width, y),
                good ? IM_COL32(46, 224, 105, 255) : IM_COL32(239, 242, 247, 255),
                safe);
            y += 22.0f;
        }

        void DrawV12GenericGuide() noexcept
        {
            const auto rect = BeginV12GuideCard("PAGE GUIDE", "Current feature context");
            auto* draw = ImGui::GetWindowDrawList();
            const char* description = CurrentV11Description();
            if (!description)
                description = "Use the controls on this page to manage the selected feature.";
            draw->AddText(
                FontOrDefault(g_Medium),
                12.0f,
                ImVec2(rect.min.x + 16.0f, rect.min.y + 75.0f),
                ImGui::GetColorU32(V11Theme::MutedText),
                description,
                nullptr,
                252.0f);
            draw->AddLine(
                ImVec2(rect.min.x + 16.0f, rect.max.y - 48.0f),
                ImVec2(rect.max.x - 16.0f, rect.max.y - 48.0f),
                ImGui::GetColorU32(V11Theme::Separator),
                1.0f);
            draw->AddText(
                FontOrDefault(g_Medium),
                11.0f,
                ImVec2(rect.min.x + 16.0f, rect.max.y - 34.0f),
                ImGui::GetColorU32(V11Theme::MutedText),
                "Hover controls for live help on the right rail.");
        }

        void DrawV12VehicleGuide() noexcept
        {
            const auto state = Game::Mods::VehicleModificationRuntime::Get().Snapshot();
            const auto rect = BeginV12GuideCard("VEHICLE STATUS", "Live workshop snapshot");
            float y = rect.min.y + 76.0f;
            char vehicle[24]{};
            char wheel[24]{};
            char mod[24]{};
            char action[24]{};
            std::snprintf(vehicle, sizeof(vehicle), "%d", state.vehicle);
            std::snprintf(wheel, sizeof(wheel), "%d", state.wheelType);
            std::snprintf(mod, sizeof(mod), "%d / %d", state.currentMod, state.modCount);
            std::snprintf(action, sizeof(action), "%d", static_cast<int>(state.lastAction));
            DrawV12GuideRow(rect, y, "Vehicle", state.valid ? vehicle : "NONE", state.valid);
            DrawV12GuideRow(rect, y, "Wheel family", state.valid ? wheel : "N/A");
            DrawV12GuideRow(rect, y, "Observed mod", state.valid ? mod : "N/A");
            DrawV12GuideRow(rect, y, "Turbo", state.turbo ? "ON" : "OFF", state.turbo);
            DrawV12GuideRow(rect, y, "Xenon", state.xenon ? "ON" : "OFF", state.xenon);
            DrawV12GuideRow(rect, y, "Low grip", state.driftTyres ? "ON" : "OFF", state.driftTyres);
            DrawV12GuideRow(rect, y, "Spawn pending", state.spawnPending ? "YES" : "NO", !state.spawnPending);
            DrawV12GuideRow(rect, y, "Last action ID", action, state.lastActionSucceeded);
        }

        void DrawV12WorldGuide() noexcept
        {
            const auto state = Game::World::WorldRuntime::Get().Snapshot();
            const auto rect = BeginV12GuideCard("WORLD STATUS", "Live world-control state");
            float y = rect.min.y + 76.0f;
            char clock[24]{};
            char density[24]{};
            if (state.clockHour >= 0 && state.clockMinute >= 0)
                std::snprintf(clock, sizeof(clock), "%02d:%02d", state.clockHour, state.clockMinute);
            else
                std::snprintf(clock, sizeof(clock), "N/A");
            std::snprintf(density, sizeof(density), "%.2f / %.2f", state.pedDensity, state.vehicleDensity);
            DrawV12GuideRow(rect, y, "Clock", clock);
            DrawV12GuideRow(rect, y, "Freeze time", state.freezeClock ? "ON" : "OFF", state.freezeClock);
            DrawV12GuideRow(rect, y, "Weather", state.weatherOverrideActive ? state.weatherCode.c_str() : "GAME CONTROL");
            DrawV12GuideRow(rect, y, "Blackout", state.blackout ? "ON" : "OFF", state.blackout);
            DrawV12GuideRow(rect, y, "Ped / traffic", density);
            DrawV12GuideRow(rect, y, "Density loop", state.densityLoopRunning ? "ACTIVE" : "IDLE", state.densityLoopRunning);
            DrawV12GuideRow(rect, y, "World loop", state.worldControlLoopRunning ? "ACTIVE" : "IDLE", state.worldControlLoopRunning);
            DrawV12GuideRow(rect, y, "Last result", state.haveResult ? (state.lastSucceeded ? "SUCCESS" : "FAILED") : "NONE", state.lastSucceeded);
        }

        void DrawV12RecoveryGuide() noexcept
        {
            const auto state = Game::Recovery::RecoveryRuntime::Get().Snapshot();
            const auto rect = BeginV12GuideCard("RECOVERY STATUS", "Live character and business state");
            float y = rect.min.y + 76.0f;
            char character[24]{};
            char rank[24]{};
            char rp[24]{};
            int warehouses{};
            for (const auto& warehouse : state.warehouses)
                warehouses += warehouse.owned ? 1 : 0;
            char warehouseText[24]{};
            std::snprintf(character, sizeof(character), state.characterIndex >= 0 ? "MP%d" : "N/A", state.characterIndex);
            std::snprintf(rank, sizeof(rank), "%d", state.onlineRank);
            std::snprintf(rp, sizeof(rp), "%.2fx", state.observedRpMultiplier);
            std::snprintf(warehouseText, sizeof(warehouseText), "%d / 5", warehouses);
            DrawV12GuideRow(rect, y, "Runtime", state.running ? "ONLINE" : "OFFLINE", state.running);
            DrawV12GuideRow(rect, y, "Online session", state.sessionStarted ? "ACTIVE" : "OFFLINE", state.sessionStarted);
            DrawV12GuideRow(rect, y, "Character", character);
            DrawV12GuideRow(rect, y, "Rank", state.unlockRankReady ? rank : "WAITING");
            DrawV12GuideRow(rect, y, "RP multiplier", state.rpMultiplierReady ? rp : "WAITING");
            DrawV12GuideRow(rect, y, "Warehouses", warehouseText);
            DrawV12GuideRow(rect, y, "Bunker", state.bunker.owned ? (state.bunker.setup ? "READY" : "SETUP") : "NONE", state.bunker.owned && state.bunker.setup);
            DrawV12GuideRow(rect, y, "Action pending", state.actionPending ? "YES" : "NO", !state.actionPending);
        }

        void DrawV12NetworkGuide() noexcept
        {
            const auto state = Game::NetworkFeatures::NetworkRuntime::Get().Snapshot();
            const auto rect = BeginV12GuideCard("ONLINE STATUS", "Live session and player backend");
            float y = rect.min.y + 76.0f;
            char players[24]{};
            char local[24]{};
            char host[24]{};
            std::snprintf(players, sizeof(players), "%d", state.playerRoster.activeCount);
            std::snprintf(local, sizeof(local), "%d", state.playerRoster.localPlayer);
            std::snprintf(host, sizeof(host), "%d", state.playerRoster.freemodeHost);
            DrawV12GuideRow(rect, y, "Runtime", state.running ? "ONLINE" : "OFFLINE", state.running);
            DrawV12GuideRow(rect, y, "Session", state.sessionStarted ? "ACTIVE" : "OFFLINE", state.sessionStarted);
            DrawV12GuideRow(rect, y, "Players", players);
            DrawV12GuideRow(rect, y, "Local ID", local);
            DrawV12GuideRow(rect, y, "Freemode host", host);
            DrawV12GuideRow(rect, y, "Globals", state.scriptGlobalsReady ? "READY" : "WAITING", state.scriptGlobalsReady);
            DrawV12GuideRow(rect, y, "Phone silence", state.silencePhoneCalls ? "ON" : "OFF", state.silencePhoneCalls);
            DrawV12GuideRow(rect, y, "Death barrier", state.disableDeathBarriers ? "OFF" : "NORMAL");
        }

        void DrawV12UtilitiesGuide() noexcept
        {
            const auto rect = BeginV12GuideCard("UTILITY STATUS", "Persistent local convenience state");
            float y = rect.min.y + 76.0f;
            DrawV12GuideRow(rect, y, "Coordinates", MiscPanelDetail::g_ShowCoordinates.load(std::memory_order_acquire) ? "ON" : "OFF");
            DrawV12GuideRow(rect, y, "Heading", MiscPanelDetail::g_ShowHeading.load(std::memory_order_acquire) ? "ON" : "OFF");
            DrawV12GuideRow(rect, y, "FPS overlay", MiscPanelDetail::g_ShowFps.load(std::memory_order_acquire) ? "ON" : "OFF");
            DrawV12GuideRow(rect, y, "Session overlay", MiscPanelDetail::g_ShowSessionInfo.load(std::memory_order_acquire) ? "ON" : "OFF");
            DrawV12GuideRow(rect, y, "Camera shake", MiscPanelDetail::g_DisableCameraShake.load(std::memory_order_acquire) ? "DISABLED" : "NORMAL");
            char clock[24]{};
            const int hour = MiscPanelDetail::g_ClockHours.load(std::memory_order_acquire);
            const int minute = MiscPanelDetail::g_ClockMinutes.load(std::memory_order_acquire);
            if (hour >= 0 && minute >= 0)
                std::snprintf(clock, sizeof(clock), "%02d:%02d", hour, minute);
            else
                std::snprintf(clock, sizeof(clock), "N/A");
            DrawV12GuideRow(rect, y, "GTA clock", clock);
        }

        template<typename GuideFn, typename Fn>
        void RenderV12PageSurfaceWithGuide(GuideFn&& guide, Fn&& render) noexcept
        {
            const ImVec2 windowPos = ImGui::GetWindowPos();
            const float height = ImGui::GetWindowHeight();
            auto* draw = ImGui::GetWindowDrawList();

            const ImVec2 min{windowPos.x + 206.0f, windowPos.y + 7.0f};
            const ImVec2 max{windowPos.x + 1025.0f, windowPos.y + std::max(505.0f, height - 12.0f)};
            draw->AddRectFilled(min, max, IM_COL32(8, 11, 16, 246), 8.0f);
            draw->AddRect(min, max, IM_COL32(77, 82, 91, 92), 8.0f, 0, 1.0f);
            draw->AddRectFilled(
                ImVec2(min.x + 1.0f, min.y + 1.0f),
                ImVec2(min.x + 4.0f, max.y - 1.0f),
                ImGui::GetColorU32(ImVec4(V11Theme::Accent.x, V11Theme::Accent.y, V11Theme::Accent.z, 0.78f)),
                4.0f);
            draw->AddLine(
                ImVec2(min.x + 16.0f, min.y + 1.0f),
                ImVec2(min.x + 174.0f, min.y + 1.0f),
                ImGui::GetColorU32(ImVec4(V11Theme::Accent.x, V11Theme::Accent.y, V11Theme::Accent.z, 0.55f)),
                1.5f);

            guide();

            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
            ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);
            render();
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(3);
        }

        template<typename Fn>
        void RenderV12PageSurface(Fn&& render) noexcept
        {
            RenderV12PageSurfaceWithGuide([] { DrawV12GenericGuide(); }, std::forward<Fn>(render));
        }

        inline void RenderV12PlayerPanel(std::size_t page) noexcept { RenderV12PageSurface([&] { RenderPlayerPanel(page); }); }
        inline void RenderV12PlayerOnlinePanel() noexcept { RenderV12PageSurface([] { RenderPlayerOnlinePanel(); }); }
        inline void RenderV12WeaponPanel(std::size_t page) noexcept { RenderV12PageSurface([&] { RenderWeaponPanel(page); }); }
        inline void RenderV12VehicleGeneralPanel() noexcept { RenderV12PageSurfaceWithGuide([] { DrawV12VehicleGuide(); }, [] { RenderVehicleGeneralPanel(); }); }
        inline void RenderV12VehiclePaintPanel() noexcept { RenderV12PageSurfaceWithGuide([] { DrawV12VehicleGuide(); }, [] { RenderVehiclePaintPanel(); }); }
        inline void RenderV12VehicleModificationPanel() noexcept { RenderV12PageSurfaceWithGuide([] { DrawV12VehicleGuide(); }, [] { RenderVehicleModificationPanel(); }); }
        inline void RenderV12PersonalVehiclePanel() noexcept { RenderV12PageSurfaceWithGuide([] { DrawV12VehicleGuide(); }, [] { RenderPersonalVehiclePanel(); }); }
        inline void RenderV12GamePanel(std::size_t page) noexcept { RenderV12PageSurface([&] { RenderGamePanel(page); }); }
        inline void RenderV12NetworkPanel(std::size_t page) noexcept { RenderV12PageSurfaceWithGuide([] { DrawV12NetworkGuide(); }, [&] { RenderNetworkPanel(page); }); }
        inline void RenderV12WorldPanel(std::size_t page) noexcept { RenderV12PageSurfaceWithGuide([] { DrawV12WorldGuide(); }, [&] { RenderWorldPanel(page); }); }
        inline void RenderV12BusinessPanel() noexcept { RenderV12PageSurface([] { RenderBusinessPanel(); }); }
        inline void RenderV12RecoveryPanel(std::size_t page) noexcept { RenderV12PageSurfaceWithGuide([] { DrawV12RecoveryGuide(); }, [&] { RenderRecoveryPanel(page); }); }
        inline void RenderV12CasinoPanel() noexcept { RenderV12PageSurface([] { RenderCasinoLuckyWheelPanel(); }); }
        inline void RenderV12ProtectionPanel(std::size_t page) noexcept { RenderV12PageSurface([&] { RenderProtectionPanel(page); }); }
        inline void RenderV12SettingsPanel(std::size_t page) noexcept { RenderV12PageSurface([&] { RenderSettingsPanel(page); }); }
        inline void RenderV12MiscPanel(std::size_t page) noexcept { RenderV12PageSurfaceWithGuide([] { DrawV12UtilitiesGuide(); }, [&] { RenderMiscPanel(page); }); }
        inline void RenderV12NativeToolsPanel(std::size_t page) noexcept { RenderV12PageSurface([&] { RenderNativeToolsPanel(page); }); }
    }
}
