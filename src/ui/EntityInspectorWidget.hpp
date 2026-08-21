#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../game/EntityInspectorNatives.hpp"
#include "../game/PlayerNatives.hpp"
#include "../game/VehicleNatives.hpp"
#include "../runtime/GameRuntime.hpp"

#include <imgui.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iomanip>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

namespace Tutones::UI
{
    namespace EntityInspectorDetail
    {
        using Vector3 = Game::Native::NativeVector3;

        struct Snapshot final
        {
            bool nativeReady{};
            bool rayHit{};
            bool targetFound{};
            Game::Entity handle{};
            std::optional<int> entityType;
            std::optional<Game::Hash> modelHash;
            std::string modelName;
            std::optional<Vector3> position;
            std::optional<float> heading;
            std::optional<Vector3> velocity;
            std::optional<float> speed;
            std::optional<int> health;
            std::optional<int> maxHealth;
            std::optional<float> distance;
            std::optional<Game::Hash> materialHash;
            std::optional<Vector3> hitPosition;

            std::string vehicleMake;
            std::string vehiclePlate;
            std::optional<int> vehicleClass;
            std::optional<float> vehicleEngineHealth;
            std::optional<float> vehicleBodyHealth;

            std::optional<int> pedType;
            std::optional<bool> pedIsPlayer;
            std::optional<int> pedArmour;
            std::optional<bool> pedInVehicle;
            std::optional<Game::Vehicle> pedVehicle;

            std::string message{"Aim the crosshair at an entity."};
        };

        inline const char* EntityTypeName(int type) noexcept
        {
            switch (type)
            {
            case 1: return "Ped";
            case 2: return "Vehicle";
            case 3: return "Object";
            default: return "Unknown";
            }
        }

        inline std::string HexHash(Game::Hash hash)
        {
            std::ostringstream stream;
            stream << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << hash;
            return stream.str();
        }

        inline float Distance(const Vector3& a, const Vector3& b) noexcept
        {
            const float dx = a.x - b.x;
            const float dy = a.y - b.y;
            const float dz = a.z - b.z;
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        }

        inline float Magnitude(const Vector3& value) noexcept
        {
            return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
        }

        inline Vector3 RotationToDirection(const Vector3& rotation) noexcept
        {
            constexpr float DegToRad = 0.01745329251994329577f;
            const float pitch = rotation.x * DegToRad;
            const float yaw = rotation.z * DegToRad;
            const float horizontal = std::abs(std::cos(pitch));
            return Vector3{
                -std::sin(yaw) * horizontal,
                std::cos(yaw) * horizontal,
                std::sin(pitch)};
        }

        inline std::string SafeLabel(const char* label)
        {
            if (!label || !*label)
                return {};
            const std::string value(label);
            return value == "NULL" ? std::string{} : value;
        }

        class Runtime final
        {
        public:
            static Runtime& Get() noexcept
            {
                static Runtime runtime;
                return runtime;
            }

            void RequestScan() noexcept
            {
                const auto now = std::chrono::steady_clock::now();
                if (m_NextScan != std::chrono::steady_clock::time_point{} && now < m_NextScan)
                    return;
                m_NextScan = now + std::chrono::milliseconds(75);

                bool expected = false;
                if (!m_TaskQueued.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                    return;

                if (!Tutones::Runtime::GameRuntime::Get().Enqueue([this]()
                    {
                        ScanTick();
                        m_TaskQueued.store(false, std::memory_order_release);
                    }))
                {
                    m_TaskQueued.store(false, std::memory_order_release);
                }
            }

            [[nodiscard]] Snapshot GetSnapshot() const
            {
                std::scoped_lock lock(m_Mutex);
                return m_Snapshot;
            }

        private:
            Runtime() = default;

            void SetUnavailable(const char* message) noexcept
            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot = {};
                m_Snapshot.message = message ? message : "Entity inspector unavailable.";
            }

            void ScanTick() noexcept
            {
                const auto localPed = Game::PlayerNatives::PlayerPedId();
                if (!localPed || *localPed == 0)
                {
                    SetUnavailable("Local player ped is unavailable.");
                    m_PendingShapeTest = 0;
                    return;
                }

                if (m_PendingShapeTest != 0)
                {
                    const auto result = Game::EntityInspectorNatives::GetShapeTestResultIncludingMaterial(m_PendingShapeTest);
                    if (!result)
                    {
                        SetUnavailable("Shape-test result native is unavailable.");
                        m_PendingShapeTest = 0;
                        return;
                    }

                    if (result->status == 1)
                        return;

                    m_PendingShapeTest = 0;
                    if (result->status == 2)
                        PublishResult(*localPed, *result);
                }

                StartRay(*localPed);
            }

            void StartRay(Game::Ped localPed) noexcept
            {
                const auto camera = Game::EntityInspectorNatives::GetGameplayCamCoord();
                const auto rotation = Game::EntityInspectorNatives::GetGameplayCamRot(2);
                if (!camera || !rotation)
                {
                    SetUnavailable("Gameplay camera natives are unavailable.");
                    return;
                }

                const Vector3 direction = RotationToDirection(*rotation);
                const Vector3 finish{
                    camera->x + direction.x * 1000.0f,
                    camera->y + direction.y * 1000.0f,
                    camera->z + direction.z * 1000.0f};

                const auto handle = Game::EntityInspectorNatives::StartShapeTestLosProbe(
                    *camera,
                    finish,
                    -1,
                    localPed,
                    7);

                if (!handle || *handle == 0)
                {
                    SetUnavailable("Unable to start crosshair shape test.");
                    return;
                }

                m_PendingShapeTest = *handle;
                std::scoped_lock lock(m_Mutex);
                m_Snapshot.nativeReady = true;
            }

            void PublishResult(Game::Ped localPed, const Game::EntityInspectorNatives::ShapeResult& result) noexcept
            {
                Snapshot next;
                next.nativeReady = true;
                next.rayHit = result.hit;
                next.materialHash = result.materialHash;
                next.hitPosition = result.endCoords;

                if (!result.hit)
                {
                    next.message = "Nothing under crosshair.";
                    Store(std::move(next));
                    return;
                }

                if (result.entity == 0)
                {
                    next.message = "World surface under crosshair (no entity handle).";
                    if (const auto localCoords = Game::VehicleNatives::GetEntityCoords(localPed))
                        next.distance = Distance(*localCoords, result.endCoords);
                    Store(std::move(next));
                    return;
                }

                const auto exists = Game::Natives::DoesEntityExist(result.entity);
                if (!exists || !*exists)
                {
                    next.message = "Crosshair entity expired before inspection.";
                    Store(std::move(next));
                    return;
                }

                next.targetFound = true;
                next.handle = result.entity;
                next.entityType = Game::EntityInspectorNatives::GetEntityType(result.entity);
                next.modelHash = Game::PlayerNatives::GetEntityModel(result.entity);
                next.position = Game::VehicleNatives::GetEntityCoords(result.entity);
                next.heading = Game::VehicleNatives::GetEntityHeading(result.entity);
                next.velocity = Game::EntityInspectorNatives::GetEntityVelocity(result.entity);
                next.speed = Game::EntityInspectorNatives::GetEntitySpeed(result.entity);
                next.health = Game::PlayerNatives::GetEntityHealth(result.entity);
                next.maxHealth = Game::PlayerNatives::GetEntityMaxHealth(result.entity);

                if (next.position)
                {
                    if (const auto localCoords = Game::VehicleNatives::GetEntityCoords(localPed))
                        next.distance = Distance(*localCoords, *next.position);
                }

                const int type = next.entityType.value_or(0);
                if (type == 2)
                {
                    const auto vehicle = static_cast<Game::Vehicle>(result.entity);
                    if (next.modelHash)
                    {
                        const auto displayKey = Game::VehicleNatives::GetDisplayNameFromVehicleModel(*next.modelHash);
                        if (displayKey && *displayKey)
                        {
                            const auto displayName = Game::VehicleNatives::GetLabelText(*displayKey);
                            next.modelName = displayName ? SafeLabel(*displayName) : std::string{};
                            if (next.modelName.empty())
                                next.modelName = SafeLabel(*displayKey);
                        }

                        const auto makeKey = Game::VehicleNatives::GetMakeNameFromVehicleModel(*next.modelHash);
                        if (makeKey && *makeKey)
                        {
                            const auto makeName = Game::VehicleNatives::GetLabelText(*makeKey);
                            next.vehicleMake = makeName ? SafeLabel(*makeName) : std::string{};
                            if (next.vehicleMake.empty())
                                next.vehicleMake = SafeLabel(*makeKey);
                        }
                        next.vehicleClass = Game::VehicleNatives::GetVehicleClassFromName(*next.modelHash);
                    }

                    if (const auto plate = Game::VehicleNatives::GetVehicleNumberPlateText(vehicle))
                        next.vehiclePlate = *plate;
                    next.vehicleEngineHealth = Game::EntityInspectorNatives::GetVehicleEngineHealth(vehicle);
                    next.vehicleBodyHealth = Game::EntityInspectorNatives::GetVehicleBodyHealth(vehicle);
                }
                else if (type == 1)
                {
                    const auto ped = static_cast<Game::Ped>(result.entity);
                    next.pedType = Game::EntityInspectorNatives::GetPedType(ped);
                    next.pedIsPlayer = Game::EntityInspectorNatives::IsPedAPlayer(ped);
                    next.pedArmour = Game::PlayerNatives::GetPedArmour(ped);
                    next.pedInVehicle = Game::Natives::IsPedInAnyVehicle(ped, false);
                    if (next.pedInVehicle.value_or(false))
                        next.pedVehicle = Game::Natives::GetVehiclePedIsIn(ped, false);
                }

                if (next.modelName.empty())
                {
                    if (type == 1)
                        next.modelName = "Ped model";
                    else if (type == 3)
                        next.modelName = "Object model";
                    else
                        next.modelName = "Unresolved model";
                }

                next.message = std::string(EntityTypeName(type)) + " targeted under crosshair.";
                Store(std::move(next));
            }

            void Store(Snapshot&& snapshot) noexcept
            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot = std::move(snapshot);
            }

            mutable std::mutex m_Mutex;
            Snapshot m_Snapshot;
            std::atomic<bool> m_TaskQueued{false};
            std::chrono::steady_clock::time_point m_NextScan{};
            int m_PendingShapeTest{};
        };

        inline std::string BuildDebugData(const Snapshot& snapshot)
        {
            std::ostringstream out;
            out << "Tutones Entity Inspector\n";
            out << "Target Under Crosshair: " << (snapshot.targetFound ? "yes" : "no") << '\n';
            out << "Entity Type: " << (snapshot.entityType ? EntityTypeName(*snapshot.entityType) : "N/A") << '\n';
            out << "Model Name: " << (snapshot.modelName.empty() ? "N/A" : snapshot.modelName) << '\n';
            out << "Model Hash: " << (snapshot.modelHash ? HexHash(*snapshot.modelHash) : "N/A") << '\n';
            out << "Handle: " << (snapshot.targetFound ? std::to_string(snapshot.handle) : "N/A") << '\n';

            if (snapshot.position)
                out << "Position: " << snapshot.position->x << ", " << snapshot.position->y << ", " << snapshot.position->z << '\n';
            else if (snapshot.hitPosition)
                out << "Hit Position: " << snapshot.hitPosition->x << ", " << snapshot.hitPosition->y << ", " << snapshot.hitPosition->z << '\n';
            else
                out << "Position: N/A\n";

            out << "Heading: " << (snapshot.heading ? std::to_string(*snapshot.heading) : "N/A") << '\n';
            if (snapshot.velocity)
            {
                out << "Velocity: " << snapshot.velocity->x << ", " << snapshot.velocity->y << ", " << snapshot.velocity->z << '\n';
                const float speed = snapshot.speed.value_or(Magnitude(*snapshot.velocity));
                out << "Speed: " << speed << " m/s\n";
            }
            else
                out << "Velocity: N/A\n";

            out << "Health: ";
            if (snapshot.health)
            {
                out << *snapshot.health;
                if (snapshot.maxHealth)
                    out << " / " << *snapshot.maxHealth;
                out << '\n';
            }
            else
                out << "N/A\n";

            out << "Distance: " << (snapshot.distance ? std::to_string(*snapshot.distance) + " m" : "N/A") << '\n';
            out << "Material: " << (snapshot.materialHash ? HexHash(*snapshot.materialHash) : "N/A") << '\n';

            if (snapshot.entityType.value_or(0) == 2)
            {
                out << "Vehicle Make: " << (snapshot.vehicleMake.empty() ? "N/A" : snapshot.vehicleMake) << '\n';
                out << "Vehicle Class: " << (snapshot.vehicleClass ? std::to_string(*snapshot.vehicleClass) : "N/A") << '\n';
                out << "Plate: " << (snapshot.vehiclePlate.empty() ? "N/A" : snapshot.vehiclePlate) << '\n';
                out << "Engine Health: " << (snapshot.vehicleEngineHealth ? std::to_string(*snapshot.vehicleEngineHealth) : "N/A") << '\n';
                out << "Body Health: " << (snapshot.vehicleBodyHealth ? std::to_string(*snapshot.vehicleBodyHealth) : "N/A") << '\n';
            }
            else if (snapshot.entityType.value_or(0) == 1)
            {
                out << "Ped Type: " << (snapshot.pedType ? std::to_string(*snapshot.pedType) : "N/A") << '\n';
                out << "Ped Is Player: " << (snapshot.pedIsPlayer ? (*snapshot.pedIsPlayer ? "yes" : "no") : "N/A") << '\n';
                out << "Ped Armour: " << (snapshot.pedArmour ? std::to_string(*snapshot.pedArmour) : "N/A") << '\n';
                out << "Ped In Vehicle: " << (snapshot.pedInVehicle ? (*snapshot.pedInVehicle ? "yes" : "no") : "N/A") << '\n';
                if (snapshot.pedVehicle)
                    out << "Ped Vehicle Handle: " << *snapshot.pedVehicle << '\n';
            }

            out << "Status: " << snapshot.message << '\n';
            return out.str();
        }

        inline void ValueLine(const char* label, const std::string& value) noexcept
        {
            ImGui::TextDisabled("%s", label);
            ImGui::SameLine(154.0f);
            ImGui::TextWrapped("%s", value.c_str());
        }
    }

    inline void RenderEntityInspectorWidget() noexcept
    {
        using namespace EntityInspectorDetail;

        static bool liveTargeting = true;
        if (liveTargeting)
            Runtime::Get().RequestScan();

        const Snapshot snapshot = Runtime::Get().GetSnapshot();

        ImGui::SeparatorText("Entity Inspector");
        ImGui::Checkbox("Target Under Crosshair", &liveTargeting);
        DescribeLastV11Item("Continuously raycast from the gameplay camera and inspect the entity currently under the crosshair. Your own ped is ignored by the ray.");
        ImGui::SameLine();
        ImGui::TextDisabled("%s", liveTargeting ? "LIVE" : "PAUSED");

        ValueLine("Entity Type", snapshot.entityType ? EntityTypeName(*snapshot.entityType) : "N/A");

        std::string modelLine = snapshot.modelName.empty() ? "N/A" : snapshot.modelName;
        if (snapshot.modelHash)
            modelLine += " / " + HexHash(*snapshot.modelHash);
        ValueLine("Model Name / Hash", modelLine);
        ValueLine("Handle", snapshot.targetFound ? std::to_string(snapshot.handle) : "N/A");

        if (snapshot.position)
        {
            char text[128]{};
            std::snprintf(text, sizeof(text), "%.3f, %.3f, %.3f", snapshot.position->x, snapshot.position->y, snapshot.position->z);
            ValueLine("Position", text);
        }
        else if (snapshot.hitPosition)
        {
            char text[128]{};
            std::snprintf(text, sizeof(text), "hit %.3f, %.3f, %.3f", snapshot.hitPosition->x, snapshot.hitPosition->y, snapshot.hitPosition->z);
            ValueLine("Position", text);
        }
        else
            ValueLine("Position", "N/A");

        if (snapshot.heading)
        {
            char text[64]{};
            std::snprintf(text, sizeof(text), "%.2f deg", *snapshot.heading);
            ValueLine("Heading", text);
        }
        else
            ValueLine("Heading", "N/A");

        if (snapshot.velocity)
        {
            char text[160]{};
            const float speedMps = snapshot.speed.value_or(Magnitude(*snapshot.velocity));
            std::snprintf(text, sizeof(text), "%.3f, %.3f, %.3f  (%.2f m/s)", snapshot.velocity->x, snapshot.velocity->y, snapshot.velocity->z, speedMps);
            ValueLine("Velocity", text);
        }
        else
            ValueLine("Velocity", "N/A");

        if (snapshot.health)
        {
            std::string health = std::to_string(*snapshot.health);
            if (snapshot.maxHealth)
                health += " / " + std::to_string(*snapshot.maxHealth);
            ValueLine("Health", health);
        }
        else
            ValueLine("Health", "N/A");

        if (snapshot.distance)
        {
            char text[64]{};
            std::snprintf(text, sizeof(text), "%.2f m", *snapshot.distance);
            ValueLine("Distance", text);
        }
        else
            ValueLine("Distance", "N/A");

        ValueLine("Material", snapshot.materialHash ? HexHash(*snapshot.materialHash) : "N/A");

        if (snapshot.entityType.value_or(0) == 2 && ImGui::CollapsingHeader("Vehicle Info", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ValueLine("Make", snapshot.vehicleMake.empty() ? "N/A" : snapshot.vehicleMake);
            ValueLine("Class", snapshot.vehicleClass ? std::to_string(*snapshot.vehicleClass) : "N/A");
            ValueLine("Plate", snapshot.vehiclePlate.empty() ? "N/A" : snapshot.vehiclePlate);
            if (snapshot.vehicleEngineHealth)
            {
                char text[64]{};
                std::snprintf(text, sizeof(text), "%.1f", *snapshot.vehicleEngineHealth);
                ValueLine("Engine Health", text);
            }
            if (snapshot.vehicleBodyHealth)
            {
                char text[64]{};
                std::snprintf(text, sizeof(text), "%.1f", *snapshot.vehicleBodyHealth);
                ValueLine("Body Health", text);
            }
        }

        if (snapshot.entityType.value_or(0) == 1 && ImGui::CollapsingHeader("Ped Info", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ValueLine("Ped Type", snapshot.pedType ? std::to_string(*snapshot.pedType) : "N/A");
            ValueLine("Player Ped", snapshot.pedIsPlayer ? (*snapshot.pedIsPlayer ? "Yes" : "No") : "N/A");
            ValueLine("Armour", snapshot.pedArmour ? std::to_string(*snapshot.pedArmour) : "N/A");
            ValueLine("In Vehicle", snapshot.pedInVehicle ? (*snapshot.pedInVehicle ? "Yes" : "No") : "N/A");
            if (snapshot.pedVehicle)
                ValueLine("Vehicle Handle", std::to_string(*snapshot.pedVehicle));
        }

        if (ImGui::Button("Copy Debug Data", ImVec2(-1.0f, 0.0f)))
        {
            const std::string debug = BuildDebugData(snapshot);
            ImGui::SetClipboardText(debug.c_str());
        }
        DescribeLastV11Item("Copy the current inspector snapshot to the clipboard for logs, bug reports or native debugging.");

        ImGui::TextWrapped("%s", snapshot.message.c_str());
    }
}
