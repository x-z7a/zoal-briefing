#include "imgui_window.hpp"

#include <XPLMPanelGraphics.h>
#include <XPLMProcessing.h>

#include <cfloat>
#include <cstdint>
#include <utility>
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"

namespace zoal_briefing::xplane {

// The panel-graphics helpers take ImGui's buffers as they are.
static_assert(sizeof(ImDrawVert) == 5 * sizeof(float),
              "XPLMMesh_t wants 20-byte vertices: pos, uv, packed RGBA8");
static_assert(sizeof(ImDrawIdx) == sizeof(std::uint16_t),
              "XPLMMesh_t wants 16-bit indices");

namespace {

void *texture_handle(ImTextureID id) {
  return reinterpret_cast<void *>(static_cast<std::uintptr_t>(id));
}

class ContextScope {
public:
  explicit ContextScope(ImGuiContext *context)
      : previous_(ImGui::GetCurrentContext()) {
    ImGui::SetCurrentContext(context);
  }
  ~ContextScope() { ImGui::SetCurrentContext(previous_); }
  ContextScope(const ContextScope &) = delete;
  ContextScope &operator=(const ContextScope &) = delete;

private:
  ImGuiContext *previous_;
};

} // namespace

ImguiWindow::ImguiWindow(int left, int top, int right, int bottom,
                         BuildFn build, Options options)
    : build_(std::move(build)), options_(options) {
  context_ = ImGui::CreateContext();
  {
    ContextScope scope(context_);
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.BackendPlatformName = "zoal-briefing xplm";
    io.BackendRendererName = "zoal-briefing xplm panel graphics";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    io.Fonts->TexDesiredFormat = ImTextureFormat_RGBA32;
    io.ConfigMacOSXBehaviors = false;
    io.ConfigInputTrickleEventQueue = true;
  }

  XPLMCreateWindow_t params = {};
  params.structSize = sizeof(params);
  params.left = left;
  params.top = top;
  params.right = right;
  params.bottom = bottom;
  params.visible = 0;
  params.drawWindowFunc = draw_cb;
  params.mouseClickCallback = click_cb;
  params.keyDownAction = key_cb;
  params.ref = this;

  window_ = XPLMCreateWindow(&params);
  if (window_) {
    last_time_ = XPLMGetTime();
  }
}

ImguiWindow::~ImguiWindow() {
  if (window_) {
    XPLMDestroyWindow(window_);
    window_ = nullptr;
  }
  ImGui::DestroyContext(context_);
  context_ = nullptr;
}

bool ImguiWindow::visible() const {
  if (!window_) return false;
  return XPLMGetWindowIsVisible(window_) == 1;
}

void ImguiWindow::set_visible(bool v) {
  if (!window_) return;
  if (v) {
    XPLMShowWindow(window_);
  } else {
    XPLMHideWindow(window_);
  }
}

void ImguiWindow::bring_to_front() {
  if (window_) XPLMBringWindowToFront(window_);
}

void ImguiWindow::set_geometry(int left, int top, int right, int bottom) {
  if (!window_) return;
  XPLMSetWindowGeometry(window_, left, top, right, bottom);
}

void ImguiWindow::geometry(int *left, int *top, int *right, int *bottom) const {
  if (window_) XPLMGetWindowInfo(window_, left, top, right, bottom);
}

void ImguiWindow::set_framebuffer_scale(float scale) {
  framebuffer_scale_ = scale;
}

void ImguiWindow::draw() {
  int left = 0, top2 = 0, right = 0, bottom = 0;
  if (window_) XPLMGetWindowInfo(window_, &left, &top2, &right, &bottom);
  
  float w = static_cast<float>(right - left);
  float h = static_cast<float>(top2 - bottom);
  if (w <= 0 || h <= 0) return;

  ImGuiIO &io = ImGui::GetIO();
  io.DisplaySize = ImVec2(w, h);
  
  float time = XPLMGetTime();
  float dt = std::max(time - last_time_, 0.0f);
  last_time_ = time;
  io.DeltaTime = dt;

  // Keyboard state (simplified)
  for (int i = 0; i < 256; ++i) {
    bool down = XPLMIsKeyDown(i);
    io.KeysDown[i] = down;
    if (down) io.KeyDownDuration[i] = dt;
  }
  io.KeyShift = io.KeysDown[VK_LSHIFT] || io.KeysDown[VK_RSHIFT];
  io.KeyCtrl = io.KeysDown[VK_LCONTROL] || io.KeysDown[VK_RCONTROL];
  io.KeyAlt = io.KeysDown[VK_LMENU] || io.KeysDown[VK_RMENU];

  ContextScope scope(context_);
  
  ImGui::NewFrame();
  build_();
  ImGui::Render();

  // Render with X-Plane panel graphics
  int l = 0, t = 0, r = 0, b = 0;
  if (window_) XPLMGetWindowInfo(window_, &l, &t, &r, &b);
  
  float pw = static_cast<float>(r - l) * framebuffer_scale_;
  float ph = static_cast<float>(t - b) * framebuffer_scale_;
  
  ImDrawData *draw_data = ImGui::GetDrawData();
  
  XPLMDrawPanel_t info = {};
  info.structSize = sizeof(info);
  info.left = l;
  info.top = t;
  info.right = r;
  info.bottom = b;
  info.pixelScaleX = framebuffer_scale_;
  info.pixelScaleY = framebuffer_scale_;
  
  XPLMDrawPanel(draw_data, &info);
}

int ImguiWindow::click(int x, int y, XPLMMouseStatus status, int button) {
  ContextScope scope(context_);
  ImGuiIO &io = ImGui::GetIO();
  
  // Convert from panel coordinates to local window coords
  int left = 0, top2 = 0, right = 0, bottom = 0;
  if (window_) XPLMGetWindowInfo(window_, &left, &top2, &right, &bottom);
  float wx = static_cast<float>(x - left);
  float wy = static_cast<float>(top2 - y);
  
  switch (status) {
    case xplm_MouseDown: 
      io.MouseDown[button] = true; 
      io.MousePos = ImVec2(wx, wy); 
      break;
    case xplm_Up: 
      io.MouseDown[button] = false; 
      break;
    case xplm_Drag: 
      io.MousePos = ImVec2(wx, wy); 
      break;
  }
  
  ImGui::NewFrame();
  build_();
  ImGui::Render();
  
  // Re-render with panel graphics after click
  int l = 0, t = 0, r = 0, b = 0;
  if (window_) XPLMGetWindowInfo(window_, &l, &t, &r, &b);
  float pw = static_cast<float>(r - l) * framebuffer_scale_;
  float ph = static_cast<float>(t - b) * framebuffer_scale_;
  
  ImDrawData *draw_data = ImGui::GetDrawData();
  XPLMDrawPanel_t info = {};
  info.structSize = sizeof(info);
  info.left = l; info.top = t;
  info.right = r; info.bottom = b;
  info.pixelScaleX = framebuffer_scale_;
  info.pixelScaleY = framebuffer_scale_;
  XPLMDrawPanel(draw_data, &info);
  
  return 1;
}

void ImguiWindow::draw_cb(XPLMWindowID w, void *ref) {
  auto *self = reinterpret_cast<ImguiWindow *>(ref);
  self->draw();
}

int ImguiWindow::click_cb(XPLMWindowID w, int x, int y, XPLMMouseStatus status, void *ref) {
  auto *self = reinterpret_cast<ImguiWindow *>(ref);
  return self->click(x, y, status, 0);
}

void ImguiWindow::key_cb(XPLMWindowID w, char key, XPLMKeyFlags flags, char virtual_key, void *ref, int user_data) {
  (void)w; (void)flags; (void)virtual_key; (void)ref; (void)user_data;
}

} // namespace zoal_briefing::xplane
