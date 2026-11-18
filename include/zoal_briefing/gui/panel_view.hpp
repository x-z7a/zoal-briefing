#ifndef ZOAL_BRIEFING_GUI_PANEL_VIEW_HPP
#define ZOAL_BRIEFING_GUI_PANEL_VIEW_HPP

#include "zoal_briefing/gui/panel_session.hpp"
#include "zoal_briefing/gui/panel_theme.hpp"
#include <functional>
#include <string>
#include <vector>
#include <cstdint>

namespace zoal_briefing::gui {

// --- the typeface --------------------------------------------------------------

struct Typeface {
  // ImFont* pointers are filled in at runtime by the panel window.
  void *regular = nullptr;   // ImGuiFont*
  void *semibold = nullptr;  // ImGuiFont*  
  void *bold = nullptr;      // ImGuiFont*
  float em_to_line = 1.2f;
  float css(float px) const { return px * em_to_line; }
};

// --- panel view state (pilot edits, which tab is up) ---------------------------

struct PanelViewState {
  int tab = 0;  // 0 Home, 1 Settings
  
  // Settings fields.
  char wake_word[64] = {};
  char llm_endpoint[512] = {};
  char model_name[128] = {};
  
  // Track whether settings have been loaded from disk.
  bool wake_word_loaded = false;
  bool llm_endpoint_loaded = false;
  bool model_name_loaded = false;
  
  // Stored copies for dirty-checking (reload when plugin value changes).
  std::string wake_word_stored;
  std::string llm_endpoint_stored;
  std::string model_name_stored;
};

// One frame's context.
struct PanelFrame {
  std::string version;
  uint64_t steady_ms = 0;
  int64_t wall_ms = 0;
  // Deferred actions (rendered after the next draw call).
  std::vector<std::function<void()>> *later = nullptr;
};

// --- panel result ----------------------------------------------------------------

struct PanelViewResult {
  bool drag_started = false;
  bool dragging = false;
};

// Draws one frame of the briefing panel. Returns true when a tab change or
// save needs to trigger an action callback.
PanelViewResult draw_panel(PanelSession &session, PanelViewState &state,
                           const Typeface &face, const PanelFrame &frame,
                           std::function<void(const std::string &action,
                                              const std::string &payload)> on_action);

} // namespace zoal_briefing::gui

#endif // ZOAL_BRIEFING_GUI_PANEL_VIEW_HPP
