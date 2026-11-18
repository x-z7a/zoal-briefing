#ifndef ZOAL_BRIEFING_XPLANE_PANEL_WINDOW_HPP
#define ZOAL_BRIEFING_XPLANE_PANEL_WINDOW_HPP

#include "zoal_briefing/gui/panel_session.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <functional>

struct ImGuiContext;

namespace zoal_briefing::xplane {

// The briefing radio panel: a Dear ImGui window drawn natively by X-Plane
// (panel-graphics helpers), so a click on it never makes X-Plane's "active
// browser" — the state that blanks other plugins' UIs.
//
// It is a view of gui::PanelSession and nothing else: no X-Plane here in the
// view layer, so the same frame code can be tested without the SDK.

struct PanelConfig {
  std::string plugin_version;
};

// Start creates the window (hidden), installs the toggle command and menu.
void start_panel(const PanelConfig &config);
void stop_panel();

// Call once per flight loop: drains pressed actions from the last frame.
void tick_panel();

} // namespace zoal_briefing::xplane

#endif
