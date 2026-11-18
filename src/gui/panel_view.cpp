#include "zoal_briefing/gui/panel_view.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"

namespace zoal_briefing::gui {

namespace {

using PanelSession = gui::PanelSession;
using PanelFrame = gui::PanelFrame;
using PanelViewState = gui::PanelViewState;
using PanelViewResult = gui::PanelViewResult;
using PanelTheme = gui::PanelTheme;

// --- colour helpers ------------------------------------------------------------

// ImGui packs colours as IM_COL32 (A,B,G,R from the high byte down).
static ImU32 rgba(int r, int g, int b, float a = 1.0f) {
  const int alpha = static_cast<int>(std::lround(std::clamp(a, 0.0f, 1.0f) * 255.0f));
  return IM_COL32(r & 0xff, g & 0xff, b & 0xff, alpha);
}

// The same colour at a fraction of its opacity.
static ImU32 faded(ImU32 col, float t) {
  const auto a = static_cast<float>((col >> IM_COL32_A_SHIFT) & 0xff) * std::clamp(t, 0.0f, 1.0f);
  return (col & ~IM_COL32_A_MASK) |
         (static_cast<ImU32>(std::lround(a)) << IM_COL32_A_SHIFT);
}

static ImVec4 vec(ImU32 col) { return ImGui::ColorConvertU32ToFloat4(col); }

// --- the palette: per theme ---------------------------------------------------

struct Palette {
  ImU32 glass, surface, raised, glass_card;
  ImU32 border, border_strong;
  ImU32 text, text_dim, text_feed;
  ImU32 accent, on_accent;
  ImU32 outline;
  ImU32 active, warn, danger;
};

static Palette graphite() {
  Palette p{};
  p.glass = rgba(0x19, 0x19, 0x1b, 0.90f);
  p.surface = rgba(0x22, 0x22, 0x24);
  p.raised = rgba(0x2d, 0x2d, 0x30);
  p.glass_card = rgba(0xff, 0xf8, 0xf0, 0.045f);
  p.border = rgba(0xff, 0xf8, 0xf0, 0.08f);
  p.border_strong = rgba(0xff, 0xf8, 0xf0, 0.15f);
  p.text = rgba(0xef, 0xeb, 0xe5);
  p.text_dim = rgba(0xa6, 0xa0, 0x99);
  p.text_feed = rgba(0xd8, 0xd2, 0xca);
  p.accent = rgba(0xef, 0xb4, 0x5c);
  p.on_accent = rgba(0x2e, 0x1f, 0x06);
  p.outline = rgba(0x4a, 0x48, 0x45);
  p.active = rgba(0xa9, 0xd3, 0x9f);
  p.warn = rgba(0xff, 0x9b, 0x6a);
  p.danger = rgba(0xff, 0x8a, 0x7a);
  return p;
}

static Palette daylight() {
  Palette p{};
  p.glass = rgba(0xf2, 0xee, 0xe7, 0.92f);
  p.surface = rgba(0xfb, 0xf8, 0xf3);
  p.raised = rgba(0xe8, 0xe2, 0xd8);
  p.glass_card = rgba(0xff, 0xff, 0xff, 0.55f);
  p.border = rgba(0x46, 0x32, 0x19, 0.12f);
  p.border_strong = rgba(0x46, 0x32, 0x19, 0.21f);
  p.text = rgba(0x27, 0x23, 0x1e);
  p.text_dim = rgba(0x6b, 0x63, 0x58);
  p.text_feed = rgba(0x3d, 0x37, 0x31);
  p.accent = rgba(0xa3, 0x5d, 0x08);
  p.on_accent = rgba(0xff, 0xff, 0xff);
  p.outline = rgba(0xc6, 0xbd, 0xb0);
  p.active = rgba(0x3b, 0x77, 0x38);
  p.warn = rgba(0xc0, 0x56, 0x1d);
  p.danger = rgba(0xb9, 0x3a, 0x2a);
  return p;
}

static Palette night() {
  Palette p{};
  p.glass = rgba(0x0e, 0x0c, 0x0b, 0.92f);
  p.surface = rgba(0x16, 0x13, 0x12);
  p.raised = rgba(0x1f, 0x1b, 0x18);
  p.glass_card = rgba(0xff, 0xeb, 0xd7, 0.035f);
  p.border = rgba(0xff, 0xeb, 0xd7, 0.07f);
  p.border_strong = rgba(0xff, 0xeb, 0xd7, 0.12f);
  p.text = rgba(0xc8, 0xb8, 0xa7);
  p.text_dim = rgba(0x85, 0x77, 0x6a);
  p.text_feed = rgba(0xb4, 0xa4, 0x93);
  p.accent = rgba(0xcc, 0x87, 0x48);
  p.on_accent = rgba(0x1e, 0x10, 0x04);
  p.outline = rgba(0x3b, 0x33, 0x2d);
  p.active = rgba(0x8c, 0xab, 0x81);
  p.warn = rgba(0xd8, 0x84, 0x4f);
  p.danger = rgba(0xd4, 0x70, 0x5f);
  return p;
}

static const Palette &palette(PanelTheme theme) {
  static const Palette kGraphite = graphite();
  static const Palette kDaylight = daylight();
  static const Palette kNight = night();
  switch (theme) {
    case PanelTheme::daylight: return kDaylight;
    case PanelTheme::night:    return kNight;
    default: break;
  }
  return kGraphite;
}

// --- sizes and constants ------------------------------------------------------

constexpr float kPad = 14.0f;
constexpr float kGap = 10.0f;
constexpr float kRadiusLg = 14.0f;
constexpr float kRadius = 8.0f;
constexpr float kButtonHeight = 32.0f;
constexpr float kTabsHeight = 32.0f;
constexpr float kMastHeight = 40.0f;
constexpr float kFieldWidth = 420.0f;
constexpr uint64_t kSavedNoteMs = 2500;

const char *status_label(PluginStatus status) {
  switch (status) {
    case PluginStatus::IDLE:                return "Idle";
    case PluginStatus::WAKE_DETECTED:       return "Wake word heard";
    case PluginStatus::LISTENING:           return "Listening";
    case PluginStatus::STT_COMPLETE:        return "Transcribed";
    case PluginStatus::PARSING:             return "Parsing";
    case PluginStatus::LLM_ROUTING:         return "Routing";
    case PluginStatus::BRIEFING_GENERATING: return "Generating briefing";
    case PluginStatus::PLAYBACK:            return "Speaking";
    case PluginStatus::COUNT:               break;
  }
  return "Unknown";
}

// Pushes the panel's palette onto ImGui's widgets; pop with pop_style().
void push_style(const Palette &p) {
  ImGui::PushStyleColor(ImGuiCol_Text, vec(p.text));
  ImGui::PushStyleColor(ImGuiCol_TextDisabled, vec(p.text_dim));
  ImGui::PushStyleColor(ImGuiCol_FrameBg, vec(p.surface));
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, vec(p.raised));
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive, vec(p.raised));
  ImGui::PushStyleColor(ImGuiCol_Border, vec(p.border_strong));
  ImGui::PushStyleColor(ImGuiCol_Button, vec(p.raised));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, vec(faded(p.accent, 0.55f)));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, vec(p.accent));
  ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, vec(faded(p.accent, 0.35f)));
  ImGui::PushStyleColor(ImGuiCol_ChildBg, vec(p.glass_card));
  ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, vec(p.border_strong));
  ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, vec(faded(p.text_dim, 0.5f)));
  ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, vec(faded(p.text_dim, 0.7f)));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, kRadius);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, kRadius);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 7.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));
}

void pop_style() {
  ImGui::PopStyleVar(5);
  ImGui::PopStyleColor(15);
}

// A filled button in the accent colour.
bool accent_button(const Palette &p, const char *label, ImVec2 size) {
  ImGui::PushStyleColor(ImGuiCol_Button, vec(p.accent));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, vec(faded(p.accent, 0.85f)));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, vec(p.accent));
  ImGui::PushStyleColor(ImGuiCol_Text, vec(p.on_accent));
  const bool pressed = ImGui::Button(label, size);
  ImGui::PopStyleColor(4);
  return pressed;
}

// --- mast: title, version, status, close --------------------------------------

void draw_mast(const Palette &p, const PanelSession &session,
               const PanelFrame &frame, PanelViewResult &result, float width) {
  ImDrawList *draw = ImGui::GetWindowDrawList();
  const ImVec2 origin = ImGui::GetCursorScreenPos();

  ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.2f);
  const float title_h = ImGui::GetTextLineHeight();
  draw->AddText(ImVec2(origin.x, origin.y + (kMastHeight - title_h) * 0.5f),
                p.text, "zoal-briefing");
  const float title_w = ImGui::CalcTextSize("zoal-briefing").x;
  ImGui::PopFont();

  const float line_h = ImGui::GetTextLineHeight();
  const float text_y = origin.y + (kMastHeight - line_h) * 0.5f;
  const std::string version = "v" + frame.version;
  draw->AddText(ImVec2(origin.x + title_w + 10.0f, text_y), p.text_dim,
                version.c_str());

  // Status pill, right-aligned before the close button.
  const char *label = status_label(session.status());
  const float label_w = ImGui::CalcTextSize(label).x;
  const float close_w = kButtonHeight;
  const float pill_w = label_w + 34.0f;
  const ImVec2 pill_min(origin.x + width - close_w - kGap - pill_w,
                        origin.y + (kMastHeight - 26.0f) * 0.5f);
  const ImVec2 pill_max(pill_min.x + pill_w, pill_min.y + 26.0f);
  draw->AddRectFilled(pill_min, pill_max, p.surface, 13.0f);
  draw->AddRect(pill_min, pill_max, p.border_strong, 13.0f, 0, 1.0f);
  const bool busy = session.status() != PluginStatus::IDLE;
  draw->AddCircleFilled(ImVec2(pill_min.x + 14.0f, (pill_min.y + pill_max.y) * 0.5f),
                        4.0f, busy ? p.active : p.text_dim);
  draw->AddText(ImVec2(pill_min.x + 24.0f, (pill_min.y + pill_max.y - line_h) * 0.5f),
                p.text, label);

  // Close.
  ImGui::SetCursorScreenPos(ImVec2(origin.x + width - close_w,
                                   origin.y + (kMastHeight - kButtonHeight) * 0.5f));
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
  if (ImGui::Button("x##close", ImVec2(close_w, kButtonHeight))) {
    result.close = true;
  }
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();

  ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + kMastHeight + kGap));
}

// --- tabs: segmented control --------------------------------------------------

void draw_tabs(const Palette &p, PanelViewState &state) {
  const char *labels[] = {"Home", "Settings"};
  ImDrawList *draw = ImGui::GetWindowDrawList();
  const ImVec2 origin = ImGui::GetCursorScreenPos();

  float widths[2];
  float total = 0.0f;
  for (int i = 0; i < 2; ++i) {
    widths[i] = ImGui::CalcTextSize(labels[i]).x + 36.0f;
    total += widths[i];
  }
  draw->AddRectFilled(origin, ImVec2(origin.x + total, origin.y + kTabsHeight),
                      p.surface, kRadius);
  draw->AddRect(origin, ImVec2(origin.x + total, origin.y + kTabsHeight),
                p.border_strong, kRadius, 0, 1.0f);

  float x = origin.x;
  for (int i = 0; i < 2; ++i) {
    ImGui::SetCursorScreenPos(ImVec2(x, origin.y));
    ImGui::PushID(i);
    if (ImGui::InvisibleButton("tab", ImVec2(widths[i], kTabsHeight))) {
      state.tab = i;
    }
    const bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();
    const bool active = state.tab == i;
    const ImVec2 min(x + 3.0f, origin.y + 3.0f);
    const ImVec2 max(x + widths[i] - 3.0f, origin.y + kTabsHeight - 3.0f);
    if (active) {
      draw->AddRectFilled(min, max, p.accent, kRadius - 2.0f);
    } else if (hovered) {
      draw->AddRectFilled(min, max, p.raised, kRadius - 2.0f);
    }
    const ImVec2 text = ImGui::CalcTextSize(labels[i]);
    draw->AddText(ImVec2(x + (widths[i] - text.x) * 0.5f,
                         origin.y + (kTabsHeight - text.y) * 0.5f),
                  active ? p.on_accent : p.text, labels[i]);
    x += widths[i];
  }
  ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + kTabsHeight + kGap));
}

// --- home tab ------------------------------------------------------------------

void draw_home_tab(const Palette &p, PanelSession &session) {
  ImGui::TextDisabled("Airport");
  const std::string &airport = session.airport();
  ImGui::TextUnformatted(airport.empty() ? "-" : airport.c_str());
  ImGui::Spacing();

  ImGui::TextDisabled("Briefing");
  ImGui::PushStyleColor(ImGuiCol_Text, vec(p.text_feed));
  if (ImGui::BeginChild("##briefing", ImVec2(0.0f, 0.0f),
                        ImGuiChildFlags_Borders,
                        ImGuiWindowFlags_None)) {
    const std::string &text = session.briefing_text();
    if (text.empty()) {
      ImGui::TextDisabled("Waiting for a briefing request.");
    } else {
      ImGui::TextWrapped("%s", text.c_str());
    }
  }
  ImGui::EndChild();
  ImGui::PopStyleColor();
}

// --- settings tab --------------------------------------------------------------

void text_field(const char *label, const char *id, char *buffer, size_t size,
                const std::function<void(const char *)> &on_change) {
  ImGui::TextDisabled("%s", label);
  ImGui::SetNextItemWidth(kFieldWidth);
  if (ImGui::InputText(id, buffer, size)) {
    on_change(buffer);
  }
  ImGui::Spacing();
}

void draw_settings_tab(const Palette &p, PanelSession &session,
                       PanelViewState &state, const PanelFrame &frame) {
  text_field("Wake word", "##wake", state.wake_word, sizeof(state.wake_word),
             [&](const char *v) { session.set_wake_word(v); });
  text_field("LLM endpoint", "##llm", state.llm_endpoint,
             sizeof(state.llm_endpoint),
             [&](const char *v) { session.set_llm_endpoint(v); });
  text_field("Model name", "##model", state.model_name, sizeof(state.model_name),
             [&](const char *v) { session.set_model_name(v); });

  ImGui::TextDisabled("Theme");
  const PanelTheme current = parse_panel_theme(session.theme());
  const PanelTheme themes[] = {PanelTheme::graphite, PanelTheme::daylight,
                               PanelTheme::night};
  for (int i = 0; i < 3; ++i) {
    if (i > 0) ImGui::SameLine();
    const std::string name = to_panel_theme_string(themes[i]);
    const bool selected = themes[i] == current;
    if (selected) {
      (void)accent_button(p, name.c_str(), ImVec2(0.0f, kButtonHeight));
    } else if (ImGui::Button(name.c_str(), ImVec2(0.0f, kButtonHeight))) {
      session.set_theme(name);
    }
  }
  ImGui::Spacing();
  ImGui::Spacing();

  if (accent_button(p, "Save", ImVec2(120.0f, kButtonHeight))) {
    session.save_settings();
    state.saved_at_ms = frame.steady_ms == 0 ? 1 : frame.steady_ms;
  }
  if (state.saved_at_ms != 0 && frame.steady_ms - state.saved_at_ms < kSavedNoteMs) {
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, vec(p.active));
    ImGui::TextUnformatted("Settings saved.");
    ImGui::PopStyleColor();
  }
}

} // namespace

// --- main panel (exported from this translation unit) --------------------------

PanelViewResult draw_panel(PanelSession &session, PanelViewState &state,
                           const PanelFrame &frame) {
  PanelViewResult result;
  const Palette &p = palette(parse_panel_theme(session.theme()));
  const ImVec2 display = ImGui::GetIO().DisplaySize;

  // One borderless ImGui window fills the X-Plane window; the panel draws its
  // own chrome.
  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
  ImGui::SetNextWindowSize(display);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kPad, kPad));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  const ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse;
  ImGui::Begin("##zoal-briefing-panel", nullptr, flags);
  ImGui::PopStyleVar(3);

  ImDrawList *bg = ImGui::GetWindowDrawList();
  bg->AddRectFilled(ImVec2(0.0f, 0.0f), display, p.glass, kRadiusLg);
  bg->AddRect(ImVec2(0.5f, 0.5f), ImVec2(display.x - 0.5f, display.y - 0.5f),
              p.border_strong, kRadiusLg, 0, 1.0f);

  push_style(p);
  const float width = display.x - kPad * 2.0f;
  draw_mast(p, session, frame, result, width);
  draw_tabs(p, state);
  if (state.tab == 0) {
    draw_home_tab(p, session);
  } else {
    draw_settings_tab(p, session, state, frame);
  }
  pop_style();

  // Anywhere off the controls is the window's grip.
  const bool on_background = ImGui::IsWindowHovered() &&
                             !ImGui::IsAnyItemHovered() && !ImGui::IsAnyItemActive();
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && on_background) {
    state.drag_active = true;
    result.drag_started = true;
  } else if (state.drag_active && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    result.dragging = true;
  } else {
    state.drag_active = false;
  }

  ImGui::End();
  return result;
}

} // namespace zoal_briefing::gui
