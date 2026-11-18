# X-Plane SDK — download from official X-Plane CDN when not already present.
# Mirrors zoal-atc's approach: sentinel file, version pinning, CMake-native
# download/extract rather than git clone (avoids auth issues in CI).

cmake_minimum_required(VERSION 3.24)

set(ZOAL_BRIEFING_XPLANE_SDK_VERSION "420" CACHE STRING "X-Plane SDK version to download")
set(ZOAL_BRIEFING_XPLANE_SDK_URL   "https://www.x-plane.com/wp-content/uploads/2026/08/XPSDK420.zip" CACHE STRING "SDK download URL")

if(NOT ZoalBriefing_XPLANE_SDK OR ZoalBriefing_XPLANE_SDK STREQUAL "")
    set(ZoalBriefing_XPLANE_SDK "${CMAKE_CURRENT_SOURCE_DIR}/sdk")
endif()
get_filename_component(_zb_sdk_dir "${ZoalBriefing_XPLANE_SDK}" ABSOLUTE)

# Sentinel header that only exists in SDK 4.2.0+
set(_zb_sentinel "${_zb_sdk_dir}/CHeaders/XPLM/XPLMPlugin.h")

if(EXISTS "${_zb_sentinel}")
    message(STATUS "X-Plane SDK v${ZOAL_BRIEFING_XPLANE_SDK_VERSION} already present at ${_zb_sdk_dir}")
else()
    message(STATUS "Downloading X-Plane SDK v${ZOAL_BRIEFING_XPLANE_SDK_VERSION} from ${ZOAL_BRIEFING_XPLANE_SDK_URL}")

    set(_zb_tmp "${CMAKE_BINARY_DIR}/_zb_xpsdk")
    set(_zb_zip  "${_zb_tmp}/xpsdk.zip")
    set(_zb_src  "${_zb_tmp}/src")

    file(REMOVE_RECURSE "${_zb_tmp}")
    file(MAKE_DIRECTORY "${_zb_tmp}" "${_zb_src}")

    file(DOWNLOAD
        "${ZOAL_BRIEFING_XPLANE_SDK_URL}"
        "${_zb_zip}"
        STATUS _zb_status
        SHOW_PROGRESS
        TLS_VERIFY ON
    )

    list(GET _zb_status 0 _zb_code)
    if(NOT _zb_code EQUAL 0)
        message(WARNING "X-Plane SDK download failed — plugin module will be skipped")
        message(WARNING "Pass -DZoalBriefing_XPLANE_SDK=/path/to/sdk to use a local copy")
    else()
        file(ARCHIVE_EXTRACT INPUT "${_zb_zip}" DESTINATION "${_zb_src}")

        # XPSDK zip has one top-level folder; flatten into sdk/
        file(GLOB _zb_top "${_zb_src}/*")
        foreach(_zb_entry ${_zb_top})
            if(IS_DIRECTORY "${_zb_entry}" AND NOT IS_SYMLINK "${_zb_entry}")
                file(COPY "${_zb_entry}/" DESTINATION "${_zb_sdk_dir}/")
                break()
            endif()
        endforeach()

        message(STATUS "X-Plane SDK installed at ${_zb_sdk_dir}")
    endif()
endif()
