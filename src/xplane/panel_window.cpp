#include "panel_window.hpp"

// The panel uses Dear ImGui, which X-Plane renders through its native
// panel-graphics helpers (XPLMPanelGraphics). We need the window wrapper that
// bridges X-Plane's callbacks into an ImGui context.

#include "imgui_window.hpp"

#include <XPLMDisplay.h>
#include <XPLMMenus.h>
#include <XPLMPlugin.h>
#include <XPLMUtilities.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>

#include "imgui.h"
#include "zoal_briefing/gui/panel_view.hpp"
#include "zoal_briefing/gui/panel_session.hpp"

namespace zoal_briefing::xplane {

namespace {

constexpr int kOpenWidth = 1120;
constexpr int kOpenHeight = 740;
constexpr int kMinWidth = 760;
constexpr int kMinHeight = 320;

// Per-panel state: session, view state, window.
struct Panel {
  std::unique_ptr<ImguiWindow> window;
  gui::PanelSession session;
  gui::PanelViewState view_state;
  std::string version;
  bool initialized = false;
};

std::unique_ptr<Panel> g_panel;

void panel_log(const std::string &message) {
  XPLMDebugString(("[zoal-briefing] " + message + "\n").c_str());
}

void setup_view_state() {
  if (!g_panel || !g_panel->window) return;
  
  // Load settings into the view state from disk on first frame.
  if (!g_panel->session.settings_loaded()) {
    g_panel->session.load_settings();
  }
  
  // Sync stored copies to panel_view's dirty-checking.
  strncpy(g_panel->view_state.wake_word, 
          g_panel->session.wake_word().c_str(), 
          sizeof(g_panel->view_state.wake_word));
  strncpy(g_panel->view_state.llm_endpoint,
          g_panel->session.llm_endpoint().c_str(),
          sizeof(g_panel->view_state.llm_endpoint));
  strncpy(g_panel->view_state.model_name,
          g_panel->session.model_name().c_str(),
          sizeof(g_panel->view_state.model_name));
  
  g_panel->view_state.wake_word_stored = g_panel->session.wake_word();
  g_panel->view_state.llm_endpoint_stored = g_panel->session.llm_endpoint();
  g_panel->view_state.model_name_stored = g_panel->session.model_name();
}

void build_panel() {
  if (!g_panel || !g_panel->window) return;
  
  // Ensure settings are loaded.
  setup_view_state();
  
  gui::PanelFrame frame;
  frame.version = g_panel->version;
  frame.steady_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  frame.wall_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();

  // Draw the panel.
  gui::draw_panel(g_panel->session, g_panel->view_state, frame);
}

// Centred on the main screen the first time it opens.
void place_initially() {
  int left = 0, top2 = 0, right = 0, bottom = 0;
  XPLMGetScreenBoundsGlobal(&left, &top2, &right, &bottom);
  const int screen_w = right - left;
  const int screen_h = top2 - bottom;
  const int width = std::max(kMinWidth, 
                            std::min(kOpenWidth, screen_w - 80));
  const int height = std::max(kMinHeight, 
                             std::min(kOpenHeight, screen_h - 120));
  const int x = left + (screen_w - width) / 2;
  const int y = top2 - (screen_h - height) / 2;
  if (g_panel && g_panel->window) {
    g_panel->window->set_geometry(x, y, x + width, y - height);
  }
}

void set_shown(bool shown) {
  if (!g_panel || !g_panel->window) return;
  if (shown) place_initially();
  g_panel->window->set_visible(shown);
  if (shown) g_panel->window->bring_to_front();
}

int toggle_handler(XPLMCommandRef, XPLMCommandPhase phase, void *) {
  if (phase == xplm_CommandBegin && g_panel) {
    set_shown(!g_panel->window->visible());
  }
  return 1;
}

void install_menu() {
  XPLMMenuID plugins = XPLMFindPluginsMenu();
  if (!plugins) {
    panel_log("no plugins menu found");
    return;
  }
  const int index = XPLMAppendMenuItem(plugins, "zoal-briefing", nullptr, 1);
  if (index < 0) {
    panel_log("could not append the zoal-briefing menu");
    return;
  }
  XPLMMenuID menu = XPLMCreateMenu("zoal-briefing", plugins, index, 
                                   nullptr, nullptr);
  if (!menu) {
    panel_log("could not create the zoal-briefing menu");
    return;
  }
  panel_log("plugins menu installed");
}

std::uint64_t steady_now() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch()).count();
}

} // namespace

void start_panel(const PanelConfig &config) {
  if (g_panel || config.plugin_version.empty()) return;

  g_panel = std::make_unique<Panel>();
  g_panel->version = config.plugin_version;

  ImguiWindow::Options options;
  options.keyboard = true;
  options.resizable = true;
  options.min_width = kMinWidth;
  options.min_height = kMinHeight;

  int left = 0, top2 = 0, right = 0, bottom = 0;
  XPLMGetScreenBoundsGlobal(&left, &top2, &right, &bottom);
  int screen_w = right - left;
  int screen_h = top2 - bottom;
  int width = std::min(kOpenWidth, 
                      std::max(screen_w - 80, kMinWidth));
  int height = std::min(kOpenHeight,
                       std::max(screen_h - 120, kMinHeight));
  int x = left + (screen_w - width) / 2;
  int y = top2 - (screen_h - height) / 2;

  g_panel->window = std::make_unique<ImguiWindow>(
      x, y, x + width, y - height, build_panel, options);

  if (!g_panel->window->valid()) {
    panel_log("X-Plane did not create the panel window");
    g_panel.reset();
    return;
  }

  // Set up ImGui context for initial style.
  {
    ImGuiContext *previous = ImGui::GetCurrentContext();
    if (!previous) {
      // The window creation should have already set up the context.
    } else {
      ImGui::SetCurrentContext(previous);
    }
  }

  // Create a default PanelSession instance.
  
  install_menu();
  
  panel_log("panel started");
}

void stop_panel() {
  if (!g_panel) return;
  
  // Save any pending settings before destroying.
  g_panel->session.save_settings();
  g_panel.reset();
}

void tick_panel() {
  if (!g_panel || !g_panel->window) return;
}

} // namespace zoal_briefing::xplane
