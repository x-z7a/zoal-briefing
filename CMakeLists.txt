cmake_minimum_required(VERSION 3.24)

project(zoal_briefing
    VERSION 0.1.0
    DESCRIPTION "BriefingMaster - Voice-driven aviation briefing system for X-Plane"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ZOAL_BRIEFING_BUILD_TESTS "Build SDK-free unit tests" ON)
option(ZOAL_BRIEFING_BUILD_PLUGIN "Build the X-Plane plugin module" OFF)

find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# Core library: state machine, cache builder, intent parser, LLM bridge,
# simbrief client, data types, and audio pipeline.
# ---------------------------------------------------------------------------
add_library(zoal_briefing_core STATIC
    src/data_types/airport.cpp
    src/data_types/cifp.cpp
    src/data_types/flight_plan.cpp
    src/data_types/sid_star.cpp
    src/data_types/weather.cpp

    src/state_machine/state_machine.cpp

    src/cache_builder/apt_dat_parser.cpp
    src/cache_builder/cache_builder.cpp
    src/cache_builder/cifp_parser.cpp
    src/cache_builder/grib_parser.cpp
    src/cache_builder/navigraph_client.cpp

    src/intent_parser/intent_parser.cpp
    src/intent_parser/keyword_router.cpp

    src/llm_bridge/api_mode.cpp
    src/llm_bridge/cli_mode.cpp
    src/llm_bridge/fallback_chain.cpp
    src/llm_bridge/llm_bridge.cpp

    src/simbrief_client/client.cpp

    src/audio_pipeline/stt_transcriber.cpp
    src/audio_pipeline/tts_speaker.cpp
    src/audio_pipeline/wake_word_engine.cpp

    src/xplane/plugin_state.cpp
)

target_include_directories(zoal_briefing_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(zoal_briefing_core PUBLIC cxx_std_17)
set_target_properties(zoal_briefing_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(zoal_briefing_core PUBLIC Threads::Threads)

if(WIN32)
    target_link_libraries(zoal_briefing_core PUBLIC ws2_32)
endif()

if(MSVC)
    target_compile_options(zoal_briefing_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(zoal_briefing_core PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
    )
endif()

# ---------------------------------------------------------------------------
# Plugin module: X-Plane integration (xplane_plugin, plugin_state).
# ---------------------------------------------------------------------------
if(ZOAL_BRIEFING_BUILD_PLUGIN)
    add_library(zoal_briefing MODULE
        src/xplane/xplane_plugin.cpp
    )

    target_link_libraries(zoal_briefing PRIVATE zoal_briefing_core)

    set_target_properties(zoal_briefing PROPERTIES
        OUTPUT_NAME "zoal_briefing"
        PREFIX ""
        SUFFIX ".xpl"
    )

    # X-Plane loads the plugin directly, so a MinGW build must not require 
    # the MinGW runtime DLLs to be installed beside the .xpl.
    if(WIN32 AND MINGW)
        target_link_options(zoal_briefing PRIVATE
            -static-libgcc
            -static-libstdc++
            -static
        )
    endif()

    if(APPLE)
        target_link_libraries(zoal_briefing PRIVATE
            "-framework CoreAudio"
            "-framework CoreFoundation"
            "-framework AudioToolbox"
            "-framework AudioUnit"
        )
        set_target_properties(zoal_briefing PROPERTIES
            BUILD_WITH_INSTALL_RPATH TRUE
            BUILD_RPATH "@loader_path"
            INSTALL_RPATH "@loader_path"
        )
    elseif(UNIX)
        target_link_libraries(zoal_briefing PRIVATE pthread dl m)
        set_target_properties(zoal_briefing PROPERTIES
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH "\$ORIGIN"
        )
    elseif(WIN32)
        target_link_libraries(zoal_briefing PRIVATE ole32 uuid winmm)
    endif()

endif()

# ---------------------------------------------------------------------------
# Unit tests (SDK-free, link against core only).
# ---------------------------------------------------------------------------
if(ZOAL_BRIEFING_BUILD_TESTS)
    enable_testing()

    function(zoal_briefing_add_test name source)
        add_executable(${name} ${source})
        target_link_libraries(${name} PRIVATE zoal_briefing_core)
        if(MSVC)
            target_compile_options(${name} PRIVATE /W4 /permissive-)
        else()
            target_compile_options(${name} PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wsign-conversion
            )
        endif()
        add_test(NAME ${name} COMMAND ${name})
    endfunction()

    zoal_briefing_add_test(zoal_briefing_sm_tests test/state_machine_test.cpp)
    zoal_briefing_add_test(zoal_briefing_intent_tests test/intent_parser_test.cpp)
    zoal_briefing_add_test(zoal_briefing_apt_dat_tests test/apt_dat_parser_test.cpp)
    zoal_briefing_add_test(zoal_briefing_llm_bridge_tests test/llm_bridge_test.cpp)
    zoal_briefing_add_test(zoal_briefing_simbrief_tests test/simbrief_client_test.cpp)
endif()
