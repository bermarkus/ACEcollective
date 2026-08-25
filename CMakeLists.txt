cmake_minimum_required(VERSION 3.16)
project(ACEcollective VERSION 0.1.0)

# Specify C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# macOS Bundle Configuration
if(APPLE)
    set(MACOSX_BUNDLE_BUNDLE_NAME "ACEcollective v0.1.0")
    set(MACOSX_BUNDLE_BUNDLE_VERSION "0.1.0")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0")
    set(MACOSX_BUNDLE_LONG_VERSION_STRING "0.1.0")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.alphacoherence.acecollective")
    set(MACOSX_BUNDLE_COPYRIGHT "Copyright (c) 2026 ACEcollective. All rights reserved.")
endif()

# Static runtime on Windows -- matches ACEapp's convention so both apps can
# ever share a machine/toolchain without a runtime mismatch.
if(MSVC)
    foreach(flag_var
        CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE)
        if(${flag_var} MATCHES "/MD")
            string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
        endif()
    endforeach()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/EHsc)
    add_compile_options(/wd4251)
    add_compile_options(/wd4996)
endif()

# Configure GLFW (windowing library for Dear ImGui) -- vendored the same way
# as ACEapp_multi; see .gitignore for why vendor/glfw isn't committed.
option(GLFW_BUILD_EXAMPLES "Build the GLFW example programs" OFF)
option(GLFW_BUILD_TESTS "Build the GLFW test programs" OFF)
option(GLFW_BUILD_DOCS "Build the GLFW documentation" OFF)
option(GLFW_INSTALL "Generate installation target" OFF)
add_subdirectory(vendor/glfw)

# LSL (Lab Streaming Layer) is not optional here -- ACEcollective's entire
# purpose is receiving ACEapp's LSL streams -- but it's still probed the way
# ACEapp_multi does (prebuilt SDK dropped into lib/lsl/) so the same setup
# instructions apply to both apps.
find_library(LSL_LIB
    NAMES lsl lsl.lib
    PATHS
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/lsl/lib
    PATH_SUFFIXES Release Debug
)

set(LSL_AVAILABLE OFF)
if(LSL_LIB)
    set(LSL_AVAILABLE ON)
    message(STATUS "Found LSL: ${LSL_LIB}")
else()
    message(WARNING "\n--------------------------------------------------------------------")
    message(WARNING "LSL (liblsl) library not found.")
    message(WARNING "Download the prebuilt SDK from:")
    message(WARNING "  https://github.com/sccn/liblsl/releases")
    message(WARNING "and extract lsl.lib/lsl.dll to: ${CMAKE_CURRENT_SOURCE_DIR}/lib/lsl/lib")
    message(WARNING "and the include/ folder to: ${CMAKE_CURRENT_SOURCE_DIR}/lib/lsl/inc")
    message(WARNING "ACEcollective cannot receive any streams without it.")
    message(WARNING "--------------------------------------------------------------------\n")
endif()

# Include directories for all platforms
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/glfw/include
)
if(LSL_AVAILABLE)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib/lsl/inc)
endif()

# Dear ImGui core + GLFW/OpenGL3 backends
set(IMGUI_SOURCES
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_demo.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_tables.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/imgui/backends/imgui_impl_glfw.cpp
    vendor/imgui/backends/imgui_impl_opengl3.cpp
)

set(ACECOLLECTIVE_SOURCES
    src/main.cpp
    src/application.cpp
    src/processing/interbrain_connectivity_calculator.cpp
    src/lsl/participant_receiver.cpp
    src/lsl/coherence_outlet.cpp
    src/storage/coherence_csv_writer.cpp
    src/utils/app_paths.cpp
    src/utils/console_logger.cpp
    src/ui/panel_base.cpp
    src/ui/panel_manager.cpp
    src/ui/panels/connect_panel.cpp
    src/ui/panels/interbrain_connectivity_panel.cpp
    src/ui/panels/settings_panel.cpp
    ${IMGUI_SOURCES}
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(APPLE)
        set(BUNDLE_TYPE MACOSX_BUNDLE)
    else()
        set(BUNDLE_TYPE WIN32)
    endif()
    add_executable(ACEcollective ${BUNDLE_TYPE} ${ACECOLLECTIVE_SOURCES})
else()
    if(APPLE)
        add_executable(ACEcollective MACOSX_BUNDLE ${ACECOLLECTIVE_SOURCES})
    else()
        add_executable(ACEcollective ${ACECOLLECTIVE_SOURCES})
    endif()
endif()

# Find OpenGL (required for Dear ImGui)
find_package(OpenGL REQUIRED)

# Platform-specific settings
if(MSVC)
    target_compile_definitions(ACEcollective PRIVATE
        WIN32
        _WINDOWS
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
    )
    target_compile_options(ACEcollective PRIVATE $<$<CONFIG:Release>:/Zi>)
    target_link_options(ACEcollective PRIVATE $<$<CONFIG:Release>:/DEBUG /OPT:REF /OPT:ICF>)
elseif(APPLE)
    target_compile_definitions(ACEcollective PRIVATE __APPLE__)
    find_library(COCOA_LIBRARY Cocoa)
    find_library(IOKIT_LIBRARY IOKit)
    find_library(CORE_FOUNDATION_LIBRARY CoreFoundation)
    find_library(CORE_VIDEO_LIBRARY CoreVideo)
    find_library(UNIFORM_TYPE_IDENTIFIERS_LIBRARY UniformTypeIdentifiers)
endif()

target_link_libraries(ACEcollective PRIVATE
    glfw
    ${OPENGL_LIBRARIES}
)

if(APPLE)
    target_link_libraries(ACEcollective PRIVATE
        ${COCOA_LIBRARY}
        ${IOKIT_LIBRARY}
        ${CORE_FOUNDATION_LIBRARY}
        ${CORE_VIDEO_LIBRARY}
        ${UNIFORM_TYPE_IDENTIFIERS_LIBRARY}
    )
elseif(WIN32)
    target_link_libraries(ACEcollective PRIVATE
        kernel32 user32 gdi32 winspool shell32 ole32 oleaut32 uuid comdlg32 advapi32
    )
endif()

if(LSL_AVAILABLE)
    target_compile_definitions(ACEcollective PRIVATE LSL_ENABLED=1)
    target_link_libraries(ACEcollective PRIVATE ${LSL_LIB})
else()
    target_compile_definitions(ACEcollective PRIVATE LSL_DISABLED=1)
endif()

# Copy LSL DLL to the output directory on Windows
if(LSL_AVAILABLE AND WIN32)
    get_filename_component(LSL_LIB_DIR ${LSL_LIB} DIRECTORY)
    add_custom_command(TARGET ACEcollective POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LSL_LIB_DIR}/lsl.dll"
            "$<TARGET_FILE_DIR:ACEcollective>"
    )
endif()

install(TARGETS ACEcollective DESTINATION bin)
