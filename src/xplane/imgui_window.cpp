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

static_assert(sizeof(ImDrawVert) == 5 * sizeof(float),
              "XPLMMesh_t wants 20-byte vertices: pos, uv, packed RGBA8");
static_assert(sizeof(ImDrawIdx) == sizeof(std::uint16_t),
              "XPLMMesh_t wants 16-bit indices; keep ImDrawIdx at its default");

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

void render_panel(ImguiWindow *self, ImDrawData *draw_data) {
  if (draw_data->CmdListsCount == 0) return;

  int total_verts = 0, total_idxs = 0;
  for (const ImDrawList *list : draw_data->CmdLists) {
    total_verts += list->VtxBuffer.Size;
    total_idxs += list->IdxBuffer.Size;
  }

  std::vector<float> verts(total_verts * 5);
  std::vector<uint16_t> idxs(total_idxs);
  
  int voff = 0, ioff = 0;
  for (const ImDrawList *list : draw_data->CmdLists) {
    memcpy(verts.data() + voff * 5, list->VtxBuffer.Data, 
           list->VtxBuffer.Size * sizeof(ImDrawVert));
    memcpy(idxs.data() + ioff, list->IdxBuffer.Data,
           list->IdxBuffer.Size * sizeof(ImDrawIdx));
    voff += list->VtxBuffer.Size;
    ioff += list->IdxBuffer.Size;
  }

  XPLMMesh_t mesh = {};
  mesh.vertex_count = total_verts;
  mesh.vertices = verts.data();
  mesh.index_count = total_idxs;
  mesh.indices = idxs.data();

  std::vector<XPLMDrawCall_t> calls;
  for (const ImDrawList *list : draw_data->CmdLists) {
    for (const ImDrawCmd &cmd : list->CmdBuffer) {
      if (cmd.UserCallback != nullptr || cmd.ElemCount == 0) continue;
      XPLMDrawCall_t call = {};
      call.tex_ref = texture_handle(cmd.GetTexID());
      call.scissors[0] = cmd.ClipRect.x - draw_data->DisplayPos.x;
      call.scissors[1] = cmd.ClipRect.y - draw_data->DisplayPos.y;
      call.scissors[2] = cmd.ClipRect.z - draw_data->DisplayPos.x;
      call.scissors[3] = cmd.ClipRect.w - draw_data->DisplayPos.y;
      call.idx_offset = static_cast<int>(cmd.IdxOffset);
      call.element_count = static_cast<int>(cmd.ElemCount);
      call.vtx_offset = static_cast<int>(cmd.VtxOffset);
      calls.push_back(call);
    }
  }

  if (calls.empty()) return;

  int left = 0, top = 0, right = 0, bottom = 0;
  self->geometry(&left, &top, &right, &bottom);
  
  XPLMTransformPush();
  XPLMTransformTranslate(static_cast<float>(left), static_cast<float>(top));
  XPLMTransformScale(1.0f, -1.0f);

  XPLMDrawCalls(&mesh, static_cast<int>(calls.size()), calls.data());
  XPLMTransformPop();
}

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
  params.handleMouseClickFunc = click_cb;
  params.handleKeyFunc = key_cb;
  params.refcon = this;
  params.decorateAsFloatingWindow =
      options_.resizable ? xplm_WindowDecorationSelfDecoratedResizable
                         : xplm_WindowDecorationSelfDecorated;
  params.layer = xplm_WindowLayerFloatingWindows;
  params.contentType = xplm_WindowContentTypePanelGraphics;
  window_ = XPLMCreateWindowEx(&params);
  if (window_) {
    last_time_ = XPLMGetElapsedTime();
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
  return window_ && XPLMGetWindowIsVisible(window_) != 0;
}

void ImguiWindow::set_visible(bool v) {
  if (!window_) return;
  XPLMSetWindowIsVisible(window_, v ? 1 : 0);
}

void ImguiWindow::bring_to_front() {
  if (window_) XPLMBringWindowToFront(window_);
}

void ImguiWindow::set_geometry(int left, int top, int right, int bottom) {
  if (!window_) return;
  XPLMSetWindowGeometry(window_, left, top, right, bottom);
}

void ImguiWindow::geometry(int *left, int *top, int *right,
                           int *bottom) const {
  int l = 0, t = 0, r = 0, b = 0;
  if (window_) XPLMGetWindowGeometry(window_, &l, &t, &r, &b);
  if (left) *left = l;
  if (top) *top = t;
  if (right) *right = r;
  if (bottom) *bottom = b;
}

void ImguiWindow::draw() {
  int left = 0, top2 = 0, right = 0, bottom = 0;
  geometry(&left, &top2, &right, &bottom);
  
  float w = static_cast<float>(right - left);
  float h = static_cast<float>(top2 - bottom);
  if (w <= 0 || h <= 0) return;

  ContextScope scope(context_);
  ImGuiIO &io = ImGui::GetIO();
  io.DisplaySize = ImVec2(w, h);
  
  const float now = XPLMGetElapsedTime();
  io.DeltaTime = (last_time_ > 0.0f && now > last_time_) ? now - last_time_ : 1.0f / 60.0f;
  last_time_ = now;

  ImGui::NewFrame();
  if (build_) build_();
  ImGui::Render();

  ImDrawData *draw_data = ImGui::GetDrawData();
  render_panel(this, draw_data);
}

int ImguiWindow::click(int x, int y, XPLMMouseStatus status, int button) {
  ContextScope scope(context_);
  ImGuiIO &io = ImGui::GetIO();
  
  int left = 0, top2 = 0, right = 0, bottom = 0;
  if (window_) XPLMGetWindowGeometry(window_, &left, &top2, &right, &bottom);
  float wx = static_cast<float>(x - left);
  float wy = static_cast<float>(top2 - y);
  
  switch (status) {
    case xplm_MouseDown: 
      io.MouseDown[button] = true; 
      io.MousePos = ImVec2(wx, wy); 
      break;
    case xplm_MouseUp: 
      io.MouseDown[button] = false; 
      break;
    case xplm_MouseDrag: 
      io.MousePos = ImVec2(wx, wy); 
      break;
  }
  
  ImGui::NewFrame();
  if (build_) build_();
  ImGui::Render();

  ImDrawData *draw_data = ImGui::GetDrawData();
  render_panel(this, draw_data);
  
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

void ImguiWindow::key_cb(XPLMWindowID w, char key, XPLMKeyFlags flags, 
                        char virtual_key, void *ref, int user_data) {
  (void)w; (void)key; (void)flags; (void)virtual_key; (void)ref; (void)user_data;
}

} // namespace zoal_briefing::xplane
