#ifndef ZOAL_BRIEFING_GUI_PANEL_WINDOW_HPP
#define ZOAL_BRIEFING_GUI_PANEL_WINDOW_HPP

#include "zoal_briefing/gui/panel_session.hpp"
#include <memory>
#include <string>

struct ImGuiWindow;
struct ImFont;

namespace zoal_briefing::xplane {

// The panel window: what the pilot sees and touches in X-Plane.
class PanelWindow {
public:
  struct Options {
    int open_width = 1120;
    int open_height = 740;
    int min_width = 760;
    int min_height = 320;
  };

  ~PanelWindow();
  
  // Open the panel at its initial size, centred on the main display.
  void show(const PanelSession &session, const Options &options = {});
  
  // Close and destroy the panel.
  void hide();
  
  // Whether the panel is currently visible.
  bool visible() const;

  // Process UI actions from the panel view (e.g., save settings).
  void on_action(const std::string &action, const std::string &payload);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// Called by X-Plane's flight loop to update the panel.
void tick_panel();

} // namespace zoal_briefing::xplane

#endif // ZOAL_BRIEFING_GUI_PANEL_WINDOW_HPP
