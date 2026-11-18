#include "zoal_briefing/xplane/panel_window.hpp"
#include "zoal_briefing/gui/panel_session.hpp"
#include "zoal_briefing/gui/panel_view.hpp"
#include "zoal_briefing/gui/panel_theme.hpp"

// X-Plane SDK forward declarations -- actual types from XPLM*.h
// struct ImGuiWindow; // placeholder for X-Plane's panel graphics system

#include <memory>
#include <string>
#include <utility>

namespace zoal_briefing::xplane {

struct PanelWindow::Impl {
  bool visible = false;
  
  // Plugin session model.
  PanelSession session_;
  
  // View state (what the pilot typed, which tab is shown).
  PanelViewState view_state;
  
  // The current theme.
  std::string theme_name = "graphite";
  
  // X-Plane window handle (opaque pointer for now).
  void *window_handle = nullptr;
  
  // Command for toggling panel visibility.
  void *command_ref = nullptr;
};

PanelWindow::~PanelWindow() {}

void PanelWindow::show(const PanelSession &session, const Options &options) {
  (void)session; (void)options;
  // In a real implementation:
  // - Create X-Plane panel window via XPLMCreatePanel
  // - Initialize ImGui context with XPLM panel graphics API
  // - Load typeface (Inter/Roboto) 
  // - Set up event handlers for pilot interactions
  impl_->visible = true;
  
  // Create command to toggle panel.
  // g_panel_command = XPLMCreateCommand("zoal_briefing/show_panel", "Show briefing panel");
}

void PanelWindow::hide() {
  impl_->visible = false;
  // In a real implementation: destroy the window handle, clean up ImGui context
}

bool PanelWindow::visible() const {
  return impl_->visible;
}

void PanelWindow::on_action(const std::string &action, const std::string &payload) {
  (void)action; (void)payload;
  // Handle panel actions:
  // - "save_settings" -> update zoal-briefing.json
  // - "open_panel" -> show the panel  
  // - etc.
}

void tick_panel() {
  // Called from X-Plane's flight loop. In a real implementation:
  // - Update session state from datarefs
  // - Draw the panel via ImGui + XPLM panel graphics
  // - Process any pending actions
}

} // namespace zoal_briefing::xplane
