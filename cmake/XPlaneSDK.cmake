# X-Plane SDK — download from official X-Plane GitHub releases.
# Uses release ZIP archives (no git clone): avoids auth issues in CI.
# Mirrors the pattern: sentinel file, version pinning, CMake-native
# download/extract rather than git operations.

if(NOT DEFINED ZOAL_BRIEFING_XPLANE_SDK OR ZOAL_BRIEFING_XPLANE_SDK STREQUAL "")
    set(ZOAL_BRIEFING_XPLANE_SDK "${CMAKE_CURRENT_SOURCE_DIR}/sdk")
endif()

get_filename_component(_zb_sdk_dir "${ZOAL_BRIEFING_XPLANE_SDK}" ABSOLUTE)

# Version pin — update this when X-Plane releases a new SDK.
# The tag is v4.4.0-b5 on the X-Plane GitHub repo:
#   https://github.com/x-plane/X-Plane-SDK/releases/tag/v4.4.0-b5
set(ZOAL_BRIEFING_SDK_VERSION "4.4.0-b5" CACHE STRING "X-Plane SDK version to download")
set(ZOAL_BRIEFING_SDK_URL
    "https://github.com/x-plane/X-Plane-SDK/archive/refs/tags/v${ZOAL_BRIEFING_SDK_VERSION}.zip"
    CACHE STRING "SDK download URL (release ZIP from X-Plane GitHub)")

# Sentinel header that only exists in SDK 4.4.0+ (panel-graphics ImGui helpers)
set(_zb_sentinel "${_zb_sdk_dir}/XPLM/XPLMPanelGraphics.h")

if(EXISTS "${_zb_sentinel}")
    message(STATUS "X-Plane SDK v${ZOAL_BRIEFING_SDK_VERSION} already present at ${_zb_sdk_dir}")
else()
    message(STATUS "Downloading X-Plane SDK ${ZOAL_BRIEFING_SDK_VERSION} from GitHub releases")

    set(_zb_tmp "${CMAKE_BINARY_DIR}/_zb_xpsdk")
    set(_zb_zip  "${_zb_tmp}/xpsdk.zip")
    set(_zb_src  "${_zb_tmp}/src")

    file(REMOVE_RECURSE "${_zb_tmp}")
    file(MAKE_DIRECTORY "${_zb_tmp}" "${_zb_src}")

    file(DOWNLOAD
        "${ZOAL_BRIEFING_SDK_URL}"
        "${_zb_zip}"
        STATUS _zb_dl_status
        SHOW_PROGRESS
        TLS_VERIFY ON
    )

    list(GET _zb_dl_status 0 _zb_code)
    list(GET _zb_dl_status 1 _zb_msg)
    if(NOT _zb_code EQUAL 0)
        message(FATAL_ERROR "X-Plane SDK download failed: ${_zb_msg}")
    endif()

    file(ARCHIVE_EXTRACT INPUT "${_zb_zip}" DESTINATION "${_zb_src}")

    # Flatten the archive: the zip contains a top-level directory named
    # X-Plane-SDK-4.4.0-b5/ (from the tag), containing CHeaders/, Libraries/, etc.
    # We need to copy that contents up into sdk/ so the SDK has both the
    # legacy structure (CHeaders/, Libraries/) and the flattened XPLM/.
    file(MAKE_DIRECTORY "${_zb_src}/XPLM" "${_zb_src}/XPWidgets")
    file(MAKE_DIRECTORY
        "${_zb_sdk_dir}/XPLM"
        "${_zb_sdk_dir}/XPWidgets"
        "${_zb_sdk_dir}/Libraries/Win"
        "${_zb_sdk_dir}/Libraries/Mac"
        "${_zb_sdk_dir}/Libraries/Lin"
    )

    # Flatten XPLM headers from nested CHeaders/XPLM/ to flat XPLM/
    file(GLOB_RECURSE _zb_xplm_headers
        LIST_DIRECTORIES false
        "${_zb_src}/*/CHeaders/XPLM/*.h"
    )
    foreach(_zb_file IN LISTS _zb_xplm_headers)
        file(COPY "${_zb_file}" DESTINATION "${_zb_sdk_dir}/XPLM")
    endforeach()

    # Flatten XPWidgets headers
    file(GLOB_RECURSE _zb_widget_headers
        LIST_DIRECTORIES false
        "${_zb_src}/*/CHeaders/Widgets/*.h"
    )
    foreach(_zb_file IN LISTS _zb_widget_headers)
        file(COPY "${_zb_file}" DESTINATION "${_zb_sdk_dir}/XPWidgets")
    endforeach()

    # Flatten Windows libraries
    file(GLOB_RECURSE _zb_win_libs
        LIST_DIRECTORIES false
        "${_zb_src}/*/Libraries/Win/*.lib"
    )
    foreach(_zb_file IN LISTS _zb_win_libs)
        file(COPY "${_zb_file}" DESTINATION "${_zb_sdk_dir}/Libraries/Win")
    endforeach()

    # Flatten Linux libraries
    file(GLOB_RECURSE _zb_lin_libs
        LIST_DIRECTORIES false
        "${_zb_src}/*/Libraries/Lin/*.so"
    )
    foreach(_zb_file IN LISTS _zb_lin_libs)
        file(COPY "${_zb_file}" DESTINATION "${_zb_sdk_dir}/Libraries/Lin")
    endforeach()

    # Flatten Mac frameworks
    file(GLOB _zb_mac_fws
        LIST_DIRECTORIES true
        "${_zb_src}/*/Libraries/Mac/*.framework"
    )
    foreach(_zb_fw IN LISTS _zb_mac_fws)
        if(IS_DIRECTORY "${_zb_fw}")
            file(COPY "${_zb_fw}" DESTINATION "${_zb_sdk_dir}/Libraries/Mac")
        endif()
    endforeach()

    message(STATUS "X-Plane SDK installed at ${_zb_sdk_dir}")
endif()
