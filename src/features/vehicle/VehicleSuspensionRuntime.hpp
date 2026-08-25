#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/Natives.hpp"
#include "../../game/memory/PatternScanner.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <thread>

namespace Tutones::Game::Mods
{
    class VehicleSuspensionRuntime final
    {
    public:
        static VehicleSuspensionRuntime& Get() noexcept
        {
            static VehicleSuspensionRuntime instance;
            return instance;
        }

        [[nodiscard]] bool Enabled() const noexcept
        {
            return m_Enabled.load(std::memory_order_acquire);
        }

        [[nodiscard]] float LoweringAmount() const noexcept
        {
            return m_LoweringAmount.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool Supported() const noexcept
        {
            return m_Supported.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool LastWriteSucceeded() const noexcept
        {
            return m_LastWriteSucceeded.load(std::memory_order_acquire);
        }

        [[nodiscard]] const char* ActivePathName() const noexcept
        {
            return m_Supported.load(std::memory_order_acquire)
                ? "Enhanced CHandlingData fSuspensionRaise"
                : "resolving Enhanced handling data";
        }

        void SetEnabled(bool enabled) noexcept
        {
            m_Enabled.store(enabled, std::memory_order_release);
            if (enabled)
            {
                m_RefreshRequested.store(true, std::memory_order_release);
                m_LastWriteSucceeded.store(false, std::memory_order_release);
                EnsureTicking();
                return;
            }

            auto& runtime = Runtime::GameRuntime::Get();
            if (runtime.IsOnGameThread())
            {
                RestoreLastVehicle(true);
                return;
            }

            static_cast<void>(runtime.Enqueue([this] {
                if (!m_Enabled.load(std::memory_order_acquire))
                    RestoreLastVehicle(true);
            }));
        }

        void SetLoweringAmount(float amount) noexcept
        {
            if (!std::isfinite(amount))
                amount = 0.0f;

            m_LoweringAmount.store(std::clamp(amount, 0.0f, 0.20f), std::memory_order_release);
            m_RefreshRequested.store(true, std::memory_order_release);
            if (m_Enabled.load(std::memory_order_acquire))
                EnsureTicking();
        }

        void Shutdown() noexcept
        {
            m_Enabled.store(false, std::memory_order_release);

            const auto cleanup = [this] {
                RestoreLastVehicle(true);
            };

            auto& runtime = Runtime::GameRuntime::Get();
            if (runtime.IsOnGameThread())
            {
                cleanup();
            }
            else if (runtime.IsInitialized())
            {
                const auto cleaned = std::make_shared<std::atomic<bool>>(false);
                if (runtime.Enqueue([cleanup, cleaned] {
                        cleanup();
                        cleaned->store(true, std::memory_order_release);
                    }))
                {
                    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
                    while (!cleaned->load(std::memory_order_acquire)
                        && std::chrono::steady_clock::now() < deadline)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                }
            }

            RestoreCachedRaiseDirect();
            m_Ticking.store(false, std::memory_order_release);
            m_Supported.store(false, std::memory_order_release);
            m_LastWriteSucceeded.store(false, std::memory_order_release);
        }

    private:
        using HandleToPtrFn = void* (*)(int handle);

        // GTA5 Enhanced CVehicle/CHandlingData layout. Enhanced structure dumps
        // place the vehicle handling pointer at CVehicle + 0x960. The handling
        // layout keeps fSuspensionRaise at +0xD0; negative values lower the body.
        static constexpr std::ptrdiff_t EnhancedHandlingDataPtrOffset = 0x960;
        static constexpr std::ptrdiff_t HandlingMassOffset = 0x0C;
        static constexpr std::ptrdiff_t HandlingAccelerationOffset = 0x4C;
        static constexpr std::ptrdiff_t HandlingBrakeForceOffset = 0x6C;
        static constexpr std::ptrdiff_t SuspensionForceOffset = 0xBC;
        static constexpr std::ptrdiff_t SuspensionRaiseOffset = 0xD0;

        VehicleSuspensionRuntime() = default;
        ~VehicleSuspensionRuntime()
        {
            RestoreCachedRaiseDirect();
        }
        VehicleSuspensionRuntime(const VehicleSuspensionRuntime&) = delete;
        VehicleSuspensionRuntime& operator=(const VehicleSuspensionRuntime&) = delete;

        [[nodiscard]] Vehicle CurrentVehicle() const noexcept
        {
            if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread())
                return 0;

            const auto ped = Natives::PlayerPedId();
            if (!ped || *ped == 0)
                return 0;

            const auto inVehicle = Natives::IsPedInAnyVehicle(*ped, false);
            if (!inVehicle || !*inVehicle)
                return 0;

            const auto vehicle = Natives::GetVehiclePedIsIn(*ped, false);
            if (!vehicle || *vehicle == 0)
                return 0;

            const auto exists = Natives::DoesEntityExist(*vehicle);
            return exists && *exists ? *vehicle : 0;
        }

        static bool IsReadable(const void* address, std::size_t size) noexcept
        {
            if (!address || size == 0)
                return false;

            MEMORY_BASIC_INFORMATION memory{};
            if (::VirtualQuery(address, &memory, sizeof(memory)) != sizeof(memory))
                return false;
            if (memory.State != MEM_COMMIT || (memory.Protect & PAGE_GUARD) != 0 || memory.Protect == PAGE_NOACCESS)
                return false;

            const auto start = reinterpret_cast<std::uintptr_t>(address);
            const auto base = reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
            if (start < base || size > memory.RegionSize)
                return false;
            return start - base <= memory.RegionSize - size;
        }

        static bool IsWritable(void* address, std::size_t size) noexcept
        {
            if (!IsReadable(address, size))
                return false;

            MEMORY_BASIC_INFORMATION memory{};
            if (::VirtualQuery(address, &memory, sizeof(memory)) != sizeof(memory))
                return false;

            switch (memory.Protect & 0xFF)
            {
            case PAGE_READWRITE:
            case PAGE_WRITECOPY:
            case PAGE_EXECUTE_READWRITE:
            case PAGE_EXECUTE_WRITECOPY:
                return true;
            default:
                return false;
            }
        }

        template <typename T>
        static bool ReadValue(const void* address, T& value) noexcept
        {
            if (!IsReadable(address, sizeof(T)))
                return false;

            std::memcpy(&value, address, sizeof(T));
            return true;
        }

        template <typename T>
        static bool WriteValue(void* address, const T& value) noexcept
        {
            if (!IsWritable(address, sizeof(T)))
                return false;

            std::memcpy(address, &value, sizeof(T));
            return true;
        }

        bool ResolveHandleToPtr() noexcept
        {
            if (m_HandleToPtr)
                return true;
            if (m_HandleResolutionAttempted)
                return false;

            const auto& module = GamePointers::Get().Module();
            if (!module.IsValid())
                return false;

            m_HandleResolutionAttempted = true;

            // Current YimMenuV2 Enhanced handle conversion block.
            constexpr auto handlesAndPtrsPattern = "0F 1F 84 00 00 00 00 00 89 F8 0F 28 FE 41";
            auto* handlesMatch = Memory::PatternScanner::FindFirst(module, handlesAndPtrsPattern);
            auto* handleToPtrAddress = handlesMatch
                ? Memory::PatternScanner::ResolveRip(handlesMatch + 0x22)
                : nullptr;
            if (!handleToPtrAddress || !module.Contains(handleToPtrAddress))
            {
                TUTONES_LOG_WARN(
                    "vehicle.suspension",
                    "Enhanced HandleToPtr resolution failed; extra suspension lowering is unavailable");
                return false;
            }

            m_HandleToPtr = reinterpret_cast<HandleToPtrFn>(handleToPtrAddress);
            TUTONES_LOG_INFO(
                "vehicle.suspension",
                "Enhanced HandleToPtr resolved for CHandlingData ride-height control");
            return true;
        }

        [[nodiscard]] static bool IsPlausibleHandlingData(std::uintptr_t handlingAddress) noexcept
        {
            if (handlingAddress == 0)
                return false;

            const auto* base = reinterpret_cast<const std::byte*>(handlingAddress);
            float mass{};
            float acceleration{};
            float brakeForce{};
            float suspensionForce{};
            float suspensionRaise{};

            if (!ReadValue(base + HandlingMassOffset, mass)
                || !ReadValue(base + HandlingAccelerationOffset, acceleration)
                || !ReadValue(base + HandlingBrakeForceOffset, brakeForce)
                || !ReadValue(base + SuspensionForceOffset, suspensionForce)
                || !ReadValue(base + SuspensionRaiseOffset, suspensionRaise))
            {
                return false;
            }

            return std::isfinite(mass)
                && std::isfinite(acceleration)
                && std::isfinite(brakeForce)
                && std::isfinite(suspensionForce)
                && std::isfinite(suspensionRaise)
                && mass > 50.0f && mass < 100000.0f
                && acceleration >= 0.0f && acceleration < 100.0f
                && brakeForce >= 0.0f && brakeForce < 50.0f
                && suspensionForce >= 0.0f && suspensionForce < 100.0f
                && suspensionRaise > -2.0f && suspensionRaise < 2.0f;
        }

        [[nodiscard]] bool GetSuspensionRaiseAddress(Vehicle vehicle, float*& outRaise) noexcept
        {
            outRaise = nullptr;
            if (vehicle == 0 || !ResolveHandleToPtr() || !m_HandleToPtr)
                return false;

            void* vehicleAddress = m_HandleToPtr(vehicle);
            if (!vehicleAddress || !IsReadable(vehicleAddress, EnhancedHandlingDataPtrOffset + sizeof(void*)))
                return false;

            std::uintptr_t handlingAddress{};
            auto* handlingPointerAddress = reinterpret_cast<std::byte*>(vehicleAddress) + EnhancedHandlingDataPtrOffset;
            if (!ReadValue(handlingPointerAddress, handlingAddress)
                || !IsPlausibleHandlingData(handlingAddress))
            {
                m_Supported.store(false, std::memory_order_release);
                return false;
            }

            auto* raiseAddress = reinterpret_cast<float*>(handlingAddress + SuspensionRaiseOffset);
            if (!IsWritable(raiseAddress, sizeof(float)))
            {
                m_Supported.store(false, std::memory_order_release);
                return false;
            }

            m_Supported.store(true, std::memory_order_release);
            outRaise = raiseAddress;
            return true;
        }

        void ClearCachedVehicle() noexcept
        {
            m_LastVehicle = 0;
            m_LastRaiseAddress = nullptr;
            m_OriginalRaise = 0.0f;
            m_HasOriginalRaise = false;
            m_LastWriteSucceeded.store(false, std::memory_order_release);
        }

        void RestoreCachedRaiseDirect() noexcept
        {
            if (m_HasOriginalRaise && m_LastRaiseAddress)
                static_cast<void>(WriteValue(m_LastRaiseAddress, m_OriginalRaise));
            ClearCachedVehicle();
        }

        void RestoreLastVehicle(bool refreshPhysics) noexcept
        {
            const Vehicle previousVehicle = m_LastVehicle;
            const bool restored = m_HasOriginalRaise
                && m_LastRaiseAddress
                && WriteValue(m_LastRaiseAddress, m_OriginalRaise);

            if (refreshPhysics && restored && previousVehicle != 0)
                static_cast<void>(Natives::SetVehicleOnGroundProperly(previousVehicle, 5.0f));

            ClearCachedVehicle();
        }

        void EnsureTicking() noexcept
        {
            if (!m_Enabled.load(std::memory_order_acquire))
                return;

            bool expected = false;
            if (!m_Ticking.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return;

            if (!Runtime::GameRuntime::Get().Enqueue([this] { TickOnGameThread(); }))
                m_Ticking.store(false, std::memory_order_release);
        }

        void TickOnGameThread() noexcept
        {
            const bool enabled = m_Enabled.load(std::memory_order_acquire);
            const Vehicle vehicle = CurrentVehicle();

            if (enabled && vehicle != 0)
            {
                if (m_LastVehicle != 0 && m_LastVehicle != vehicle)
                {
                    RestoreLastVehicle(true);
                    m_RefreshRequested.store(true, std::memory_order_release);
                }

                if (m_LastVehicle == 0)
                {
                    float* raiseAddress{};
                    float originalRaise{};
                    if (GetSuspensionRaiseAddress(vehicle, raiseAddress)
                        && ReadValue(raiseAddress, originalRaise)
                        && std::isfinite(originalRaise))
                    {
                        m_LastVehicle = vehicle;
                        m_LastRaiseAddress = raiseAddress;
                        m_OriginalRaise = originalRaise;
                        m_HasOriginalRaise = true;
                        m_RefreshRequested.store(true, std::memory_order_release);
                        TUTONES_LOG_INFO(
                            "vehicle.suspension",
                            "Extra lowering attached to Enhanced CVehicle + 0x960 -> CHandlingData + 0xD0");
                    }
                    else
                    {
                        m_LastWriteSucceeded.store(false, std::memory_order_release);
                    }
                }

                if (m_LastVehicle == vehicle && m_HasOriginalRaise && m_LastRaiseAddress)
                {
                    const float lowering = m_LoweringAmount.load(std::memory_order_acquire);
                    const float targetRaise = std::clamp(m_OriginalRaise - lowering, -0.50f, 0.50f);
                    const bool wrote = WriteValue(m_LastRaiseAddress, targetRaise);
                    m_LastWriteSucceeded.store(wrote, std::memory_order_release);

                    if (wrote && m_RefreshRequested.exchange(false, std::memory_order_acq_rel))
                    {
                        // fSuspensionRaise is consumed by vehicle physics. Re-grounding
                        // once after an enable/slider change makes the new ride height
                        // visible immediately instead of waiting for the car to move.
                        static_cast<void>(Natives::SetVehicleOnGroundProperly(vehicle, 5.0f));
                    }
                }
            }
            else
            {
                RestoreLastVehicle(false);
                if (enabled)
                    m_Supported.store(false, std::memory_order_release);
            }

            if (m_Enabled.load(std::memory_order_acquire)
                && Runtime::GameRuntime::Get().Enqueue([this] { TickOnGameThread(); }))
            {
                return;
            }

            RestoreLastVehicle(false);
            m_Ticking.store(false, std::memory_order_release);
            if (m_Enabled.load(std::memory_order_acquire))
                EnsureTicking();
        }

        std::atomic<bool> m_Enabled{false};
        std::atomic<float> m_LoweringAmount{0.08f};
        std::atomic<bool> m_Supported{false};
        std::atomic<bool> m_LastWriteSucceeded{false};
        std::atomic<bool> m_RefreshRequested{false};
        std::atomic<bool> m_Ticking{false};
        bool m_HandleResolutionAttempted{};
        HandleToPtrFn m_HandleToPtr{};
        Vehicle m_LastVehicle{};
        float* m_LastRaiseAddress{};
        float m_OriginalRaise{};
        bool m_HasOriginalRaise{};
    };
}
