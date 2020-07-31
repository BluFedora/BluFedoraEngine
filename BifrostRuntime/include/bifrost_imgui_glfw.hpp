#ifndef BIFROST_IMGUI_GLFW
#define BIFROST_IMGUI_GLFW

#include "bifrost/graphics/bifrost_gfx_handle.h"
#include "bifrost/platform/bifrost_platform_fwd.h"

namespace bifrost
{
  using Event = bfEvent;

  namespace imgui
  {
    void startup(bfGfxContextHandle graphics, BifrostWindow* window);
    void onEvent(BifrostWindow* target_window, Event& evt);
    void beginFrame(bfTextureHandle surface, float window_width, float window_height, float delta_time);
    void endFrame();
    void shutdown();

    // Helpers

    void setupDefaultRenderPass(bfGfxCommandListHandle command_list, bfTextureHandle surface);
  }  // namespace imgui

}  // namespace bifrost

#endif /* BIFROST_IMGUI_GLFW */