#ifndef ZOAL_BRIEFING_XPLANE_IMGUI_WINDOW_HPP
#define ZOAL_BRIEFING_XPLANE_IMGUI_WINDOW_HPP

#include <XPLMDisplay.h>

#include <functional>
#include <utility>

struct ImGuiContext;
struct ImTextureData;

namespace zoal_briefing::xplane {

// ImguiWindow is an X-Plane window whose contents are a Dear ImGui frame.
// It uses the panel-graphics helpers (XPLMPanelGraphics) so X-Plane renders it
// natively, and clicks never make it a browser window.
class ImguiWindow {
public:
  using BuildFn = std::function<void()>;

  struct Options {
    bool keyboard = false;
    bool resizable = false;
    int min_width = 0;
    int min_height = 0;
  };

  // Created hidden, at the given global boxel rectangle (y up).
  ImguiWindow(int left, int top, int right, int bottom, BuildFn build,
              Options options);
  ImguiWindow(int left, int top, int right, int bottom, BuildFn build)
      : ImguiWindow(left, top, right, bottom, std::move(build), Options{}) {}
  ~ImguiWindow();

  ImguiWindow(const ImguiWindow &) = delete;
  ImguiWindow &operator=(const ImguiWindow &) = delete;

  bool valid() const { return window_ != nullptr; }
  bool visible() const;
  void set_visible(bool visible);
  void bring_to_front();
  void set_geometry(int left, int top, int right, int bottom);
  void geometry(int *left, int *top, int *right, int *bottom) const;

  ImGuiContext *context() const { return context_; }

  void set_framebuffer_scale(float scale);
  float framebuffer_scale() const { return framebuffer_scale_; }

private:
  static void draw_cb(XPLMWindowID, void *refcon);
  static int click_cb(XPLMWindowID, int x, int y, XPLMMouseStatus status,
                      void *refcon);
  static void key_cb(XPLMWindowID, char, XPLMKeyFlags, char, void *, int);

  void draw();
  int click(int x, int y, XPLMMouseStatus status, int button);

  XPLMWindowID window_ = nullptr;
  ImGuiContext *context_ = nullptr;
  BuildFn build_;
  Options options_;
  float last_time_ = 0.0f;
  float framebuffer_scale_ = 1.0f;
};

} // namespace zoal_briefing::xplane

#endif
