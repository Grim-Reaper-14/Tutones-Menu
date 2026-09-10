cmake_minimum_required(VERSION 3.24)

project(TutonesMenuV2 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Tutones Menu V2 currently targets Windows only.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Tutones Menu V2 requires a 64-bit build.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

add_library(TutonesMenuV2 SHARED
    src/app/DllMain.cpp
    src/core/Application.cpp
    src/core/logging/Logger.cpp
    src/backend/Backend.cpp
    src/backend/native/NativeRuntime.cpp
    src/backend/script/ScriptRuntime.cpp
    src/backend/hooking/HookManager.cpp
    src/backend/features/teleport/TeleportRuntime.cpp
    src/frontend/MenuModel.cpp
    src/frontend/render/Renderer.cpp
)

target_include_directories(TutonesMenuV2 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(TutonesMenuV2 PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(TutonesMenuV2 PRIVATE
        /W4
        /permissive-
        /EHsc
        /MP
    )
endif()

target_link_libraries(TutonesMenuV2 PRIVATE
    user32
    d3d12
    dxgi
)

set_target_properties(TutonesMenuV2 PROPERTIES
    OUTPUT_NAME "Tutones-MenuV2"
    PREFIX ""
)

if(BUILD_TESTING)
    add_executable(TutonesV2MenuModelTests
        tests/MenuModelTests.cpp
        src/frontend/MenuModel.cpp
    )
    target_include_directories(TutonesV2MenuModelTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    if(MSVC)
        target_compile_options(TutonesV2MenuModelTests PRIVATE
            /W4
            /permissive-
            /EHsc
        )
    endif()
    add_test(NAME TutonesV2MainMenuCategories COMMAND TutonesV2MenuModelTests)
endif()
