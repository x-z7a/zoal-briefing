#include "zoal_briefing/gui/panel_theme.hpp"

#include <array>
#include <string_view>

namespace zoal_briefing::gui {

namespace {

constexpr std::array<std::string_view, 3> kThemes = {"graphite", "daylight", "night"};

} // namespace

std::optional<std::string> known_panel_theme(std::string_view name) {
  for (const auto theme : kThemes) {
    if (name == theme) {
      return std::string(theme);
    }
  }
  return std::nullopt;
}

std::string panel_theme_or_default(std::string_view stored) {
  return known_panel_theme(stored).value_or(std::string(kDefaultPanelTheme));
}

} // namespace zoal_briefing::gui
