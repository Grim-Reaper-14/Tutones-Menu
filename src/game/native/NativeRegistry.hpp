#pragma once

#include "NativeCallContext.hpp"
#include "../GamePointers.hpp"

#include <Windows.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace Tutones::Game::Native
{
    enum class NativeId : std::size_t
    {
        PlayerPedId,
        DoesEntityExist,
        GetEntityModel,
        IsPedInAnyVehicle,
        GetVehiclePedIsIn,

        GetVehicleColours,
        SetVehicleColours,
        GetVehicleExtraColours,
        SetVehicleExtraColours,
        GetVehicleModColor1,
        SetVehicleModColor1,
        GetVehicleModColor2,
        SetVehicleModColor2,
        GetIsVehiclePrimaryColourCustom,
        GetIsVehicleSecondaryColourCustom,
        GetVehicleCustomPrimaryColour,
        GetVehicleCustomSecondaryColour,
        SetVehicleCustomPrimaryColour,
        SetVehicleCustomSecondaryColour,
        ClearVehicleCustomPrimaryColour,
        ClearVehicleCustomSecondaryColour,

        Count,
    };

    class NativeRegistry final
    {
    public:
        static NativeRegistry& Get() noexcept;

        bool Initialize(InitNativeTablesFn initNativeTables) noexcept;
        void Shutdown() noexcept;

        void MarkGameThread(DWORD threadId) noexcept;
        [[nodiscard]] bool IsReady() const noexcept;
        [[nodiscard]] bool CanInvokeOnCurrentThread() const noexcept;
        [[nodiscard]] NativeHandler Handler(NativeId id) const noexcept;
        [[nodiscard]] const char* Name(NativeId id) const noexcept;
        [[nodiscard]] NativeHash Hash(NativeId id) const noexcept;

    private:
        NativeRegistry() = default;
        ~NativeRegistry() = default;
        NativeRegistry(const NativeRegistry&) = delete;
        NativeRegistry& operator=(const NativeRegistry&) = delete;

        std::array<NativeHandler, static_cast<std::size_t>(NativeId::Count)> m_Handlers{};
        std::atomic<bool> m_Ready{false};
        std::atomic<DWORD> m_GameThreadId{0};
    };
}
