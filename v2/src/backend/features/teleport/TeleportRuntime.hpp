#pragma once

#include <mutex>
#include <optional>

namespace TutonesV2::Backend::Features::Teleport
{
    struct Vector3
    {
        float x{};
        float y{};
        float z{};
    };

    enum class TeleportTarget
    {
        Waypoint,
        Objective,
        Coordinates,
        PersonalVehicle
    };

    struct TeleportRequest
    {
        TeleportTarget target{TeleportTarget::Waypoint};
        Vector3 coordinates{};
        bool keepVehicle{true};
    };

    class TeleportRuntime final
    {
    public:
        static TeleportRuntime& Get() noexcept;

        bool Initialize() noexcept;
        void Shutdown() noexcept;

        bool Submit(TeleportRequest request) noexcept;
        [[nodiscard]] std::optional<TeleportRequest> ConsumePending() noexcept;
        [[nodiscard]] bool IsReady() const noexcept;

    private:
        TeleportRuntime() = default;

        mutable std::mutex m_Mutex;
        std::optional<TeleportRequest> m_Pending;
        bool m_Ready{};
    };
}
