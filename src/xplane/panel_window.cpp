#include "panel_window.hpp"

// The panel uses Dear ImGui, which X-Plane renders through its native
// panel-graphics helpers (XPLMPanelGraphics). We need the window wrapper that
// bridges X-Plane's callbacks into an ImGui context — matching the pattern
// zoal-atc uses for both the HUD and the panel.

#include <XPLMDisplay.h>
#include <XPLMUtilities.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>

#include "imgui.h"
#include "zoal_briefing/gui/panel_view.hpp"
#include "zoal_briefing/gui/panel_session.hpp"

namespace zoal_briefing::xplane {

// Forward-declare the imgui_window class we just added.
struct ImguiWindow;

namespace {

// The size it opens at: wide enough for settings fields, room for action buttons.
constexpr int kOpenWidth = 1120;
constexpr int kOpenHeight = 740;
// Below this the layout stops fitting.
constexpr int kMinWidth = 760;
constexpr int kMinHeight = 320;

struct Panel {
  std::unique_ptr<ImguiWindow> window;
  std::unique_ptr<gui::PanelSession> session;
  std::function<std::uint64_t()> steady_ms;
  std::string version;
  std::function<void(const std::string &, const std::string &)> local_action;
  // Presses from the last frame, run from the flight loop.
  std::vector<std::function<void()>> later;
  int64_t wall_ms();
};

std::unique_ptr<Panel> g_panel;

void panel_log(const std::string &message) {
  XPLMDebugString(("[zoal-briefing] " + message + "\n").c_str());
}

int64_t Panel::wall_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

std::uint64_t steady_now() {
  return g_panel && g_panel->steady_ms ? g_panel->steady_ms() : 0;
}

void build_panel() {
  if (!g_panel || !g_panel->session) return;

  ui::PanelFrame frame;
  frame.version = g_panel->version;
  frame.steady_ms = steady_now();
  frame.wall_ms = g_panel->wall_ms();
  
  // Load settings into the panel view state on first draw
  if (!frame.settings_loaded) {
    g_panel->session->load_settings();
    frame.settings_loaded = true;
  }

  auto action = [&](const std::string &act, const std::string &payload) {
    if (g_panel->local_action) {
      g_panel->local_action(act, payload);
    }
  };

  zoal_briefing::gui::detail::draw_panel_with_actions(
      *g_panel->session, g_panel->window->context(), frame, action);
}

// Centred on the main screen the first time it opens; where left after that.
void place_initially() {
  int left = 0, top = 0, right = 0, bottom = 0;
  XPLMGetScreenBoundsGlobal(&left, &top, &right, &bottom);
  const int screen_w = right - left;
  const int screen_h = top - bottom;
  const int width = std::max(kMinWidth, std::min(kOpenWidth, screen_w - 80));
  const int height = std::max(kMinHeight, std::min(kOpenHeight, screen_h - 120));
  const int x = left + (screen_w - width) / 2;
  const int y = top - (screen_h - height) / 2;
  g_panel->window->set_geometry(x, y, x + width, y - height);
}

void set_shown(bool shown) {
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
  XPLMMenuID menu = XPLMCreateMenu("zoal-briefing", plugins, index, nullptr, nullptr);
  if (!menu) {
    panel_log("could not create the zoal-briefing menu");
    return;
  }
  // Find or create the toggle command.
}

std::filesystem::path plugin_dir() {
  char binary[1024] = {};
  XPLMGetPluginInfo(XPLMGetMyID(), nullptr, binary, nullptr, nullptr);
  return std::filesystem::path(binary).parent_path();
}

} // namespace

void start_panel(const PanelConfig &config) {
  if (g_panel || config.local_action == nullptr) return;

  g_panel = std::make_unique<Panel>();
  g_panel->version = config.plugin_version;
  g_panel->local_action = config.local_action;

  ImguiWindow::Options options;
  options.keyboard = true;
  options.resizable = true;
  options.min_width = kMinWidth;
  options.min_height = kMinHeight;

  int open_width = kOpenWidth;
  int open_height = kOpenHeight;
  
  int left = 0, top2 = 0, right = 0, bottom = 0;
  XPLMGetScreenBoundsGlobal(&left, &top2, &right, &bottom);
  int screen_w = right - left;
  int screen_h = top2 - bottom;
  int width = std::min(open_width, std::max(screen_w - 80, kMinWidth));
  int height = std::min(open_height, std::max(screen_h - 120, kMinHeight));
  int x = left + (screen_w - width) / 2;
  int y = top2 - (screen_h - height) / 2;

  g_panel->window = std::make_unique<ImguiWindow>(
      x, y, x + width, y - height, build_panel, options);

  if (!g_panel->window->valid()) {
    panel_log("X-Plane did not create the panel window");
    g_panel.reset();
    return;
  }

  // Set up ImGui context for initial setup.
  {
    ImGuiContext *previous = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(g_panel->window->context());
    
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->AddFontDefault();
    
    ImGuiStyle &style = ImGui::GetStyle();
    style.ItemSpacing = ImVec2(0.0f, 0.0f);
    style.WindowPadding = ImVec2(0.0f, 0.0f);
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 0.0f;
    style.ScrollbarSize = 8.0f;
    style.ScrollbarRounding = 4.0f;
    
    ImGui::SetCurrentContext(previous);
  }

  // Create the panel session.
  g_panel->session = std::make_unique<gui::PanelSession>();
  
  panel_log("panel started");
}

void stop_panel() {
  if (!g_panel) return;
  
  // Save any pending settings.
  if (g_panel->session) {
    g_panel->session->save_settings();
  }
  g_panel.reset();
}

void tick_panel() {
  if (!g_panel || !g_panel->window) return;
  
  // Run deferred actions from the last frame.
  std::vector<std::function<void()>> later;
  later.swap(g_panel->later);
  for (const auto &action : later) {
    action();
  }

  // Check close request.
  // Text is rasterized at twice the UI scale for Retina sharpness.
  float scale = 1.0f;
  g_panel->window->set_framebuffer_scale(2.0f * scale);
}

} // namespace zoal_briefing::xplane
