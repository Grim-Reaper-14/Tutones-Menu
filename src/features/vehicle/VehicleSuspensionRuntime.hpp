#pragma once

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

        void SetEnabled(bool enabled) noexcept
        {
            m_Enabled.store(enabled, std::memory_order_release);
            if (enabled)
            {
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

            // If the normal GTA-thread cleanup could not run during DLL teardown,
            // restore the cached writable address directly as a final fail-safe.
            RestoreCachedHeightDirect();
            m_Ticking.store(false, std::memory_order_release);
        }

    private:
        using HandleToPtrFn = void* (*)(int handle);

        static constexpr std::ptrdiff_t VisualSuspensionHeightOffset = 0x7C;

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

            // YimMenuV2 Enhanced resolves both conversions from this same block.
            // Tutones already resolves PtrToHandle from match - 0x0A; the mirrored
            // call at +0x22 is GTA Enhanced's HandleToPtr function.
            constexpr auto handlesAndPtrsPattern = "0F 1F 84 00 00 00 00 00 89 F8 0F 28 FE 41";
            auto* handlesMatch = Memory::PatternScanner::FindFirst(module, handlesAndPtrsPattern);
            auto* handleToPtrAddress = handlesMatch
                ? Memory::PatternScanner::ResolveRip(handlesMatch + 0x22)
                : nullptr;
            if (!handleToPtrAddress || !module.Contains(handleToPtrAddress))
                return false;

            // GTA's vehicle wheel update path exposes the current CVehicle offsets:
            // +8 = wheel count, +15 = wheel-array pointer, +23 = steering angle.
            constexpr auto wheelsPattern = "E8 ? ? ? ? 48 63 87 ? ? ? ? 48 8B 8F";
            auto* wheelsMatch = Memory::PatternScanner::FindFirst(module, wheelsPattern);
            if (!wheelsMatch)
                return false;

            std::uint32_t wheelsOffset{};
            std::memcpy(&wheelsOffset, wheelsMatch + 0x0F, sizeof(wheelsOffset));
            if (wheelsOffset < sizeof(void*) || wheelsOffset > 0x4000)
                return false;

            m_HandleToPtr = reinterpret_cast<HandleToPtrFn>(handleToPtrAddress);
            m_WheelsPtrOffset = wheelsOffset;
            m_Supported.store(true, std::memory_order_release);
            return true;
        }

        [[nodiscard]] bool GetSuspensionAddress(Vehicle vehicle, float*& outHeight) noexcept
        {
            outHeight = nullptr;
            if (vehicle == 0 || !ResolveSupport() || !m_HandleToPtr)
                return false;

            void* vehicleAddress = m_HandleToPtr(vehicle);
            if (!vehicleAddress)
                return false;

            std::uintptr_t wheelsAddress{};
            auto* wheelsPointerAddress = reinterpret_cast<std::byte*>(vehicleAddress) + m_WheelsPtrOffset;
            if (!ReadValue(wheelsPointerAddress, wheelsAddress) || wheelsAddress == 0)
                return false;

            auto* heightAddress = reinterpret_cast<float*>(wheelsAddress + VisualSuspensionHeightOffset);
            if (!IsReadable(heightAddress, sizeof(float)))
                return false;

            outHeight = heightAddress;
            return true;
        }

        [[nodiscard]] bool ReadSuspensionHeight(Vehicle vehicle, float& height, float*& address) noexcept
        {
            address = nullptr;
            if (!GetSuspensionAddress(vehicle, address))
                return false;
            return ReadValue(address, height) && std::isfinite(height);
        }

        [[nodiscard]] bool WriteSuspensionHeight(Vehicle vehicle, float height) noexcept
        {
            if (!std::isfinite(height))
                return false;

            float* address{};
            if (!GetSuspensionAddress(vehicle, address))
                return false;
            return WriteValue(address, height);
        }

        void RestoreCachedHeightDirect() noexcept
        {
            if (m_HasOriginalHeight && m_LastHeightAddress)
                static_cast<void>(WriteValue(m_LastHeightAddress, m_OriginalHeight));

            m_LastVehicle = 0;
            m_LastHeightAddress = nullptr;
            m_OriginalHeight = 0.0f;
            m_HasOriginalHeight = false;
        }

        void RestoreLastVehicle() noexcept
        {
            if (m_LastVehicle == 0)
            {
                RestoreCachedHeightDirect();
                return;
            }

            if (m_HasOriginalHeight)
            {
                const auto exists = Natives::DoesEntityExist(m_LastVehicle);
                if (exists && *exists)
                {
                    if (WriteSuspensionHeight(m_LastVehicle, m_OriginalHeight))
                    {
                        m_LastVehicle = 0;
                        m_LastHeightAddress = nullptr;
                        m_OriginalHeight = 0.0f;
                        m_HasOriginalHeight = false;
                        return;
                    }
                }
            }

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
                    if (ReadSuspensionHeight(vehicle, originalHeight, heightAddress))
                    {
                        m_LastVehicle = vehicle;
                        m_LastHeightAddress = heightAddress;
                        m_OriginalHeight = originalHeight;
                        m_HasOriginalHeight = true;
                    }
                }

                if (m_LastVehicle == vehicle && m_HasOriginalHeight)
                {
                    // Positive suspension-height values lower the visual wheel position.
                    // Apply this as an additive drop so normal LSC suspension stays intact.
                    const float lowering = m_LoweringAmount.load(std::memory_order_acquire);
                    const float targetHeight = std::clamp(m_OriginalHeight + lowering, -0.25f, 0.35f);
                    static_cast<void>(WriteSuspensionHeight(vehicle, targetHeight));
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
        std::atomic<bool> m_Ticking{false};
        bool m_ResolutionAttempted{};
        HandleToPtrFn m_HandleToPtr{};
        std::uint32_t m_WheelsPtrOffset{};
        Vehicle m_LastVehicle{};
        float* m_LastHeightAddress{};
        float m_OriginalHeight{};
        bool m_HasOriginalHeight{};
    };
}
