#pragma once

#include "Natives.hpp"
#include "PlayerNatives.hpp"
#include "GamePointers.hpp"
#include "native/NativeCallContext.hpp"
#include "native/NativeRegistry.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>

namespace Tutones::Game
{
    using Vector3 = Native::NativeVector3;

    namespace VehicleNatives
    {
        namespace Detail
        {
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

            enum SpawnerHandlerIndex : std::size_t
            {
                GetModelDimensions,
                GetOffsetFromEntityInWorldCoords,
                SpawnerHandlerCount,
            };

            inline std::array<Native::NativeHandler, SpawnerHandlerCount>& SpawnerHandlers() noexcept
            {
                static std::array<Native::NativeHandler, SpawnerHandlerCount> handlers{};
                return handlers;
            }

            inline bool ResolveSpawnerHandlers() noexcept
            {
                auto& handlers = SpawnerHandlers();
                if (handlers[0] && handlers[1])
                    return true;
                if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread())
                    return false;

                const auto init = GamePointers::Get().InitNativeTables();
                if (!init)
                    return false;

                // Current Enhanced hashes from YimMenuV2's enhanced crossmap.
                std::array<std::uint64_t, SpawnerHandlerCount> slots{
                    0xC93BAF616F1C680Full, // GET_MODEL_DIMENSIONS
                    0x0D1381B6E0F3987Dull, // GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS
                };

                NativeProgram program{};
                program.nativeCount = static_cast<std::uint32_t>(slots.size());
                program.nativeEntrypoints = reinterpret_cast<Native::NativeHandler*>(slots.data());
                init(&program);

                for (std::size_t i = 0; i < slots.size(); ++i)
                    handlers[i] = reinterpret_cast<Native::NativeHandler>(static_cast<std::uintptr_t>(slots[i]));
                return handlers[0] && handlers[1];
            }

            enum PlateHandlerIndex : std::size_t
            {
                GetNumberPlateText,
                SetNumberPlateText,
                GetNumberPlateTextIndex,
                SetNumberPlateTextIndex,
                PlateHandlerCount,
            };

            inline std::array<Native::NativeHandler, PlateHandlerCount>& PlateHandlers() noexcept
            {
                static std::array<Native::NativeHandler, PlateHandlerCount> handlers{};
                return handlers;
            }

            inline bool ResolvePlateHandlers() noexcept
            {
                auto& handlers = PlateHandlers();
                if (handlers[0] && handlers[1] && handlers[2] && handlers[3])
                    return true;
                if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread())
                    return false;

                const auto init = GamePointers::Get().InitNativeTables();
                if (!init)
                    return false;

                // GTA V Enhanced mappings verified against YimMenuV2's current enhanced crossmap.
                std::array<std::uint64_t, PlateHandlerCount> slots{
                    0xCA7159F2C5FF745Aull, // GET_VEHICLE_NUMBER_PLATE_TEXT
                    0x3FEAE59CDE6D3946ull, // SET_VEHICLE_NUMBER_PLATE_TEXT
                    0x4F06416A18248EA0ull, // GET_VEHICLE_NUMBER_PLATE_TEXT_INDEX
                    0x05D3F682DDA06C20ull, // SET_VEHICLE_NUMBER_PLATE_TEXT_INDEX
                };

                NativeProgram program{};
                program.nativeCount = static_cast<std::uint32_t>(slots.size());
                program.nativeEntrypoints = reinterpret_cast<Native::NativeHandler*>(slots.data());
                init(&program);

                for (std::size_t i = 0; i < slots.size(); ++i)
                    handlers[i] = reinterpret_cast<Native::NativeHandler>(static_cast<std::uintptr_t>(slots[i]));
                return handlers[0] && handlers[1] && handlers[2] && handlers[3];
            }

            inline bool ModelDimensions(Hash model, Vector3& minimum, Vector3& maximum) noexcept
            {
                if (!ResolveSpawnerHandlers())
                    return false;
                Native::CallContext context;
                if (!context.PushArg(model) || !context.PushArg(&minimum) || !context.PushArg(&maximum))
                    return false;
                SpawnerHandlers()[GetModelDimensions](&context);
                context.FixVectors();
                return std::isfinite(minimum.x) && std::isfinite(minimum.y) && std::isfinite(minimum.z)
                    && std::isfinite(maximum.x) && std::isfinite(maximum.y) && std::isfinite(maximum.z);
            }

            inline std::optional<Vector3> OffsetFromEntity(Entity entity, float x, float y, float z) noexcept
            {
                if (entity == 0 || !ResolveSpawnerHandlers())
                    return std::nullopt;
                Native::CallContext context;
                if (!context.PushArg(entity) || !context.PushArg(x) || !context.PushArg(y) || !context.PushArg(z))
                    return std::nullopt;
                SpawnerHandlers()[GetOffsetFromEntityInWorldCoords](&context);
                context.FixVectors();
                const auto result = context.GetReturnValue<Vector3>();
                if (!std::isfinite(result.x) || !std::isfinite(result.y) || !std::isfinite(result.z))
                    return std::nullopt;
                return result;
            }
        }

        [[nodiscard]] inline std::optional<float> GetEntityHeading(Entity entity) noexcept
        {
            return Native::NativeInvoker::Invoke<float>(Native::NativeId::GetEntityHeading, entity);
        }

        [[nodiscard]] inline std::optional<Vector3> GetEntityCoords(Entity entity, bool alive = false) noexcept
        {
            return Native::NativeInvoker::Invoke<Vector3>(
                Native::NativeId::GetEntityCoords,
                entity,
                static_cast<std::int32_t>(alive));
        }

        [[nodiscard]] inline std::optional<Vehicle> GetVehiclePedIsUsing(Ped ped) noexcept
        {
            return Native::NativeInvoker::Invoke<Vehicle>(Native::NativeId::GetVehiclePedIsUsing, ped);
        }

        [[nodiscard]] inline std::optional<bool> IsModelAVehicle(Hash model) noexcept
        {
            const auto result = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::IsModelAVehicle, model);
            return result ? std::optional<bool>(*result != 0) : std::nullopt;
        }

        [[nodiscard]] inline std::optional<Vehicle> CreateVehicle(
            Hash model,
            float x,
            float y,
            float z,
            float heading,
            bool isNetwork = true,
            bool netMissionEntity = false,
            bool p7 = false) noexcept
        {
            // YimMenuV2 places spawned vehicles using the model's dimensions and
            // GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS instead of a fixed distance.
            // Fall back to the caller coordinates if those two natives are unavailable.
            float spawnX = x;
            float spawnY = y;
            float spawnZ = z;

            const auto ped = PlayerNatives::PlayerPedId();
            if (ped && *ped != 0)
            {
                Vector3 minimum{};
                Vector3 maximum{};
                if (Detail::ModelDimensions(model, minimum, maximum))
                {
                    const float length = maximum.y - minimum.y;
                    if (std::isfinite(length) && length > 0.25f)
                    {
                        const auto offset = Detail::OffsetFromEntity(*ped, 0.0f, length, 0.0f);
                        if (offset)
                        {
                            spawnX = offset->x;
                            spawnY = offset->y;
                            spawnZ = offset->z;
                        }
                    }
                }
            }

            return Native::NativeInvoker::Invoke<Vehicle>(
                Native::NativeId::CreateVehicle,
                model,
                spawnX,
                spawnY,
                spawnZ,
                heading,
                static_cast<std::int32_t>(isNetwork),
                static_cast<std::int32_t>(netMissionEntity),
                static_cast<std::int32_t>(p7));
        }

        inline bool SetPedIntoVehicle(Ped ped, Vehicle vehicle, int seatIndex = -1) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetPedIntoVehicle,
                ped,
                vehicle,
                seatIndex);
        }

        [[nodiscard]] inline std::optional<int> GetVehicleClassFromName(Hash model) noexcept
        {
            return Native::NativeInvoker::Invoke<int>(Native::NativeId::GetVehicleClassFromName, model);
        }

        [[nodiscard]] inline std::optional<const char*> GetDisplayNameFromVehicleModel(Hash model) noexcept
        {
            return Native::NativeInvoker::Invoke<const char*>(Native::NativeId::GetDisplayNameFromVehicleModel, model);
        }

        [[nodiscard]] inline std::optional<const char*> GetMakeNameFromVehicleModel(Hash model) noexcept
        {
            return Native::NativeInvoker::Invoke<const char*>(Native::NativeId::GetMakeNameFromVehicleModel, model);
        }

        [[nodiscard]] inline std::optional<Vehicle> GetClosestVehicle(
            float x, float y, float z, float radius, Hash model = 0, int flags = 70) noexcept
        {
            return Native::NativeInvoker::Invoke<Vehicle>(
                Native::NativeId::GetClosestVehicle, x, y, z, radius, model, flags);
        }

        [[nodiscard]] inline std::optional<const char*> GetModTextLabel(Vehicle vehicle, int modType, int modIndex) noexcept
        {
            return Native::NativeInvoker::Invoke<const char*>(Native::NativeId::GetModTextLabel, vehicle, modType, modIndex);
        }

        [[nodiscard]] inline std::optional<const char*> GetLabelText(const char* label) noexcept
        {
            return Native::NativeInvoker::Invoke<const char*>(Native::NativeId::GetLabelText, label);
        }

        inline bool GetVehicleTyreSmokeColor(Vehicle vehicle, int& red, int& green, int& blue) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::GetVehicleTyreSmokeColor, vehicle, &red, &green, &blue);
        }

        inline bool SetVehicleTyreSmokeColor(Vehicle vehicle, int red, int green, int blue) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::SetVehicleTyreSmokeColor, vehicle, red, green, blue);
        }

        [[nodiscard]] inline std::optional<int> GetVehicleXenonLightColor(Vehicle vehicle) noexcept
        {
            return Native::NativeInvoker::Invoke<int>(Native::NativeId::GetVehicleXenonLightColor, vehicle);
        }

        inline bool SetVehicleXenonLightColor(Vehicle vehicle, int colorIndex) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::SetVehicleXenonLightColor, vehicle, colorIndex);
        }

        [[nodiscard]] inline std::optional<bool> GetVehicleNeonEnabled(Vehicle vehicle, int index) noexcept
        {
            const auto result = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::GetVehicleNeonEnabled, vehicle, index);
            return result ? std::optional<bool>(*result != 0) : std::nullopt;
        }

        inline bool SetVehicleNeonEnabled(Vehicle vehicle, int index, bool enabled) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetVehicleNeonEnabled, vehicle, index, static_cast<std::int32_t>(enabled));
        }

        inline bool GetVehicleNeonColour(Vehicle vehicle, int& red, int& green, int& blue) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::GetVehicleNeonColour, vehicle, &red, &green, &blue);
        }

        inline bool SetVehicleNeonColour(Vehicle vehicle, int red, int green, int blue) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(Native::NativeId::SetVehicleNeonColour, vehicle, red, green, blue);
        }

        [[nodiscard]] inline std::optional<bool> GetVehicleTyresCanBurst(Vehicle vehicle) noexcept
        {
            const auto result = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::GetVehicleTyresCanBurst, vehicle);
            return result ? std::optional<bool>(*result != 0) : std::nullopt;
        }

        inline bool SetVehicleTyresCanBurst(Vehicle vehicle, bool canBurst) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetVehicleTyresCanBurst, vehicle, static_cast<std::int32_t>(canBurst));
        }

        [[nodiscard]] inline std::optional<bool> GetDriftTyresSet(Vehicle vehicle) noexcept
        {
            const auto result = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::GetDriftTyresSet, vehicle);
            return result ? std::optional<bool>(*result != 0) : std::nullopt;
        }

        inline bool SetDriftTyres(Vehicle vehicle, bool enabled) noexcept
        {
            return Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetDriftTyres, vehicle, static_cast<std::int32_t>(enabled));
        }

        [[nodiscard]] inline std::optional<std::string> GetVehicleNumberPlateText(Vehicle vehicle) noexcept
        {
            if (vehicle == 0 || !Detail::ResolvePlateHandlers())
                return std::nullopt;

            Native::CallContext context;
            if (!context.PushArg(vehicle))
                return std::nullopt;
            Detail::PlateHandlers()[Detail::GetNumberPlateText](&context);

            const char* text = context.GetReturnValue<const char*>();
            if (!text)
                return std::nullopt;

            std::string result(text);
            if (result.size() > 8)
                result.resize(8);
            return result;
        }

        inline bool SetVehicleNumberPlateText(Vehicle vehicle, std::string_view text) noexcept
        {
            if (vehicle == 0 || !Detail::ResolvePlateHandlers())
                return false;

            char plate[9]{};
            const std::size_t length = text.size() < 8 ? text.size() : 8;
            if (length != 0)
                std::memcpy(plate, text.data(), length);

            Native::CallContext context;
            if (!context.PushArg(vehicle) || !context.PushArg(plate))
                return false;
            Detail::PlateHandlers()[Detail::SetNumberPlateText](&context);
            return true;
        }

        [[nodiscard]] inline std::optional<int> GetVehicleNumberPlateTextIndex(Vehicle vehicle) noexcept
        {
            if (vehicle == 0 || !Detail::ResolvePlateHandlers())
                return std::nullopt;

            Native::CallContext context;
            if (!context.PushArg(vehicle))
                return std::nullopt;
            Detail::PlateHandlers()[Detail::GetNumberPlateTextIndex](&context);
            return context.GetReturnValue<int>();
        }

        inline bool SetVehicleNumberPlateTextIndex(Vehicle vehicle, int index) noexcept
        {
            if (vehicle == 0 || index < 0 || index > 12 || !Detail::ResolvePlateHandlers())
                return false;

            Native::CallContext context;
            if (!context.PushArg(vehicle) || !context.PushArg(index))
                return false;
            Detail::PlateHandlers()[Detail::SetNumberPlateTextIndex](&context);
            return true;
        }
    }
}
