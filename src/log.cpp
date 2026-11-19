#include "zoal_briefing/log.hpp"

#include <iostream>
#include <mutex>

namespace zoal_briefing::log {

namespace {

std::mutex &sink_mutex() {
  static std::mutex m;
  return m;
}

Sink &sink() {
  static Sink s;
  return s;
}

void write(const char *level, const std::string &component, const std::string &message) {
  std::string line = "[zoal-briefing] ";
  if (level[0] != '\0') {
    line += level;
    line += ' ';
  }
  line += '[' + component + "] " + message;

  std::lock_guard<std::mutex> lock(sink_mutex());
  if (sink()) {
    sink()(line);
  } else {
    std::cerr << line << std::endl;
  }
}

} // namespace

void set_sink(Sink s) {
  std::lock_guard<std::mutex> lock(sink_mutex());
  sink() = std::move(s);
}

void reset_sink() { set_sink(nullptr); }

void info(const std::string &component, const std::string &message) {
  write("", component, message);
}

void warn(const std::string &component, const std::string &message) {
  write("WARN", component, message);
}

void error(const std::string &component, const std::string &message) {
  write("ERROR", component, message);
}

} // namespace zoal_briefing::log
