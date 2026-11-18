#include "zoal_briefing/gui/panel_view.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
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

static unsigned rgba(int r, int g, int b, float a = 1.0f) {
  return static_cast<unsigned>((1.0f - a) * 255.0f) << 24 | 
         (static_cast<unsigned>(r & 0xff) << 16) |
         (static_cast<unsigned>(g & 0xff) << 8) |
         static_cast<unsigned>(b & 0xff);
}

static unsigned faded(unsigned col, float t) {
  unsigned r = static_cast<unsigned>(((col >> 16) & 0xff) * t);
  unsigned g = static_cast<unsigned>(((col >> 8) & 0xff) * t);
  unsigned b = static_cast<unsigned>((col & 0xff) * t);
  return static_cast<unsigned>((1.0f - t) * 255.0f) << 24 | r << 16 | g << 8 | b;
}

// --- the palette: per theme ---------------------------------------------------

struct Palette {
  unsigned glass, surface, raised, glass_card;
  unsigned border, border_strong;
  unsigned text, text_dim, text_feed;
  unsigned accent, on_accent;
  unsigned outline;
  unsigned active, warn, danger;
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

constexpr float kPadTop = 12.0f;
constexpr float kPadX = 14.0f;
constexpr float kPadBottom = 14.0f;
constexpr float kGap = 10.0f;
constexpr float kRadiusLg = 14.0f;
constexpr float kButtonHeight = 34.0f;
constexpr float kTabsHeight = 36.0f;
constexpr float kMastHeight = 44.0f;

// --- tabs: segmented control --------------------------------------------------

static void draw_tabs(const Palette &p, PanelViewState &state,
                      ImVec2 origin, float width) {
  const char *labels[] = {"Home", "Settings"};
  float total_w = 0;
  float widths[2];
  for (int i = 0; i < 2; ++i) {
    ImVec2 tw = ImGui::CalcTextSize(labels[i], nullptr, false, 100);
    widths[i] = tw.x + 32.0f;
    total_w += widths[i];
  }

  float x = origin.x + (width - total_w) * 0.5f;
  ImDrawList *draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + kTabsHeight),
                      p.surface, 8.0f);
  draw->AddRect(origin, ImVec2(origin.x + width, origin.y + kTabsHeight),
                p.border_strong, 8.0f, 1.0f);

  float cx = x;
  for (int i = 0; i < 2; ++i) {
    bool active = state.tab == i;
    ImVec2 tp(cx, origin.y);
    draw->AddRectFilled(tp,
                        ImVec2(tp.x + widths[i], tp.y + kTabsHeight),
                        active ? p.accent : p.surface,
                        8.0f);
    draw->AddRect(tp,
                  ImVec2(tp.x + widths[i], tp.y + kTabsHeight),
                  active ? p.on_accent : p.border_strong,
                  8.0f, 1.0f);
    draw->AddText(nullptr, 14.0f,
                  ImVec2(tp.x + 16.f, tp.y + (kTabsHeight - 18.f) * 0.5f),
                  active ? p.on_accent : p.text, labels[i]);
    cx += widths[i];
  }
}

// --- home tab ------------------------------------------------------------------

static void draw_home_tab(const Palette &p, PanelSession &session,
                          ImVec2 origin, float width) {
  (void)p; (void)origin; (void)width;
  
  ImDrawList *draw = ImGui::GetWindowDrawList();
  float y = origin.y + kPadX;

  draw->AddText(nullptr, 14.0f, ImVec2(origin.x + kPadX, y), p.text,
                "zoal-briefing");
  y += 28.0f;

  const char *status_labels[] = {
    "[IDLE]", "[WAKE DETECTED]", "[LISTENING]", "[STT COMPLETE]",
    "[PARSING]", "[LLM ROUTING]", "[BRIEFING GENERATING]", "[PLAYBACK]"
  };
  
  // Draw status indicator.
  draw->AddText(nullptr, 13.0f, ImVec2(origin.x + kPadX, y), p.text_dim,
                "Status: ");
  PluginStatus status = session.status();
  const char *status_label = "[???]";
  if (status >= PluginStatus::IDLE && status < PluginStatus::COUNT) {
    status_label = status_labels[static_cast<int>(status)];
  }
  ImVec2 status_pos(origin.x + kPadX + ImGui::CalcTextSize("Status: ").x, y);
  draw->AddText(nullptr, 13.0f, status_pos, p.text, status_label);
  
  y += 26.0f;
  
  // Airport field.
  const std::string &airport = session.airport();
  if (!airport.empty()) {
    draw->AddText(nullptr, 14.0f, ImVec2(origin.x + kPadX, y), p.text, "Airport:");
    y += 22.0f;
    
    bool editable = session.connected();
    if (editable) {
      draw->AddRectFilled(ImVec2(origin.x + kPadX - 4, y - 4),
                          ImVec2(origin.x + width - kPadX + 4, y + 30),
                          p.glass_card, 8.0f);
    }
    
    draw->AddText(nullptr, 14.0f, 
                  ImVec2(origin.x + kPadX, y + 6.f),
                  editable ? p.accent : p.text, airport.c_str());
    y += 36.0f;
  }

  // Briefing text area.
  draw->AddText(nullptr, 14.0f, ImVec2(origin.x + kPadX, y), p.text, "Briefing:");
  y += 22.0f;

  const std::string &briefing_text = session.briefing_text();
  if (briefing_text.empty()) {
    draw->AddText(nullptr, 14.0f, ImVec2(origin.x + kPadX, y), p.text_dim,
                  "— waiting —");
  } else {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(p.text));
    ImGui::Text("%s", briefing_text.c_str());
    ImGui::PopStyleColor();
  }
}

// --- settings tab -----------------------------------------------------------------

static void draw_settings_tab(const Palette &p, PanelSession &session,
                              PanelViewState &state, ImVec2 origin, float width) {
  (void)p; (void)origin; (void)width;
  
  ImDrawList *draw = ImGui::GetWindowDrawList();
  float y = origin.y + kPadX;

  draw->AddText(nullptr, 14.0f, ImVec2(origin.x + kPadX, y), p.text, "Settings");
  y += 30.0f;

  // Wake word field
  draw->AddText(nullptr, 14.0f, ImVec2(origin.x + kPadX, y), p.text, "Wake word");
  y += 22.0f;
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::ColorConvertU32ToFloat4(p.glass_card));
  ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(p.border));
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(p.text_feed));
  if (ImGui::InputText("##wake", state.wake_word, sizeof(state.wake_word))) {
    session.set_wake_word(state.wake_word);
  }
  ImGui::PopStyleColor(3);
  y += 42.0f;

  // LLM endpoint field
  draw->AddText(nullptr, 14.0f, ImVec2(origin.x + kPadX, y), p.text, "LLM endpoint");
  y += 22.0f;
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::ColorConvertU32ToFloat4(p.glass_card));
  ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(p.border));
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(p.text_feed));
  if (ImGui::InputText("##llm", state.llm_endpoint, sizeof(state.llm_endpoint))) {
    session.set_llm_endpoint(state.llm_endpoint);
  }
  ImGui::PopStyleColor(3);
  y += 42.0f;

  // Model name field
  draw->AddText(nullptr, 14.0f, ImVec2(origin.x + kPadX, y), p.text, "Model name");
  y += 22.0f;
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::ColorConvertU32ToFloat4(p.glass_card));
  ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(p.border));
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(p.text_feed));
  if (ImGui::InputText("##model", state.model_name, sizeof(state.model_name))) {
    session.set_model_name(state.model_name);
  }
  ImGui::PopStyleColor(3);
  y += 28.0f;

  // Save button
  y += 8.0f;
  float save_y = y + 16.0f;
  ImVec2 btn_pos(origin.x + kPadX, save_y);
  float btn_w = width - kPadX * 2.0f;
  ImGuiIO &io = ImGui::GetIO();

  bool mouse_over = (io.MousePos.x >= btn_pos.x && io.MousePos.x <= btn_pos.x + btn_w &&
                    io.MousePos.y >= save_y && io.MousePos.y <= save_y + kButtonHeight);

  draw->AddRectFilled(btn_pos, ImVec2(btn_pos.x + btn_w, btn_pos.y + kButtonHeight),
                     mouse_over ? p.accent : rgba(0x3d, 0x31, 0x21),
                     kRadiusLg);
  draw->AddRect(btn_pos, ImVec2(btn_pos.x + btn_w, btn_pos.y + kButtonHeight),
               p.border_strong, kRadiusLg, 1.0f);

  ImVec2 btn_size = ImGui::CalcTextSize("Save", nullptr, false, btn_w);
  draw->AddText(nullptr, 14.0f,
               ImVec2(btn_pos.x + (btn_w - btn_size.x) * 0.5f,
                     save_y + (kButtonHeight - 18.f) * 0.5f),
               mouse_over ? p.on_accent : rgba(0xf8, 0xdc, 0xb2), "Save");

  bool saved = false;
  if (mouse_over && io.MouseClicked[0]) {
    saved = true;
  }

  if (saved) {
    session.save_settings();
  }

  if (saved) {
    draw->AddText(nullptr, 14.0f, ImVec2(origin.x + kPadX, save_y + 40.f),
                 p.active, "Settings saved.");
  }
}

// Close anonymous namespace here — all exports go below.

} // namespace

// --- main panel (exported from this translation unit) --------------------------

PanelViewResult draw_panel(PanelSession &session, PanelViewState &state,
                           const PanelFrame &frame) {
  (void)frame;

  PanelViewResult result;
  const Palette &p = palette(parse_panel_theme(session.theme()));

  ImGuiIO &io = ImGui::GetIO();
  const ImVec2 display = io.DisplaySize;

  ImGuiStyle &style = ImGui::GetStyle();
  style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
  style.Colors[ImGuiCol_ScrollbarGrab] =
      ImGui::ColorConvertU32ToFloat4(p.border_strong);
  style.Colors[ImGuiCol_ScrollbarGrabHovered] =
      ImGui::ColorConvertU32ToFloat4(faded(p.text_dim, 0.5f));
  style.Colors[ImGuiCol_ScrollbarGrabActive] =
      ImGui::ColorConvertU32ToFloat4(faded(p.text_dim, 0.7f));

  ImDrawList *draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(ImVec2(0.0f, 0.0f), display, p.glass, kRadiusLg);
  draw->AddRect(ImVec2(0.0f, 0.0f), display, p.border_strong, kRadiusLg, 1.0f);

  // Mast: version text.
  {
    ImVec2 mast_min(kPadX, kPadTop);
    ImVec2 mast_max(display.x - kPadX, kPadTop + kMastHeight);
    draw->AddRectFilled(mast_min, mast_max, rgba(0x00, 0x00, 0x00, 0.15f),
                       kRadiusLg);
    std::string version_str("zoal-briefing v" + frame.version);
    draw->AddText(nullptr, 11.0f, ImVec2(mast_min.x + 8.f, kPadTop + 8.f),
                 p.text_dim, version_str.c_str());
  }

  // Tabs.
  float tab_y = kPadTop + kMastHeight + kGap;
  draw_tabs(p, state, ImVec2(kPadX, tab_y), display.x - kPadX * 2.0f);
  tab_y += kTabsHeight + kGap;

  // Content area based on selected tab.
  if (state.tab == 0) {
    draw_home_tab(p, session, ImVec2(kPadX, tab_y),
                  display.x - kPadX * 2.0f);
  } else {
    draw_settings_tab(p, session, state, ImVec2(kPadX, tab_y),
                     display.x - kPadX * 2.0f);
  }

  // Panel grip.
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered() &&
      !ImGui::IsAnyItemActive()) {
    result.drag_started = true;
  }

  return result;
}

} // namespace zoal_briefing::gui
