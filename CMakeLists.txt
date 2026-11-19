cmake_minimum_required(VERSION 3.24)

project(zoal_briefing
    VERSION 1.0.0
    DESCRIPTION "Voice-driven briefing system for X-Plane"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# compile_commands.json in every build dir, for clang-tidy (scripts/lint.sh) and editors.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ZoalBriefing_BUILD_TESTS    "Build unit tests (SDK-free)"   ON)
option(ZoalBriefing_BUILD_PLUGIN   "Build X-Plane .xpl plugin"     OFF)
# The audio pipeline: openWakeWord (on ONNX Runtime), whisper.cpp, Piper (via
# sherpa-onnx) and miniaudio. Off by default so the core tests need nothing;
# `make plugin` turns it on where its dependencies are pinned (see scripts/).
option(ZoalBriefing_WITH_AUDIO     "Build the audio pipeline"      OFF)

set(ZoalBriefing_XPLANE_SDK "" CACHE PATH "Path to the X-Plane SDK root")
set(ZoalBriefing_FREETYPE_SOURCE "" CACHE PATH "FreeType source tree (see scripts/ensure-freetype.sh)")
set(ZoalBriefing_WHISPER_SOURCE "" CACHE PATH "whisper.cpp source tree (see scripts/ensure-whisper-src.sh)")
set(ZoalBriefing_SHERPA_ONNX "" CACHE PATH "sherpa-onnx build + ONNX Runtime headers (see scripts/ensure-sherpa-onnx.sh)")
set(ZoalBriefing_MINIAUDIO "" CACHE PATH "Directory holding miniaudio.h (see scripts/ensure-miniaudio.sh)")
set(ZoalBriefing_MODELS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.cache/models" CACHE PATH "Models for the audio self-test (see scripts/ensure-models.sh)")

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
    src/log.cpp

    # Plugin glue (SDK-free parts)
    src/xplane/plugin_state.cpp
    src/xplane/briefing_responder.cpp

    # State machine & LLM bridge
    src/state_machine/state_machine.cpp
    src/llm_bridge/llm_bridge.cpp
    src/llm_bridge/api_mode.cpp
    src/llm_bridge/cli_mode.cpp
    src/llm_bridge/fallback_chain.cpp
    src/llm_bridge/llm_bridge_router.cpp
    src/llm_bridge/ollama_client.cpp
    src/llm_bridge/briefing_prompts.cpp
    src/llm_bridge/briefing_generator.cpp

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

    # SimBrief OFP (HTTP goes through util::HttpClient, implemented in
    # zoal_briefing_http below)
    src/simbrief/ofp.cpp
    src/simbrief/client.cpp
    src/simbrief/plan_watcher.cpp

    # Intent parsing
    src/intent_parser/intent_parser.cpp
    src/intent_parser/keyword_router.cpp

    # Audio pipeline: the dependency-free parts (the engines are in
    # zoal_briefing_audio below)
    src/audio/config.cpp
    src/audio/input/endpointer.cpp
    src/audio/tts/sentences.cpp
    src/audio/tts/voice_pool.cpp
    src/audio/voice_loop.cpp

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

# ── HTTP (libcurl where found) ────────────────────────────────────────────────
# SimBrief and the LLM go through util::HttpClient. libcurl comes with macOS and
# Linux; where it is missing (Windows, for now) every request fails with a
# clear error instead, and the plugin still builds and loads.
find_package(CURL QUIET)
if(CURL_FOUND)
    add_library(zoal_briefing_http STATIC src/util/curl_http_client.cpp)
    target_link_libraries(zoal_briefing_http PUBLIC zoal_briefing_core PRIVATE CURL::libcurl)
    message(STATUS "HTTP via libcurl ${CURL_VERSION_STRING}")
else()
    add_library(zoal_briefing_http STATIC src/util/no_http_client.cpp)
    target_link_libraries(zoal_briefing_http PUBLIC zoal_briefing_core)
    message(WARNING "libcurl not found: the plugin builds without HTTP (no SimBrief, no LLM)")
endif()
set_target_properties(zoal_briefing_http PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Dev tool: fetch a SimBrief OFP and generate its briefings outside X-Plane.
add_executable(briefing_check tools/briefing_check.cpp)
target_link_libraries(briefing_check PRIVATE zoal_briefing_http)
if(NOT ZoalBriefing_BUILD_TESTS)
    set_target_properties(briefing_check PROPERTIES EXCLUDE_FROM_ALL ON)
endif()

# ── audio pipeline (optional) ─────────────────────────────────────────────────
if(ZoalBriefing_WITH_AUDIO)
    foreach(_dep_check
            "ZoalBriefing_WHISPER_SOURCE;include/whisper.h;scripts/ensure-whisper-src.sh"
            "ZoalBriefing_SHERPA_ONNX;include/sherpa-onnx/c-api/c-api.h;scripts/ensure-sherpa-onnx.sh"
            "ZoalBriefing_SHERPA_ONNX;include/onnxruntime/onnxruntime_c_api.h;scripts/ensure-sherpa-onnx.sh"
            "ZoalBriefing_MINIAUDIO;miniaudio.h;scripts/ensure-miniaudio.sh")
        list(GET _dep_check 0 _dep_var)
        list(GET _dep_check 1 _dep_file)
        list(GET _dep_check 2 _dep_script)
        if(NOT EXISTS "${${_dep_var}}/${_dep_file}")
            message(FATAL_ERROR "${_dep_file} not found under ${_dep_var}='${${_dep_var}}'. Run ${_dep_script}.")
        endif()
    endforeach()

    # whisper.cpp, static and portable (the plugin runs on other machines):
    # Metal with embedded shaders on macOS, plain CPU without OpenMP elsewhere.
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
    set(GGML_NATIVE OFF CACHE BOOL "" FORCE)
    if(APPLE)
        set(GGML_METAL ON CACHE BOOL "" FORCE)
        set(GGML_METAL_EMBED_LIBRARY ON CACHE BOOL "" FORCE)
        set(GGML_BLAS ON CACHE BOOL "" FORCE)
        set(GGML_BLAS_VENDOR "Apple" CACHE STRING "" FORCE)
    else()
        set(GGML_OPENMP OFF CACHE BOOL "" FORCE)
    endif()
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    add_subdirectory("${ZoalBriefing_WHISPER_SOURCE}" "${CMAKE_BINARY_DIR}/whisper" EXCLUDE_FROM_ALL)

    add_library(zoal_briefing_audio STATIC
        src/audio/wake/openwakeword.cpp
        src/audio/stt/whisper_stt.cpp
        src/audio/tts/piper_tts.cpp
        src/audio/input/mic.cpp
        src/audio/miniaudio.cpp
        src/audio/voice_pipeline.cpp
        src/audio/output/speaker.cpp
    )
    set_target_properties(zoal_briefing_audio PROPERTIES POSITION_INDEPENDENT_CODE ON)
    # src/ for the private headers shared across folders (audio/miniaudio.hpp).
    target_include_directories(zoal_briefing_audio PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_include_directories(zoal_briefing_audio SYSTEM PRIVATE
        "${ZoalBriefing_SHERPA_ONNX}/include"
        "${ZoalBriefing_SHERPA_ONNX}/include/onnxruntime"
        "${ZoalBriefing_MINIAUDIO}"
    )
    target_link_libraries(zoal_briefing_audio PUBLIC zoal_briefing_core PRIVATE whisper)
    if(NOT MSVC)
        target_compile_options(zoal_briefing_audio PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    # sherpa-onnx carries ONNX Runtime; openWakeWord runs on that same copy.
    if(APPLE)
        # Static, in link order: the C API over the core, the phonemizer and
        # its data, and ONNX Runtime underneath everything.
        foreach(_lib
                sherpa-onnx-c-api sherpa-onnx-core sherpa-onnx-fstfar sherpa-onnx-fst
                sherpa-onnx-kaldifst-core kaldi-decoder-core kaldi-native-fbank-core
                piper_phonemize espeak-ng ucd onnxruntime ssentencepiece_core kissfft-float)
            target_link_libraries(zoal_briefing_audio PUBLIC "${ZoalBriefing_SHERPA_ONNX}/lib/lib${_lib}.a")
        endforeach()
        target_link_libraries(zoal_briefing_audio PUBLIC
            "-framework Foundation" "-framework CoreML"
            "-framework CoreAudio" "-framework AudioToolbox" "-framework CoreFoundation")
        set(ZoalBriefing_AUDIO_RUNTIME "")
    elseif(WIN32)
        # Shared MSVC builds with a C ABI; the DLLs ship beside the .xpl.
        target_link_libraries(zoal_briefing_audio PUBLIC
            "${ZoalBriefing_SHERPA_ONNX}/lib/sherpa-onnx-c-api.lib"
            "${ZoalBriefing_SHERPA_ONNX}/lib/onnxruntime.lib" ole32 winmm)
        set(ZoalBriefing_AUDIO_RUNTIME
            "${ZoalBriefing_SHERPA_ONNX}/lib/sherpa-onnx-c-api.dll"
            "${ZoalBriefing_SHERPA_ONNX}/lib/onnxruntime.dll"
            "${ZoalBriefing_SHERPA_ONNX}/lib/onnxruntime_providers_shared.dll")
    else()
        target_link_libraries(zoal_briefing_audio PUBLIC
            "${ZoalBriefing_SHERPA_ONNX}/lib/libsherpa-onnx-c-api.so"
            "${ZoalBriefing_SHERPA_ONNX}/lib/libonnxruntime.so" dl pthread m)
        set(ZoalBriefing_AUDIO_RUNTIME
            "${ZoalBriefing_SHERPA_ONNX}/lib/libsherpa-onnx-c-api.so"
            "${ZoalBriefing_SHERPA_ONNX}/lib/libonnxruntime.so")
    endif()

    # Dev tool: self-test without a microphone, or the live loop on mic + speakers.
    add_executable(voice_check tools/voice_check.cpp)
    target_link_libraries(voice_check PRIVATE zoal_briefing_audio)
    foreach(_runtime IN LISTS ZoalBriefing_AUDIO_RUNTIME)
        add_custom_command(TARGET voice_check POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_runtime}" "$<TARGET_FILE_DIR:voice_check>")
    endforeach()

    message(STATUS "Audio pipeline enabled (whisper: ${ZoalBriefing_WHISPER_SOURCE})")
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
        src/xplane/plugin.cpp
        # The UI's SDK side: the X-Plane <-> ImGui bridge and the panel window.
        # panel_view draws with ImGui, which only the plugin links.
        src/gui/imgui_window.cpp
        src/gui/panel_window.cpp
        src/gui/panel_view.cpp
        # Speech into X-Plane's sound system.
        src/audio/output/xplane_bus.cpp
    )

    target_include_directories(zoal_briefing PRIVATE
        ${ZoalBriefing_XPLANE_SDK}
        ${ZoalBriefing_XPLANE_SDK}/XPLM
        ${ZoalBriefing_XPLANE_SDK}/CHeaders
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        # SDK-side headers live beside their sources: "gui/panel_window.hpp".
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(zoal_briefing PRIVATE zoal_briefing_core zoal_briefing_http zoal_briefing_imgui)
    if(ZoalBriefing_WITH_AUDIO)
        target_link_libraries(zoal_briefing PRIVATE zoal_briefing_audio)
        target_compile_definitions(zoal_briefing PRIVATE ZOAL_BRIEFING_HAS_AUDIO=1)
        foreach(_runtime IN LISTS ZoalBriefing_AUDIO_RUNTIME)
            add_custom_command(TARGET zoal_briefing POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_runtime}" "$<TARGET_FILE_DIR:zoal_briefing>")
        endforeach()
    endif()

    # Enable all XPLM API features (matches zoal-atc pattern)

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

    # Export only the five plugin entry points. The .xpl carries its own ONNX
    # Runtime, whisper and sherpa-onnx; other plugins (zoal-atc) carry theirs,
    # and none of them may see another's.
    set(_zb_entry_points XPluginStart XPluginStop XPluginEnable XPluginDisable XPluginReceiveMessage)
    if(APPLE)
        foreach(_sym IN LISTS _zb_entry_points)
            target_link_options(zoal_briefing PRIVATE "LINKER:-exported_symbol,_${_sym}")
        endforeach()
    elseif(UNIX)
        target_link_options(zoal_briefing PRIVATE
            "LINKER:--version-script=${CMAKE_CURRENT_SOURCE_DIR}/cmake/plugin_exports.map")
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
    zb_add_test(state_machine_tests test/state_machine_test.cpp)
    zb_add_test(audio_config_tests test/audio_config_test.cpp)
    zb_add_test(endpointer_tests test/endpointer_test.cpp)
    zb_add_test(voice_loop_tests test/voice_loop_test.cpp)
    zb_add_test(sentences_tests test/sentences_test.cpp)
    zb_add_test(voice_pool_tests test/voice_pool_test.cpp)
    zb_add_test(simbrief_tests test/simbrief_test.cpp)
    target_compile_definitions(simbrief_tests PRIVATE
        ZB_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/fixtures")
    zb_add_test(briefing_generator_tests test/briefing_generator_test.cpp)
    target_compile_definitions(briefing_generator_tests PRIVATE
        ZB_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/fixtures")
    zb_add_test(briefing_responder_tests test/briefing_responder_test.cpp)
    target_compile_definitions(briefing_responder_tests PRIVATE
        ZB_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/fixtures")

    # The real engines against the real models: only with the audio pipeline
    # built and the models downloaded (scripts/ensure-models.sh).
    if(ZoalBriefing_WITH_AUDIO AND EXISTS "${ZoalBriefing_MODELS_DIR}/openwakeword/melspectrogram.onnx")
        add_test(NAME voice_selftest COMMAND voice_check selftest "${ZoalBriefing_MODELS_DIR}")
    endif()

    message(STATUS "Unit tests enabled")
endif()

message(STATUS "zoal-briefing v${PROJECT_VERSION} configured (tests=${ZoalBriefing_BUILD_TESTS}, plugin=${_zb_sdk_present})")
