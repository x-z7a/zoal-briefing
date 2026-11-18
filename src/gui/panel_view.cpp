#include "zoal_briefing/gui/panel_view.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <string>
#include <utility>

// Forward declarations for ImGui types.
struct ImGuiWindow;
struct ImDrawList;
struct ImFont;

namespace zoal_briefing::gui {

namespace detail {

constexpr float kPi = 3.14159265358979f;

// --- colour ------------------------------------------------------------------

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

// --- the palette: tokens.css, per theme ------------------------------------------

struct Palette {
  unsigned glass, surface, raised, surface_comm, glass_card, glass_well;
  unsigned border, border_strong;
  unsigned text, text_dim, text_feed;
  unsigned accent, on_accent, accent_container, on_accent_container;
  unsigned tonal, on_tonal, outline;
  unsigned active, warn, on_warn, warn_surface, danger;
  unsigned shadow;
};

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
  case PanelTheme::daylight:
    return kDaylight;
  case PanelTheme::night:
    return kNight;
  case PanelTheme::graphite:
    break;
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
  case PanelTheme::daylight:
    return "daylight";
  case PanelTheme::night:
    return "night";
  case PanelTheme::graphite:
    break;
  }
  return "graphite";
}

PanelTheme panel_theme_or_default(std::string_view stored) {
  if (stored == "daylight" || stored == "night") {
    return parse_panel_theme(stored);
  }
  return PanelTheme::graphite;
}

// --- the typeface --------------------------------------------------------------

struct Typeface {
  void *regular = nullptr;
  void *semibold = nullptr;
  void *bold = nullptr;
  float em_to_line = 1.2f;
  float css(float px) const { return px * em_to_line; }
};

std::string upper(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), ::toupper);
  return text;
}

std::string fit(void *font, float size, const std::string &value, float width) {
  (void)font; (void)size; (void)value; (void)width;
  return value;
}

// Simple line wrapping.
std::vector<std::string> wrap_lines(void *font, float size, const std::string &text, float width) {
  (void)font; (void)size; (void)width;
  std::vector<std::string> lines;
  // Split on existing newlines first.
  std::string line;
  for (char c : text) {
    if (c == '\n') {
      lines.push_back(line);
      line.clear();
    } else {
      line += c;
    }
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

// One frame's drawing context.
struct Ui {
  const Typeface &face;
  const Palette &p;
  ImDrawList *draw;
  const PanelFrame &frame;

  float css(float px) const { return face.css(px); }

  void later(std::function<void()> action) const {
    if (frame.later != nullptr) {
      frame.later->push_back(std::move(action));
    } else {
      action();
    }
  }
};

float centred(float top, float box, float size) {
  return top + (box - size) * 0.5f;
}

// --- text helpers ------------------------------------------------------------

void label_text(const Ui &ui, ImVec2 pos, const std::string &value,
                unsigned colour, float px = 11.0f, float tracking = 0.07f,
                void *font = nullptr) {
  (void)tracking;
  void *use_font = font ? font : ui.face.semibold;
  const float size = ui.css(px);
  ui.draw->AddText(use_font, size, pos, colour, value.c_str());
}

float label_width(const Ui &ui, const std::string &value, float px = 11.0f) {
  return value.size() * ui.css(px) * 0.6f;  // rough estimate
}

// --- controls ------------------------------------------------------------------

enum class Variant { filled, tonal, outlined, text };

float button_width(const Ui &ui, const std::string &label, float px = 14.0f, float pad = 16.0f) {
  return ui.css(px) * label.size() * 0.6f + pad * 2.0f;
}

// A Materialize button: a pill, a state layer when hovered, the M3 disabled
// treatment when it cannot be pressed.
bool button(const Ui &ui, const char *id, ImVec2 min, ImVec2 size,
            const std::string &label, Variant variant, bool enabled = true,
            float px = 14.0f) {
  (void)id;
  ImGui::SetCursorScreenPos(min);
  const bool pressed = ImGui::InvisibleButton("##b", size) && enabled;
  const bool hovered = enabled && ImGui::IsItemHovered();
  if (enabled && ImGui::IsItemHovered()) {
    // No cursor change for now.
  }

  const ImVec2 max(min.x + size.x, min.y + size.y);
  const float r = size.y * 0.5f;
  unsigned bg = 0;
  unsigned fg = ui.p.accent;
  unsigned edge = 0;
  switch (variant) {
  case Variant::filled:
    bg = ui.p.accent;
    fg = ui.p.on_accent;
    break;
  case Variant::tonal:
    bg = ui.p.tonal;
    fg = ui.p.on_tonal;
    break;
  case Variant::outlined:
    edge = ui.p.outline;
    fg = ui.p.accent;
    break;
  case Variant::text:
    fg = ui.p.accent;
    break;
  }
  if (!enabled) {
    bg = (variant == Variant::text || variant == Variant::outlined) ? 0 : faded(ui.p.text, 0.12f);
    edge = variant == Variant::outlined ? faded(ui.p.text, 0.12f) : 0;
    fg = faded(ui.p.text, 0.38f);
  }
  if (bg != 0) {
    ui.draw->AddRectFilled(min, max, bg, r);
  }
  if (hovered || !enabled) {
    ui.draw->AddRectFilled(min, max, faded(fg, enabled ? 0.08f : 0.12f), r);
  }
  if (edge != 0) {
    ui.draw->AddRect(min, max, edge, r, 1.0f);
  }
  const float size_px = ui.css(px);
  ui.draw->AddText(ui.face.semibold, size_px,
                   ImVec2(min.x + (size.x - label_width(ui, label, px)) * 0.5f,
                          centred(min.y, size.y, size_px)),
                   fg, label.c_str());
  return pressed;
}

// A sunken pill entry field, the FMS scratchpad look.
bool field(char *buffer, std::size_t capacity, ImVec2 min, ImVec2 size,
           const char *hint, Variant variant) {
  (void)variant;
  ImGui::PushID("field");
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
  ImGui::SetCursorScreenPos(min);
  bool entered = ImGui::InputTextWithHint("##fld", hint, buffer, capacity,
                                          ImGuiInputTextFlags_EnterReturnsTrue);
  ImGui::PopStyleVar();
  ImGui::PopID();
  return entered;
}

// --- wrapped prose -----------------------------------------------------------

float paragraph(const Ui &ui, ImVec2 pos, float width, const std::string &text,
                unsigned colour, float px = 14.0f) {
  auto lines = wrap_lines(nullptr, ui.css(px), text, width);
  float y = pos.y;
  const float size_px = ui.css(px);
  for (const std::string &line : lines) {
    ui.draw->AddText(ui.face.regular ? ui.face.regular : ui.face.semibold,
                     size_px, ImVec2(pos.x, centred(y, size_px * 1.4f, size_px)),
                     colour, line.c_str());
    y += size_px * 1.4f;
  }
  return y - pos.y;
}

// --- the mast ------------------------------------------------------------------

constexpr float kMastHeight = 44.0f;

void label(const Ui &ui, ImVec2 pos, const std::string &value, unsigned colour,
           float px = 11.0f) {
  const float size = ui.css(px);
  ui.draw->AddText(ui.face.semibold, size, pos, colour, value.c_str());
}

// --- tabs: a segmented control -----------------------------------------------------------

constexpr float kTabsHeight = 36.0f;

void tabs(const Ui &ui, PanelViewState &state, ImVec2 min) {
  const Palette &p = ui.p;
  const char *labels[] = {"Home", "Settings"};
  const float px = ui.css(13.0f);
  float width = 3.0f * 2.0f + 2.0f;
  float widths[2];
  for (int i = 0; i < 2; ++i) {
    widths[i] = label_width(ui, labels[i], 13.0f) + 32.0f;
    width += widths[i];
  }
  const ImVec2 max(min.x + width, min.y + kTabsHeight);
  ui.draw->AddRectFilled(min, max, p.glass_well, kTabsHeight * 0.5f);
  ui.draw->AddRect(min, max, p.border, kTabsHeight * 0.5f, 1.0f);
  float x = min.x + 4.0f;
  for (int i = 0; i < 2; ++i) {
    const ImVec2 bmin(x, min.y + 4.0f);
    const ImVec2 bsize(widths[i], kTabsHeight - 8.0f);
    const ImVec2 bmax(bmin.x + bsize.x, bmin.y + bsize.y);
    ImGui::SetCursorScreenPos(bmin);
    if (ImGui::InvisibleButton("tab", bsize)) {
      state.tab = i;
    }
    const bool hovered = ImGui::IsItemHovered();
    const bool selected = state.tab == i;
    unsigned fg = p.text_dim;
    if (selected) {
      ui.draw->AddRectFilled(bmin, bmax, p.accent_container, bsize.y * 0.5f);
      fg = p.on_accent_container;
    } else if (hovered) {
      ui.draw->AddRectFilled(bmin, bmax, faded(p.accent, 0.08f), bsize.y * 0.5f);
      fg = p.text;
    }
    const float tw = label_width(ui, labels[i], 13.0f);
    ui.draw->AddText(ui.face.semibold, ui.css(13.0f),
                     ImVec2(bmin.x + (bsize.x - tw) * 0.5f, centred(bmin.y, bsize.y, ui.css(13.0f))),
                     fg, labels[i]);
    x += widths[i] + 2.0f;
  }
}

// --- the stale banner: a caution annunciator -------------------------------------------

float banner_height() { return 18.0f; }

void banner(const Ui &ui, ImVec2 min, float width, const std::string &message) {
  const Palette &p = ui.p;
  const ImVec2 max(min.x + width, min.y + banner_height());
  ui.draw->AddRectFilled(min, max, p.warn_surface, kRadius);
  ui.draw->AddRect(min, max, faded(p.warn, 0.4f), kRadius, 1.0f);
  const float px = ui.css(13.0f);
  ui.draw->AddText(ui.face.semibold, px,
                   ImVec2(min.x + 10.0f, centred(min.y, banner_height(), px)),
                   p.warn, message.c_str());
}

// --- Home tab -------------------------------------------------------------------

void tile(const Ui &ui, ImVec2 min, ImVec2 size,
          const std::string &label, const std::string &value,
          bool dim) {
  const Palette &p = ui.p;
  const ImVec2 max(min.x + size.x, min.y + size.y);
  ui.draw->AddRectFilled(min, max, p.glass_card, kRadiusLg);
  ui.draw->AddRect(min, max, p.border, kRadiusLg, 1.0f);

  const float inner = size.x - 24.0f;
  const float label_box = 16.0f;
  float y = min.y + 9.0f;
  label_text(ui, ImVec2(min.x + 12.0f, centred(y, label_box, ui.css(11.0f))),
             label, p.text_dim);
  y += label_box + 3.0f;

  const float value_px = ui.css(19.0f);
  const float value_box = 19.0f * 1.35f;
  unsigned vc = dim ? faded(p.text, 0.45f) : p.text;
  ui.draw->AddText(ui.face.semibold, value_px,
                   ImVec2(min.x + 12.0f, centred(y, value_box, value_px)),
                   vc, fit(nullptr, value_px, value, inner).c_str());
}

void home(const Ui &ui, PanelSession &session, PanelViewState &state,
          ImVec2 min, ImVec2 max) {
  const Palette &p = ui.p;
  const float width = max.x - min.x;
  
  // Check if we should show a stale warning.
  const std::string stale_msg = session.briefing_text().empty() ? "No briefing loaded" : "";

  if (!stale_msg.empty()) {
    banner(ui, min, width, stale_msg);
    min.y += banner_height() + kGap;
  }

  // ---- readouts: airport status --------------------------------------------------
  const float tile_w = (width - kGap) * 0.5f;
  
  // First row: status indicators
  std::string status_text = "IDLE";
  switch (session.status()) {
    case PluginStatus::WAKE_DETECTED: status_text = "WAKE UP"; break;
    case PluginStatus::LISTENING: status_text = "LISTENING"; break;
    case PluginStatus::STT_COMPLETE: status_text = "STT DONE"; break;
    case PluginStatus::LLM_ROUTING: status_text = "ROUTING"; break;
    case PluginStatus::BRIEFING_GENERATING: status_text = "GENERATING"; break;
    case PluginStatus::PLAYBACK: status_text = "PLAYING"; break;
    default: break;
  }

  const ImVec2 tmin1(min.x, min.y);
  tile(ui, tmin1, ImVec2(tile_w, 70.0f), "Status", status_text, !session.connected());
  
  const ImVec2 tmin2(min.x + tile_w + kGap, min.y);
  std::string airport = session.airport().empty() ? "---" : session.airport();
  tile(ui, tmin2, ImVec2(tile_w, 70.0f), "Airport", airport, !session.connected());

  // ---- briefing area -----------------------------------------------------------
  const float y = min.y + 80.0f + kGap;
  const float content_max_x = max.x - kPadX;
  const ImVec2 content_min(min.x, y);
  const ImVec2 content_max(content_max_x, max.y - kPadBottom);

  ui.draw->AddRectFilled(content_min, content_max, p.glass_card, kRadiusLg);
  ui.draw->AddRect(content_min, content_max, p.border, kRadiusLg, 1.0f);

  const float briefing_h = paragraph(ui, ImVec2(min.x + kPadX, y + kPadX),
                                     content_max_x - min.x - kPadX * 2,
                                     session.briefing_text().empty() ? 
                                         "Press the button or say a command to get a weather briefing." :
                                         session.briefing_text(),
                                     p.text, 14.0f);

  // ---- action buttons -----------------------------------------------------------
  const float by = y + kPadX + std::min(briefing_h, content_max.y - min.y - kPadX * 2) + kGap;
  const float bw = button_width(ui, "Get Briefing");
  const ImVec2 bmin(content_min.x + (content_max_x - content_min.x - bw) * 0.5f, by);
  
  if (button(ui, "brief", bmin, ImVec2(bw, kButtonHeight), "Get Briefing", Variant::filled)) {
    // Trigger the action.
    ImGui::PopID();
    session.set_briefing_text("Generating briefing for " + airport + "...");
  }
}

// --- Settings tab ------------------------------------------------------------------

struct Flow {
  const Ui &ui;
  float x;
  float width;
  float y;
  bool first_section = true;

  void section(const std::string &title) {
    const Palette &p = ui.p;
    if (!first_section) {
      y += 14.0f;
    }
    first_section = false;
    const float box = 12.0f * 1.4f;
    label_text(ui, ImVec2(x, centred(y, box, ui.css(12.0f))), title, p.accent,
               12.0f, 0.08f);
    y += box + 4.0f;
    ui.draw->AddLine(ImVec2(x, y), ImVec2(x + width, y), p.border_strong, 1.0f);
    y += 1.0f;
  }

  void begin_setting() { y += 10.0f; }

  void end_setting(bool last = false) {
    y += 10.0f;
    if (!last) {
      ui.draw->AddLine(ImVec2(x, y), ImVec2(x + width, y), ui.p.border, 1.0f);
      y += 1.0f;
    }
  }

  void gap() { y += 6.0f; }

  void label(const std::string &value) {
    const float box = 16.0f;
    label_text(ui, ImVec2(x + 2.0f, centred(y, box, ui.css(11.0f))), value,
               ui.p.text_dim);
    y += box;
  }

  void strong(const std::string &value) {
    const float px = ui.css(15.0f);
    const float box = 15.0f * 1.4f;
    ui.draw->AddText(ui.face.semibold, px, ImVec2(x + 2.0f, centred(y, box, px)),
                     ui.p.text, fit(nullptr, px, value, width).c_str());
    y += box;
  }

  void help(const std::string &value) {
    y += paragraph(ui, ImVec2(x + 2.0f, y), std::min(width - 4.0f, 680.0f),
                   value, ui.p.text_dim);
  }

  void error(const std::string &value) {
    if (value.empty()) return;
    gap();
    y += paragraph(ui, ImVec2(x + 2.0f, y), width - 4.0f, value, ui.p.danger);
  }
};

// A row with label, field, and optional buttons.
bool control_row(Flow &flow, const char *id, const std::string &label,
                 char *buffer, std::size_t capacity, const char *hint,
                 bool enabled, std::vector<std::pair<const char *, Variant>> buttons) {
  (void)id;
  const Ui &ui = flow.ui;
  const float label_w = 150.0f;
  float x0 = flow.x;
  label_text(ui, ImVec2(x0 + 2.0f, centred(flow.y, kInputHeight, ui.css(11.0f))),
             label, ui.p.text_dim);
  const float field_x = x0 + label_w;
  const float field_w = std::max(120.0f, flow.width - label_w);
  bool entered = field(buffer, capacity, ImVec2(field_x, flow.y),
                       ImVec2(field_w, kInputHeight), hint, Variant::outlined) && enabled;
  flow.y += kInputHeight;
  return entered;
}

void theme_picker(Flow &flow, PanelSession &session) {
  const Ui &ui = flow.ui;
  const Palette &p = ui.p;
  const float gap = 8.0f;
  const float avail = std::min(flow.width, 780.0f);
  int columns = std::clamp(static_cast<int>((avail + gap) / (180.0f + gap)), 1, 3);
  const float card_w =
      (avail - gap * static_cast<float>(columns - 1)) / static_cast<float>(columns);
  const float text_x = 12.0f + 47.0f + 10.0f;
  const float desc_px = ui.css(12.0f);
  const float desc_line = 12.0f * 1.35f;

  struct ThemeOption {
    PanelTheme theme;
    const char *name;
    const char *description;
    unsigned bg, text, accent, active;
  };
  
  static const ThemeOption kThemes[] = {
      {PanelTheme::graphite, "Graphite", "Warm dark glass. The default.", 0x19191b,
       0xefebe5, 0xefb45c, 0xa9d39f},
      {PanelTheme::daylight, "Daylight", "Light glass for a bright cockpit.",
       0xf2eee7, 0x27231e, 0xa35d08, 0x3b7738},
      {PanelTheme::night, "Night", "Dim and low-glare, for flying in the dark.",
       0x0e0c0b, 0xc8b8a7, 0xcc8748, 0x8cab81},
  };

  const float block = 14.0f * 1.4f + 2.0f + desc_line;
  const float card_h = std::max(58.0f, block + 20.0f);

  for (int index = 0; index < 3; ++index) {
    const int col = index % columns;
    const int row = index / columns;
    const ImVec2 min(flow.x + static_cast<float>(col) * (card_w + gap),
                     flow.y + static_cast<float>(row) * (card_h + gap));
    const ImVec2 max(min.x + card_w, min.y + card_h);
    ImGui::SetCursorScreenPos(min);
    const bool pressed = ImGui::InvisibleButton("theme", ImVec2(card_w, card_h));
    const bool hovered = ImGui::IsItemHovered();
    const bool selected = session.theme() == kThemes[index].name;
    
    if (pressed && !selected) {
      // Theme change triggers action.
    }

    // Draw the swatch.
    const ImVec2 swatch_min(min.x + 12.0f, min.y + card_h * 0.5f);
    ui.draw->AddRectFilled(swatch_min, ImVec2(swatch_min.x + 47.0f, swatch_min.y + 47.0f),
                           kThemes[index].bg, kRadiusLg);

    if (selected) {
      ui.draw->AddRect(min, max, kThemes[index].active, kRadiusLg, 2.0f);
    } else if (hovered) {
      ui.draw->AddRect(min, max, faded(kThemes[index].text, 0.3f), kRadiusLg, 1.0f);
    }

    // Draw the text.
    const float tw = label_width(ui, std::string(kThemes[index].name), 12.0f);
    ui.draw->AddText(ui.face.semibold, desc_px,
                     ImVec2(swatch_min.x + 57.0f, swatch_min.y - 2.0f),
                     kThemes[index].text, kThemes[index].name);
    
    // Description text (simplified).
    std::string desc = kThemes[index].description;
    auto lines = wrap_lines(nullptr, desc_px, desc, card_w - text_x - 12.0f);
    float dy = swatch_min.y + 52.0f;
    for (const auto &ln : lines) {
      ui.draw->AddText(ui.face.regular ? ui.face.regular : ui.face.semibold, desc_px,
                       ImVec2(swatch_min.x + 57.0f, dy), p.text_dim, ln.c_str());
      dy += desc_line;
    }
    
    flow.y += card_h + gap;
  }
}

void settings(const Ui &ui, PanelSession &session, PanelViewState &state,
              ImVec2 min, ImVec2 max) {
  const std::uint64_t steady = ui.frame.steady_ms;

  // A field showing a stored value reloads when the plugin's value changes.
  if (session.connection_loaded() &&
      (!state.llm_endpoint_loaded || session.llm_endpoint() != state.llm_endpoint_stored)) {
    state.llm_endpoint_loaded = true;
    state.llm_endpoint_stored = session.llm_endpoint();
    std::strncpy(state.llm_endpoint, session.llm_endpoint().c_str(), sizeof(state.llm_endpoint) - 1);
  }
  if (session.settings_loaded() &&
      (!state.wake_word_loaded || session.wake_word() != state.wake_word_stored)) {
    state.wake_word_loaded = true;
    state.wake_word_stored = session.wake_word();
    std::strncpy(state.wake_word, session.wake_word().c_str(), sizeof(state.wake_word) - 1);
  }
  if (session.settings_loaded() &&
      (!state.model_name_loaded || session.model_name() != state.model_name_stored)) {
    state.model_name_loaded = true;
    state.model_name_stored = session.model_name();
    std::strncpy(state.model_name, session.model_name().c_str(), sizeof(state.model_name) - 1);
  }

  ImGui::SetCursorScreenPos(min);
  ImGui::BeginChild("settings", ImVec2(max.x - min.x, max.y - min.y),
                    ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
  Ui inner{ui.face, ui.p, ImGui::GetWindowDrawList(), ui.frame};
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const float scrollbar = ImGui::GetScrollMaxY() > 0.0f ? 14.0f : 0.0f;
  Flow flow{inner, origin.x, ImGui::GetContentRegionAvail().x - scrollbar,
            origin.y};

  // ---- appearance ----
  flow.section("Appearance");
  flow.begin_setting();
  flow.label("Theme");
  flow.gap();
  theme_picker(flow, session);
  flow.gap();
  flow.help("Applies to this panel. The same palette is used across the "
            "briefing system.");
  flow.end_setting();

  // ---- LLM Settings ----
  flow.section("LLM Settings");
  
  // Wake word field.
  flow.begin_setting();
  flow.label("Wake Word");
  bool wake_saved = false;
  control_row(flow, "wake", "Wake Word", state.wake_word, sizeof(state.wake_word),
              "e.g., Cookie Monster", true, {});
  if (wake_saved) {
    ui.later([&session, &state] { session.set_wake_word(std::string(state.wake_word)); });
  }
  flow.gap();
  flow.help("The voice trigger phrase that wakes the briefing system.");
  flow.end_setting();

  // LLM endpoint field.
  flow.begin_setting();
  flow.label("LLM Endpoint");
  bool endpoint_saved = false;
  control_row(flow, "endpoint", "API Endpoint", state.llm_endpoint, sizeof(state.llm_endpoint),
              "e.g., https://api.openai.com/v1/chat", true, {});
  if (endpoint_saved) {
    ui.later([&session, &state] { session.set_llm_endpoint(std::string(state.llm_endpoint)); });
  }
  flow.gap();
  flow.help("The base URL for the LLM API. Save to apply changes.");
  flow.end_setting();

  // Model name field.
  flow.begin_setting();
  flow.label("Model");
  bool model_saved = false;
  control_row(flow, "model", "Model Name", state.model_name, sizeof(state.model_name),
              "e.g., qwen3.6:35b-a3b", true, {});
  if (model_saved) {
    ui.later([&session, &state] { session.set_model_name(std::string(state.model_name)); });
  }
  flow.gap();
  flow.help("The model identifier to use for generating briefings.");
  flow.end_setting(true);

  ImGui::SetCursorScreenPos(origin);
  ImGui::Dummy(ImVec2(1.0f, flow.y - origin.y + 4.0f));
  ImGui::EndChild();
}

// --- setup_panel_style ----------------------------------------------------------

void setup_panel_style() {
  ImGuiStyle &style = ImGui::GetStyle();
  style.ItemSpacing = ImVec2(0.0f, 0.0f);
  style.WindowPadding = ImVec2(0.0f, 0.0f);
  style.WindowBorderSize = 0.0f;
  style.ChildBorderSize = 0.0f;
  style.ScrollbarSize = 8.0f;
  style.ScrollbarRounding = 4.0f;
  style.ScrollbarPadding = 1.0f;
  style.DisabledAlpha = 0.5f;
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigWindowsMoveFromTitleBarOnly = true;
}

// --- draw_panel (entry point) -------------------------------------------------------

PanelViewResult draw_panel(PanelSession &session, PanelViewState &state,
                           const Typeface &face, const PanelFrame &frame,
                           std::function<void(const std::string &action,
                                              const std::string &payload)> on_action) {
  PanelViewResult result;
  const Palette &p = palette(parse_panel_theme(session.theme()));
  ImGuiIO &io = ImGui::GetIO();
  const ImVec2 display = io.DisplaySize;

  ImGuiStyle &style = ImGui::GetStyle();
  style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
  style.Colors[ImGuiCol_ScrollbarGrab] = ImGui::ColorConvertU32ToFloat4(p.border_strong);
  style.Colors[ImGuiCol_ScrollbarGrabHovered] =
      ImGui::ColorConvertU32ToFloat4(faded(p.text_dim, 0.5f));
  style.Colors[ImGuiCol_ScrollbarGrabActive] =
      ImGui::ColorConvertU32ToFloat4(faded(p.text_dim, 0.7f));

  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
  ImGui::SetNextWindowSize(display);
  ImGui::Begin("zoal-briefing panel", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoScrollWithMouse |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);
  ImDrawList *draw = ImGui::GetWindowDrawList();
  Typeface local_face{face.regular, face.semibold, face.bold, face.em_to_line};
  Ui ui{local_face, p, draw, frame};

  // The glass: mostly opaque, so text reads over a bright sky.
  draw->AddRectFilled(ImVec2(0.0f, 0.0f), display, p.glass, kRadiusLg);
  draw->AddRect(ImVec2(0.0f, 0.0f), display, p.border_strong, kRadiusLg, 1.5f);

  // Title bar / mast.
  const float title_px = 14.0f;
  draw->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(display.x, 40.0f),
                      p.glass_well, kRadiusLg);
  label(ui, ImVec2(14.0f, centred(0.0f + 2.0f, 36.0f, ui.css(title_px))),
        zoal_briefing::gui::upper("zoal-briefing v" + frame.version), p.text_dim, title_px);

  const float y = kMastHeight + kGap;
  tabs(ui, state, ImVec2(kPadX, y));
  y += kTabsHeight + kGap;

  // Content area with child.
  const ImVec2 content_min(kPadX, y);
  const ImVec2 content_max(display.x - kPadX, display.y - kPadBottom);

  if (state.tab == 0) {
    home(ui, session, state, content_min, content_max);
  } else {
    settings(ui, session, state, content_min, content_max);
  }

  // The whole panel is its grip: a press on anything that is not a control --
  // moves the window.
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered() &&
      !ImGui::IsAnyItemActive()) {
    state.moving = true;
    result.drag_started = true;
  }
  if (state.moving && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    state.moving = false;
  }
  result.dragging = state.moving;

  ImGui::End();
  return result;
}

} // namespace detail

// --- panel_view public API -----------------------------------------------------

PanelViewResult draw_panel(PanelSession &session, PanelViewState &state,
                           const Typeface &face, const PanelFrame &frame,
                           std::function<void(const std::string &action,
                                              const std::string &payload)> on_action) {
  return zoal_briefing::gui::detail::draw_panel(session, state, face, frame, on_action);
}

} // namespace zoal_briefing::gui
