#ifndef ZOAL_BRIEFING_LOG_HPP
#define ZOAL_BRIEFING_LOG_HPP

#include <functional>
#include <string>

namespace zoal_briefing::log {

// One logger for the whole plugin, SDK-free so the core can use it from any
// thread. Lines look like "[zoal-briefing] [state] IDLE -> WAKE_DETECTED: ...".
//
// The sink decides where a line goes. The default writes to stderr (tests and
// the dev tool). The plugin installs a sink that queues lines for the main
// thread, because XPLMDebugString must not be called from worker threads.
using Sink = std::function<void(const std::string &line)>;

void set_sink(Sink sink);
void reset_sink();

void info(const std::string &component, const std::string &message);
void warn(const std::string &component, const std::string &message);
void error(const std::string &component, const std::string &message);

} // namespace zoal_briefing::log

#endif // ZOAL_BRIEFING_LOG_HPP
