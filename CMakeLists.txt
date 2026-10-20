# SPDX-FileCopyrightText: 2026 Juan Medina
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.25)
project(RaidViewer LANGUAGES CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(RaidViewer WIN32 src/main.cpp)

target_compile_features(RaidViewer PRIVATE cxx_std_20)
set_target_properties(RaidViewer PROPERTIES CXX_EXTENSIONS OFF)

target_compile_definitions(RaidViewer PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
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

# Copy images from project root to target output directory on build
add_custom_command(
    TARGET RaidViewer
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/images"
        "$<TARGET_FILE_DIR:RaidViewer>/images"
    COMMENT "Copying images to output directory..."
)

# Run target: builds RaidViewer and launches it
add_custom_target(run
    COMMAND $<TARGET_FILE:RaidViewer>
    DEPENDS RaidViewer
    WORKING_DIRECTORY $<TARGET_FILE_DIR:RaidViewer>
    COMMENT "Running RaidViewer..."
)