#ifndef ZOAL_BRIEFING_XPLANE_LOGGER_H
#define ZOAL_BRIEFING_XPLANE_LOGGER_H

// <iostream> is needed for the SDK-free fallback path.
// Placing it at file scope avoids the "std not found" namespace issue.
#ifdef ZOAL_BRIEFING_USE_XPLANE_SDK
#include <XPLMUtilities.h>
#else
#include <iostream>
#endif

namespace zoal_briefing::xplane {

static constexpr const char* kLogPrefix = "[zoal-briefing] ";

// Lightweight logger: X-Plane debug console in the plugin, stderr in test builds.
inline void xplane_debug_log(const char* msg) {
#ifdef ZOAL_BRIEFING_USE_XPLANE_SDK
    XPLMDebugString(kLogPrefix);
    XPLMDebugString(msg);
    XPLMDebugString("\n");
#else
    std::cerr << kLogPrefix << msg << std::endl;
#endif
}

} // namespace zoal_briefing::xplane

#endif // ZOAL_BRIEFING_XPLANE_LOGGER_H
