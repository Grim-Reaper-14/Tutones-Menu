#pragma once

#include "../../core/filesystem/FileSystem.hpp"
#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/Natives.hpp"
#include "../../game/native/NativeCallContext.hpp"
#include "../../game/native/NativeInvoker.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <Windows.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace Tutones::Game::NativeTools
{
    struct ToolkitSnapshot final
    {
        bool nativeReady{};
        Hash selectedWeapon{};
        int weaponTint{-1};
        bool freecamEnabled{};
        int freecamHandle{};
        float freecamX{};
        float freecamY{};
        float freecamZ{};
        float freecamPitch{};
        float freecamYaw{};
        float freecamFov{70.0f};
        bool vehiclePerformanceEnabled{};
        int activeBlip{};
        int activeParticle{};
        int currentInterior{};
        std::size_t bodyguardCount{};
        std::uint64_t lastProbeHash{};
        std::uintptr_t lastProbeAddress{};
        bool lastProbeResolved{};
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        std::string message{"Ready"};
    };

    class EnhancedNativeToolkit final
    {
    public:
        static EnhancedNativeToolkit& Get() noexcept
        {
            static EnhancedNativeToolkit instance;
            return instance;
        }

        [[nodiscard]] ToolkitSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            ToolkitSnapshot snapshot = m_Snapshot;
            snapshot.nativeReady = Native::NativeRegistry::Get().IsReady();
            snapshot.freecamEnabled = m_FreecamEnabled.load(std::memory_order_acquire);
            snapshot.freecamFov = m_FreecamFov.load(std::memory_order_acquire);
            snapshot.vehiclePerformanceEnabled = m_PerformanceEnabled.load(std::memory_order_acquire);
            snapshot.pending = m_Pending.load(std::memory_order_acquire);
            snapshot.bodyguardCount = m_Bodyguards.size();
            return snapshot;
        }

        void RequestSample() noexcept
        {
            const auto now = ::GetTickCount64();
            const auto next = m_NextSampleMs.load(std::memory_order_acquire);
            if (now < next)
                return;
            m_NextSampleMs.store(now + 250, std::memory_order_release);

            static_cast<void>(Runtime::GameRuntime::Get().Enqueue([this] {
                const Ped ped = CurrentPed();
                if (ped == 0)
                    return;

                Hash weapon{};
                int tint{-1};
                int interior{};
                if (Call(GetSelectedPedWeapon, weapon, ped) && weapon != 0)
                    static_cast<void>(Call(GetWeaponTint, tint, ped, weapon));
                static_cast<void>(Call(GetInteriorFromEntity, interior, ped));

                std::scoped_lock lock(m_Mutex);
                m_Snapshot.selectedWeapon = weapon;
                m_Snapshot.weaponTint = tint;
                m_Snapshot.currentInterior = interior;
            }));
        }

        [[nodiscard]] bool QueueSetWeaponTint(int tint)
        {
            tint = std::clamp(tint, 0, 31);
            return QueueAction("Weapon tint", [this, tint] {
                const Ped ped = CurrentPed();
                if (ped == 0)
                    return false;

                Hash weapon{};
                if (!Call(GetSelectedPedWeapon, weapon, ped) || weapon == 0)
                    return false;
                if (!CallVoid(SetWeaponTint, ped, weapon, tint))
                    return false;

                int current{-1};
                const bool ok = Call(GetWeaponTint, current, ped, weapon) && current == tint;
                if (ok)
                {
                    std::scoped_lock lock(m_Mutex);
                    m_Snapshot.selectedWeapon = weapon;
                    m_Snapshot.weaponTint = current;
                }
                return ok;
            });
        }

        [[nodiscard]] bool QueueWeaponComponent(std::uint32_t componentHash, bool add)
        {
            if (componentHash == 0)
                return false;

            return QueueAction(add ? "Add weapon component" : "Remove weapon component", [this, componentHash, add] {
                const Ped ped = CurrentPed();
                if (ped == 0)
                    return false;

                Hash weapon{};
                if (!Call(GetSelectedPedWeapon, weapon, ped) || weapon == 0)
                    return false;

                std::int32_t compatible{};
                if (!Call(DoesWeaponTakeComponent, compatible, weapon, componentHash) || compatible == 0)
                    return false;

                const bool dispatched = add
                    ? CallVoid(GiveWeaponComponent, ped, weapon, componentHash)
                    : CallVoid(RemoveWeaponComponent, ped, weapon, componentHash);
                if (!dispatched)
                    return false;

                std::int32_t has{};
                if (!Call(HasWeaponComponent, has, ped, weapon, componentHash))
                    return false;
                return add ? has != 0 : has == 0;
            });
        }

        [[nodiscard]] bool QueueSetProp(int slot, int drawable, int texture)
        {
            if (slot < 0 || slot > 7 || drawable < 0 || texture < 0)
                return false;

            return QueueAction("Set player prop", [this, slot, drawable, texture] {
                const Ped ped = CurrentPed();
                if (ped == 0 || !CallVoid(SetPedPropIndex, ped, slot, drawable, texture, std::int32_t{1}))
                    return false;

                int currentDrawable{-1};
                int currentTexture{-1};
                return Call(GetPedPropIndex, currentDrawable, ped, slot)
                    && Call(GetPedPropTextureIndex, currentTexture, ped, slot)
                    && currentDrawable == drawable
                    && currentTexture == texture;
            });
        }

        [[nodiscard]] bool QueueClearProp(int slot)
        {
            if (slot < 0 || slot > 7)
                return false;

            return QueueAction("Clear player prop", [this, slot] {
                const Ped ped = CurrentPed();
                if (ped == 0 || !CallVoid(ClearPedProp, ped, slot))
                    return false;

                int drawable{};
                return Call(GetPedPropIndex, drawable, ped, slot) && drawable < 0;
            });
        }

        [[nodiscard]] bool QueueSaveOutfit(std::string name)
        {
            name = SanitizeName(std::move(name));
            if (name.empty())
                return false;

            return QueueAction("Save outfit", [this, name = std::move(name)] {
                const Ped ped = CurrentPed();
                if (ped == 0)
                    return false;

                const auto model = Native::NativeInvoker::Invoke<Hash>(Native::NativeId::GetEntityModel, ped);
                if (!model)
                    return false;

                nlohmann::json data;
                data["version"] = 1;
                data["model"] = *model;
                data["components"] = nlohmann::json::array();
                for (int slot = 0; slot < 12; ++slot)
                {
                    const auto drawable = Native::NativeInvoker::Invoke<int>(Native::NativeId::GetPedDrawableVariation, ped, slot);
                    const auto texture = Native::NativeInvoker::Invoke<int>(Native::NativeId::GetPedTextureVariation, ped, slot);
                    const auto palette = Native::NativeInvoker::Invoke<int>(Native::NativeId::GetPedPaletteVariation, ped, slot);
                    if (!drawable || !texture || !palette)
                        return false;
                    data["components"].push_back({
                        {"drawable", *drawable},
                        {"texture", *texture},
                        {"palette", *palette},
                    });
                }

                data["props"] = nlohmann::json::array();
                for (int slot = 0; slot < 8; ++slot)
                {
                    int drawable{-1};
                    int texture{};
                    if (!Call(GetPedPropIndex, drawable, ped, slot))
                        return false;
                    if (drawable >= 0 && !Call(GetPedPropTextureIndex, texture, ped, slot))
                        return false;
                    data["props"].push_back({
                        {"drawable", drawable},
                        {"texture", texture},
                    });
                }

                auto& files = Core::FileSystem::Get();
                const auto directory = OutfitDirectory();
                if (!files.EnsureDirectory(directory))
                    return false;
                return files.WriteText(directory / (name + ".json"), data.dump(2));
            });
        }

        [[nodiscard]] bool QueueLoadOutfit(std::string name)
        {
            name = SanitizeName(std::move(name));
            if (name.empty())
                return false;

            return QueueAction("Load outfit", [this, name = std::move(name)] {
                const Ped ped = CurrentPed();
                if (ped == 0)
                    return false;

                auto& files = Core::FileSystem::Get();
                std::string text;
                if (!files.ReadText(OutfitDirectory() / (name + ".json"), text) || text.empty())
                    return false;

                try
                {
                    const auto data = nlohmann::json::parse(text);
                    const auto currentModel = Native::NativeInvoker::Invoke<Hash>(Native::NativeId::GetEntityModel, ped);
                    const auto savedModel = data.value("model", static_cast<Hash>(0));
                    if (!currentModel || savedModel == 0 || *currentModel != savedModel)
                        return false;

                    const auto& components = data.at("components");
                    const auto& props = data.at("props");
                    if (!components.is_array() || components.size() != 12 || !props.is_array() || props.size() != 8)
                        return false;

                    bool ok = true;
                    for (int slot = 0; slot < 12; ++slot)
                    {
                        const auto& entry = components.at(static_cast<std::size_t>(slot));
                        const int drawable = entry.value("drawable", 0);
                        const int texture = entry.value("texture", 0);
                        const int palette = entry.value("palette", 0);
                        ok = Native::NativeInvoker::InvokeVoid(
                            Native::NativeId::SetPedComponentVariation,
                            ped,
                            slot,
                            drawable,
                            texture,
                            palette) && ok;

                        const auto currentDrawable = Native::NativeInvoker::Invoke<int>(Native::NativeId::GetPedDrawableVariation, ped, slot);
                        const auto currentTexture = Native::NativeInvoker::Invoke<int>(Native::NativeId::GetPedTextureVariation, ped, slot);
                        ok = currentDrawable && currentTexture
                            && *currentDrawable == drawable
                            && *currentTexture == texture
                            && ok;
                    }

                    for (int slot = 0; slot < 8; ++slot)
                    {
                        const auto& entry = props.at(static_cast<std::size_t>(slot));
                        const int drawable = entry.value("drawable", -1);
                        const int texture = entry.value("texture", 0);
                        if (drawable < 0)
                        {
                            ok = CallVoid(ClearPedProp, ped, slot) && ok;
                        }
                        else
                        {
                            ok = CallVoid(SetPedPropIndex, ped, slot, drawable, texture, std::int32_t{1}) && ok;
                        }

                        int currentDrawable{};
                        if (!Call(GetPedPropIndex, currentDrawable, ped, slot))
                            ok = false;
                        else if (drawable < 0)
                            ok = currentDrawable < 0 && ok;
                        else
                        {
                            int currentTexture{};
                            ok = currentDrawable == drawable
                                && Call(GetPedPropTextureIndex, currentTexture, ped, slot)
                                && currentTexture == texture
                                && ok;
                        }
                    }
                    return ok;
                }
                catch (...)
                {
                    return false;
                }
            });
        }

        [[nodiscard]] std::vector<std::string> SavedOutfitNames() const
        {
            std::vector<std::string> names;
            auto& files = Core::FileSystem::Get();
            const auto directory = OutfitDirectory();
            if (!files.IsInitialized() || !files.EnsureDirectory(directory))
                return names;

            for (const auto& path : files.ListFiles(directory, false))
            {
                if (path.extension() == ".json")
                    names.push_back(path.stem().string());
            }
            std::sort(names.begin(), names.end());
            names.erase(std::unique(names.begin(), names.end()), names.end());
            return names;
        }

        [[nodiscard]] bool QueuePlayAnimation(std::string dictionary, std::string name, bool loop, bool upperBody)
        {
            if (dictionary.empty() || name.empty() || m_AnimationLoading.exchange(true, std::memory_order_acq_rel))
                return false;

            {
                std::scoped_lock lock(m_Mutex);
                m_AnimationDictionary = std::move(dictionary);
                m_AnimationName = std::move(name);
                m_AnimationLoop = loop;
                m_AnimationUpperBody = upperBody;
                m_AnimationAttempts = 0;
            }
            SetPending("Loading animation dictionary");
            if (!Runtime::GameRuntime::Get().Enqueue([this] { AnimationLoadTick(); }))
            {
                m_AnimationLoading.store(false, std::memory_order_release);
                Finish(false, "Animation queue unavailable");
                return false;
            }
            return true;
        }

        [[nodiscard]] bool QueueStopAnimation()
        {
            return QueueAction("Stop animation", [this] {
                const Ped ped = CurrentPed();
                return ped != 0 && CallVoid(ClearPedTasks, ped);
            });
        }

        [[nodiscard]] bool SetFreecamEnabled(bool enabled)
        {
            const bool previous = m_FreecamEnabled.exchange(enabled, std::memory_order_acq_rel);
            if (previous == enabled)
                return true;

            SetPending(enabled ? "Starting freecam" : "Stopping freecam");
            if (enabled)
            {
                if (!Runtime::GameRuntime::Get().Enqueue([this] { StartFreecamOnGameThread(); }))
                {
                    m_FreecamEnabled.store(false, std::memory_order_release);
                    Finish(false, "Freecam queue unavailable");
                    return false;
                }
                return true;
            }

            if (!Runtime::GameRuntime::Get().Enqueue([this] { StopFreecamOnGameThread(true); }))
            {
                Finish(false, "Freecam shutdown queue unavailable");
                return false;
            }
            return true;
        }

        void SetFreecamSpeed(float speed) noexcept
        {
            m_FreecamSpeed.store(std::clamp(speed, 0.05f, 20.0f), std::memory_order_release);
        }

        void SetFreecamFov(float fov) noexcept
        {
            m_FreecamFov.store(std::clamp(fov, 20.0f, 120.0f), std::memory_order_release);
        }

        [[nodiscard]] float FreecamSpeed() const noexcept
        {
            return m_FreecamSpeed.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool QueueVehicleDoor(int door, bool open)
        {
            if (door < 0 || door > 7)
                return false;

            return QueueAction(open ? "Open vehicle door" : "Close vehicle door", [this, door, open] {
                const Vehicle vehicle = CurrentVehicle();
                if (vehicle == 0)
                    return false;
                return open
                    ? CallVoid(SetVehicleDoorOpen, vehicle, door, std::int32_t{0}, std::int32_t{0})
                    : CallVoid(SetVehicleDoorShut, vehicle, door, std::int32_t{0});
            });
        }

        [[nodiscard]] bool QueueVehicleWindow(int window, bool down)
        {
            if (window < 0 || window > 7)
                return false;

            return QueueAction(down ? "Roll window down" : "Roll window up", [this, window, down] {
                const Vehicle vehicle = CurrentVehicle();
                if (vehicle == 0)
                    return false;
                return down ? CallVoid(RollDownWindow, vehicle, window) : CallVoid(RollUpWindow, vehicle, window);
            });
        }

        [[nodiscard]] bool QueueVehicleEngine(bool enabled)
        {
            return QueueAction(enabled ? "Start vehicle engine" : "Stop vehicle engine", [this, enabled] {
                const Vehicle vehicle = CurrentVehicle();
                return vehicle != 0 && CallVoid(
                    SetVehicleEngineOn,
                    vehicle,
                    static_cast<std::int32_t>(enabled),
                    std::int32_t{1},
                    std::int32_t{1});
            });
        }

        [[nodiscard]] bool QueueVehicleLights(int state)
        {
            state = std::clamp(state, 0, 3);
            return QueueAction("Vehicle lights", [this, state] {
                const Vehicle vehicle = CurrentVehicle();
                return vehicle != 0 && CallVoid(SetVehicleLights, vehicle, state);
            });
        }

        void ConfigureVehiclePerformance(bool enabled, float topSpeedModifier, float torque, bool reduceGrip) noexcept
        {
            m_PerformanceTopSpeed.store(std::clamp(topSpeedModifier, -100.0f, 500.0f), std::memory_order_release);
            m_PerformanceTorque.store(std::clamp(torque, 0.1f, 10.0f), std::memory_order_release);
            m_PerformanceReduceGrip.store(reduceGrip, std::memory_order_release);
            m_PerformanceEnabled.store(enabled, std::memory_order_release);

            if (enabled)
            {
                bool expected = false;
                if (m_PerformanceTicking.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                    static_cast<void>(Runtime::GameRuntime::Get().Enqueue([this] { PerformanceTick(); }));
            }
            else
            {
                static_cast<void>(Runtime::GameRuntime::Get().Enqueue([this] { RestorePerformance(); }));
            }
        }

        [[nodiscard]] bool QueueAddPlayerBlip(int sprite, int color, bool route)
        {
            sprite = std::clamp(sprite, 1, 1000);
            color = std::clamp(color, 0, 100);
            return QueueAction("Create player marker", [this, sprite, color, route] {
                RemoveActiveBlipOnGameThread();
                const Ped ped = CurrentPed();
                if (ped == 0)
                    return false;
                const auto coords = Native::NativeInvoker::Invoke<Native::NativeVector3>(Native::NativeId::GetEntityCoords, ped, std::int32_t{0});
                if (!coords)
                    return false;

                int blip{};
                if (!Call(AddBlipForCoord, blip, coords->x, coords->y, coords->z) || blip == 0)
                    return false;
                const bool ok = CallVoid(SetBlipSprite, blip, sprite)
                    && CallVoid(SetBlipColour, blip, color)
                    && CallVoid(SetBlipRoute, blip, static_cast<std::int32_t>(route));
                if (!ok)
                {
                    static_cast<void>(CallVoid(RemoveBlip, &blip));
                    return false;
                }

                std::scoped_lock lock(m_Mutex);
                m_Snapshot.activeBlip = blip;
                return true;
            });
        }

        [[nodiscard]] bool QueueRemoveBlip()
        {
            return QueueAction("Remove marker", [this] { return RemoveActiveBlipOnGameThread(); });
        }

        [[nodiscard]] bool QueueStartParticle(std::string asset, std::string effect, float scale)
        {
            if (asset.empty() || effect.empty() || m_ParticleLoading.exchange(true, std::memory_order_acq_rel))
                return false;
            {
                std::scoped_lock lock(m_Mutex);
                m_ParticleAsset = std::move(asset);
                m_ParticleEffect = std::move(effect);
                m_ParticleScale = std::clamp(scale, 0.05f, 10.0f);
                m_ParticleAttempts = 0;
            }
            SetPending("Loading particle asset");
            if (!Runtime::GameRuntime::Get().Enqueue([this] { ParticleLoadTick(); }))
            {
                m_ParticleLoading.store(false, std::memory_order_release);
                Finish(false, "Particle queue unavailable");
                return false;
            }
            return true;
        }

        [[nodiscard]] bool QueueStopParticle()
        {
            return QueueAction("Stop particle", [this] { return StopParticleOnGameThread(); });
        }

        [[nodiscard]] bool QueueSpawnBodyguard()
        {
            return QueueAction("Spawn bodyguard", [this] {
                const Ped player = CurrentPed();
                if (player == 0)
                    return false;
                {
                    std::scoped_lock lock(m_Mutex);
                    if (m_Bodyguards.size() >= 4)
                        return false;
                }

                const auto model = Native::NativeInvoker::Invoke<Hash>(Native::NativeId::GetEntityModel, player);
                const auto coords = Native::NativeInvoker::Invoke<Native::NativeVector3>(Native::NativeId::GetEntityCoords, player, std::int32_t{0});
                const auto heading = Native::NativeInvoker::Invoke<float>(Native::NativeId::GetEntityHeading, player);
                if (!model || !coords || !heading)
                    return false;

                Ped guard{};
                if (!Call(
                        CreatePed,
                        guard,
                        26,
                        *model,
                        coords->x + 1.5f,
                        coords->y + 1.5f,
                        coords->z,
                        *heading,
                        std::int32_t{0},
                        std::int32_t{1}) || guard == 0)
                    return false;

                int group{};
                if (!Call(GetPedGroupIndex, group, player)
                    || !CallVoid(SetPedAsGroupMember, guard, group)
                    || !CallVoid(
                        TaskFollowToOffsetOfEntity,
                        guard,
                        player,
                        0.0f,
                        -1.5f,
                        0.0f,
                        3.0f,
                        -1,
                        2.0f,
                        std::int32_t{1}))
                {
                    static_cast<void>(CallVoid(DeletePed, &guard));
                    return false;
                }

                Hash weapon{};
                if (Call(GetSelectedPedWeapon, weapon, player) && weapon != 0)
                    static_cast<void>(Native::NativeInvoker::InvokeVoid(
                        Native::NativeId::GiveWeaponToPed,
                        guard,
                        weapon,
                        9999,
                        std::int32_t{0},
                        std::int32_t{1}));

                std::scoped_lock lock(m_Mutex);
                m_Bodyguards.push_back(guard);
                return true;
            });
        }

        [[nodiscard]] bool QueueDismissBodyguards()
        {
            return QueueAction("Dismiss bodyguards", [this] {
                std::vector<Ped> guards;
                {
                    std::scoped_lock lock(m_Mutex);
                    guards.swap(m_Bodyguards);
                }
                bool ok = true;
                for (Ped guard : guards)
                    ok = CallVoid(DeletePed, &guard) && ok;
                return ok;
            });
        }

        [[nodiscard]] bool QueueBodyguardsAttackAimedPed()
        {
            return QueueAction("Bodyguards attack target", [this] {
                const auto playerId = Native::NativeInvoker::Invoke<int>(Native::NativeId::PlayerId);
                if (!playerId)
                    return false;

                Entity target{};
                std::int32_t aimed{};
                if (!Call(GetAimedEntity, aimed, *playerId, &target) || aimed == 0 || target == 0)
                    return false;

                std::vector<Ped> guards;
                {
                    std::scoped_lock lock(m_Mutex);
                    guards = m_Bodyguards;
                }
                if (guards.empty())
                    return false;

                bool ok = true;
                for (Ped guard : guards)
                    ok = CallVoid(TaskCombatPed, guard, static_cast<Ped>(target), 0, 16) && ok;
                return ok;
            });
        }

        [[nodiscard]] bool QueueInteriorEntitySet(std::string entitySet, bool enabled, int color)
        {
            if (entitySet.empty())
                return false;
            color = std::clamp(color, 0, 15);

            return QueueAction(enabled ? "Activate interior set" : "Deactivate interior set", [this, entitySet = std::move(entitySet), enabled, color] {
                const Ped ped = CurrentPed();
                int interior{};
                if (ped == 0 || !Call(GetInteriorFromEntity, interior, ped) || interior <= 0)
                    return false;

                const bool changed = enabled
                    ? CallVoid(ActivateInteriorEntitySet, interior, entitySet.c_str())
                    : CallVoid(DeactivateInteriorEntitySet, interior, entitySet.c_str());
                if (!changed)
                    return false;
                if (enabled)
                    static_cast<void>(CallVoid(SetInteriorEntitySetColor, interior, entitySet.c_str(), color));

                const bool refreshed = CallVoid(RefreshInterior, interior);
                if (refreshed)
                {
                    std::scoped_lock lock(m_Mutex);
                    m_Snapshot.currentInterior = interior;
                }
                return refreshed;
            });
        }

        [[nodiscard]] bool QueueIpl(std::string name, bool request)
        {
            if (name.empty())
                return false;
            return QueueAction(request ? "Request IPL" : "Remove IPL", [this, name = std::move(name), request] {
                return request ? CallVoid(RequestIpl, name.c_str()) : CallVoid(RemoveIpl, name.c_str());
            });
        }

        [[nodiscard]] bool QueueRefreshInterior()
        {
            return QueueAction("Refresh interior", [this] {
                const Ped ped = CurrentPed();
                int interior{};
                if (ped == 0 || !Call(GetInteriorFromEntity, interior, ped) || interior <= 0)
                    return false;
                const bool ok = CallVoid(RefreshInterior, interior);
                if (ok)
                {
                    std::scoped_lock lock(m_Mutex);
                    m_Snapshot.currentInterior = interior;
                }
                return ok;
            });
        }

        [[nodiscard]] bool QueueProbe(std::uint64_t enhancedHash)
        {
            if (enhancedHash == 0)
                return false;
            SetPending("Probing Enhanced native hash");
            if (!Runtime::GameRuntime::Get().Enqueue([this, enhancedHash] {
                    const auto address = ResolveRaw(enhancedHash);
                    {
                        std::scoped_lock lock(m_Mutex);
                        m_Snapshot.lastProbeHash = enhancedHash;
                        m_Snapshot.lastProbeAddress = address;
                        m_Snapshot.lastProbeResolved = address != 0;
                    }
                    Finish(address != 0, address != 0 ? "Native handler resolved" : "Native handler unavailable");
                }))
            {
                Finish(false, "Probe queue unavailable");
                return false;
            }
            return true;
        }

    private:
        enum HandlerIndex : std::size_t
        {
            GetSelectedPedWeapon,
            SetWeaponTint,
            GetWeaponTint,
            GiveWeaponComponent,
            RemoveWeaponComponent,
            HasWeaponComponent,
            DoesWeaponTakeComponent,
            GetGameplayCamCoord,
            GetGameplayCamRot,
            CreateCamWithParams,
            SetCamActive,
            RenderScriptCams,
            DestroyCam,
            SetCamCoord,
            SetCamRot,
            SetCamFov,
            RequestAnimDict,
            HasAnimDictLoaded,
            RemoveAnimDict,
            TaskPlayAnim,
            StopAnimTask,
            ClearPedTasks,
            SetVehicleDoorOpen,
            SetVehicleDoorShut,
            RollDownWindow,
            RollUpWindow,
            SetVehicleEngineOn,
            SetVehicleLights,
            ModifyVehicleTopSpeed,
            SetVehicleTorque,
            SetVehicleReduceGrip,
            GetPedPropIndex,
            GetPedPropTextureIndex,
            SetPedPropIndex,
            ClearPedProp,
            AddBlipForCoord,
            SetBlipSprite,
            SetBlipColour,
            SetBlipRoute,
            RemoveBlip,
            RequestPtfxAsset,
            HasPtfxAssetLoaded,
            RemovePtfxAsset,
            UsePtfxAsset,
            StartPtfxLoopedOnEntity,
            StopPtfxLooped,
            CreatePed,
            DeletePed,
            GetPedGroupIndex,
            SetPedAsGroupMember,
            TaskFollowToOffsetOfEntity,
            TaskCombatPed,
            GetAimedEntity,
            GetInteriorFromEntity,
            ActivateInteriorEntitySet,
            DeactivateInteriorEntitySet,
            SetInteriorEntitySetColor,
            RefreshInterior,
            RequestIpl,
            RemoveIpl,
            HandlerCount,
        };

        // Every entry below is the Enhanced-side value from YimMenuV2's current
        // enhanced crossmap. Resolution is lazy so one missing native only disables
        // the feature that needs it rather than taking the entire toolkit offline.
        static constexpr std::array<std::uint64_t, HandlerCount> HandlerHashes{
            0xB0D77D90171EC35Full, // GET_SELECTED_PED_WEAPON
            0xC37D2709B04BD397ull, // SET_PED_WEAPON_TINT_INDEX
            0x6C81F95CADD1E6D0ull, // GET_PED_WEAPON_TINT_INDEX
            0x6D5FA72F8C43D132ull, // GIVE_WEAPON_COMPONENT_TO_PED
            0x80E6FC2ACEAF8AA3ull, // REMOVE_WEAPON_COMPONENT_FROM_PED
            0x5EDED4B3E1A48E68ull, // HAS_PED_GOT_WEAPON_COMPONENT
            0x0C985A2C6C77023Dull, // DOES_WEAPON_TAKE_WEAPON_COMPONENT
            0xCF141FCD0940B0A3ull, // GET_GAMEPLAY_CAM_COORD
            0xD84A545408A3099Aull, // GET_GAMEPLAY_CAM_ROT
            0x2CB6AB601EB7D2D9ull, // CREATE_CAM_WITH_PARAMS
            0x4CBC5D1BC117616Bull, // SET_CAM_ACTIVE
            0xE37AF9002E782BA0ull, // RENDER_SCRIPT_CAMS
            0x85E6A1E36B5E2E4Dull, // DESTROY_CAM
            0x1761457F86AD0EE2ull, // SET_CAM_COORD
            0x5E5CEC33463AD803ull, // SET_CAM_ROT
            0x58BDA5D9262F5D30ull, // SET_CAM_FOV
            0x80813AC549A1E8AEull, // REQUEST_ANIM_DICT
            0xE100DD4F82A51BDEull, // HAS_ANIM_DICT_LOADED
            0x268BE77F77533D03ull, // REMOVE_ANIM_DICT
            0x10425721983AE158ull, // TASK_PLAY_ANIM
            0x08D8528BA8E43641ull, // STOP_ANIM_TASK
            0x974022927CB47E68ull, // CLEAR_PED_TASKS
            0xBFE60A5CC0C835D8ull, // SET_VEHICLE_DOOR_OPEN
            0x6515021478088FBCull, // SET_VEHICLE_DOOR_SHUT
            0x260EEEEBF5F35F72ull, // ROLL_DOWN_WINDOW
            0x9303D5873A8A413Aull, // ROLL_UP_WINDOW
            0xC229299217554C78ull, // SET_VEHICLE_ENGINE_ON
            0xBA3C1A9AA7FD9616ull, // SET_VEHICLE_LIGHTS
            0xAC89BB42FE09CC80ull, // MODIFY_VEHICLE_TOP_SPEED
            0xF1C985BBEC6B6321ull, // SET_VEHICLE_ENGINE_TORQUE_MULTIPLIER
            0xF8EC8E90E8D24CA7ull, // SET_VEHICLE_REDUCE_GRIP
            0xB204F40D393426B6ull, // GET_PED_PROP_INDEX
            0x0DC23FA727759F9Full, // GET_PED_PROP_TEXTURE_INDEX
            0x7F08C4791E6D6969ull, // SET_PED_PROP_INDEX
            0x09397806857F5DFBull, // CLEAR_PED_PROP
            0x34864AB7DA700AA6ull, // ADD_BLIP_FOR_COORD
            0x4C905FB262965D5Dull, // SET_BLIP_SPRITE
            0x61183D6239A9D7B8ull, // SET_BLIP_COLOUR
            0xBC64B805EE071A37ull, // SET_BLIP_ROUTE
            0xFE54B8568B2ABD12ull, // REMOVE_BLIP
            0xEBEE7DC21AB44EC9ull, // REQUEST_NAMED_PTFX_ASSET
            0x939D49C9FAA8139Aull, // HAS_NAMED_PTFX_ASSET_LOADED
            0x90D778E278B533C0ull, // REMOVE_NAMED_PTFX_ASSET
            0xD03F4780B97A39AEull, // USE_PARTICLE_FX_ASSET
            0x62750FD2BDD8BD49ull, // START_PARTICLE_FX_LOOPED_ON_ENTITY
            0x182120534CCF9023ull, // STOP_PARTICLE_FX_LOOPED
            0xB1DBFEB95C0EFB88ull, // CREATE_PED
            0x734A9F4537A31459ull, // DELETE_PED
            0x26B246D60FABB3E2ull, // GET_PED_GROUP_INDEX
            0x03AB73582A77DBD3ull, // SET_PED_AS_GROUP_MEMBER
            0x329B82704ED2A3E3ull, // TASK_FOLLOW_TO_OFFSET_OF_ENTITY
            0x62A5310368A20EFAull, // TASK_COMBAT_PED
            0x66EE98F15844BE4Dull, // GET_ENTITY_PLAYER_IS_FREE_AIMING_AT
            0xF8F35890F43ED2AEull, // GET_INTERIOR_FROM_ENTITY
            0x907994FF361E5295ull, // ACTIVATE_INTERIOR_ENTITY_SET
            0x62BCE536D41AC07Dull, // DEACTIVATE_INTERIOR_ENTITY_SET
            0x0A047107933868D4ull, // _SET_INTERIOR_ENTITY_SET_COLOR
            0xEEC112F70F9E6543ull, // REFRESH_INTERIOR
            0xECFC57F5F11BCD83ull, // REQUEST_IPL
            0x5373E9377066509Eull, // REMOVE_IPL
        };

        struct NativeProgram final
        {
            std::byte pad00[0x2C]{};
            std::uint32_t nativeCount{};
            std::byte pad30[0x10]{};
            Native::NativeHandler* nativeEntrypoints{};
            std::byte pad48[0x38]{};
        };

        static_assert(offsetof(NativeProgram, nativeCount) == 0x2C);
        static_assert(offsetof(NativeProgram, nativeEntrypoints) == 0x40);
        static_assert(sizeof(NativeProgram) == 0x80);

        EnhancedNativeToolkit() = default;

        [[nodiscard]] static bool IsExecutable(std::uintptr_t address) noexcept
        {
            if (address == 0)
                return false;
            MEMORY_BASIC_INFORMATION memory{};
            if (::VirtualQuery(reinterpret_cast<const void*>(address), &memory, sizeof(memory)) != sizeof(memory))
                return false;
            if (memory.State != MEM_COMMIT || (memory.Protect & PAGE_GUARD) != 0 || memory.Protect == PAGE_NOACCESS)
                return false;

            switch (memory.Protect & 0xFF)
            {
            case PAGE_EXECUTE:
            case PAGE_EXECUTE_READ:
            case PAGE_EXECUTE_READWRITE:
            case PAGE_EXECUTE_WRITECOPY:
                return true;
            default:
                return false;
            }
        }

        [[nodiscard]] std::uintptr_t ResolveRaw(std::uint64_t hash) noexcept
        {
            if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread())
                return 0;
            const auto init = GamePointers::Get().InitNativeTables();
            if (!init)
                return 0;

            std::uint64_t slot = hash;
            NativeProgram program{};
            program.nativeCount = 1;
            program.nativeEntrypoints = reinterpret_cast<Native::NativeHandler*>(&slot);
            init(&program);

            const auto address = static_cast<std::uintptr_t>(slot);
            return IsExecutable(address) ? address : 0;
        }

        [[nodiscard]] bool Resolve(std::size_t index) noexcept
        {
            if (index >= m_Handlers.size())
                return false;
            if (m_Handlers[index])
                return true;

            const auto address = ResolveRaw(HandlerHashes[index]);
            if (address == 0)
                return false;
            m_Handlers[index] = reinterpret_cast<Native::NativeHandler>(address);
            return true;
        }

        template<typename Ret, typename... Args>
        [[nodiscard]] bool Call(std::size_t index, Ret& out, Args&&... args) noexcept
        {
            if (!Resolve(index))
                return false;
            Native::CallContext context;
            if (!(context.PushArg(std::forward<Args>(args)) && ...))
                return false;
            m_Handlers[index](&context);
            context.FixVectors();
            out = context.GetReturnValue<Ret>();
            return true;
        }

        template<typename... Args>
        [[nodiscard]] bool CallVoid(std::size_t index, Args&&... args) noexcept
        {
            if (!Resolve(index))
                return false;
            Native::CallContext context;
            if (!(context.PushArg(std::forward<Args>(args)) && ...))
                return false;
            m_Handlers[index](&context);
            context.FixVectors();
            return true;
        }

        template<typename Fn>
        [[nodiscard]] bool QueueAction(const char* label, Fn&& fn)
        {
            if (!Native::NativeRegistry::Get().IsReady() || m_Pending.exchange(true, std::memory_order_acq_rel))
                return false;

            const std::string actionLabel = label ? label : "Action";
            SetStatus(false, false, actionLabel + " queued");
            if (!Runtime::GameRuntime::Get().Enqueue([this, actionLabel, action = std::forward<Fn>(fn)]() mutable {
                    const bool ok = action();
                    Finish(ok, actionLabel + (ok ? " succeeded" : " failed"));
                }))
            {
                Finish(false, "Game-thread queue unavailable");
                return false;
            }
            return true;
        }

        void SetPending(std::string message)
        {
            m_Pending.store(true, std::memory_order_release);
            SetStatus(false, false, std::move(message));
        }

        void SetStatus(bool haveResult, bool success, std::string message)
        {
            std::scoped_lock lock(m_Mutex);
            m_Snapshot.haveResult = haveResult;
            m_Snapshot.lastSucceeded = success;
            m_Snapshot.message = std::move(message);
        }

        void Finish(bool success, std::string message)
        {
            SetStatus(true, success, std::move(message));
            m_Pending.store(false, std::memory_order_release);
        }

        [[nodiscard]] Ped CurrentPed() const noexcept
        {
            const auto ped = Natives::PlayerPedId();
            return ped ? *ped : 0;
        }

        [[nodiscard]] Vehicle CurrentVehicle() const noexcept
        {
            const Ped ped = CurrentPed();
            if (ped == 0)
                return 0;
            const auto inVehicle = Natives::IsPedInAnyVehicle(ped, true);
            if (!inVehicle || !*inVehicle)
                return 0;
            const auto vehicle = Natives::GetVehiclePedIsIn(ped, true);
            return vehicle ? *vehicle : 0;
        }

        [[nodiscard]] static std::filesystem::path OutfitDirectory()
        {
            return Core::FileSystem::Get().RootPath(Core::FileSystem::Root::Config) / "outfits";
        }

        [[nodiscard]] static std::string SanitizeName(std::string name)
        {
            std::string clean;
            clean.reserve(std::min<std::size_t>(name.size(), 48));
            for (const char ch : name)
            {
                if (clean.size() >= 48)
                    break;
                if ((ch >= 'a' && ch <= 'z')
                    || (ch >= 'A' && ch <= 'Z')
                    || (ch >= '0' && ch <= '9')
                    || ch == '-'
                    || ch == '_')
                {
                    clean.push_back(ch);
                }
                else if (ch == ' ')
                {
                    clean.push_back('_');
                }
            }
            return clean;
        }

        void AnimationLoadTick() noexcept
        {
            const Ped ped = CurrentPed();
            std::string dict;
            std::string name;
            bool loop{};
            bool upper{};
            int attempts{};
            {
                std::scoped_lock lock(m_Mutex);
                dict = m_AnimationDictionary;
                name = m_AnimationName;
                loop = m_AnimationLoop;
                upper = m_AnimationUpperBody;
                attempts = ++m_AnimationAttempts;
            }

            if (ped == 0 || !CallVoid(RequestAnimDict, dict.c_str()))
            {
                m_AnimationLoading.store(false, std::memory_order_release);
                return Finish(false, "Animation dictionary request failed");
            }

            std::int32_t loaded{};
            if (Call(HasAnimDictLoaded, loaded, dict.c_str()) && loaded != 0)
            {
                const int flags = (loop ? 1 : 0) | (upper ? 16 : 0);
                const bool ok = CallVoid(
                    TaskPlayAnim,
                    ped,
                    dict.c_str(),
                    name.c_str(),
                    8.0f,
                    -8.0f,
                    -1,
                    flags,
                    1.0f,
                    std::int32_t{0},
                    std::int32_t{0},
                    std::int32_t{0});
                static_cast<void>(CallVoid(RemoveAnimDict, dict.c_str()));
                m_AnimationLoading.store(false, std::memory_order_release);
                return Finish(ok, ok ? "Animation playing" : "Animation dispatch failed");
            }

            if (attempts >= 120 || !Runtime::GameRuntime::Get().Enqueue([this] { AnimationLoadTick(); }))
            {
                m_AnimationLoading.store(false, std::memory_order_release);
                Finish(false, "Animation dictionary load timed out");
            }
        }

        void StartFreecamOnGameThread() noexcept
        {
            Native::NativeVector3 position{};
            Native::NativeVector3 rotation{};
            if (!Call(GetGameplayCamCoord, position)
                || !Call(GetGameplayCamRot, rotation, 2))
            {
                m_FreecamEnabled.store(false, std::memory_order_release);
                return Finish(false, "Gameplay camera state unavailable");
            }

            const float fov = m_FreecamFov.load(std::memory_order_acquire);
            int cam{};
            if (!Call(
                    CreateCamWithParams,
                    cam,
                    "DEFAULT_SCRIPTED_CAMERA",
                    position.x,
                    position.y,
                    position.z,
                    rotation.x,
                    rotation.y,
                    rotation.z,
                    fov,
                    std::int32_t{1},
                    2) || cam == 0
                || !CallVoid(SetCamActive, cam, std::int32_t{1})
                || !CallVoid(RenderScriptCams, std::int32_t{1}, std::int32_t{1}, 300, std::int32_t{1}, std::int32_t{0}, 0))
            {
                if (cam != 0)
                    static_cast<void>(CallVoid(DestroyCam, cam, std::int32_t{0}));
                m_FreecamEnabled.store(false, std::memory_order_release);
                return Finish(false, "Script camera creation failed");
            }

            m_CamHandle = cam;
            m_CamX = position.x;
            m_CamY = position.y;
            m_CamZ = position.z;
            m_CamPitch = rotation.x;
            m_CamRoll = rotation.y;
            m_CamYaw = rotation.z;
            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot.freecamHandle = cam;
                m_Snapshot.freecamX = m_CamX;
                m_Snapshot.freecamY = m_CamY;
                m_Snapshot.freecamZ = m_CamZ;
                m_Snapshot.freecamPitch = m_CamPitch;
                m_Snapshot.freecamYaw = m_CamYaw;
                m_Snapshot.freecamFov = fov;
            }

            Finish(true, "Freecam active");
            if (!Runtime::GameRuntime::Get().Enqueue([this] { FreecamTick(); }))
            {
                m_FreecamEnabled.store(false, std::memory_order_release);
                StopFreecamOnGameThread(false);
            }
        }

        void FreecamTick() noexcept
        {
            if (!m_FreecamEnabled.load(std::memory_order_acquire) || m_CamHandle == 0)
                return StopFreecamOnGameThread(false);

            float speed = m_FreecamSpeed.load(std::memory_order_acquire);
            if ((::GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0)
                speed *= 3.0f;

            constexpr float TurnSpeed = 1.25f;
            if ((::GetAsyncKeyState(VK_LEFT) & 0x8000) != 0) m_CamYaw += TurnSpeed;
            if ((::GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0) m_CamYaw -= TurnSpeed;
            if ((::GetAsyncKeyState(VK_UP) & 0x8000) != 0) m_CamPitch = std::clamp(m_CamPitch + TurnSpeed, -89.0f, 89.0f);
            if ((::GetAsyncKeyState(VK_DOWN) & 0x8000) != 0) m_CamPitch = std::clamp(m_CamPitch - TurnSpeed, -89.0f, 89.0f);

            constexpr float Pi = 3.14159265358979323846f;
            const float yaw = m_CamYaw * Pi / 180.0f;
            const float pitch = m_CamPitch * Pi / 180.0f;
            const float forwardX = -std::sin(yaw) * std::cos(pitch);
            const float forwardY = std::cos(yaw) * std::cos(pitch);
            const float forwardZ = std::sin(pitch);
            const float rightX = std::cos(yaw);
            const float rightY = std::sin(yaw);

            if ((::GetAsyncKeyState('W') & 0x8000) != 0) { m_CamX += forwardX * speed; m_CamY += forwardY * speed; m_CamZ += forwardZ * speed; }
            if ((::GetAsyncKeyState('S') & 0x8000) != 0) { m_CamX -= forwardX * speed; m_CamY -= forwardY * speed; m_CamZ -= forwardZ * speed; }
            if ((::GetAsyncKeyState('D') & 0x8000) != 0) { m_CamX += rightX * speed; m_CamY += rightY * speed; }
            if ((::GetAsyncKeyState('A') & 0x8000) != 0) { m_CamX -= rightX * speed; m_CamY -= rightY * speed; }
            if ((::GetAsyncKeyState(VK_SPACE) & 0x8000) != 0) m_CamZ += speed;
            if ((::GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) m_CamZ -= speed;

            const float fov = m_FreecamFov.load(std::memory_order_acquire);
            const bool ok = CallVoid(SetCamCoord, m_CamHandle, m_CamX, m_CamY, m_CamZ)
                && CallVoid(SetCamRot, m_CamHandle, m_CamPitch, m_CamRoll, m_CamYaw, 2)
                && CallVoid(SetCamFov, m_CamHandle, fov);
            if (!ok)
            {
                m_FreecamEnabled.store(false, std::memory_order_release);
                return StopFreecamOnGameThread(false);
            }

            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot.freecamX = m_CamX;
                m_Snapshot.freecamY = m_CamY;
                m_Snapshot.freecamZ = m_CamZ;
                m_Snapshot.freecamPitch = m_CamPitch;
                m_Snapshot.freecamYaw = m_CamYaw;
                m_Snapshot.freecamFov = fov;
            }

            if (!Runtime::GameRuntime::Get().Enqueue([this] { FreecamTick(); }))
            {
                m_FreecamEnabled.store(false, std::memory_order_release);
                StopFreecamOnGameThread(false);
            }
        }

        void StopFreecamOnGameThread(bool report) noexcept
        {
            if (m_CamHandle != 0)
            {
                static_cast<void>(CallVoid(RenderScriptCams, std::int32_t{0}, std::int32_t{1}, 250, std::int32_t{1}, std::int32_t{0}, 0));
                static_cast<void>(CallVoid(SetCamActive, m_CamHandle, std::int32_t{0}));
                static_cast<void>(CallVoid(DestroyCam, m_CamHandle, std::int32_t{0}));
            }
            m_CamHandle = 0;
            m_FreecamEnabled.store(false, std::memory_order_release);
            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot.freecamHandle = 0;
            }
            if (report)
                Finish(true, "Freecam disabled");
        }

        void PerformanceTick() noexcept
        {
            if (!m_PerformanceEnabled.load(std::memory_order_acquire))
            {
                RestorePerformance();
                m_PerformanceTicking.store(false, std::memory_order_release);
                return;
            }

            const Vehicle vehicle = CurrentVehicle();
            if (vehicle != 0)
            {
                if (m_LastPerformanceVehicle != 0 && m_LastPerformanceVehicle != vehicle)
                    RestorePerformance();
                m_LastPerformanceVehicle = vehicle;
                static_cast<void>(CallVoid(ModifyVehicleTopSpeed, vehicle, m_PerformanceTopSpeed.load(std::memory_order_acquire)));
                static_cast<void>(CallVoid(SetVehicleTorque, vehicle, m_PerformanceTorque.load(std::memory_order_acquire)));
                static_cast<void>(CallVoid(SetVehicleReduceGrip, vehicle, static_cast<std::int32_t>(m_PerformanceReduceGrip.load(std::memory_order_acquire))));
            }
            else
            {
                RestorePerformance();
            }

            if (!Runtime::GameRuntime::Get().Enqueue([this] { PerformanceTick(); }))
            {
                RestorePerformance();
                m_PerformanceTicking.store(false, std::memory_order_release);
            }
        }

        void RestorePerformance() noexcept
        {
            if (m_LastPerformanceVehicle == 0)
                return;
            static_cast<void>(CallVoid(ModifyVehicleTopSpeed, m_LastPerformanceVehicle, 0.0f));
            static_cast<void>(CallVoid(SetVehicleTorque, m_LastPerformanceVehicle, 1.0f));
            static_cast<void>(CallVoid(SetVehicleReduceGrip, m_LastPerformanceVehicle, std::int32_t{0}));
            m_LastPerformanceVehicle = 0;
        }

        [[nodiscard]] bool RemoveActiveBlipOnGameThread() noexcept
        {
            int blip{};
            {
                std::scoped_lock lock(m_Mutex);
                blip = m_Snapshot.activeBlip;
                m_Snapshot.activeBlip = 0;
            }
            return blip == 0 || CallVoid(RemoveBlip, &blip);
        }

        void ParticleLoadTick() noexcept
        {
            std::string asset;
            std::string effect;
            float scale{};
            int attempts{};
            {
                std::scoped_lock lock(m_Mutex);
                asset = m_ParticleAsset;
                effect = m_ParticleEffect;
                scale = m_ParticleScale;
                attempts = ++m_ParticleAttempts;
            }

            if (!CallVoid(RequestPtfxAsset, asset.c_str()))
            {
                m_ParticleLoading.store(false, std::memory_order_release);
                return Finish(false, "Particle asset request failed");
            }

            std::int32_t loaded{};
            if (Call(HasPtfxAssetLoaded, loaded, asset.c_str()) && loaded != 0)
            {
                StopParticleOnGameThread();
                const Ped ped = CurrentPed();
                if (ped == 0 || !CallVoid(UsePtfxAsset, asset.c_str()))
                {
                    m_ParticleLoading.store(false, std::memory_order_release);
                    return Finish(false, "Particle asset activation failed");
                }

                int handle{};
                const bool ok = Call(
                    StartPtfxLoopedOnEntity,
                    handle,
                    effect.c_str(),
                    ped,
                    0.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 0.0f,
                    scale,
                    std::int32_t{0}, std::int32_t{0}, std::int32_t{0});
                if (ok && handle != 0)
                {
                    std::scoped_lock lock(m_Mutex);
                    m_Snapshot.activeParticle = handle;
                }
                static_cast<void>(CallVoid(RemovePtfxAsset, asset.c_str()));
                m_ParticleLoading.store(false, std::memory_order_release);
                return Finish(ok && handle != 0, ok && handle != 0 ? "Particle effect started" : "Particle effect failed");
            }

            if (attempts >= 120 || !Runtime::GameRuntime::Get().Enqueue([this] { ParticleLoadTick(); }))
            {
                m_ParticleLoading.store(false, std::memory_order_release);
                Finish(false, "Particle asset load timed out");
            }
        }

        [[nodiscard]] bool StopParticleOnGameThread() noexcept
        {
            int handle{};
            {
                std::scoped_lock lock(m_Mutex);
                handle = m_Snapshot.activeParticle;
                m_Snapshot.activeParticle = 0;
            }
            return handle == 0 || CallVoid(StopPtfxLooped, handle, std::int32_t{0});
        }

        std::array<Native::NativeHandler, HandlerCount> m_Handlers{};
        mutable std::mutex m_Mutex;
        ToolkitSnapshot m_Snapshot{};
        std::atomic<bool> m_Pending{false};
        std::atomic<ULONGLONG> m_NextSampleMs{0};

        std::atomic<bool> m_FreecamEnabled{false};
        std::atomic<float> m_FreecamSpeed{0.45f};
        std::atomic<float> m_FreecamFov{70.0f};
        int m_CamHandle{};
        float m_CamX{};
        float m_CamY{};
        float m_CamZ{};
        float m_CamPitch{};
        float m_CamRoll{};
        float m_CamYaw{};

        std::atomic<bool> m_PerformanceEnabled{false};
        std::atomic<bool> m_PerformanceTicking{false};
        std::atomic<float> m_PerformanceTopSpeed{0.0f};
        std::atomic<float> m_PerformanceTorque{1.0f};
        std::atomic<bool> m_PerformanceReduceGrip{false};
        Vehicle m_LastPerformanceVehicle{};

        std::atomic<bool> m_AnimationLoading{false};
        std::string m_AnimationDictionary{};
        std::string m_AnimationName{};
        bool m_AnimationLoop{};
        bool m_AnimationUpperBody{};
        int m_AnimationAttempts{};

        std::atomic<bool> m_ParticleLoading{false};
        std::string m_ParticleAsset{};
        std::string m_ParticleEffect{};
        float m_ParticleScale{1.0f};
        int m_ParticleAttempts{};

        std::vector<Ped> m_Bodyguards{};
    };
}
