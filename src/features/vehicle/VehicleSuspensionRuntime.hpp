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
    enum class VehicleSuspensionPath : std::uint8_t
    {
        None,
        VisualWheelHeight,
        EnhancedHandlingRaise,
    };

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

        [[nodiscard]] VehicleSuspensionPath ActivePath() const noexcept
        {
            return static_cast<VehicleSuspensionPath>(m_ActivePath.load(std::memory_order_acquire));
        }

        [[nodiscard]] const char* ActivePathName() const noexcept
        {
            switch (ActivePath())
            {
            case VehicleSuspensionPath::VisualWheelHeight:
                return "Enhanced wheel visual-height";
            case VehicleSuspensionPath::EnhancedHandlingRaise:
                return "Enhanced handling fSuspensionRaise";
            default:
                return "resolving";
            }
        }

        void SetEnabled(bool enabled) noexcept
        {
            m_Enabled.store(enabled, std::memory_order_release);
            if (enabled)
            {
                m_LastWriteSucceeded.store(false, std::memory_order_release);
                EnsureTicking();
                return;
            }

            auto& runtime = Runtime::GameRuntime::Get();
            if (runtime.IsOnGameThread())
            {
                RestoreLastVehicle();
                return;
            }

            static_cast<void>(runtime.Enqueue([this] {
                if (!m_Enabled.load(std::memory_order_acquire))
                    RestoreLastVehicle();
            }));
        }

        void SetLoweringAmount(float amount) noexcept
        {
            if (!std::isfinite(amount))
                amount = 0.0f;

            m_LoweringAmount.store(std::clamp(amount, 0.0f, 0.20f), std::memory_order_release);
            if (m_Enabled.load(std::memory_order_acquire))
                EnsureTicking();
        }

        void Shutdown() noexcept
        {
            m_Enabled.store(false, std::memory_order_release);

            const auto cleanup = [this] {
                RestoreLastVehicle();
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

            RestoreCachedHeightDirect();
            m_Ticking.store(false, std::memory_order_release);
            m_LastWriteSucceeded.store(false, std::memory_order_release);
            m_ActivePath.store(static_cast<std::uint8_t>(VehicleSuspensionPath::None), std::memory_order_release);
        }

    private:
        using HandleToPtrFn = void* (*)(int handle);

        // FiveM exposes this CVehicle wheel-block field at +0x7C. Some Enhanced
        // builds still retain the same wheel layout, so keep it as the preferred
        // immediate visual path when its signature resolves.
        static constexpr std::ptrdiff_t VisualSuspensionHeightOffset = 0x7C;

        // GTA5 Enhanced CVehicle layout. Public Enhanced structure dumps place
        // CHandlingData at CVehicle + 0x960. fSuspensionRaise remains at +0xD0
        // in CHandlingData and negative values lower the vehicle.
        static constexpr std::ptrdiff_t EnhancedHandlingDataPtrOffset = 0x960;
        static constexpr std::ptrdiff_t SuspensionForceOffset = 0xBC;
        static constexpr std::ptrdiff_t SuspensionRaiseOffset = 0xD0;
        static constexpr std::ptrdiff_t HandlingMassOffset = 0x0C;
        static constexpr std::ptrdiff_t HandlingAccelerationOffset = 0x4C;
        static constexpr std::ptrdiff_t HandlingBrakeForceOffset = 0x6C;

        VehicleSuspensionRuntime() = default;
        ~VehicleSuspensionRuntime()
        {
            RestoreCachedHeightDirect();
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

        bool ResolveSupport() noexcept
        {
            if (m_ResolutionAttempted)
                return m_Supported.load(std::memory_order_acquire);

            const auto& module = GamePointers::Get().Module();
            if (!module.IsValid())
                return false;

            m_ResolutionAttempted = true;

            // YimMenuV2 Enhanced resolves both handle conversions from this block.
            constexpr auto handlesAndPtrsPattern = "0F 1F 84 00 00 00 00 00 89 F8 0F 28 FE 41";
            auto* handlesMatch = Memory::PatternScanner::FindFirst(module, handlesAndPtrsPattern);
            auto* handleToPtrAddress = handlesMatch
                ? Memory::PatternScanner::ResolveRip(handlesMatch + 0x22)
                : nullptr;
            if (!handleToPtrAddress || !module.Contains(handleToPtrAddress))
            {
                TUTONES_LOG_WARN("vehicle.suspension", "Enhanced HandleToPtr resolution failed; extra lowering is unavailable");
                return false;
            }

            m_HandleToPtr = reinterpret_cast<HandleToPtrFn>(handleToPtrAddress);

            // This wheel signature originated in the FiveM/Legacy vehicle path and
            // is optional on Enhanced. If it does not match, do NOT fail the feature;
            // fall back to Enhanced CVehicle->CHandlingData->fSuspensionRaise.
            constexpr auto wheelsPattern = "E8 ? ? ? ? 48 63 87 ? ? ? ? 48 8B 8F";
            auto* wheelsMatch = Memory::PatternScanner::FindFirst(module, wheelsPattern);
            if (wheelsMatch)
            {
                std::uint32_t wheelsOffset{};
                std::memcpy(&wheelsOffset, wheelsMatch + 0x0F, sizeof(wheelsOffset));
                if (wheelsOffset >= sizeof(void*) && wheelsOffset <= 0x4000)
                {
                    m_WheelsPtrOffset = wheelsOffset;
                    TUTONES_LOG_INFO("vehicle.suspension", "Enhanced wheel visual-height path resolved");
                }
            }

            if (m_WheelsPtrOffset == 0)
            {
                TUTONES_LOG_INFO(
                    "vehicle.suspension",
                    "Wheel visual-height signature is unavailable on this Enhanced build; using CHandlingData fSuspensionRaise fallback");
            }

            m_Supported.store(true, std::memory_order_release);
            return true;
        }

        [[nodiscard]] static bool IsPlausibleHandlingData(std::uintptr_t handlingAddress) noexcept
        {
            if (handlingAddress == 0)
                return false;

            float mass{};
            float acceleration{};
            float brakeForce{};
            float suspensionForce{};
            float suspensionRaise{};

            const auto* base = reinterpret_cast<const std::byte*>(handlingAddress);
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

        [[nodiscard]] bool TryGetVisualWheelHeightAddress(void* vehicleAddress, float*& outHeight) noexcept
        {
            outHeight = nullptr;
            if (!vehicleAddress || m_WheelsPtrOffset == 0)
                return false;

            std::uintptr_t wheelsAddress{};
            auto* wheelsPointerAddress = reinterpret_cast<std::byte*>(vehicleAddress) + m_WheelsPtrOffset;
            if (!ReadValue(wheelsPointerAddress, wheelsAddress) || wheelsAddress == 0)
                return false;

            auto* heightAddress = reinterpret_cast<float*>(wheelsAddress + VisualSuspensionHeightOffset);
            float value{};
            if (!ReadValue(heightAddress, value)
                || !std::isfinite(value)
                || value <= -2.0f
                || value >= 2.0f
                || !IsWritable(heightAddress, sizeof(float)))
            {
                return false;
            }

            outHeight = heightAddress;
            return true;
        }

        [[nodiscard]] bool TryGetEnhancedHandlingRaiseAddress(void* vehicleAddress, float*& outHeight) noexcept
        {
            outHeight = nullptr;
            if (!vehicleAddress)
                return false;

            std::uintptr_t handlingAddress{};
            auto* handlingPointerAddress = reinterpret_cast<std::byte*>(vehicleAddress) + EnhancedHandlingDataPtrOffset;
            if (!ReadValue(handlingPointerAddress, handlingAddress)
                || !IsPlausibleHandlingData(handlingAddress))
            {
                return false;
            }

            auto* raiseAddress = reinterpret_cast<float*>(handlingAddress + SuspensionRaiseOffset);
            if (!IsWritable(raiseAddress, sizeof(float)))
                return false;

            outHeight = raiseAddress;
            return true;
        }

        [[nodiscard]] bool GetSuspensionAddress(
            Vehicle vehicle,
            float*& outHeight,
            VehicleSuspensionPath& outPath) noexcept
        {
            outHeight = nullptr;
            outPath = VehicleSuspensionPath::None;
            if (vehicle == 0 || !ResolveSupport() || !m_HandleToPtr)
                return false;

            void* vehicleAddress = m_HandleToPtr(vehicle);
            if (!vehicleAddress || !IsReadable(vehicleAddress, sizeof(void*)))
                return false;

            if (TryGetVisualWheelHeightAddress(vehicleAddress, outHeight))
            {
                outPath = VehicleSuspensionPath::VisualWheelHeight;
                return true;
            }

            if (TryGetEnhancedHandlingRaiseAddress(vehicleAddress, outHeight))
            {
                outPath = VehicleSuspensionPath::EnhancedHandlingRaise;
                return true;
            }

            return false;
        }

        [[nodiscard]] bool ReadSuspensionHeight(
            Vehicle vehicle,
            float& height,
            float*& address,
            VehicleSuspensionPath& path) noexcept
        {
            address = nullptr;
            path = VehicleSuspensionPath::None;
            if (!GetSuspensionAddress(vehicle, address, path))
                return false;
            return ReadValue(address, height) && std::isfinite(height);
        }

        void RestoreCachedHeightDirect() noexcept
        {
            if (m_HasOriginalHeight && m_LastHeightAddress)
                static_cast<void>(WriteValue(m_LastHeightAddress, m_OriginalHeight));

            m_LastVehicle = 0;
            m_LastHeightAddress = nullptr;
            m_OriginalHeight = 0.0f;
            m_LastPath = VehicleSuspensionPath::None;
            m_HasOriginalHeight = false;
            m_LastWriteSucceeded.store(false, std::memory_order_release);
            m_ActivePath.store(static_cast<std::uint8_t>(VehicleSuspensionPath::None), std::memory_order_release);
        }

        void RestoreLastVehicle() noexcept
        {
            RestoreCachedHeightDirect();
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
                    RestoreLastVehicle();

                if (m_LastVehicle == 0)
                {
                    float originalHeight{};
                    float* heightAddress{};
                    VehicleSuspensionPath path{VehicleSuspensionPath::None};
                    if (ReadSuspensionHeight(vehicle, originalHeight, heightAddress, path))
                    {
                        m_LastVehicle = vehicle;
                        m_LastHeightAddress = heightAddress;
                        m_OriginalHeight = originalHeight;
                        m_LastPath = path;
                        m_HasOriginalHeight = true;
                        m_ActivePath.store(static_cast<std::uint8_t>(path), std::memory_order_release);

                        if (path == VehicleSuspensionPath::VisualWheelHeight)
                        {
                            TUTONES_LOG_INFO(
                                "vehicle.suspension",
                                "Extra lowering attached to Enhanced wheel visual-height path");
                        }
                        else if (path == VehicleSuspensionPath::EnhancedHandlingRaise)
                        {
                            TUTONES_LOG_INFO(
                                "vehicle.suspension",
                                "Extra lowering attached to Enhanced CHandlingData fSuspensionRaise fallback");
                        }
                    }
                    else
                    {
                        m_LastWriteSucceeded.store(false, std::memory_order_release);
                        m_ActivePath.store(static_cast<std::uint8_t>(VehicleSuspensionPath::None), std::memory_order_release);
                    }
                }

                if (m_LastVehicle == vehicle && m_HasOriginalHeight && m_LastHeightAddress)
                {
                    const float lowering = m_LoweringAmount.load(std::memory_order_acquire);
                    float targetHeight{};

                    if (m_LastPath == VehicleSuspensionPath::EnhancedHandlingRaise)
                    {
                        // fSuspensionRaise uses negative values to lower the body.
                        targetHeight = std::clamp(m_OriginalHeight - lowering, -0.50f, 0.50f);
                    }
                    else
                    {
                        // The wheel visual-height field uses positive values to lower.
                        targetHeight = std::clamp(m_OriginalHeight + lowering, -0.25f, 0.35f);
                    }

                    m_LastWriteSucceeded.store(
                        WriteValue(m_LastHeightAddress, targetHeight),
                        std::memory_order_release);
                }
            }
            else
            {
                RestoreLastVehicle();
            }

            if (m_Enabled.load(std::memory_order_acquire)
                && Runtime::GameRuntime::Get().Enqueue([this] { TickOnGameThread(); }))
            {
                return;
            }

            RestoreLastVehicle();
            m_Ticking.store(false, std::memory_order_release);
            if (m_Enabled.load(std::memory_order_acquire))
                EnsureTicking();
        }

        std::atomic<bool> m_Enabled{false};
        std::atomic<float> m_LoweringAmount{0.08f};
        std::atomic<bool> m_Supported{false};
        std::atomic<bool> m_LastWriteSucceeded{false};
        std::atomic<bool> m_Ticking{false};
        std::atomic<std::uint8_t> m_ActivePath{static_cast<std::uint8_t>(VehicleSuspensionPath::None)};
        bool m_ResolutionAttempted{};
        HandleToPtrFn m_HandleToPtr{};
        std::uint32_t m_WheelsPtrOffset{};
        Vehicle m_LastVehicle{};
        float* m_LastHeightAddress{};
        float m_OriginalHeight{};
        VehicleSuspensionPath m_LastPath{VehicleSuspensionPath::None};
        bool m_HasOriginalHeight{};
    };
}
