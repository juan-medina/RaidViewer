# SPDX-FileCopyrightText: 2026 Juan Medina
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.25)
project(RaidViewer LANGUAGES CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(RaidViewer WIN32
    include/RaidViewer/app_state.hpp
    include/RaidViewer/config.hpp
    include/RaidViewer/display.hpp
    include/RaidViewer/input_manager.hpp
    include/RaidViewer/platform.hpp
    include/RaidViewer/renderer.hpp
    include/RaidViewer/strategy_deck.hpp
    include/RaidViewer/window.hpp
    src/app_state.cpp
    src/config.cpp
    src/display.cpp
    src/input_manager.cpp
    src/main.cpp
    src/platform.cpp
    src/renderer.cpp
    src/strategy_deck.cpp
    src/window.cpp
)

target_include_directories(RaidViewer PRIVATE include)

target_compile_features(RaidViewer PRIVATE cxx_std_20)
set_target_properties(RaidViewer PROPERTIES CXX_EXTENSIONS OFF)

target_compile_definitions(RaidViewer PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _HAS_EXCEPTIONS=0
)

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    string(REPLACE "/EHsc" "/EHs-c-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REPLACE "/O2" "/O1" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    string(REPLACE "/Ob2" "/Ob1" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    target_compile_options(RaidViewer PRIVATE
        /W4
        /WX
        /permissive-
        /utf-8
        /GR-
        $<$<CONFIG:Release>:/Gy>
        $<$<CONFIG:Release>:/Gw>
    )
    target_link_options(RaidViewer PRIVATE
        /ENTRY:mainCRTStartup
        $<$<CONFIG:Release>:/OPT:REF>
        $<$<CONFIG:Release>:/OPT:ICF>
    )
else()
    string(REPLACE "-O3" "-Os" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    target_compile_options(RaidViewer PRIVATE
        -Wall
        -Wextra
        -pedantic
        -Werror
        -fno-rtti
        -fno-exceptions
        $<$<CONFIG:Release>:-ffunction-sections>
        $<$<CONFIG:Release>:-fdata-sections>
    )
    target_link_options(RaidViewer PRIVATE
        -Xlinker /entry:mainCRTStartup
        $<$<CONFIG:Release>:-Wl,--gc-sections>
    )
endif()

# All four libraries ship natively with Windows / Visual Studio SDK
target_link_libraries(RaidViewer PRIVATE
    user32
    gdi32
    gdiplus
    shlwapi
)


# Run target: builds RaidViewer and launches it
add_custom_target(run
    COMMAND $<TARGET_FILE:RaidViewer>
    DEPENDS RaidViewer
    WORKING_DIRECTORY $<TARGET_FILE_DIR:RaidViewer>
    COMMENT "Running RaidViewer..."
)