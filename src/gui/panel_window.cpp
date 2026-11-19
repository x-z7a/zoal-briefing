#include "gui/panel_window.hpp"

// The panel uses Dear ImGui, which X-Plane renders through its native
// panel-graphics helpers (XPLMPanelGraphics). ImguiWindow bridges X-Plane's
// callbacks into an ImGui context.

#include "gui/imgui_window.hpp"

#include <XPLMDataAccess.h>
#include <XPLMDisplay.h>
#include <XPLMMenus.h>
#include <XPLMPlugin.h>
#include <XPLMUtilities.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "imgui.h"
#include "imgui_freetype.h"
#include "zoal_briefing/gui/panel_session.hpp"
#include "zoal_briefing/gui/panel_view.hpp"
#include "zoal_briefing/log.hpp"

namespace zoal_briefing::gui {

namespace {

constexpr const char *kPanelCommandName = "zoal_briefing/toggle_panel";

constexpr int kOpenWidth = 720;
constexpr int kOpenHeight = 520;
constexpr int kMinWidth = 520;
constexpr int kMinHeight = 360;
// CSS sizes a font by its em, ImGui by its whole line: Inter's line is 1.21
// ems (Roboto's 1.17), so this draws what CSS's 14px does -- as zoal-atc.
constexpr float kFontSize = 14.0f * 1.21f;

struct Panel {
  std::unique_ptr<ImguiWindow> window;
  gui::PanelSession session;
  gui::PanelViewState view_state;
  std::string version;
  ImFont *title_font = nullptr;
  // Presses from the last frame, run from the flight loop.
  std::vector<std::function<void()>> later;
  gui::PanelViewResult last;
  // Where the drag started: the mouse and the window, in global boxels.
  bool dragging = false;
  int drag_mouse_x = 0;
  int drag_mouse_y = 0;
  int drag_left = 0;
  int drag_top = 0;
  bool placed = false;
  XPLMCommandRef command = nullptr;
  XPLMMenuID menu = nullptr;
  XPLMDataRef ui_scale = nullptr;
};

std::unique_ptr<Panel> g_panel;

std::uint64_t steady_now() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

std::int64_t wall_now() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

void copy_field(char *dst, std::size_t size, const std::string &src) {
  std::strncpy(dst, src.c_str(), size - 1);
  dst[size - 1] = '\0';
}

// Settings come off disk once, into both the session and the text fields.
void load_view_state() {
  Panel &panel = *g_panel;
  if (panel.view_state.settings_loaded) return;
  if (!panel.session.settings_loaded()) {
    panel.session.load_settings();
  }
  copy_field(panel.view_state.wake_word, sizeof(panel.view_state.wake_word),
             panel.session.wake_word());
  copy_field(panel.view_state.llm_endpoint,
             sizeof(panel.view_state.llm_endpoint),
             panel.session.llm_endpoint());
  copy_field(panel.view_state.model_name, sizeof(panel.view_state.model_name),
             panel.session.model_name());
  panel.view_state.wake_word_stored = panel.session.wake_word();
  panel.view_state.llm_endpoint_stored = panel.session.llm_endpoint();
  panel.view_state.model_name_stored = panel.session.model_name();
  panel.view_state.settings_loaded = true;
}

void build_panel() {
  if (!g_panel) return;
  gui::PanelFrame frame;
  frame.version = g_panel->version;
  frame.steady_ms = steady_now();
  frame.wall_ms = wall_now();
  frame.later = &g_panel->later;
  frame.title_font = g_panel->title_font;
  g_panel->last = gui::draw_panel(g_panel->session, g_panel->view_state, frame);
}

// Centred on the main screen the first time it opens; where the pilot left it
// after that.
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
  g_panel->placed = true;
}

void set_shown(bool shown) {
  if (!g_panel || !g_panel->window) return;
  if (shown && !g_panel->placed) place_initially();
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
    log::warn("panel", "no plugins menu; the panel is reachable by command only");
    return;
  }
  const int index = XPLMAppendMenuItem(plugins, "zoal-briefing", nullptr, 1);
  if (index < 0) {
    log::warn("panel", "could not append the zoal-briefing menu");
    return;
  }
  g_panel->menu =
      XPLMCreateMenu("zoal-briefing", plugins, index, nullptr, nullptr);
  if (!g_panel->menu) {
    log::warn("panel", "could not create the zoal-briefing menu");
    return;
  }
  XPLMAppendMenuItemWithCommand(g_panel->menu, "Toggle Panel", g_panel->command);
  // The plugin creates these before the panel starts.
  if (XPLMCommandRef listen = XPLMFindCommand("zoal_briefing/listen")) {
    XPLMAppendMenuItemWithCommand(g_panel->menu, "Listen Now", listen);
  }
  if (XPLMCommandRef cancel = XPLMFindCommand("zoal_briefing/cancel")) {
    XPLMAppendMenuItemWithCommand(g_panel->menu, "Cancel", cancel);
  }
}

// The plugin's own folder: <plugins>/zoal-briefing/<platform>/zoal-briefing.xpl.
std::filesystem::path plugin_dir() {
  char binary[1024] = {};
  XPLMGetPluginInfo(XPLMGetMyID(), nullptr, binary, nullptr, nullptr);
  return std::filesystem::path(binary).parent_path().parent_path();
}

std::filesystem::path xplane_fonts_dir() {
  char system_path[1024] = {};
  XPLMGetSystemPath(system_path);
  return std::filesystem::path(system_path) / "Resources" / "fonts";
}

// Inter, shipped in the plugin's fonts/ (the face zoal-atc's panel is set in),
// rasterized by FreeType with light hinting. Roboto, X-Plane's own UI face, is
// on every install; it has no semibold, so bold stands in.
void load_fonts() {
  ImGuiIO &io = ImGui::GetIO();
  // Light hinting snaps stems and baselines to whole pixels vertically, so
  // small text is crisp, while letter spacing keeps the font's own widths.
  io.Fonts->FontLoaderFlags = ImGuiFreeTypeLoaderFlags_LightHinting;

  const std::filesystem::path inter = plugin_dir() / "fonts";
  const std::filesystem::path roboto = xplane_fonts_dir();
  std::error_code ec;
  std::filesystem::path regular = inter / "Inter-Regular.ttf";
  std::filesystem::path semibold = inter / "Inter-SemiBold.ttf";
  if (!std::filesystem::exists(regular, ec)) {
    log::warn("panel", "no Inter in " + inter.string() + "; using X-Plane's Roboto");
    regular = roboto / "Roboto-Regular.ttf";
    semibold = roboto / "Roboto-Bold.ttf";
  }

  // The first font added is the default. The size is only that: ImGui 1.92
  // rasterizes each size asked for.
  ImFont *body = nullptr;
  if (std::filesystem::exists(regular, ec)) {
    body = io.Fonts->AddFontFromFileTTF(regular.string().c_str(), kFontSize);
  }
  if (!body) {
    log::warn("panel", "could not load " + regular.string() + "; using ImGui's built-in font");
    io.Fonts->AddFontDefault();
  }
  if (std::filesystem::exists(semibold, ec)) {
    g_panel->title_font =
        io.Fonts->AddFontFromFileTTF(semibold.string().c_str(), kFontSize);
  }
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
  g_panel->window = std::make_unique<ImguiWindow>(0, kOpenHeight, kOpenWidth, 0,
                                                  build_panel, options);
  if (!g_panel->window->valid()) {
    log::warn("panel", "X-Plane did not create the panel window (it needs X-Plane "
              "12.4.4 or newer)");
    g_panel.reset();
    return;
  }
  {
    ImGuiContext *previous = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(g_panel->window->context());
    load_fonts();
    ImGui::GetStyle().FontSizeBase = kFontSize;
    ImGui::SetCurrentContext(previous);
  }
  g_panel->ui_scale = XPLMFindDataRef("sim/graphics/misc/user_interface_scale");

  load_view_state();

  g_panel->command = XPLMCreateCommand(kPanelCommandName,
                                       "zoal-briefing: Toggle the briefing panel");
  XPLMRegisterCommandHandler(g_panel->command, toggle_handler, 1, nullptr);
  install_menu();
  log::info("panel", "ready; open it with Plugins > zoal-briefing > Toggle Panel or bind " +
            std::string(kPanelCommandName));
}

void stop_panel() {
  if (!g_panel) return;
  if (g_panel->menu) {
    XPLMDestroyMenu(g_panel->menu);
  }
  if (g_panel->command) {
    XPLMUnregisterCommandHandler(g_panel->command, toggle_handler, 1, nullptr);
  }
  g_panel->later.clear();
  g_panel->session.save_settings();
  g_panel->window.reset();
  g_panel.reset();
}

gui::PanelSession *panel_session() { return g_panel ? &g_panel->session : nullptr; }

void tick_panel() {
  if (!g_panel || !g_panel->window) return;

  // What was pressed, outside the draw callback that saw it.
  std::vector<std::function<void()>> later;
  later.swap(g_panel->later);
  for (const auto &action : later) {
    action();
  }

  const gui::PanelViewResult last = std::exchange(g_panel->last, {});
  if (last.close) {
    set_shown(false);
  }

  // The window follows the mouse in global boxels while the background is
  // held, since the window moving under it would make local coordinates useless.
  if (g_panel->window->visible() && (last.drag_started || last.dragging)) {
    int mx = 0, my = 0;
    XPLMGetMouseLocationGlobal(&mx, &my);
    int left = 0, top = 0, right = 0, bottom = 0;
    g_panel->window->geometry(&left, &top, &right, &bottom);
    if (last.drag_started || !g_panel->dragging) {
      g_panel->dragging = true;
      g_panel->drag_mouse_x = mx;
      g_panel->drag_mouse_y = my;
      g_panel->drag_left = left;
      g_panel->drag_top = top;
    } else {
      const int x = g_panel->drag_left + (mx - g_panel->drag_mouse_x);
      const int y = g_panel->drag_top + (my - g_panel->drag_mouse_y);
      g_panel->window->set_geometry(x, y, x + (right - left), y - (top - bottom));
    }
  } else {
    g_panel->dragging = false;
  }

  // Text is rasterized for the screen's real density: twice the UI scale keeps
  // it sharp on a Retina screen and a scaled one alike.
  float scale = 1.0f;
  if (g_panel->ui_scale) {
    scale = std::max(1.0f, XPLMGetDataf(g_panel->ui_scale));
  }
  g_panel->window->set_framebuffer_scale(2.0f * scale);
}

} // namespace zoal_briefing::gui
