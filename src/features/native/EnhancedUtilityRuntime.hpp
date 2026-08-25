#pragma once

#include "../../game/GamePointers.hpp"
#include "../../game/Natives.hpp"
#include "../../game/native/NativeCallContext.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::NativeTools
{
    struct EnhancedUtilitySnapshot final
    {
        bool nativeReady{};
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool haveSuspensionReadback{};
        float suspensionLowering{};
        int controlAttempts{};
        std::string message{"Ready"};
    };

    class EnhancedUtilityRuntime final
    {
    public:
        static EnhancedUtilityRuntime& Get() noexcept
        {
            static EnhancedUtilityRuntime instance;
            return instance;
        }

        [[nodiscard]] EnhancedUtilitySnapshot Snapshot() const
        {
            EnhancedUtilitySnapshot out;
            out.nativeReady = Native::NativeRegistry::Get().IsReady();
            out.pending = m_Pending.load(std::memory_order_acquire);
            out.controlAttempts = m_ControlAttempts.load(std::memory_order_acquire);

            std::scoped_lock lock(m_Mutex);
            out.haveResult = m_HaveResult;
            out.lastSucceeded = m_LastSucceeded;
            out.haveSuspensionReadback = m_HaveSuspensionReadback;
            out.suspensionLowering = m_SuspensionLowering;
            out.message = m_Message;
            return out;
        }

        bool QueueRepairCurrentVehicle()
        {
            return QueueVehicleAction("Full vehicle repair", [this](Vehicle vehicle) {
                bool ok = Natives::SetVehicleFixed(vehicle);
                ok = CallVoid(SetVehicleDeformationFixed, vehicle) && ok;
                ok = CallVoid(SetVehicleBodyHealth, vehicle, 1000.0f) && ok;
                constexpr std::array<int, 8> Tyres{{0, 1, 2, 3, 4, 5, 45, 47}};
                for (const int tyre : Tyres)
                    ok = CallVoid(SetVehicleTyreFixed, vehicle, tyre) && ok;
                return ok;
            });
        }

        bool QueueSetVehicleSpeed(float metersPerSecond)
        {
            if (!std::isfinite(metersPerSecond))
                return false;
            metersPerSecond = std::clamp(metersPerSecond, -100.0f, 250.0f);
            return QueueVehicleAction("Vehicle forward speed", [this, metersPerSecond](Vehicle vehicle) {
                return CallVoid(SetVehicleForwardSpeed, vehicle, metersPerSecond);
            });
        }

        bool QueueSetPlateText(std::string text)
        {
            text = SanitizePlate(std::move(text));
            if (text.empty())
                return false;

            return QueueVehicleAction("Vehicle plate text", [this, text = std::move(text)](Vehicle vehicle) {
                return CallVoid(SetVehicleNumberPlateText, vehicle, text.c_str());
            });
        }

        bool QueueSetVehicleExtra(int extra, bool enabled)
        {
            if (extra < 0 || extra > 20)
                return false;

            return QueueVehicleAction("Vehicle extra", [this, extra, enabled](Vehicle vehicle) {
                std::int32_t exists{};
                if (!Call(DoesExtraExist, exists, vehicle, extra) || exists == 0)
                    return false;

                // SET_VEHICLE_EXTRA's third argument is disableExtra.
                return CallVoid(SetVehicleExtra, vehicle, extra, std::int32_t{enabled ? 0 : 1});
            });
        }

        bool QueueSetWindowTint(int tint)
        {
            tint = std::clamp(tint, -1, 6);
            return QueueVehicleAction("Vehicle window tint", [this, tint](Vehicle vehicle) {
                return CallVoid(SetVehicleWindowTint, vehicle, tint);
            });
        }

        bool QueueSetVehicleGravity(bool enabled)
        {
            return QueueVehicleAction(enabled ? "Vehicle gravity enabled" : "Vehicle gravity disabled", [this, enabled](Vehicle vehicle) {
                return CallVoid(SetVehicleGravity, vehicle, std::int32_t{enabled ? 1 : 0});
            });
        }

        bool QueueSetHydraulicState(int state)
        {
            state = std::clamp(state, 0, 3);
            return QueueVehicleAction("Hydraulic vehicle state", [this, state](Vehicle vehicle) {
                return CallVoid(SetHydraulicVehicleState, vehicle, state);
            });
        }

        bool QueueReadSuspensionLowering()
        {
            if (!BeginAction("Reading Enhanced suspension lowering"))
                return false;

            if (!Runtime::GameRuntime::Get().Enqueue([this] {
                    if (!ResolveHandlers())
                        return Finish(false, "Enhanced utility natives are unavailable");

                    const Vehicle vehicle = CurrentVehicle();
                    if (vehicle == 0)
                        return Finish(false, "Enter a vehicle before reading suspension lowering");

                    float lowering{};
                    if (!Call(GetFakeSuspensionLoweringAmount, lowering, vehicle) || !std::isfinite(lowering))
                        return Finish(false, "GET_FAKE_SUSPENSION_LOWERING_AMOUNT failed");

                    {
                        std::scoped_lock lock(m_Mutex);
                        m_HaveSuspensionReadback = true;
                        m_SuspensionLowering = lowering;
                    }
                    Finish(true, "Enhanced suspension lowering read successfully");
                }))
            {
                Finish(false, "Game-thread queue unavailable");
                return false;
            }
            return true;
        }

        bool QueueSetWeather(std::string weather)
        {
            weather = SanitizeWeather(std::move(weather));
            if (weather.empty() || !BeginAction("Applying weather override"))
                return false;

            if (!Runtime::GameRuntime::Get().Enqueue([this, weather = std::move(weather)] {
                    if (!ResolveHandlers())
                        return Finish(false, "Enhanced utility natives are unavailable");
                    const bool ok = CallVoid(SetOverrideWeather, weather.c_str());
                    Finish(ok, ok ? std::string("Weather override: ") + weather : "Weather override failed");
                }))
            {
                Finish(false, "Game-thread queue unavailable");
                return false;
            }
            return true;
        }

        bool QueueSetClock(int hour, int minute, int second)
        {
            hour = std::clamp(hour, 0, 23);
            minute = std::clamp(minute, 0, 59);
            second = std::clamp(second, 0, 59);
            if (!BeginAction("Applying network clock override"))
                return false;

            if (!Runtime::GameRuntime::Get().Enqueue([this, hour, minute, second] {
                    if (!ResolveHandlers())
                        return Finish(false, "Enhanced utility natives are unavailable");
                    const bool ok = CallVoid(NetworkOverrideClockTime, hour, minute, second);
                    Finish(ok, ok ? "Clock override applied" : "Clock override failed");
                }))
            {
                Finish(false, "Game-thread queue unavailable");
                return false;
            }
            return true;
        }

        bool QueueSetBlackout(bool enabled)
        {
            if (!BeginAction(enabled ? "Enabling blackout" : "Disabling blackout"))
                return false;

            if (!Runtime::GameRuntime::Get().Enqueue([this, enabled] {
                    if (!ResolveHandlers())
                        return Finish(false, "Enhanced utility natives are unavailable");
                    const bool ok = CallVoid(SetArtificialLightsState, std::int32_t{enabled ? 1 : 0});
                    Finish(ok, ok ? (enabled ? "World blackout enabled" : "World blackout disabled") : "Blackout native failed");
                }))
            {
                Finish(false, "Game-thread queue unavailable");
                return false;
            }
            return true;
        }

    private:
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

        enum HandlerIndex : std::size_t
        {
            NetworkRequestControlOfEntity,
            NetworkHasControlOfEntity,
            SetVehicleForwardSpeed,
            SetVehicleNumberPlateText,
            DoesExtraExist,
            SetVehicleExtra,
            SetVehicleWindowTint,
            SetVehicleTyreFixed,
            SetVehicleDeformationFixed,
            SetVehicleBodyHealth,
            GetFakeSuspensionLoweringAmount,
            SetHydraulicVehicleState,
            SetVehicleGravity,
            SetOverrideWeather,
            NetworkOverrideClockTime,
            SetArtificialLightsState,
            HandlerCount,
        };

        // Current GTA5 Enhanced targets crossmapped from the canonical NativeDB
        // entries. Keeping these in one focused runtime lets a title update fail
        // this feature group without taking Tutones' core native registry down.
        static constexpr std::array<std::uint64_t, HandlerCount> HandlerHashes{
            0xF093E270C0B6B318ull, // NETWORK_REQUEST_CONTROL_OF_ENTITY
            0x1B1A446EFA398EB5ull, // NETWORK_HAS_CONTROL_OF_ENTITY
            0x93C337B66C95C99Bull, // SET_VEHICLE_FORWARD_SPEED
            0x3FEAE59CDE6D3946ull, // SET_VEHICLE_NUMBER_PLATE_TEXT
            0x579FA5568DE0C2A0ull, // DOES_EXTRA_EXIST
            0xD772F6AA66750D2Bull, // SET_VEHICLE_EXTRA
            0xFE620ED8E0A3C209ull, // SET_VEHICLE_WINDOW_TINT
            0xF516E954BCB89C18ull, // SET_VEHICLE_TYRE_FIXED
            0x1D1124C855316790ull, // SET_VEHICLE_DEFORMATION_FIXED
            0x3E7E7AD923FD91A7ull, // SET_VEHICLE_BODY_HEALTH
            0xF7553BA24C0AB0B2ull, // GET_FAKE_SUSPENSION_LOWERING_AMOUNT
            0xA15CBF61198EE519ull, // SET_HYDRAULIC_VEHICLE_STATE
            0x666DF5A2D9B9C2DFull, // SET_VEHICLE_GRAVITY
            0x88791F880F624022ull, // SET_OVERRIDE_WEATHER
            0xAFD3BC0F6EBB5474ull, // NETWORK_OVERRIDE_CLOCK_TIME
            0x771FE86D2A331DD7ull, // SET_ARTIFICIAL_LIGHTS_STATE
        };

        static constexpr int MaxControlAttempts = 45;

        EnhancedUtilityRuntime() = default;

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

        bool ResolveHandlers() noexcept
        {
            bool ready = true;
            for (const auto handler : m_Handlers)
                ready = ready && handler != nullptr;
            if (ready)
                return true;

            if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread())
                return false;

            const auto init = GamePointers::Get().InitNativeTables();
            if (!init)
                return false;

            auto slots = HandlerHashes;
            NativeProgram program{};
            program.nativeCount = static_cast<std::uint32_t>(slots.size());
            program.nativeEntrypoints = reinterpret_cast<Native::NativeHandler*>(slots.data());
            init(&program);

            for (std::size_t i = 0; i < slots.size(); ++i)
            {
                const auto address = static_cast<std::uintptr_t>(slots[i]);
                if (!IsExecutable(address))
                {
                    m_Handlers.fill(nullptr);
                    return false;
                }
                m_Handlers[i] = reinterpret_cast<Native::NativeHandler>(address);
            }
            return true;
        }

        template<typename Ret, typename... Args>
        [[nodiscard]] bool Call(std::size_t index, Ret& out, Args... args) const
        {
            if (index >= m_Handlers.size() || !m_Handlers[index])
                return false;

            Native::CallContext context;
            if (!(context.PushArg(args) && ...))
                return false;
            m_Handlers[index](&context);
            context.FixVectors();
            out = context.GetReturnValue<Ret>();
            return true;
        }

        template<typename... Args>
        [[nodiscard]] bool CallVoid(std::size_t index, Args... args) const
        {
            if (index >= m_Handlers.size() || !m_Handlers[index])
                return false;

            Native::CallContext context;
            if (!(context.PushArg(args) && ...))
                return false;
            m_Handlers[index](&context);
            context.FixVectors();
            return true;
        }

        [[nodiscard]] static Vehicle CurrentVehicle() noexcept
        {
            const auto ped = Natives::PlayerPedId();
            if (!ped || *ped == 0)
                return 0;
            const auto inVehicle = Natives::IsPedInAnyVehicle(*ped, false);
            if (!inVehicle || !*inVehicle)
                return 0;
            const auto vehicle = Natives::GetVehiclePedIsIn(*ped, false);
            return vehicle ? *vehicle : 0;
        }

        [[nodiscard]] static std::string SanitizePlate(std::string text)
        {
            std::string out;
            out.reserve(8);
            for (const unsigned char ch : text)
            {
                if (out.size() >= 8)
                    break;
                if (ch >= 0x20 && ch <= 0x7E)
                    out.push_back(static_cast<char>(ch));
            }
            return out;
        }

        [[nodiscard]] static std::string SanitizeWeather(std::string text)
        {
            std::string out;
            out.reserve(std::min<std::size_t>(text.size(), 31));
            for (char ch : text)
            {
                if (out.size() >= 31)
                    break;
                if (ch >= 'a' && ch <= 'z')
                    ch = static_cast<char>(ch - 'a' + 'A');
                if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_')
                    out.push_back(ch);
            }
            return out;
        }

        bool BeginAction(std::string message)
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            m_ControlAttempts.store(0, std::memory_order_release);
            std::scoped_lock lock(m_Mutex);
            m_Message = std::move(message);
            return true;
        }

        bool QueueVehicleAction(std::string label, std::function<bool(Vehicle)> action)
        {
            if (!BeginAction(label))
                return false;

            if (!Runtime::GameRuntime::Get().Enqueue([this, label = std::move(label), action = std::move(action)]() mutable {
                    if (!ResolveHandlers())
                        return Finish(false, "Enhanced vehicle utility natives are unavailable");

                    const Vehicle vehicle = CurrentVehicle();
                    if (vehicle == 0)
                        return Finish(false, "Enter a vehicle before using this utility");

                    m_VehicleActionLabel = std::move(label);
                    m_VehicleAction = std::move(action);
                    VehicleControlTick(vehicle, 0);
                }))
            {
                Finish(false, "Game-thread queue unavailable");
                return false;
            }
            return true;
        }

        void VehicleControlTick(Vehicle vehicle, int attempt)
        {
            const auto exists = Natives::DoesEntityExist(vehicle);
            if (!exists || !*exists)
                return Finish(false, "Vehicle disappeared before network control was acquired");

            std::int32_t hasControl{};
            if (!Call(NetworkHasControlOfEntity, hasControl, vehicle))
                return Finish(false, "NETWORK_HAS_CONTROL_OF_ENTITY failed");

            if (hasControl != 0)
            {
                const bool ok = m_VehicleAction && m_VehicleAction(vehicle);
                const std::string label = m_VehicleActionLabel;
                m_VehicleAction = {};
                m_VehicleActionLabel.clear();
                return Finish(ok, ok ? label + " applied" : label + " failed");
            }

            std::int32_t requested{};
            if (!Call(NetworkRequestControlOfEntity, requested, vehicle))
                return Finish(false, "NETWORK_REQUEST_CONTROL_OF_ENTITY failed");

            m_ControlAttempts.store(attempt + 1, std::memory_order_release);
            if (attempt + 1 >= MaxControlAttempts)
            {
                m_VehicleAction = {};
                m_VehicleActionLabel.clear();
                return Finish(false, "Timed out waiting for network control of the current vehicle");
            }

            if (!Runtime::GameRuntime::Get().Enqueue([this, vehicle, attempt] {
                    VehicleControlTick(vehicle, attempt + 1);
                }))
            {
                m_VehicleAction = {};
                m_VehicleActionLabel.clear();
                Finish(false, "Game-thread queue unavailable while requesting vehicle control");
            }
        }

        void Finish(bool success, std::string message)
        {
            {
                std::scoped_lock lock(m_Mutex);
                m_HaveResult = true;
                m_LastSucceeded = success;
                m_Message = std::move(message);
            }
            m_Pending.store(false, std::memory_order_release);
        }

        std::array<Native::NativeHandler, HandlerCount> m_Handlers{};
        std::atomic<bool> m_Pending{false};
        std::atomic<int> m_ControlAttempts{0};

        std::function<bool(Vehicle)> m_VehicleAction{};
        std::string m_VehicleActionLabel{};

        mutable std::mutex m_Mutex;
        bool m_HaveResult{};
        bool m_LastSucceeded{};
        bool m_HaveSuspensionReadback{};
        float m_SuspensionLowering{};
        std::string m_Message{"Ready"};
    };
}
