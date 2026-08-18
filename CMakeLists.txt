cmake_minimum_required(VERSION 3.24)

project(TutonesMenu LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Tutones Menu currently targets Windows only.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Tutones Menu requires a 64-bit build.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/Dependencies.cmake)

add_library(TutonesMenu SHARED
    src/app/DllMain.cpp
    src/app/Application.cpp

    src/core/CoreServices.cpp
    src/core/config/Config.cpp
    src/core/filesystem/FileSystem.cpp
    src/core/logging/Logger.cpp

    src/features/game/GameSessionRuntime.cpp
    src/features/player/OffRadarRuntime.cpp
    src/features/player/PlayerRuntime.cpp
    src/features/player/PlayerRuntimeActions.cpp
    src/features/player/PlayerRuntimeState.cpp
    src/features/vehicle/VehicleModificationRuntime.cpp
    src/features/vehicle/VehiclePaintRuntime.cpp
    src/features/vehicle/VehiclePaintService.cpp
    src/features/weapon/WeaponRuntime.cpp

    src/game/GamePointers.cpp
    src/game/GameState.cpp
    src/game/memory/BytePatch.cpp
    src/game/memory/ModuleView.cpp
    src/game/memory/PatternScanner.cpp
    src/game/native/NativeRegistry.cpp
    src/game/script/ScriptFunction.cpp
    src/game/script/ScriptPointer.cpp
    src/game/script/ScriptRuntime.cpp
    src/game/vehicle/TutonesVehiclePaintBackend.cpp
    src/game/vehicle/VehiclePaintController.cpp

    src/hooking/HookManager.cpp
    src/hooking/Win32Hook.cpp

    src/render/Renderer.cpp

    src/runtime/GameRuntime.cpp

    src/ui/Input.cpp
    src/ui/PlayerPanel.cpp
    src/ui/TutonesMenu.cpp
    src/ui/VehicleGeneralPanel.cpp
    src/ui/VehicleModificationPanel.cpp
    src/ui/VehiclePaintPanel.cpp
    src/ui/WeaponPanel.cpp
)

target_include_directories(TutonesMenu PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(TutonesMenu PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(TutonesMenu PRIVATE
        /W4
        /permissive-
        /EHsc
        /MP
    )
endif()

target_link_libraries(TutonesMenu PRIVATE
    minhook
    tutones_imgui
    d3d12
    dxgi
    user32
)

set_target_properties(TutonesMenu PROPERTIES
    OUTPUT_NAME "Tutones-Menu"
    PREFIX ""
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Source" FILES
    src/app/DllMain.cpp
    src/app/Application.cpp
    src/app/Application.hpp
    src/core/CoreServices.cpp
    src/core/CoreServices.hpp
    src/core/config/Config.cpp
    src/core/config/Config.hpp
    src/core/filesystem/FileSystem.cpp
    src/core/filesystem/FileSystem.hpp
    src/core/logging/LogLevel.hpp
    src/core/logging/LogRecord.hpp
    src/core/logging/LogSink.hpp
    src/core/logging/Logger.cpp
    src/core/logging/Logger.hpp
    src/features/game/GameSessionRuntime.cpp
    src/features/game/GameSessionRuntime.hpp
    src/features/game/GameSessionTypes.hpp
    src/features/player/OffRadarRuntime.cpp
    src/features/player/OffRadarRuntime.hpp
    src/features/player/OffRadarState.hpp
    src/features/player/PlayerRuntime.cpp
    src/features/player/PlayerRuntime.hpp
    src/features/player/PlayerRuntimeActions.cpp
    src/features/player/PlayerRuntimeState.cpp
    src/features/vehicle/VehicleModificationRuntime.cpp
    src/features/vehicle/VehicleModificationRuntime.hpp
    src/features/vehicle/VehiclePaintRuntime.cpp
    src/features/vehicle/VehiclePaintRuntime.hpp
    src/features/vehicle/VehiclePaintService.cpp
    src/features/vehicle/VehiclePaintService.hpp
    src/features/weapon/WeaponRuntime.cpp
    src/features/weapon/WeaponRuntime.hpp
    src/features/weapon/WeaponSettings.hpp
    src/game/GamePointers.cpp
    src/game/GamePointers.hpp
    src/game/GameState.cpp
    src/game/GameState.hpp
    src/game/Natives.hpp
    src/game/PlayerNatives.hpp
    src/game/memory/BytePatch.cpp
    src/game/memory/BytePatch.hpp
    src/game/memory/ModuleView.cpp
    src/game/memory/ModuleView.hpp
    src/game/memory/PatternScanner.cpp
    src/game/memory/PatternScanner.hpp
    src/game/native/NativeCallContext.hpp
    src/game/native/NativeInvoker.hpp
    src/game/native/NativeRegistry.cpp
    src/game/native/NativeRegistry.hpp
    src/game/script/ScriptFunction.cpp
    src/game/script/ScriptFunction.hpp
    src/game/script/ScriptGlobal.hpp
    src/game/script/ScriptPointer.cpp
    src/game/script/ScriptPointer.hpp
    src/game/script/ScriptRuntime.cpp
    src/game/script/ScriptRuntime.hpp
    src/game/types/ScriptProgram.hpp
    src/game/types/ScriptTypes.hpp
    src/game/vehicle/TutonesVehiclePaintBackend.cpp
    src/game/vehicle/TutonesVehiclePaintBackend.hpp
    src/game/vehicle/VehiclePaintBackend.hpp
    src/game/vehicle/VehiclePaintController.cpp
    src/game/vehicle/VehiclePaintController.hpp
    src/game/vehicle/VehiclePaintTypes.hpp
    src/hooking/HookManager.cpp
    src/hooking/HookManager.hpp
    src/hooking/HookStatus.hpp
    src/hooking/Win32Hook.cpp
    src/hooking/Win32Hook.hpp
    src/render/Renderer.cpp
    src/render/Renderer.hpp
    src/render/RendererStatus.hpp
    src/render/d3d12/FrameContext.hpp
    src/runtime/GameRuntime.cpp
    src/runtime/GameRuntime.hpp
    src/ui/Input.cpp
    src/ui/Input.hpp
    src/ui/PlayerPanel.cpp
    src/ui/PlayerPanel.hpp
    src/ui/TutonesMenu.cpp
    src/ui/TutonesMenu.hpp
    src/ui/VehicleGeneralPanel.cpp
    src/ui/VehicleGeneralPanel.hpp
    src/ui/VehicleModificationPanel.cpp
    src/ui/VehicleModificationPanel.hpp
    src/ui/VehiclePaintPanel.cpp
    src/ui/VehiclePaintPanel.hpp
    src/ui/WeaponPanel.cpp
    src/ui/WeaponPanel.hpp
)
