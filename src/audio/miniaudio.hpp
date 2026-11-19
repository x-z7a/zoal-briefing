#ifndef ZOAL_BRIEFING_AUDIO_MINIAUDIO_HPP
#define ZOAL_BRIEFING_AUDIO_MINIAUDIO_HPP

// miniaudio, configured once for everything that opens a device (input/mic,
// output/speaker). Devices only: no decoding, encoding or engine. The
// implementation is compiled once, in miniaudio.cpp.

#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#include <miniaudio.h>

#endif
