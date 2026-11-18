# Locate X-Plane SDK headers.
find_path(XPLANE_SDK_INCLUDE_DIR
    NAMES XPLMPlugin.h XPLMDataAccess.h
    PATHS ${ATC_XPLANE_SDK}/include
          ${ATC_XPLANE_SDK}
          /opt/X-Plane/SDK/include
          ~/Library/Application Support/X-Plane/SDK/include
          "$ENV{XPLANE_SDK_ROOT}/include"
    NO_DEFAULT_PATH
)

if(NOT XPLANE_SDK_INCLUDE_DIR)
    message(WARNING "X-Plane SDK not found. Set ATC_XPLANE_SDK or XPLANE_SDK_ROOT.")
else()
    message(STATUS "Found X-Plane SDK at: ${XPLANE_SDK_INCLUDE_DIR}")
endif()
