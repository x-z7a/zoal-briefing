#ifndef ZOAL_BRIEFING_GUI_PANEL_VIEW_HPP
#define ZOAL_BRIEFING_GUI_PANEL_VIEW_HPP

#include "zoal_briefing/gui/panel_session.hpp"
#include "zoal_briefing/gui/panel_theme.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct ImFont;

namespace zoal_briefing::gui {

// --- panel result --------------------------------------------------------------

struct PanelViewResult {
  // The pilot pressed on the panel background (not a control) this frame, or
  // is still holding it: the window follows the mouse.
  bool drag_started = false;
  bool dragging = false;
  // The close button was pressed.
  bool close = false;
};

// What the view keeps between frames that the session has no business
// holding: which tab is up, what is half-typed, where the log is scrolled.
struct PanelViewState {
  int tab = 0; // 0 Home, 1 Settings
  
  // Settings fields.
  char wake_word[64] = {};
  char llm_endpoint[512] = {};
  char model_name[128] = {};
  
  // Track whether settings have been loaded from disk.
  bool settings_loaded = false;

  // A background drag is in progress (started on the panel, mouse still down).
  bool drag_active = false;

  // When the last save happened, for the "Settings saved." note.
  uint64_t saved_at_ms = 0;
  
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
  // The semibold weight for headings; nullptr uses the default font.
  ImFont *title_font = nullptr;
};

// Draws one frame of the briefing panel. Returns true when a tab change or
// save needs to trigger an action callback.
PanelViewResult draw_panel(PanelSession &session, PanelViewState &state,
                           const PanelFrame &frame);

} // namespace zoal_briefing::gui

#endif
