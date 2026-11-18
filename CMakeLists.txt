cmake_minimum_required(VERSION 3.24)

project(zoal_briefing
    VERSION 1.0.0
    DESCRIPTION "Voice-driven dispatch/departure/arrival briefing system for X-Plane"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# Dependencies
# ============================================================================
find_package(Threads REQUIRED)

# ============================================================================
# Options (prefix ZoalBriefing_ for -D flags, matching zoal-atc conventions)
# ============================================================================
option(ZoalBriefing_BUILD_TESTS    "Build unit tests"              ON)
option(ZoalBriefing_BUILD_PLUGIN   "Build X-Plane plugin module"   OFF)

set(ZoalBriefing_ATC_DIR         "" CACHE PATH "Path to zoal-atc reference repo")
set(ZoalBriefing_XPLANE_SDK      "" CACHE PATH "Path to the X-Plane SDK root (empty = auto-download)")

# ============================================================================
# Include CMake helper for SDK download from X-Plane CDN
# ============================================================================
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/XPlaneSDK.cmake")
    message(FATAL_ERROR "Missing cmake/XPlaneSDK.cmake — this file was removed during refactoring")
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/XPlaneSDK.cmake")

# ============================================================================
# Core Library — compiles without X-Plane SDK headers
# ============================================================================
add_library(zoal_briefing_core STATIC
    src/state_machine/state_machine.cpp
    src/data_types/weather_summarizer.cpp
    src/llm_bridge/llm_bridge_router.cpp
    src/cache_builder/metar_file_parser.cpp
    src/cache_builder/grib_file_parser.cpp
)

target_include_directories(zoal_briefing_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(zoal_briefing_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(zoal_briefing_core PUBLIC Threads::Threads)

# X-Plane SDK headers (optional — core library should compile without them)
if(ZoalBriefing_XPLANE_SDK AND EXISTS "${ZoalBriefing_XPLANE_SDK}/CHeaders")
    target_include_directories(zoal_briefing_core PRIVATE ${ZoalBriefing_XPLANE_SDK}/CHeaders)
endif()

# zoal-atc headers (optional, for plugin builds that reference it)
if(ZoalBriefing_ATC_DIR AND EXISTS "${ZoalBriefing_ATC_DIR}/include")
    target_include_directories(zoal_briefing_core PRIVATE ${ZoalBriefing_ATC_DIR}/include)
endif()

# ============================================================================
# X-Plane Plugin Module — requires SDK headers and platform libs
# ============================================================================
set(_zb_has_sdk OFF)
if(ZoalBriefing_BUILD_PLUGIN AND ZoalBriefing_XPLANE_SDK AND EXISTS "${ZoalBriefing_XPLANE_SDK}/CHeaders")
    set(_zb_has_sdk ON)
endif()

if(_zb_has_sdk)
    add_library(zoal_briefing MODULE)
    target_sources(zoal_briefing PRIVATE src/xplane_plugin/main.cpp)
    target_link_libraries(zoal_briefing PRIVATE zoal_briefing_core)

    # Plugin needs SDK headers directly (XPLMPlugin.h, etc.)
    target_include_directories(zoal_briefing PRIVATE ${ZoalBriefing_XPLANE_SDK}/CHeaders)

    # Platform-specific linking
    if(APPLE)
        target_link_libraries(zoal_briefing PRIVATE "-framework CoreAudio" "-framework AudioToolbox")
        target_compile_options(zoal_briefing PRIVATE -fPIC)
    elseif(WIN32)
        target_link_libraries(zoal_briefing PRIVATE ws2_32 winmm)
        # MSVC: export all symbols for .xpl plugin
        target_compile_options(zoal_briefing PRIVATE /WX-)
    endif()

    set_target_properties(zoal_briefing PROPERTIES
        OUTPUT_NAME "zoal_briefing"
        PREFIX ""
        SUFFIX ".xpl"
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )

    # Detect X-Plane Developer/Plugins directory per platform
    if(APPLE)
        set(_zb_xplane_plugins "${HOME}/Library/Application Support/X-Plane/Developer/Plugins")
    elseif(WIN32)
        set(_zb_xplane_plugins "C:/X-Plane/Developer/Plugins")
    else()
        set(_zb_xplane_plugins "/opt/X-Plane/Developer/Plugins")
    endif()

    # install-plugin: copy .xpl to X-Plane Developer/Plugins folder
    add_custom_target(install-plugin
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_zb_xplane_plugins}"
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:zoal_briefing> "${_zb_xplane_plugins}/"
        DEPENDS zoal_briefing
        COMMENT "Installing zoal_briefing.xpl to ${_zb_xplane_plugins}"
    )

    message(STATUS "X-Plane plugin module enabled (output: ${CMAKE_CURRENT_BINARY_DIR}/zoal_briefing.xpl)")
else()
    if(ZoalBriefing_BUILD_PLUGIN)
        message(WARNING "Plugin module skipped: X-Plane SDK not found at ${ZoalBriefing_XPLANE_SDK}")
        message(WARNING "Pass -DZoalBriefing_XPLANE_SDK=/path/to/sdk to build the plugin")
    endif()
endif()

# ============================================================================
# Unit Tests — SDK-free, runs on all platforms
# ============================================================================
if(ZoalBriefing_BUILD_TESTS)
    enable_testing()

    function(zb_add_test name source)
        add_executable(${name} ${source})
        target_link_libraries(${name} PRIVATE zoal_briefing_core)
        if(MSVC)
            target_compile_options(${name} PRIVATE /W4 /permissive-)
        else()
            target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        add_test(NAME ${name} COMMAND ${name})
    endfunction()

    zb_add_test(metar_parser_tests test/metar_file_parser_test.cpp)
    zb_add_test(grib_parser_tests test/grib_file_parser_test.cpp)
    zb_add_test(weather_summary_tests tests/weather_summary_test.cpp)

    message(STATUS "Unit tests enabled")
endif()

message(STATUS "ZoalBriefing v${PROJECT_VERSION} configured (tests=${ZoalBriefing_BUILD_TESTS}, plugin=${_zb_has_sdk})")
