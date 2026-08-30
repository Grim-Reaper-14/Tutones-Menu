#pragma once

#include "../../game/GamePointers.hpp"
#include "../../game/Natives.hpp"
#include "../../game/native/NativeCallContext.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <Windows.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace Tutones::Game::Mods
{
    class VehicleAmmoRuntime final
    {
    public:
        static VehicleAmmoRuntime& Get() noexcept
        {
            static VehicleAmmoRuntime instance;
            return instance;
        }

        [[nodiscard]] bool Enabled() const noexcept
        {
            return m_Enabled.load(std::memory_order_acquire);
        }

        void SetEnabled(bool enabled) noexcept
        {
            m_Enabled.store(enabled, std::memory_order_release);
            EnsureTicking();
        }

        [[nodiscard]] Vehicle CurrentVehicleHandle() const noexcept
        {
            return m_CurrentVehicleView.load(std::memory_order_acquire);
        }

        [[nodiscard]] std::uint32_t CurrentWeaponHash() const noexcept
        {
            return m_CurrentWeaponView.load(std::memory_order_acquire);
        }

        [[nodiscard]] std::size_t TrackedWeaponCount() const noexcept
        {
            return m_TrackedCountView.load(std::memory_order_acquire);
        }

        void Shutdown() noexcept
        {
            m_Enabled.store(false, std::memory_order_release);
            EnsureTicking();
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

        struct TrackedWeapon final
        {
            Vehicle vehicle{};
            std::uint32_t weaponHash{};
            std::int32_t originalAmmo{};
            bool occupied{};
        };

        static_assert(offsetof(NativeProgram, nativeCount) == 0x2C);
        static_assert(offsetof(NativeProgram, nativeEntrypoints) == 0x40);
        static_assert(sizeof(NativeProgram) == 0x80);

        enum HandlerIndex : std::size_t
        {
            GetCurrentPedVehicleWeapon,
            GetVehicleWeaponRestrictedAmmo,
            SetVehicleWeaponRestrictedAmmo,
            HandlerCount,
        };

        // GTA V Enhanced 1.73 / b1158.13 mappings verified against the current
        // YimMenuV2 Enhanced crossmap. The restricted-ammo native accepts -1 as
        // unlimited, so GTA continues to own projectile spawning, targeting and fire rate.
        static constexpr std::array<std::uint64_t, HandlerCount> HandlerHashes{
            0x51B462E1DEB9F762ull, // GET_CURRENT_PED_VEHICLE_WEAPON
            0x73C3D75DAC71F876ull, // GET_VEHICLE_WEAPON_RESTRICTED_AMMO
            0x5951A2AB1DF37E03ull, // SET_VEHICLE_WEAPON_RESTRICTED_AMMO
        };

        static constexpr std::size_t MaxTrackedWeapons = 32;
        static constexpr std::int32_t InfiniteAmmo = -1;

        VehicleAmmoRuntime() = default;
        ~VehicleAmmoRuntime() = default;
        VehicleAmmoRuntime(const VehicleAmmoRuntime&) = delete;
        VehicleAmmoRuntime& operator=(const VehicleAmmoRuntime&) = delete;

        [[nodiscard]] static bool IsExecutableAddress(std::uintptr_t address) noexcept
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

        [[nodiscard]] bool ResolveHandlers() noexcept
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

            for (std::size_t index = 0; index < slots.size(); ++index)
            {
                const auto address = static_cast<std::uintptr_t>(slots[index]);
                if (!IsExecutableAddress(address))
                {
                    m_Handlers.fill(nullptr);
                    return false;
                }
                m_Handlers[index] = reinterpret_cast<Native::NativeHandler>(address);
            }

            return true;
        }

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
            return vehicle ? *vehicle : 0;
        }

        [[nodiscard]] bool CurrentVehicleWeapon(Ped ped, std::uint32_t& outWeaponHash) noexcept
        {
            if (ped == 0 || !ResolveHandlers())
                return false;

            Native::CallContext context;
            if (!context.PushArg(ped) || !context.PushArg(&outWeaponHash))
                return false;

            m_Handlers[GetCurrentPedVehicleWeapon](&context);
            return context.GetReturnValue<std::int32_t>() != 0 && outWeaponHash != 0;
        }

        [[nodiscard]] bool GetRestrictedAmmo(Vehicle vehicle, std::uint32_t weaponHash, std::int32_t& outAmmo) noexcept
        {
            if (vehicle == 0 || weaponHash == 0 || !ResolveHandlers())
                return false;

            Native::CallContext context;
            if (!context.PushArg(vehicle) || !context.PushArg(static_cast<std::int32_t>(weaponHash)))
                return false;

            m_Handlers[GetVehicleWeaponRestrictedAmmo](&context);
            outAmmo = context.GetReturnValue<std::int32_t>();
            return true;
        }

        [[nodiscard]] bool SetRestrictedAmmo(Vehicle vehicle, std::uint32_t weaponHash, std::int32_t ammo) noexcept
        {
            if (vehicle == 0 || weaponHash == 0 || !ResolveHandlers())
                return false;

            Native::CallContext context;
            if (!context.PushArg(vehicle)
                || !context.PushArg(static_cast<std::int32_t>(weaponHash))
                || !context.PushArg(ammo))
            {
                return false;
            }

            m_Handlers[SetVehicleWeaponRestrictedAmmo](&context);
            return true;
        }

        [[nodiscard]] TrackedWeapon* FindTracked(Vehicle vehicle, std::uint32_t weaponHash) noexcept
        {
            for (auto& entry : m_Tracked)
            {
                if (entry.occupied && entry.vehicle == vehicle && entry.weaponHash == weaponHash)
                    return &entry;
            }
            return nullptr;
        }

        [[nodiscard]] bool TrackWeapon(Vehicle vehicle, std::uint32_t weaponHash) noexcept
        {
            if (FindTracked(vehicle, weaponHash))
                return true;

            std::int32_t originalAmmo{};
            if (!GetRestrictedAmmo(vehicle, weaponHash, originalAmmo))
                return false;

            for (auto& entry : m_Tracked)
            {
                if (entry.occupied)
                    continue;

                entry.vehicle = vehicle;
                entry.weaponHash = weaponHash;
                entry.originalAmmo = originalAmmo;
                entry.occupied = true;
                ++m_TrackedCount;
                m_TrackedCountView.store(m_TrackedCount, std::memory_order_release);
                return true;
            }

            // Do not make a state change that cannot be restored later.
            return false;
        }

        void RestoreTracked() noexcept
        {
            for (auto& entry : m_Tracked)
            {
                if (!entry.occupied)
                    continue;

                const auto exists = Natives::DoesEntityExist(entry.vehicle);
                if (exists && *exists)
                    static_cast<void>(SetRestrictedAmmo(entry.vehicle, entry.weaponHash, entry.originalAmmo));

                entry = {};
            }

            m_TrackedCount = 0;
            m_TrackedCountView.store(0, std::memory_order_release);
            m_CurrentWeaponView.store(0, std::memory_order_release);
        }

        void EnsureTicking() noexcept
        {
            bool expected = false;
            if (!m_Ticking.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return;

            if (!QueueNextTick())
                m_Ticking.store(false, std::memory_order_release);
        }

        [[nodiscard]] bool QueueNextTick() noexcept
        {
            return Runtime::GameRuntime::Get().Enqueue([this] { TickOnGameThread(); });
        }

        void TickOnGameThread() noexcept
        {
            if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread() || !ResolveHandlers())
            {
                if (!m_Enabled.load(std::memory_order_acquire))
                {
                    m_CurrentVehicle = 0;
                    m_CurrentVehicleView.store(0, std::memory_order_release);
                    m_CurrentWeaponView.store(0, std::memory_order_release);
                    m_Ticking.store(false, std::memory_order_release);
                    return;
                }

                if (!QueueNextTick())
                    m_Ticking.store(false, std::memory_order_release);
                return;
            }

            if (!m_Enabled.load(std::memory_order_acquire))
            {
                RestoreTracked();
                m_CurrentVehicle = 0;
                m_CurrentVehicleView.store(0, std::memory_order_release);
                m_Ticking.store(false, std::memory_order_release);
                return;
            }

            const Vehicle vehicle = CurrentVehicle();
            if (vehicle != m_CurrentVehicle)
            {
                RestoreTracked();
                m_CurrentVehicle = vehicle;
                m_CurrentVehicleView.store(vehicle, std::memory_order_release);
            }

            if (vehicle != 0)
            {
                const auto ped = Natives::PlayerPedId();
                std::uint32_t weaponHash{};
                if (ped && *ped != 0 && CurrentVehicleWeapon(*ped, weaponHash))
                {
                    m_CurrentWeaponView.store(weaponHash, std::memory_order_release);
                    if (TrackWeapon(vehicle, weaponHash))
                        static_cast<void>(SetRestrictedAmmo(vehicle, weaponHash, InfiniteAmmo));
                }
                else
                {
                    m_CurrentWeaponView.store(0, std::memory_order_release);
                }
            }
            else
            {
                m_CurrentWeaponView.store(0, std::memory_order_release);
            }

            if (!QueueNextTick())
            {
                RestoreTracked();
                m_CurrentVehicle = 0;
                m_CurrentVehicleView.store(0, std::memory_order_release);
                m_Ticking.store(false, std::memory_order_release);
            }
        }

        std::atomic<bool> m_Enabled{false};
        std::atomic<bool> m_Ticking{false};
        std::atomic<Vehicle> m_CurrentVehicleView{0};
        std::atomic<std::uint32_t> m_CurrentWeaponView{0};
        std::atomic<std::size_t> m_TrackedCountView{0};

        std::array<Native::NativeHandler, HandlerCount> m_Handlers{};
        std::array<TrackedWeapon, MaxTrackedWeapons> m_Tracked{};
        Vehicle m_CurrentVehicle{};
        std::size_t m_TrackedCount{};
    };
}
