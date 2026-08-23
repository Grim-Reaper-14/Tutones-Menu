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
#include <utility>

namespace Tutones::Game::Mods
{
    struct VehicleAppearanceSnapshot final
    {
        Vehicle vehicle{};
        int windowTint{};
        int plateStyle{};
        std::string plateText{};
        int interiorColor{};
        int dashboardColor{};
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
                m_LastRequestedVehicle.store(0, std::memory_order_release);
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

        [[nodiscard]] bool QueuePlateText(Vehicle vehicle, std::string plateText)
        {
            if (vehicle == 0 || !CanQueue())
                return false;

            if (plateText.size() > 8)
                plateText.resize(8);

            SetPending("Plate text queued");
            if (Runtime::GameRuntime::Get().Enqueue([this, vehicle, plateText = std::move(plateText)] {
                    if (!ValidateVehicle(vehicle) || !ResolveHandlers())
                        return Finish(false, "Vehicle changed or plate text natives are unavailable");

                    const bool dispatched = CallVoid(SetPlateText, vehicle, plateText.c_str());
                    const char* currentRaw{};
                    const bool readBack = Call(GetPlateText, currentRaw, vehicle);
                    const std::string current = NormalizePlateText(currentRaw ? currentRaw : "");
                    const bool success = dispatched
                        && readBack
                        && PlateCompareKey(current) == PlateCompareKey(plateText);
                    if (success)
                    {
                        std::scoped_lock lock(m_Mutex);
                        m_Snapshot.vehicle = vehicle;
                        m_Snapshot.plateText = current;
                        m_Snapshot.ready = true;
                    }
                    Finish(success, success ? "Plate text verified" : "Plate text failed read-back verification");
                }))
            {
                Finish(false, "Game-thread queue unavailable");
                return false;
            }
            return true;
        }

        [[nodiscard]] bool QueueInteriorColor(Vehicle vehicle, int color)
        {
            return QueueIndexedColor(
                vehicle,
                color,
                SetInteriorColor,
                GetInteriorColor,
                "Interior color queued",
                "Interior color verified",
                "Interior color failed read-back verification",
                [](VehicleAppearanceSnapshot& snapshot, int value) { snapshot.interiorColor = value; });
        }

        [[nodiscard]] bool QueueDashboardColor(Vehicle vehicle, int color)
        {
            return QueueIndexedColor(
                vehicle,
                color,
                SetDashboardColor,
                GetDashboardColor,
                "Dashboard color queued",
                "Dashboard color verified",
                "Dashboard color failed read-back verification",
                [](VehicleAppearanceSnapshot& snapshot, int value) { snapshot.dashboardColor = value; });
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
            GetPlateText,
            SetPlateText,
            GetInteriorColor,
            SetInteriorColor,
            GetDashboardColor,
            SetDashboardColor,
            HandlerCount,
        };

        static constexpr int MinIndexedColor = 0;
        static constexpr int MaxIndexedColor = 160;

        // Legacy native names are retained in comments for readability. The values
        // below are the current GTA V Enhanced handlers from YimMenuV2's crossmap.
        static constexpr std::array<std::uint64_t, HandlerCount> HandlerHashes{
            0xDA63CE76F9AAB439ull, // GET_VEHICLE_WINDOW_TINT
            0xFE620ED8E0A3C209ull, // SET_VEHICLE_WINDOW_TINT
            0x4F06416A18248EA0ull, // GET_VEHICLE_NUMBER_PLATE_TEXT_INDEX
            0x05D3F682DDA06C20ull, // SET_VEHICLE_NUMBER_PLATE_TEXT_INDEX
            0xCA7159F2C5FF745Aull, // GET_VEHICLE_NUMBER_PLATE_TEXT
            0x3FEAE59CDE6D3946ull, // SET_VEHICLE_NUMBER_PLATE_TEXT
            0xE10BD9712D7B0CBFull, // GET_VEHICLE_EXTRA_COLOUR_5 / interior color
            0xC0C8E6AAA00F1A58ull, // SET_VEHICLE_EXTRA_COLOUR_5 / interior color
            0x4C5611B5008205EBull, // GET_VEHICLE_EXTRA_COLOUR_6 / dashboard color
            0x77B012A683295B6Eull, // SET_VEHICLE_EXTRA_COLOUR_6 / dashboard color
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
            bool allResolved = true;
            for (const auto handler : m_Handlers)
                allResolved = allResolved && handler != nullptr;
            if (allResolved)
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

            TUTONES_LOG_INFO("vehicle.appearance", "Enhanced tint, plate, interior and dashboard natives resolved");
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

        template <typename Assign>
        [[nodiscard]] bool QueueIndexedColor(
            Vehicle vehicle,
            int color,
            std::size_t setHandler,
            std::size_t getHandler,
            const char* queuedMessage,
            const char* successMessage,
            const char* failureMessage,
            Assign assign)
        {
            if (vehicle == 0 || color < MinIndexedColor || color > MaxIndexedColor || !CanQueue())
                return false;

            SetPending(queuedMessage);
            if (Runtime::GameRuntime::Get().Enqueue(
                    [this, vehicle, color, setHandler, getHandler, successMessage, failureMessage, assign] {
                        if (!ValidateVehicle(vehicle) || !ResolveHandlers())
                            return Finish(false, "Vehicle changed or cabin color natives are unavailable");

                        std::int32_t current{};
                        const bool dispatched = CallVoid(setHandler, vehicle, color);
                        const bool readBack = CallVoid(getHandler, vehicle, &current);
                        const bool success = dispatched && readBack && current == color;
                        if (success)
                        {
                            std::scoped_lock lock(m_Mutex);
                            m_Snapshot.vehicle = vehicle;
                            assign(m_Snapshot, current);
                            m_Snapshot.ready = true;
                        }
                        Finish(success, success ? successMessage : failureMessage);
                    }))
            {
                Finish(false, "Game-thread queue unavailable");
                return false;
            }
            return true;
        }

        [[nodiscard]] static std::string NormalizePlateText(std::string value)
        {
            if (value.size() > 8)
                value.resize(8);
            return value;
        }

        [[nodiscard]] static std::string PlateCompareKey(std::string value)
        {
            value = NormalizePlateText(std::move(value));
            while (!value.empty() && value.back() == ' ')
                value.pop_back();
            for (char& ch : value)
            {
                if (ch >= 'a' && ch <= 'z')
                    ch = static_cast<char>(ch - 'a' + 'A');
            }
            return value;
        }

        void RefreshOnGameThread(Vehicle vehicle) noexcept
        {
            if (!ValidateVehicle(vehicle) || !ResolveHandlers())
                return;

            std::int32_t tint{};
            std::int32_t plateStyle{};
            std::int32_t interiorColor{};
            std::int32_t dashboardColor{};
            const char* plateTextRaw{};

            const bool ready = Call(GetWindowTint, tint, vehicle)
                && Call(GetPlateStyle, plateStyle, vehicle)
                && Call(GetPlateText, plateTextRaw, vehicle)
                && CallVoid(GetInteriorColor, vehicle, &interiorColor)
                && CallVoid(GetDashboardColor, vehicle, &dashboardColor);
            if (!ready)
                return;

            std::scoped_lock lock(m_Mutex);
            m_Snapshot.vehicle = vehicle;
            m_Snapshot.windowTint = tint;
            m_Snapshot.plateStyle = plateStyle;
            m_Snapshot.plateText = NormalizePlateText(plateTextRaw ? plateTextRaw : "");
            m_Snapshot.interiorColor = interiorColor;
            m_Snapshot.dashboardColor = dashboardColor;
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
