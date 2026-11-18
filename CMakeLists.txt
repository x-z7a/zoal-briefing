cmake_minimum_required(VERSION 3.24)

project(zoal_briefing
    VERSION 1.0.0
    DESCRIPTION "Voice-driven briefing system for X-Plane"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ZOAL_BRIEFING_BUILD_TESTS    "Build unit tests"              ON)
option(ZOAL_BRIEFING_BUILD_PLUGIN   "Build X-Plane plugin module"   OFF)

# Path to the X-Plane SDK root (empty = auto-download from GitHub releases)
set(ZOAL_BRIEFING_XPLANE_SDK "" CACHE PATH "Path to the X-Plane SDK root")

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/XPlaneSDK.cmake)

find_package(Threads REQUIRED)

# ── core library (SDK-free, builds on all platforms) ──────────────────────────
add_library(zoal_briefing_core STATIC
    # GUI panel
    src/gui/panel_session.cpp
    src/gui/panel_theme.cpp
    src/gui/panel_view.cpp

    # Plugin glue
    src/xplane/plugin_state.cpp
    src/xplane/panel_window.cpp

    # State machine & LLM bridge
    src/state_machine/state_machine.cpp
    src/llm_bridge/llm_bridge.cpp
    src/llm_bridge/api_mode.cpp
    src/llm_bridge/cli_mode.cpp
    src/llm_bridge/fallback_chain.cpp
    src/llm_bridge/llm_bridge_router.cpp

    # Data types & cache builders
    src/data_types/airport.cpp
    src/data_types/cifp.cpp
    src/data_types/flight_plan.cpp
    src/data_types/sid_star.cpp
    src/data_types/weather.cpp
    src/data_types/weather_summarizer.cpp
    src/cache_builder/metar_file_parser.cpp
    src/cache_builder/grib_file_parser.cpp
    src/cache_builder/cache_builder.cpp
    src/cache_builder/taf_parser.cpp

    # Intent parsing & audio pipeline
    src/intent_parser/intent_parser.cpp
    src/intent_parser/keyword_router.cpp
)

target_include_directories(zoal_briefing_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(zoal_briefing_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(zoal_briefing_core PUBLIC Threads::Threads)

if(MSVC)
    target_compile_options(zoal_briefing_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(zoal_briefing_core PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
    )
endif()

# ── X-Plane plugin module (only when SDK present) ────────────────────────────
set(_zb_sdk_present OFF)
if(ZOAL_BRIEFING_XPLANE_SDK AND (EXISTS "${ZOAL_BRIEFING_XPLANE_SDK}/CHeaders" OR EXISTS "${ZOAL_BRIEFING_XPLANE_SDK}/XPLM"))
    set(_zb_sdk_present ON)
endif()

if(ZOAL_BRIEFING_BUILD_PLUGIN AND _zb_sdk_present)
    add_library(zoal_briefing MODULE)

    target_sources(zoal_briefing PRIVATE
        src/xplane/xplane_plugin.cpp
    )

    target_link_libraries(zoal_briefing PRIVATE zoal_briefing_core)
    target_include_directories(zoal_briefing PRIVATE ${ZOAL_BRIEFING_XPLANE_SDK})

    if(APPLE)
        target_compile_definitions(zoal_briefing PRIVATE APL=1)
        target_link_libraries(zoal_briefing PRIVATE
            "-framework CoreAudio"
            "-framework AudioToolbox"
            "-framework Foundation"
        )
        target_compile_options(zoal_briefing PRIVATE -fPIC)
    elseif(WIN32)
        target_compile_definitions(zoal_briefing PRIVATE IBM=1)
        target_link_libraries(zoal_briefing PRIVATE ws2_32 winmm)
        target_compile_options(zoal_briefing PRIVATE /WX-)
    else()
        target_compile_definitions(zoal_briefing PRIVATE LIN=1)
        target_link_libraries(zoal_briefing PRIVATE "-lX11" pthread dl m)
    endif()

    # Plugin name: zoal-briefing.xpl (hyphenated, matching plugin naming convention)
    set_target_properties(zoal_briefing PROPERTIES
        OUTPUT_NAME "zoal-briefing"
        PREFIX ""
        SUFFIX ".xpl"
    )

    message(STATUS "X-Plane plugin module enabled (output: ${CMAKE_CURRENT_BINARY_DIR}/zoal-briefing.xpl)")
else()
    if(ZOAL_BRIEFING_BUILD_PLUGIN)
        message(WARNING "Plugin module skipped: X-Plane SDK not found at ${ZOAL_BRIEFING_XPLANE_SDK}")
        message(WARNING "Pass -DZOAL_BRIEFING_XPLANE_SDK=/path/to/sdk to build the plugin")
    endif()
endif()

# ── tests (SDK-free, runs on all platforms) ───────────────────────────────────
if(ZOAL_BRIEFING_BUILD_TESTS)
    enable_testing()

    function(zb_add_test name source)
        add_executable(${name} ${source})
        target_link_libraries(${name} PRIVATE zoal_briefing_core)
        if(MSVC)
            target_compile_options(${name} PRIVATE /W4 /permissive-)
        else()
            target_compile_options(${name} PRIVATE
                -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
            )
        endif()
        add_test(NAME ${name} COMMAND ${name})
    endfunction()

    zb_add_test(metar_parser_tests test/metar_file_parser_test.cpp)
    zb_add_test(grib_parser_tests test/grib_file_parser_test.cpp)
    zb_add_test(weather_summary_tests tests/weather_summary_test.cpp)
    zb_add_test(intent_parser_tests test/intent_parser_test.cpp)

    message(STATUS "Unit tests enabled")
endif()

message(STATUS "ZoalBriefing v${PROJECT_VERSION} configured (tests=${ZOAL_BRIEFING_BUILD_TESTS}, plugin=${_zb_sdk_present})")
