cmake_minimum_required(VERSION 3.24)

project(zoal_briefing
    VERSION 1.0.0
    DESCRIPTION "Voice-driven briefing system for X-Plane"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ZoalBriefing_BUILD_TESTS    "Build unit tests (SDK-free)"   ON)
option(ZoalBriefing_BUILD_PLUGIN   "Build X-Plane .xpl plugin"     OFF)

set(ZoalBriefing_XPLANE_SDK "" CACHE PATH "Path to the X-Plane SDK root")
set(ZoalBriefing_FREETYPE_SOURCE "" CACHE PATH "FreeType source tree (see scripts/ensure-freetype.sh)")

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/XPlaneSDK.cmake)

find_package(Threads REQUIRED)

# ── Dear ImGui static library (needed by panel_view and plugin code) ─────────
set(ZOAL_BRIEFING_IMGUI_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/.cache/imgui-1.92.9b")

add_library(zoal_briefing_imgui STATIC
    "${ZOAL_BRIEFING_IMGUI_SOURCE}/imgui.cpp"
    "${ZOAL_BRIEFING_IMGUI_SOURCE}/imgui_draw.cpp"
    "${ZOAL_BRIEFING_IMGUI_SOURCE}/imgui_tables.cpp"
    "${ZOAL_BRIEFING_IMGUI_SOURCE}/imgui_widgets.cpp"
)

target_include_directories(zoal_briefing_imgui SYSTEM PUBLIC
    "${ZOAL_BRIEFING_IMGUI_SOURCE}"
)

target_compile_definitions(zoal_briefing_imgui PUBLIC
    IMGUI_DISABLE_DEMO_WINDOWS
    IMGUI_DISABLE_DEBUG_TOOLS
)

set_target_properties(zoal_briefing_imgui PROPERTIES POSITION_INDEPENDENT_CODE ON)

# FreeType rasterizes the panel's text hinted to the pixel grid, as in zoal-atc:
# stb_truetype, the default, leaves small UI text thin and soft. Built from
# source with every optional dependency off, so it needs nothing from the
# system. Required for the plugin; the SDK-free tests do not draw text.
if(ZoalBriefing_BUILD_PLUGIN)
    if(NOT EXISTS "${ZoalBriefing_FREETYPE_SOURCE}/include/ft2build.h")
        message(FATAL_ERROR "FreeType source not found at '${ZoalBriefing_FREETYPE_SOURCE}'. "
            "Run scripts/ensure-freetype.sh (make plugin does).")
    endif()
    foreach(_ft_dependency ZLIB BZIP2 PNG HARFBUZZ BROTLI)
        set(FT_DISABLE_${_ft_dependency} ON CACHE BOOL "" FORCE)
    endforeach()
    set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
    add_subdirectory("${ZoalBriefing_FREETYPE_SOURCE}" "${CMAKE_BINARY_DIR}/freetype" EXCLUDE_FROM_ALL)
    set_target_properties(freetype PROPERTIES POSITION_INDEPENDENT_CODE ON)

    target_sources(zoal_briefing_imgui PRIVATE
        "${ZOAL_BRIEFING_IMGUI_SOURCE}/misc/freetype/imgui_freetype.cpp")
    target_include_directories(zoal_briefing_imgui SYSTEM PUBLIC
        "${ZOAL_BRIEFING_IMGUI_SOURCE}/misc/freetype")
    target_compile_definitions(zoal_briefing_imgui PUBLIC IMGUI_ENABLE_FREETYPE)
    target_link_libraries(zoal_briefing_imgui PUBLIC freetype)
endif()

# ── core library (SDK-free, builds on all platforms) ──────────────────────────
add_library(zoal_briefing_core STATIC
    # Plugin glue (SDK-free parts)
    src/xplane/plugin_state.cpp

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
    src/cache_builder/apt_dat_parser.cpp
    src/cache_builder/cache_builder.cpp
    src/cache_builder/cifp_parser.cpp
    src/cache_builder/grib_file_parser.cpp
    src/cache_builder/grib_parser.cpp
    src/cache_builder/metar_file_parser.cpp
    src/cache_builder/navigraph_client.cpp
    src/cache_builder/taf_parser.cpp
    src/cache_builder/weather_summarizer.cpp

    # Intent parsing & audio pipeline
    src/intent_parser/intent_parser.cpp
    src/intent_parser/keyword_router.cpp

    # Panel view state & session (shared with plugin module)
    src/gui/panel_session.cpp
    src/gui/panel_theme.cpp
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
if(ZoalBriefing_XPLANE_SDK AND EXISTS "${ZoalBriefing_XPLANE_SDK}/CHeaders")
    set(_zb_sdk_present ON)
elseif(ZoalBriefing_XPLANE_SDK AND EXISTS "${ZoalBriefing_XPLANE_SDK}/XPLM")
    set(_zb_sdk_present ON)
elseif(NOT ZoalBriefing_XPLANE_SDK AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sdk/XPLM")
    set(ZoalBriefing_XPLANE_SDK "${CMAKE_CURRENT_SOURCE_DIR}/sdk" CACHE PATH "Auto-detected X-Plane SDK" FORCE)
    set(_zb_sdk_present ON)
endif()

if(ZoalBriefing_BUILD_PLUGIN AND _zb_sdk_present)
    message(STATUS "Building zoal-briefing .xpl plugin (SDK: ${ZoalBriefing_XPLANE_SDK})")

    # X-Plane SDK 4.4+ provides panel-graphics ImGui helpers (XPLMPanelGraphics.h)
    if(NOT EXISTS "${ZoalBriefing_XPLANE_SDK}/XPLM/XPLMPanelGraphics.h")
        message(FATAL_ERROR "X-Plane SDK 4.4+ required (needs XPLMPanelGraphics.h).")
    endif()

    add_library(zoal_briefing MODULE)

    target_sources(zoal_briefing PRIVATE
        src/xplane/imgui_window.cpp
        src/xplane/panel_window.cpp
        src/xplane/xplane_plugin.cpp
        # Panel view (moved from core — must be in plugin so draw_panel symbol exports correctly)
        src/gui/panel_view.cpp
    )

    target_include_directories(zoal_briefing PRIVATE
        ${ZoalBriefing_XPLANE_SDK}
        ${ZoalBriefing_XPLANE_SDK}/XPLM
        ${ZoalBriefing_XPLANE_SDK}/CHeaders
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(zoal_briefing PRIVATE zoal_briefing_core zoal_briefing_imgui)

    # Enable all XPLM API features (matches zoal-atc pattern)
    # xplane_logger.h writes to X-Plane's Log.txt in the plugin (stderr elsewhere).
    target_compile_definitions(zoal_briefing PRIVATE ZOAL_BRIEFING_USE_XPLANE_SDK=1)

    target_compile_definitions(zoal_briefing PRIVATE
        XPLM200=1 XPLM210=1 XPLM300=1 XPLM301=1
        XPLM400=1 XPLM410=1 XPLM420=1 XPLM430=1 XPLM440=1
    )

    # ── SDK linking (matches zoal-atc pattern) ────────────────────────────
    if(APPLE)
        target_compile_definitions(zoal_briefing PRIVATE APL=1)
        set(_zb_xplm_framework "${ZoalBriefing_XPLANE_SDK}/Libraries/Mac/XPLM.framework/Versions/Current/XPLM")
        if(EXISTS "${_zb_xplm_framework}")
            message(STATUS "Linking against XPLM framework: ${_zb_xplm_framework}")
            target_link_libraries(zoal_briefing PRIVATE "${_zb_xplm_framework}")
        else()
            message(WARNING "XPLM.framework not found, using dynamic lookup")
            set_target_properties(zoal_briefing PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
        endif()
        target_link_libraries(zoal_briefing PRIVATE
            "-framework CoreAudio"
            "-framework AudioToolbox"
            "-framework Foundation"
        )
        set_target_properties(zoal_briefing PROPERTIES
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH "@loader_path"
        )
    elseif(WIN32)
        target_compile_definitions(zoal_briefing PRIVATE IBM=1)
        find_library(_zb_xplm_win NAMES XPLM_64 XPLM 
            PATHS "${ZoalBriefing_XPLANE_SDK}/Libraries/Win" NO_DEFAULT_PATH)
        if(_zb_xplm_win)
            message(STATUS "Linking against XPLM Windows library: ${_zb_xplm_win}")
            target_link_libraries(zoal_briefing PRIVATE "${_zb_xplm_win}")
        endif()
        # MinGW: statically link gcc and stdc++ runtimes so the .xpl ships standalone.
        if(MINGW)
            target_link_options(zoal_briefing PRIVATE
                -static-libgcc
                -static-libstdc++
                -static
            )
        endif()
        target_link_libraries(zoal_briefing PRIVATE ws2_32 winmm ole32 uuid)
        target_compile_options(zoal_briefing PRIVATE /WX-)
    else()
        target_compile_definitions(zoal_briefing PRIVATE LIN=1)
        find_library(_zb_xplm_lin NAMES XPLM_64.so XPLM_64
            PATHS "${ZoalBriefing_XPLANE_SDK}/Libraries/Lin" NO_DEFAULT_PATH)
        if(_zb_xplm_lin)
            message(STATUS "Linking against XPLM Linux library: ${_zb_xplm_lin}")
            target_link_libraries(zoal_briefing PRIVATE "${_zb_xplm_lin}")
        endif()
        target_link_libraries(zoal_briefing PRIVATE "-lX11" pthread dl m)
    endif()

    # The .xpl name uses a hyphen (zoal-briefing.xpl), matching the zoal-* convention.
    set_target_properties(zoal_briefing PROPERTIES
        OUTPUT_NAME "zoal-briefing"
        PREFIX ""
        SUFFIX ".xpl"
    )

else()
    if(ZoalBriefing_BUILD_PLUGIN)
        message(WARNING "Plugin module skipped: X-Plane SDK not found")
        message(WARNING "Pass -DZoalBriefing_XPLANE_SDK=/path/to/sdk to build the plugin")
    endif()
endif()

# ── tests (SDK-free, runs on all platforms) ───────────────────────────────────
if(ZoalBriefing_BUILD_TESTS)
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

message(STATUS "zoal-briefing v${PROJECT_VERSION} configured (tests=${ZoalBriefing_BUILD_TESTS}, plugin=${_zb_sdk_present})")
