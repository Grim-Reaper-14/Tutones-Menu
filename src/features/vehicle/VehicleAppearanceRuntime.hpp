#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/GameState.hpp"
#include "../../game/native/NativeCallContext.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <Windows.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

namespace Tutones::Game::Mods
{
    struct VehicleAppearanceSnapshot final
    {
        Vehicle vehicle{};
        int windowTint{};
        int plateStyle{};
        bool ready{};
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        std::string message{"Ready"};
    };

    class VehicleAppearanceRuntime final
    {
    public:
        static VehicleAppearanceRuntime& Get() noexcept
        {
            static VehicleAppearanceRuntime instance;
            return instance;
        }

        void RequestRefresh(Vehicle vehicle) noexcept
        {
            if (vehicle == 0)
            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot = {};
                return;
            }

            const auto now = ::GetTickCount64();
            const auto previousVehicle = m_LastRequestedVehicle.exchange(vehicle, std::memory_order_acq_rel);
            const auto next = m_NextRefreshMs.load(std::memory_order_acquire);
            if (previousVehicle == vehicle && now < next)
                return;

            m_NextRefreshMs.store(now + 250, std::memory_order_release);
            bool expected = false;
            if (!m_RefreshQueued.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return;

            if (!Runtime::GameRuntime::Get().Enqueue([this, vehicle] {
                    RefreshOnGameThread(vehicle);
                    m_RefreshQueued.store(false, std::memory_order_release);
                }))
            {
                m_RefreshQueued.store(false, std::memory_order_release);
            }
        }

        [[nodiscard]] bool QueueWindowTint(Vehicle vehicle, int tint)
        {
            if (vehicle == 0 || tint < 0 || tint > 6 || !CanQueue())
                return false;

            SetPending("Window tint queued");
            if (Runtime::GameRuntime::Get().Enqueue([this, vehicle, tint] {
                    if (!ValidateVehicle(vehicle) || !ResolveHandlers())
                        return Finish(false, "Vehicle changed or tint natives are unavailable");

                    std::int32_t current{};
                    const bool dispatched = CallVoid(SetWindowTint, vehicle, tint);
                    const bool readBack = Call(GetWindowTint, current, vehicle);
                    const bool success = dispatched && readBack && current == tint;
                    if (success)
                    {
                        std::scoped_lock lock(m_Mutex);
                        m_Snapshot.vehicle = vehicle;
                        m_Snapshot.windowTint = current;
                        m_Snapshot.ready = true;
                    }
                    Finish(success, success ? "Window tint verified" : "Window tint failed read-back verification");
                }))
            {
                Finish(false, "Game-thread queue unavailable");
                return false;
            }
            return true;
        }

        [[nodiscard]] bool QueuePlateStyle(Vehicle vehicle, int style)
        {
            if (vehicle == 0 || style < 0 || style > 12 || !CanQueue())
                return false;

            SetPending("Plate style queued");
            if (Runtime::GameRuntime::Get().Enqueue([this, vehicle, style] {
                    if (!ValidateVehicle(vehicle) || !ResolveHandlers())
                        return Finish(false, "Vehicle changed or plate natives are unavailable");

                    std::int32_t current{};
                    const bool dispatched = CallVoid(SetPlateStyle, vehicle, style);
                    const bool readBack = Call(GetPlateStyle, current, vehicle);
                    const bool success = dispatched && readBack && current == style;
                    if (success)
                    {
                        std::scoped_lock lock(m_Mutex);
                        m_Snapshot.vehicle = vehicle;
                        m_Snapshot.plateStyle = current;
                        m_Snapshot.ready = true;
                    }
                    Finish(success, success ? "Plate style verified" : "Plate style failed read-back verification");
                }))
            {
                Finish(false, "Game-thread queue unavailable");
                return false;
            }
            return true;
        }

        [[nodiscard]] VehicleAppearanceSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            auto snapshot = m_Snapshot;
            snapshot.pending = m_Pending.load(std::memory_order_acquire);
            return snapshot;
        }

    private:
        enum HandlerIndex : std::size_t
        {
            GetWindowTint,
            SetWindowTint,
            GetPlateStyle,
            SetPlateStyle,
            HandlerCount,
        };

        // Current Enhanced-side hashes from the YimMenuV2 crossmap.
        static constexpr std::array<std::uint64_t, HandlerCount> HandlerHashes{
            0xDA63CE76F9AAB439ull, // GET_VEHICLE_WINDOW_TINT
            0xFE620ED8E0A3C209ull, // SET_VEHICLE_WINDOW_TINT
            0x4F06416A18248EA0ull, // GET_VEHICLE_NUMBER_PLATE_TEXT_INDEX
            0x05D3F682DDA06C20ull, // SET_VEHICLE_NUMBER_PLATE_TEXT_INDEX
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

        VehicleAppearanceRuntime() = default;

        [[nodiscard]] bool CanQueue() noexcept
        {
            return Native::NativeRegistry::Get().IsReady()
                && !m_Pending.exchange(true, std::memory_order_acq_rel);
        }

        [[nodiscard]] bool ValidateVehicle(Vehicle expected) const noexcept
        {
            const auto state = GameState::Get().Snapshot();
            return state.nativeRuntimeReady && state.inVehicle && state.vehicle == expected;
        }

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
            if (m_Handlers[0])
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
                    TUTONES_LOG_ERROR("vehicle.appearance", "Enhanced vehicle appearance native resolution failed");
                    Core::Logging::Logger::Get().Flush();
                    m_Handlers.fill(nullptr);
                    return false;
                }
                m_Handlers[i] = reinterpret_cast<Native::NativeHandler>(address);
            }

            TUTONES_LOG_INFO("vehicle.appearance", "Enhanced window tint and plate style natives resolved");
            Core::Logging::Logger::Get().Flush();
            return true;
        }

        template <typename Ret, typename... Args>
        [[nodiscard]] bool Call(std::size_t index, Ret& out, Args... args) const noexcept
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

        template <typename... Args>
        [[nodiscard]] bool CallVoid(std::size_t index, Args... args) const noexcept
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

        void RefreshOnGameThread(Vehicle vehicle) noexcept
        {
            if (!ValidateVehicle(vehicle) || !ResolveHandlers())
                return;

            std::int32_t tint{};
            std::int32_t plate{};
            if (!Call(GetWindowTint, tint, vehicle) || !Call(GetPlateStyle, plate, vehicle))
                return;

            std::scoped_lock lock(m_Mutex);
            m_Snapshot.vehicle = vehicle;
            m_Snapshot.windowTint = tint;
            m_Snapshot.plateStyle = plate;
            m_Snapshot.ready = true;
        }

        void SetPending(std::string message)
        {
            std::scoped_lock lock(m_Mutex);
            m_Snapshot.haveResult = false;
            m_Snapshot.lastSucceeded = false;
            m_Snapshot.message = std::move(message);
        }

        void Finish(bool success, std::string message)
        {
            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot.haveResult = true;
                m_Snapshot.lastSucceeded = success;
                m_Snapshot.message = std::move(message);
            }
            m_Pending.store(false, std::memory_order_release);
        }

        std::array<Native::NativeHandler, HandlerCount> m_Handlers{};
        std::atomic<bool> m_Pending{false};
        std::atomic<bool> m_RefreshQueued{false};
        std::atomic<Vehicle> m_LastRequestedVehicle{0};
        std::atomic<ULONGLONG> m_NextRefreshMs{0};
        mutable std::mutex m_Mutex;
        VehicleAppearanceSnapshot m_Snapshot{};
    };
}
