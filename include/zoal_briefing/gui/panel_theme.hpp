#ifndef ZOAL_BRIEFING_GUI_PANEL_THEME_HPP
#define ZOAL_BRIEFING_GUI_PANEL_THEME_HPP

#include <optional>
#include <string>
#include <string_view>

namespace zoal_briefing::gui {

enum class PanelTheme {
  graphite,
  daylight,
  night
};

// The colour themes the panel and HUD ship, by name (the palettes are in
// src/gui/panel_theme.cpp). Stored as `panel_theme` in zoal-briefing.json,
// so both windows agree on one.
inline constexpr std::string_view kDefaultPanelTheme = "graphite";

// known_panel_theme answers the name if it is one the panel can draw, nullopt
// otherwise. A save of anything else is refused rather than stored, so the
// config file never holds a theme the panel would have to guess about.
std::optional<std::string> known_panel_theme(std::string_view name);

// panel_theme_or_default reads a stored value: a stale or hand-edited name
// falls back to graphite instead of leaving the panel unthemed.
PanelTheme panel_theme_or_default(std::string_view stored);

// parse_panel_theme converts a string name to PanelTheme enum.
PanelTheme parse_panel_theme(std::string_view name);

// to_panel_theme_string converts PanelTheme enum to its display string.
std::string to_panel_theme_string(PanelTheme theme);

} // namespace zoal_briefing::gui

#endif // ZOAL_BRIEFING_GUI_PANEL_THEME_HPP
