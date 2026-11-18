#include "zoal_briefing/gui/panel_theme.hpp"

namespace zoal_briefing::gui {

// --- palette ------------------------------------------------------------------

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

} // namespace zoal_briefing::gui
