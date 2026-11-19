#ifndef ZOAL_BRIEFING_GUI_PANEL_WINDOW_HPP
#define ZOAL_BRIEFING_GUI_PANEL_WINDOW_HPP

#include "zoal_briefing/gui/panel_session.hpp"

#include <string>

namespace zoal_briefing::gui {

// The briefing panel: one ImguiWindow (imgui_window.hpp, the generic X-Plane
// <-> Dear ImGui bridge) drawn natively by X-Plane through its panel-graphics
// helpers, so a click on it never makes it X-Plane's "active browser" (the
// state that blanks other plugins' UIs).
//
// This layer owns the window, its toggle command and menu, fonts, dragging and
// scaling. What the panel shows is gui::draw_panel() (src/gui/panel_view.cpp),
// a view of gui::PanelSession with no X-Plane in it.

struct PanelConfig {
  std::string plugin_version;
};

// Creates the window (hidden) and installs the toggle command and menu.
void start_panel(const PanelConfig &config);
void stop_panel();

// Call once per flight loop: runs the presses from the last frame and moves a
// dragged panel.
void tick_panel();

// The panel's session, for the plugin to show state in; null when no panel.
gui::PanelSession *panel_session();

} // namespace zoal_briefing::gui

#endif
