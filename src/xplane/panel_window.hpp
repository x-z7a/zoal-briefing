#ifndef ZOAL_BRIEFING_XPLANE_PANEL_WINDOW_HPP
#define ZOAL_BRIEFING_XPLANE_PANEL_WINDOW_HPP

#include "zoal_briefing/gui/panel_session.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

// Forward declaration.
struct ImguiWindow;
struct ImGuiContext;

namespace zoal_briefing::xplane {

struct PanelConfig {
  std::string plugin_version;
  std::function<void(const std::string &action, const std::string &payload)> local_action;
};

void start_panel(const PanelConfig &config);
void stop_panel();
void tick_panel();

} // namespace zoal_briefing::xplane

#endif
