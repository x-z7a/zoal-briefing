#include "zoal_briefing/gui/panel_view.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include <functional>

// --- forward declarations for test builds (no real ImGui) ----------

struct ImVec2 { float x, y; };
struct ImDrawList {};
struct ImGuiStyle {};
namespace ImGui {
  inline void SetCursorScreenPos(const ImVec2&) {}
  inline bool InvisibleButton(const char*, const ImVec2&) { return false; }
  inline bool IsItemHovered() { return false; }
  inline void PushID(const char*) {}
  inline void PopID() {}
  inline void PopStyleVar(int) {}
}

// --- internal Palette struct & helpers -----------------------------

namespace zoal_briefing::gui {
namespace detail {

struct Palette {
  unsigned glass, surface, raised, surface_comm, glass_card, glass_well;
  unsigned border, border_strong;
  unsigned text, text_dim, text_feed;
  unsigned accent, on_accent, accent_container, on_accent_container;
  unsigned tonal, on_tonal, outline;
  unsigned active, warn, on_warn, warn_surface, danger;
  unsigned shadow;
};

unsigned rgba(unsigned hex, float alpha = 1.0f) {
  unsigned r = (hex >> 16) & 0xff;
  unsigned g = (hex >> 8) & 0xff;
  unsigned b = hex & 0xff;
  return (static_cast<unsigned>(alpha * 255) << 24) |
         (r << 16) | (g << 8) | b;
}

unsigned faded(unsigned a, float t) {
  unsigned r = ((a >> 16) & 0xff) * static_cast<unsigned>(t);
  unsigned g = ((a >> 8) & 0xff) * static_cast<unsigned>(t);
  unsigned b = (a & 0xff) * static_cast<unsigned>(t);
  return (255 - static_cast<unsigned>(t)) << 24 |
         r << 16 | g << 8 | b;
}

unsigned blend(unsigned a, unsigned b, float t) {
  unsigned ar = (a >> 16) & 0xff;
  unsigned ag = (a >> 8) & 0xff;
  unsigned ab = a & 0xff;
  unsigned br = (b >> 16) & 0xff;
  unsigned bg = (b >> 8) & 0xff;
  unsigned bb = b & 0xff;
  return ((255 - static_cast<unsigned>(t)) * ar + static_cast<unsigned>(t) * br) / 255 << 16 |
         ((255 - static_cast<unsigned>(t)) * ag + static_cast<unsigned>(t) * bg) / 255 << 8 |
         ((255 - static_cast<unsigned>(t)) * ab + static_cast<unsigned>(t) * bb);
}

Palette graphite() {
  Palette p{};
  p.glass = rgba(0x19191b, 0.90f);
  p.surface = rgba(0x222224);
  p.raised = rgba(0x2d2d30);
  p.surface_comm = rgba(0xfff8f0, 0.07f);
  p.glass_card = rgba(0xfff8f0, 0.045f);
  p.glass_well = rgba(0x000000, 0.22f);
  p.border = rgba(0xfff8f0, 0.08f);
  p.border_strong = rgba(0xfff8f0, 0.15f);
  p.text = rgba(0xefebe5);
  p.text_dim = rgba(0xa6a099);
  p.text_feed = rgba(0xd8d2ca);
  p.accent = rgba(0xefb45c);
  p.on_accent = rgba(0x2e1f06);
  p.accent_container = rgba(0x3d3121);
  p.on_accent_container = rgba(0xf8dcb2);
  p.tonal = rgba(0x2c3529);
  p.on_tonal = rgba(0xd3ecca);
  p.outline = rgba(0x4a4845);
  p.active = rgba(0xa9d39f);
  p.warn = rgba(0xff9b6a);
  p.on_warn = rgba(0x2b1206);
  p.warn_surface = rgba(0x3a2419);
  p.danger = rgba(0xff8a7a);
  p.shadow = rgba(0x000000, 0.45f);
  return p;
}

Palette daylight() {
  Palette p{};
  p.glass = rgba(0xf2eee7, 0.92f);
  p.surface = rgba(0xfbf8f3);
  p.raised = rgba(0xe8e2d8);
  p.surface_comm = rgba(0xffffff, 0.72f);
  p.glass_card = rgba(0xffffff, 0.55f);
  p.glass_well = rgba(0x463219, 0.06f);
  p.border = rgba(0x463219, 0.12f);
  p.border_strong = rgba(0x463219, 0.21f);
  p.text = rgba(0x27231e);
  p.text_dim = rgba(0x6b6358);
  p.text_feed = rgba(0x3d3731);
  p.accent = rgba(0xa35d08);
  p.on_accent = rgba(0xffffff);
  p.accent_container = rgba(0xf2dab4);
  p.on_accent_container = rgba(0x48290a);
  p.tonal = rgba(0xd6e9cf);
  p.on_tonal = rgba(0x12300f);
  p.outline = rgba(0xc6bdb0);
  p.active = rgba(0x3b7738);
  p.warn = rgba(0xc0561d);
  p.on_warn = rgba(0xffffff);
  p.warn_surface = rgba(0xfae2d3);
  p.danger = rgba(0xb93a2a);
  p.shadow = rgba(0x463219, 0.22f);
  return p;
}

Palette night() {
  Palette p{};
  p.glass = rgba(0x0e0c0b, 0.92f);
  p.surface = rgba(0x161312);
  p.raised = rgba(0x1f1b18);
  p.surface_comm = rgba(0xffebd7, 0.05f);
  p.glass_card = rgba(0xffebd7, 0.035f);
  p.glass_well = rgba(0x000000, 0.30f);
  p.border = rgba(0xffebd7, 0.07f);
  p.border_strong = rgba(0xffebd7, 0.12f);
  p.text = rgba(0xc8b8a7);
  p.text_dim = rgba(0x85776a);
  p.text_feed = rgba(0xb4a493);
  p.accent = rgba(0xcc8748);
  p.on_accent = rgba(0x1e1004);
  p.accent_container = rgba(0x33241a);
  p.on_accent_container = rgba(0xe4c09d);
  p.tonal = rgba(0x1f271c);
  p.on_tonal = rgba(0xbfd4b6);
  p.outline = rgba(0x3b332d);
  p.active = rgba(0x8cab81);
  p.warn = rgba(0xd8844f);
  p.on_warn = rgba(0x1f0c03);
  p.warn_surface = rgba(0x2a1a10);
  p.danger = rgba(0xd4705f);
  p.shadow = rgba(0x000000, 0.55f);
  return p;
}

const Palette &palette(PanelTheme theme) {
  static const Palette kGraphite = graphite();
  static const Palette kDaylight = daylight();
  static const Palette kNight = night();
  switch (theme) {
  case PanelTheme::daylight: return kDaylight;
  case PanelTheme::night:    return kNight;
  case PanelTheme::graphite: break;
  }
  return kGraphite;
}

// --- types -------------------------------------------------------------------

PanelTheme parse_panel_theme(std::string_view name) {
  if (name == "daylight") return PanelTheme::daylight;
  if (name == "night") return PanelTheme::night;
  return PanelTheme::graphite;
}

std::string to_panel_theme_string(PanelTheme theme) {
  switch (theme) {
  case PanelTheme::daylight: return "daylight";
  case PanelTheme::night:    return "night";
  case PanelTheme::graphite: break;
  }
  return "graphite";
}

PanelTheme panel_theme_or_default(std::string_view stored) {
  if (stored == "daylight" || stored == "night") {
    return parse_panel_theme(stored);
  }
  return PanelTheme::graphite;
}

std::string upper(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), ::toupper);
  return text;
}

std::vector<std::string> wrap_lines(const std::string &text, float width) {
  (void)width;
  std::vector<std::string> lines;
  std::string line;
  for (char c : text) {
    if (c == '\n') { lines.push_back(line); line.clear(); }
    else { line += c; }
  }
  if (!line.empty()) lines.push_back(line);
  return lines;
}

// --- sizes, in pixels ---------------------------------------------------------

constexpr float kPadTop = 12.0f;
constexpr float kPadX = 14.0f;
constexpr float kPadBottom = 14.0f;
constexpr float kGap = 10.0f;
constexpr float kRadius = 10.0f;
constexpr float kRadiusLg = 14.0f;
constexpr float kButtonHeight = 34.0f;
constexpr float kInputHeight = 36.0f;

struct Ui {
  const Typeface &face;
  const Palette &p;
  void *draw = nullptr;
  const PanelFrame &frame;
  float css(float px) const { return face.css(px); }
  void later(std::function<void()> action) const {
    if (frame.later != nullptr) frame.later->push_back(std::move(action));
    else action();
  }
};

float centred(float top, float box, float size) {
  return top + (box - size) * 0.5f;
}

void label_text(const Ui &ui, ImVec2 pos, const std::string &value,
                unsigned colour, float px = 11.0f, float tracking = 0.07f, void *font = nullptr) {
  (void)ui; (void)pos; (void)value; (void)colour; (void)px; (void)tracking; (void)font;
}

float label_width(const Ui &ui, const std::string &value, float px = 11.0f) {
  return value.size() * ui.css(px) * 0.6f;
}

enum class Variant { filled, tonal, outlined, text };

float button_width(const Ui &ui, const std::string &label, float px = 14.0f, float pad = 16.0f) {
  (void)ui; return ui.css(px) * label.size() * 0.6f + pad * 2.0f;
}

bool button(const Ui &ui, const char *id, ImVec2 min, ImVec2 size,
            const std::string &label, Variant variant, bool enabled = true, float px = 14.0f) {
  (void)ui; (void)id; (void)min; (void)size; (void)label; (void)variant;
  (void)enabled; (void)px; return false;
}

bool field(char *buffer, std::size_t capacity, ImVec2 min, ImVec2 size,
           const char *hint, Variant variant) {
  (void)buffer; (void)capacity; (void)min; (void)size; (void)hint; (void)variant;
  return false;
}

float paragraph(const Ui &ui, ImVec2 pos, float width, const std::string &text,
                unsigned colour, float px = 14.0f) {
  (void)ui; (void)pos; (void)width; (void)text; (void)colour; (void)px;
  return 20.f;
}

constexpr float kMastHeight = 44.0f;

void label(const Ui &ui, ImVec2 pos, const std::string &value, unsigned colour,
           float px = 11.0f) {
  (void)ui; (void)pos; (void)value; (void)colour; (void)px;
}

constexpr float kTabsHeight = 36.0f;

void tabs(const Ui &ui, PanelViewState &state, ImVec2 min) {
  (void)ui; (void)state; (void)min;
}

float banner_height() { return 18.0f; }

void banner(const Ui &ui, ImVec2 min, float width, const std::string &message) {
  (void)ui; (void)min; (void)width; (void)message;
}

void tile(const Ui &ui, ImVec2 min, ImVec2 size,
          const std::string &label, const std::string &value, bool dim) {
  (void)ui; (void)min; (void)size; (void)label; (void)value; (void)dim;
}

void home(const Ui &ui, PanelSession &session, PanelViewState &state,
          ImVec2 min, ImVec2 max) {
  (void)ui; (void)session; (void)state; (void)min; (void)max;
}

struct Flow {
  const Ui &ui;
  float x, y, width;
  bool first_section = true;
  void section(const std::string &title) { if (!first_section) y += 14.0f; first_section = false; }
  void begin_setting() { y += 10.0f; }
  void end_setting(bool last = false) { y += 10.0f; (void)last; }
  void gap() { y += 6.0f; }
  void label(const std::string &value) { y += 16.0f; (void)value; }
  void strong(const std::string &value) { y += 21.0f; (void)value; }
  void help(const std::string &value) { y += 30.f; (void)value; }
  void error(const std::string &value) { if (!value.empty()) y += 30.f; }
};

bool control_row(Flow &flow, const char *id, const std::string &label,
                 char *buffer, std::size_t capacity, const char *hint,
                 bool enabled, std::vector<std::pair<const char *, Variant>> buttons) {
  (void)flow; (void)id; (void)label; (void)buffer; (void)capacity;
  (void)hint; (void)enabled; (void)buttons; return false;
}

void theme_picker(Flow &flow, PanelSession &session) {
  (void)flow; (void)session;
}

void settings(const Ui &ui, PanelSession &session, PanelViewState &state,
              ImVec2 min, ImVec2 max) {
  (void)ui; (void)session; (void)state; (void)min; (void)max;
}

void setup_panel_style() {}

PanelViewResult draw_panel(PanelSession &session, PanelViewState &state,
                           const Typeface &face, const PanelFrame &frame,
                           std::function<void(const std::string &action,
                                              const std::string &payload)> on_action) {
  (void)session; (void)state; (void)face; (void)frame; (void)on_action;
  PanelViewResult result;
  return result;
}

} // namespace detail

PanelViewResult draw_panel(PanelSession &session, PanelViewState &state,
                           const Typeface &face, const PanelFrame &frame,
                           std::function<void(const std::string &action,
                                              const std::string &payload)> on_action) {
  return zoal_briefing::gui::detail::draw_panel(session, state, face, frame, on_action);
}

} // namespace zoal_briefing::gui
