cmake_minimum_required(VERSION 3.24)

project(zoal_briefing
    VERSION 1.0.0
    DESCRIPTION "Voice-driven dispatch/departure/arrival briefing system for X-Plane"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ZOAL_BRIEFING_BUILD_TESTS "Build SDK-free unit tests" ON)
option(ZOAL_BRIEFING_BUILD_PLUGIN "Build the X-Plane plugin module" ON)

set(ZOAL_ATC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../zoal-atc" CACHE PATH "Path to zoal-atc reference repo")
set(BRIEFINGMASTER_WHISPER_SOURCE "" CACHE PATH "whisper.cpp source tree (see scripts/ensure-whisper-src.sh)")
set(BRIEFINGMASTER_PIPER_SOURCE "" CACHE PATH "piper-cpp source tree (see scripts/ensure-piper.sh)")
set(ZOAL_BRIEFING_XPLANE_SDK "${CMAKE_CURRENT_SOURCE_DIR}/sdk" CACHE PATH "Path to the X-Plane SDK root")

find_package(Threads REQUIRED)

# --- Core Library: Local File-Based Weather & State Machine ---
add_library(zoal_briefing_core STATIC
    src/state_machine/state_machine.cpp
    src/data_types/weather_summarizer.cpp
    src/llm_bridge/llm_bridge_router.cpp
    src/cache_builder/metar_file_parser.cpp
    src/cache_builder/grib_file_parser.cpp
    src/simbrief_trigger/simbrief_trigger.cpp
)

target_include_directories(zoal_briefing_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(zoal_briefing_core PUBLIC cxx_std_17)
set_target_properties(zoal_briefing_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(zoal_briefing_core PUBLIC Threads::Threads)

if(ZOAL_BRIEFING_XPLANE_SDK)
    target_include_directories(zoal_briefing_core PRIVATE ${ZOAL_BRIEFING_XPLANE_SDK}/Headers)
endif()

if(ZOAL_ATC_DIR AND EXISTS "${ZOAL_ATC_DIR}/include")
    target_include_directories(zoal_briefing_core PRIVATE ${ZOAL_ATC_DIR}/include)
endif()

# --- Plugin Module: X-Plane Integration ---
if(ZOAL_BRIEFING_BUILD_PLUGIN)
    add_library(zoal_briefing MODULE)
    
    target_sources(zoal_briefing PRIVATE src/xplane_plugin/main.cpp)
    target_link_libraries(zoal_briefing PRIVATE zoal_briefing_core)

    if(APPLE)
        target_link_libraries(zoal_briefing PRIVATE "-framework CoreAudio" "-framework AudioToolbox")
    elseif(WIN32)
        target_link_libraries(zoal_briefing PRIVATE ws2_32 winmm)
    endif()

    set_target_properties(zoal_briefing PROPERTIES
        OUTPUT_NAME "zoal_briefing"
        PREFIX ""
        SUFFIX ".xpl"
    )
endif()

if(ZOAL_BRIEFING_BUILD_TESTS)
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

    # Tests for local file-based weather parsing
    zb_add_test(metar_parser_tests test/metar_file_parser_test.cpp)
    zb_add_test(grib_parser_tests test/grib_file_parser_test.cpp)
    zb_add_test(weather_summary_tests tests/weather_summary_test.cpp)
endif()
