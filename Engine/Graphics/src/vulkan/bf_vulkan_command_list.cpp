#include "bf_vulkan_logical_device.h"

#include "bf_vulkan_conversions.h" /* bfVKConvert* */
#include "bf_vulkan_hash.hpp"      /* hash         */

#include <cassert> /* assert */

#define CUSTOM_ALLOC nullptr

extern VkImageAspectFlags bfTexture__aspect(bfTextureHandle self);

static void AddCachedResource(bfGfxDeviceHandle device, bfBaseGfxObject* obj, std::uint64_t hash_code)
{
  obj->hash_code           = hash_code;
  obj->next                = device->cached_resources;
  device->cached_resources = obj;
}

void UpdateResourceFrame(bfBaseGfxObject* obj);

bfWindowSurfaceHandle bfGfxCmdList_window(bfGfxCommandListHandle self)
{
  return self->window;
}

bfBool32 bfGfxCmdList_begin(bfGfxCommandListHandle self)
{
  VkCommandBufferBeginInfo begin_info;
  begin_info.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.pNext            = NULL;
  begin_info.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  begin_info.pInheritanceInfo = NULL;

  const VkResult error = vkBeginCommandBuffer(self->handle, &begin_info);

  self->dynamic_state_dirty = 0xFFFF;

  assert(error == VK_SUCCESS);

  return error == VK_SUCCESS;
}

void bfGfxCmdList_pipelineBarriers(bfGfxCommandListHandle self, bfGfxPipelineStageBits src_stage, bfGfxPipelineStageBits dst_stage, const bfPipelineBarrier* barriers, uint32_t num_barriers, bfBool32 reads_same_pixel)
{
  std::uint32_t         num_memory_barriers = 0;
  VkMemoryBarrier       memory_barriers[k_bfGfxMaxPipelineBarrierWrites];
  std::uint32_t         num_buffer_barriers = 0;
  VkBufferMemoryBarrier buffer_barriers[k_bfGfxMaxPipelineBarrierWrites];
  std::uint32_t         num_image_barriers = 0;
  VkImageMemoryBarrier  image_barriers[k_bfGfxMaxPipelineBarrierWrites];

  const std::uint32_t* queue_list = self->parent->parent->queue_list.family_index;

  for (uint32_t i = 0; i < num_barriers; ++i)
  {
    const bfPipelineBarrier* const pl_barrier = barriers + i;

    switch (pl_barrier->type)
    {
      case BF_PIPELINE_BARRIER_MEMORY:
      {
        assert(num_memory_barriers < k_bfGfxMaxPipelineBarrierWrites);

        VkMemoryBarrier* const barrier = memory_barriers + num_memory_barriers;
        barrier->sType                 = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier->pNext                 = nullptr;
        barrier->srcAccessMask         = bfVkConvertAccessFlags(pl_barrier->access[0]);
        barrier->dstAccessMask         = bfVkConvertAccessFlags(pl_barrier->access[1]);

        ++num_memory_barriers;
        break;
      }
      case BF_PIPELINE_BARRIER_BUFFER:
      {
        assert(num_buffer_barriers < k_bfGfxMaxPipelineBarrierWrites);

        VkBufferMemoryBarrier* const barrier = buffer_barriers + num_buffer_barriers;
        barrier->sType                       = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier->pNext                       = nullptr;
        barrier->srcAccessMask               = bfVkConvertAccessFlags(pl_barrier->access[0]);
        barrier->dstAccessMask               = bfVkConvertAccessFlags(pl_barrier->access[1]);
        barrier->srcQueueFamilyIndex         = bfConvertQueueIndex(queue_list, pl_barrier->queue_transfer[0]);
        barrier->dstQueueFamilyIndex         = bfConvertQueueIndex(queue_list, pl_barrier->queue_transfer[1]);
        barrier->buffer                      = pl_barrier->info.buffer.handle->handle;
        barrier->offset                      = pl_barrier->info.buffer.offset;
        barrier->size                        = pl_barrier->info.buffer.size;

        ++num_buffer_barriers;
        break;
      }
      case BF_PIPELINE_BARRIER_IMAGE:
      {
        assert(num_image_barriers < k_bfGfxMaxPipelineBarrierWrites);

        VkImageMemoryBarrier* const barrier      = image_barriers + num_image_barriers;
        barrier->sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier->pNext                           = nullptr;
        barrier->srcAccessMask                   = bfVkConvertAccessFlags(pl_barrier->access[0]);
        barrier->dstAccessMask                   = bfVkConvertAccessFlags(pl_barrier->access[1]);
        barrier->oldLayout                       = bfVkConvertImgLayout(pl_barrier->info.image.layout_transition[0]);
        barrier->newLayout                       = bfVkConvertImgLayout(pl_barrier->info.image.layout_transition[1]);
        barrier->srcQueueFamilyIndex             = bfConvertQueueIndex(queue_list, pl_barrier->queue_transfer[0]);
        barrier->dstQueueFamilyIndex             = bfConvertQueueIndex(queue_list, pl_barrier->queue_transfer[1]);
        barrier->image                           = pl_barrier->info.image.handle->tex_image;
        barrier->subresourceRange.aspectMask     = bfTexture__aspect(pl_barrier->info.image.handle);
        barrier->subresourceRange.baseMipLevel   = pl_barrier->info.image.base_mip_level;
        barrier->subresourceRange.levelCount     = pl_barrier->info.image.level_count;
        barrier->subresourceRange.baseArrayLayer = pl_barrier->info.image.base_array_layer;
        barrier->subresourceRange.layerCount     = pl_barrier->info.image.layer_count;

        pl_barrier->info.image.handle->tex_layout = pl_barrier->info.image.layout_transition[1];

        ++num_image_barriers;
        break;
      }
        bfInvalidDefaultCase();
    }
  }

  vkCmdPipelineBarrier(
   self->handle,
   bfVkConvertPipelineStageFlags(src_stage),
   bfVkConvertPipelineStageFlags(dst_stage),
   reads_same_pixel ? VK_DEPENDENCY_BY_REGION_BIT : 0x0,
   num_memory_barriers,
   memory_barriers,
   num_buffer_barriers,
   buffer_barriers,
   num_image_barriers,
   image_barriers);
}

void bfGfxCmdList_setRenderpass(bfGfxCommandListHandle self, bfRenderpassHandle renderpass)
{
  self->pipeline_state.renderpass = renderpass;
  UpdateResourceFrame(&renderpass->super);
}

void bfGfxCmdList_setRenderpassInfo(bfGfxCommandListHandle self, const bfRenderpassInfo* renderpass_info)
{
  const uint64_t hash_code = bf::gfx_hash::hash(0x0, renderpass_info);

  bfRenderpassHandle rp = self->parent->cache_renderpass.find(hash_code, *renderpass_info);

  if (!rp)
  {
    rp = bfGfxDevice_newRenderpass(self->parent, renderpass_info);
    self->parent->cache_renderpass.insert(hash_code, rp, *renderpass_info);
    AddCachedResource(self->parent, &rp->super, hash_code);
  }

  bfGfxCmdList_setRenderpass(self, rp);
}

void bfGfxCmdList_setClearValues(bfGfxCommandListHandle self, const bfClearValue* clear_values)
{
  const std::size_t num_clear_colors = self->pipeline_state.renderpass->info.num_attachments;

  for (std::size_t i = 0; i < num_clear_colors; ++i)
  {
    const bfClearValue* const src_color = clear_values + i;
    VkClearValue* const       dst_color = self->clear_colors + i;

    *dst_color = bfVKConvertClearColor(src_color);
  }
}

void bfGfxCmdList_setAttachments(bfGfxCommandListHandle self, bfTextureHandle* attachments)
{
  const uint32_t num_attachments = self->pipeline_state.renderpass->info.num_attachments;
  const uint64_t hash_code       = bf::vk::hash(0x0, attachments, num_attachments);

  bfFramebufferState fb_state;
  fb_state.num_attachments = num_attachments;

  for (uint32_t i = 0; i < num_attachments; ++i)
  {
    fb_state.attachments[i] = attachments[i];
  }

  bfFramebufferHandle fb = self->parent->cache_framebuffer.find(hash_code, fb_state);

  if (!fb)
  {
    VkImageView image_views[k_bfGfxMaxAttachments];

    fb = new bfFramebuffer();

    bfBaseGfxObject_ctor(&fb->super, BF_GFX_OBJECT_FRAMEBUFFER);

    for (uint32_t i = 0; i < num_attachments; ++i)
    {
      image_views[i] = attachments[i]->tex_view;
    }

    VkFramebufferCreateInfo frame_buffer_create_params;
    frame_buffer_create_params.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    frame_buffer_create_params.pNext           = nullptr;
    frame_buffer_create_params.flags           = 0;
    frame_buffer_create_params.renderPass      = self->pipeline_state.renderpass->handle;
    frame_buffer_create_params.attachmentCount = num_attachments;
    frame_buffer_create_params.pAttachments    = image_views;
    frame_buffer_create_params.width           = static_cast<uint32_t>(attachments[0]->image_width);
    frame_buffer_create_params.height          = static_cast<uint32_t>(attachments[0]->image_height);
    frame_buffer_create_params.layers          = static_cast<uint32_t>(attachments[0]->image_depth);

    const VkResult err = vkCreateFramebuffer(self->parent->handle, &frame_buffer_create_params, nullptr, &fb->handle);
    assert(err == VK_SUCCESS);

    self->parent->cache_framebuffer.insert(hash_code, fb, fb_state);
    AddCachedResource(self->parent, &fb->super, hash_code);
  }

  self->attachment_size[0] = static_cast<uint32_t>(attachments[0]->image_width);
  self->attachment_size[1] = static_cast<uint32_t>(attachments[0]->image_height);
  self->framebuffer        = fb;

  UpdateResourceFrame(&fb->super);
}

void bfGfxCmdList_setRenderAreaAbs(bfGfxCommandListHandle self, int32_t x, int32_t y, uint32_t width, uint32_t height)
{
  self->render_area.offset.x      = x;
  self->render_area.offset.y      = y;
  self->render_area.extent.width  = width;
  self->render_area.extent.height = height;

  const float depths[2] = {0.0f, 1.0f};
  bfGfxCmdList_setViewport(self, float(x), float(y), float(width), float(height), depths);
  bfGfxCmdList_setScissorRect(self, x, y, width, height);
}

extern "C" void bfGfxCmdList_setRenderAreaRelImpl(float fb_width, float fb_height, bfGfxCommandListHandle self, float x, float y, float width, float height);

void bfGfxCmdList_setRenderAreaRel(bfGfxCommandListHandle self, float x, float y, float width, float height)
{
  bfGfxCmdList_setRenderAreaRelImpl(float(self->attachment_size[0]), float(self->attachment_size[1]), self, x, y, width, height);
}

void bfGfxCmdList_beginRenderpass(bfGfxCommandListHandle self)
{
  VkRenderPassBeginInfo begin_render_pass_info;
  begin_render_pass_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  begin_render_pass_info.pNext           = nullptr;
  begin_render_pass_info.renderPass      = self->pipeline_state.renderpass->handle;
  begin_render_pass_info.framebuffer     = self->framebuffer->handle;
  begin_render_pass_info.renderArea      = self->render_area;
  begin_render_pass_info.clearValueCount = self->pipeline_state.renderpass->info.num_attachments;
  begin_render_pass_info.pClearValues    = self->clear_colors;

  vkCmdBeginRenderPass(self->handle, &begin_render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

  self->pipeline_state.state.subpass_index = 0;
}

void bfGfxCmdList_nextSubpass(bfGfxCommandListHandle self)
{
  vkCmdNextSubpass(self->handle, VK_SUBPASS_CONTENTS_INLINE);
  ++self->pipeline_state.state.subpass_index;
}

#define state(self) self->pipeline_state.state

void bfGfxCmdList_setDrawMode(bfGfxCommandListHandle self, bfDrawMode draw_mode)
{
  state(self).draw_mode = draw_mode;
}

void bfGfxCmdList_setFrontFace(bfGfxCommandListHandle self, bfFrontFace front_face)
{
  state(self).front_face = front_face;
}

void bfGfxCmdList_setCullFace(bfGfxCommandListHandle self, bfCullFaceFlags cull_face)
{
  state(self).cull_face = cull_face;
}

void bfGfxCmdList_setDepthTesting(bfGfxCommandListHandle self, bfBool32 value)
{
  state(self).do_depth_test = value;
}

void bfGfxCmdList_setDepthWrite(bfGfxCommandListHandle self, bfBool32 value)
{
  state(self).do_depth_write = value;
}

void bfGfxCmdList_setDepthTestOp(bfGfxCommandListHandle self, bfCompareOp op)
{
  state(self).depth_test_op = op;
}

void bfGfxCmdList_setStencilTesting(bfGfxCommandListHandle self, bfBool32 value)
{
  state(self).do_stencil_test = value;
}

void bfGfxCmdList_setPrimitiveRestart(bfGfxCommandListHandle self, bfBool32 value)
{
  state(self).do_primitive_restart = value;
}

void bfGfxCmdList_setRasterizerDiscard(bfGfxCommandListHandle self, bfBool32 value)
{
  state(self).do_rasterizer_discard = value;
}

void bfGfxCmdList_setDepthBias(bfGfxCommandListHandle self, bfBool32 value)
{
  state(self).do_depth_bias = value;
}

void bfGfxCmdList_setSampleShading(bfGfxCommandListHandle self, bfBool32 value)
{
  state(self).do_sample_shading = value;
}

void bfGfxCmdList_setAlphaToCoverage(bfGfxCommandListHandle self, bfBool32 value)
{
  state(self).do_alpha_to_coverage = value;
}

void bfGfxCmdList_setAlphaToOne(bfGfxCommandListHandle self, bfBool32 value)
{
  state(self).do_alpha_to_one = value;
}

void bfGfxCmdList_setLogicOpEnabled(bfGfxCommandListHandle self, bfBool32 value)
{
  state(self).do_logic_op = value;
}

void bfGfxCmdList_setLogicOp(bfGfxCommandListHandle self, bfLogicOp op)
{
  state(self).logic_op = op;
}

void bfGfxCmdList_setPolygonFillMode(bfGfxCommandListHandle self, bfPolygonFillMode fill_mode)
{
  state(self).fill_mode = fill_mode;
}

#undef state

void bfGfxCmdList_setColorWriteMask(bfGfxCommandListHandle self, uint32_t output_attachment_idx, uint8_t color_mask)
{
  self->pipeline_state.blending[output_attachment_idx].color_write_mask = color_mask;
}

void bfGfxCmdList_setColorBlendOp(bfGfxCommandListHandle self, uint32_t output_attachment_idx, bfBlendOp op)
{
  self->pipeline_state.blending[output_attachment_idx].color_blend_op = op;
}

void bfGfxCmdList_setBlendSrc(bfGfxCommandListHandle self, uint32_t output_attachment_idx, bfBlendFactor factor)
{
  self->pipeline_state.blending[output_attachment_idx].color_blend_src = factor;
}

void bfGfxCmdList_setBlendDst(bfGfxCommandListHandle self, uint32_t output_attachment_idx, bfBlendFactor factor)
{
  self->pipeline_state.blending[output_attachment_idx].color_blend_dst = factor;
}

void bfGfxCmdList_setAlphaBlendOp(bfGfxCommandListHandle self, uint32_t output_attachment_idx, bfBlendOp op)
{
  self->pipeline_state.blending[output_attachment_idx].alpha_blend_op = op;
}

void bfGfxCmdList_setBlendSrcAlpha(bfGfxCommandListHandle self, uint32_t output_attachment_idx, bfBlendFactor factor)
{
  self->pipeline_state.blending[output_attachment_idx].alpha_blend_src = factor;
}

void bfGfxCmdList_setBlendDstAlpha(bfGfxCommandListHandle self, uint32_t output_attachment_idx, bfBlendFactor factor)
{
  self->pipeline_state.blending[output_attachment_idx].alpha_blend_dst = factor;
}

void bfGfxCmdList_setStencilFailOp(bfGfxCommandListHandle self, bfStencilFace face, bfStencilOp op)
{
  if (face == BF_STENCIL_FACE_FRONT)
  {
    self->pipeline_state.state.stencil_face_front_fail_op = op;
  }
  else
  {
    self->pipeline_state.state.stencil_face_back_fail_op = op;
  }
}

void bfGfxCmdList_setStencilPassOp(bfGfxCommandListHandle self, bfStencilFace face, bfStencilOp op)
{
  if (face == BF_STENCIL_FACE_FRONT)
  {
    self->pipeline_state.state.stencil_face_front_pass_op = op;
  }
  else
  {
    self->pipeline_state.state.stencil_face_back_pass_op = op;
  }
}

void bfGfxCmdList_setStencilDepthFailOp(bfGfxCommandListHandle self, bfStencilFace face, bfStencilOp op)
{
  if (face == BF_STENCIL_FACE_FRONT)
  {
    self->pipeline_state.state.stencil_face_front_depth_fail_op = op;
  }
  else
  {
    self->pipeline_state.state.stencil_face_back_depth_fail_op = op;
  }
}
void bfGfxCmdList_setStencilCompareOp(bfGfxCommandListHandle self, bfStencilFace face, bfCompareOp op)
{
  if (face == BF_STENCIL_FACE_FRONT)
  {
    self->pipeline_state.state.stencil_face_front_compare_op = op;
  }
  else
  {
    self->pipeline_state.state.stencil_face_back_compare_op = op;
  }
}

void bfGfxCmdList_setStencilCompareMask(bfGfxCommandListHandle self, bfStencilFace face, uint8_t cmp_mask)
{
  if (face == BF_STENCIL_FACE_FRONT)
  {
    self->pipeline_state.state.stencil_face_front_compare_mask = cmp_mask;
  }
  else
  {
    self->pipeline_state.state.stencil_face_back_compare_mask = cmp_mask;
  }

  self->dynamic_state_dirty |= BF_PIPELINE_DYNAMIC_STENCIL_COMPARE_MASK;
}

void bfGfxCmdList_setStencilWriteMask(bfGfxCommandListHandle self, bfStencilFace face, uint8_t write_mask)
{
  if (face == BF_STENCIL_FACE_FRONT)
  {
    self->pipeline_state.state.stencil_face_front_write_mask = write_mask;
  }
  else
  {
    self->pipeline_state.state.stencil_face_back_write_mask = write_mask;
  }

  self->dynamic_state_dirty |= BF_PIPELINE_DYNAMIC_STENCIL_WRITE_MASK;
}

void bfGfxCmdList_setStencilReference(bfGfxCommandListHandle self, bfStencilFace face, uint8_t ref_mask)
{
  if (face == BF_STENCIL_FACE_FRONT)
  {
    self->pipeline_state.state.stencil_face_front_reference = ref_mask;
  }
  else
  {
    self->pipeline_state.state.stencil_face_back_reference = ref_mask;
  }

  self->dynamic_state_dirty |= BF_PIPELINE_DYNAMIC_STENCIL_REFERENCE;
}

void bfGfxCmdList_setDynamicStates(bfGfxCommandListHandle self, uint16_t dynamic_states)
{
  auto& s = self->pipeline_state.state;

  const bfBool32 set_dynamic_viewport           = (dynamic_states & BF_PIPELINE_DYNAMIC_VIEWPORT) != 0;
  const bfBool32 set_dynamic_scissor            = (dynamic_states & BF_PIPELINE_DYNAMIC_SCISSOR) != 0;
  const bfBool32 set_dynamic_line_width         = (dynamic_states & BF_PIPELINE_DYNAMIC_LINE_WIDTH) != 0;
  const bfBool32 set_dynamic_depth_bias         = (dynamic_states & BF_PIPELINE_DYNAMIC_DEPTH_BIAS) != 0;
  const bfBool32 set_dynamic_blend_constants    = (dynamic_states & BF_PIPELINE_DYNAMIC_BLEND_CONSTANTS) != 0;
  const bfBool32 set_dynamic_depth_bounds       = (dynamic_states & BF_PIPELINE_DYNAMIC_DEPTH_BOUNDS) != 0;
  const bfBool32 set_dynamic_stencil_cmp_mask   = (dynamic_states & BF_PIPELINE_DYNAMIC_STENCIL_COMPARE_MASK) != 0;
  const bfBool32 set_dynamic_stencil_write_mask = (dynamic_states & BF_PIPELINE_DYNAMIC_STENCIL_WRITE_MASK) != 0;
  const bfBool32 set_dynamic_stencil_reference  = (dynamic_states & BF_PIPELINE_DYNAMIC_STENCIL_REFERENCE) != 0;

  s.dynamic_viewport           = set_dynamic_viewport;
  s.dynamic_scissor            = set_dynamic_scissor;
  s.dynamic_line_width         = set_dynamic_line_width;
  s.dynamic_depth_bias         = set_dynamic_depth_bias;
  s.dynamic_blend_constants    = set_dynamic_blend_constants;
  s.dynamic_depth_bounds       = set_dynamic_depth_bounds;
  s.dynamic_stencil_cmp_mask   = set_dynamic_stencil_cmp_mask;
  s.dynamic_stencil_write_mask = set_dynamic_stencil_write_mask;
  s.dynamic_stencil_reference  = set_dynamic_stencil_reference;

  self->dynamic_state_dirty = dynamic_states;
}

void bfGfxCmdList_setViewport(bfGfxCommandListHandle self, float x, float y, float width, float height, const float depth[2])
{
  static constexpr float k_DefaultDepth[2] = {0.0f, 1.0f};

  if (depth == nullptr)
  {
    depth = k_DefaultDepth;
  }

  auto& vp     = self->pipeline_state.viewport;
  vp.x         = x;
  vp.y         = y;
  vp.width     = width;
  vp.height    = height;
  vp.min_depth = depth[0];
  vp.max_depth = depth[1];

  self->dynamic_state_dirty |= BF_PIPELINE_DYNAMIC_VIEWPORT;
}

void bfGfxCmdList_setScissorRect(bfGfxCommandListHandle self, int32_t x, int32_t y, uint32_t width, uint32_t height)
{
  auto& s = self->pipeline_state.scissor_rect;

  s.x      = x;
  s.y      = y;
  s.width  = width;
  s.height = height;

  self->dynamic_state_dirty |= BF_PIPELINE_DYNAMIC_SCISSOR;
}

void bfGfxCmdList_setBlendConstants(bfGfxCommandListHandle self, const float constants[4])
{
  memcpy(self->pipeline_state.blend_constants, constants, sizeof(self->pipeline_state.blend_constants));

  self->dynamic_state_dirty |= BF_PIPELINE_DYNAMIC_BLEND_CONSTANTS;
}

void bfGfxCmdList_setLineWidth(bfGfxCommandListHandle self, float value)
{
  self->pipeline_state.line_width = value;

  self->dynamic_state_dirty |= BF_PIPELINE_DYNAMIC_LINE_WIDTH;
}

void bfGfxCmdList_setDepthClampEnabled(bfGfxCommandListHandle self, bfBool32 value)
{
  self->pipeline_state.state.do_depth_clamp = value;
}

void bfGfxCmdList_setDepthBoundsTestEnabled(bfGfxCommandListHandle self, bfBool32 value)
{
  self->pipeline_state.state.do_depth_bounds_test = value;
}

void bfGfxCmdList_setDepthBounds(bfGfxCommandListHandle self, float min, float max)
{
  self->pipeline_state.depth.min_bound = min;
  self->pipeline_state.depth.max_bound = max;

  self->dynamic_state_dirty |= BF_PIPELINE_DYNAMIC_DEPTH_BOUNDS;
}

void bfGfxCmdList_setDepthBiasConstantFactor(bfGfxCommandListHandle self, float value)
{
  self->pipeline_state.depth.bias_constant_factor = value;
  self->dynamic_state_dirty |= BF_PIPELINE_DYNAMIC_DEPTH_BIAS;
}

void bfGfxCmdList_setDepthBiasClamp(bfGfxCommandListHandle self, float value)
{
  self->pipeline_state.depth.bias_clamp = value;
  self->dynamic_state_dirty |= BF_PIPELINE_DYNAMIC_DEPTH_BIAS;
}

void bfGfxCmdList_setDepthBiasSlopeFactor(bfGfxCommandListHandle self, float value)
{
  self->pipeline_state.depth.bias_slope_factor = value;
  self->dynamic_state_dirty |= BF_PIPELINE_DYNAMIC_DEPTH_BIAS;
}

void bfGfxCmdList_setMinSampleShading(bfGfxCommandListHandle self, float value)
{
  self->pipeline_state.min_sample_shading = value;
}

void bfGfxCmdList_setSampleMask(bfGfxCommandListHandle self, uint32_t sample_mask)
{
  self->pipeline_state.sample_mask = sample_mask;
}

void bfGfxCmdList_bindDrawCallPipeline(bfGfxCommandListHandle self, const bfDrawCallPipeline* pipeline_state)
{
  const std::uint64_t old_subpass_idx = self->pipeline_state.state.subpass_index;

  self->pipeline_state.state               = pipeline_state->state;
  self->pipeline_state.state.subpass_index = old_subpass_idx;
  self->pipeline_state.line_width          = pipeline_state->line_width;
  self->pipeline_state.program             = pipeline_state->program;
  self->pipeline_state.vertex_layout       = pipeline_state->vertex_layout;
  std::memcpy(self->pipeline_state.blend_constants, pipeline_state->blend_constants, sizeof(pipeline_state->blend_constants));
  std::memcpy(self->pipeline_state.blending, pipeline_state->blending, sizeof(pipeline_state->blending));  // TODO(SR): This can be optimized to copy less.

  self->dynamic_state_dirty |= BF_PIPELINE_DYNAMIC_LINE_WIDTH |
                              BF_PIPELINE_DYNAMIC_BLEND_CONSTANTS |
                              BF_PIPELINE_DYNAMIC_STENCIL_COMPARE_MASK |
                              BF_PIPELINE_DYNAMIC_STENCIL_WRITE_MASK |
                              BF_PIPELINE_DYNAMIC_STENCIL_REFERENCE;
}

void bfGfxCmdList_bindVertexDesc(bfGfxCommandListHandle self, bfVertexLayoutSetHandle vertex_set_layout)
{
  self->pipeline_state.vertex_layout = vertex_set_layout;
}

void bfGfxCmdList_bindVertexBuffers(bfGfxCommandListHandle self, uint32_t first_binding, bfBufferHandle* buffers, uint32_t num_buffers, const uint64_t* offsets)
{
  assert(num_buffers < k_bfGfxMaxBufferBindings);

  VkBuffer binded_buffers[k_bfGfxMaxBufferBindings];
  uint64_t binded_offsets[k_bfGfxMaxBufferBindings];

  for (uint32_t i = 0; i < num_buffers; ++i)
  {
    binded_buffers[i] = buffers[i]->handle;
    binded_offsets[i] = offsets[i] + buffers[i]->alloc_info.offset;
  }

  vkCmdBindVertexBuffers(
   self->handle,
   first_binding,
   num_buffers,
   binded_buffers,
   binded_offsets);
}

void bfGfxCmdList_bindIndexBuffer(bfGfxCommandListHandle self, bfBufferHandle buffer, uint64_t offset, bfGfxIndexType idx_type)
{
  vkCmdBindIndexBuffer(self->handle, buffer->handle, offset, bfVkConvertIndexType(idx_type));
}

void bfGfxCmdList_bindProgram(bfGfxCommandListHandle self, bfShaderProgramHandle shader)
{
  self->pipeline_state.program = shader;
}

void bfGfxCmdList_bindDescriptorSets(bfGfxCommandListHandle self, uint32_t binding, bfDescriptorSetHandle* desc_sets, uint32_t num_desc_sets)
{
  const VkPipelineBindPoint   bind_point = self->pipeline_state.renderpass ? VK_PIPELINE_BIND_POINT_GRAPHICS : VK_PIPELINE_BIND_POINT_COMPUTE;
  const bfShaderProgramHandle program    = self->pipeline_state.program;

  assert(bind_point == VK_PIPELINE_BIND_POINT_GRAPHICS && "Compute not supported yet.");
  assert(binding + num_desc_sets <= program->num_desc_set_layouts);
  assert(num_desc_sets <= k_bfGfxDescriptorSets);

  VkDescriptorSet sets[k_bfGfxDescriptorSets];

  for (uint32_t i = 0; i < num_desc_sets; ++i)
  {
    sets[i] = desc_sets[i]->handle;
  }

  vkCmdBindDescriptorSets(
   self->handle,
   bind_point,
   program->layout,
   binding,
   num_desc_sets,
   sets,
   0,
   nullptr);
}

void bfGfxCmdList_bindDescriptorSet(bfGfxCommandListHandle self, uint32_t set_index, const bfDescriptorSetInfo* desc_set_info)
{
  bfShaderProgramHandle program = self->pipeline_state.program;

  assert(set_index < program->num_desc_set_layouts);

  const std::uint64_t   hash_code = bf::vk::hash(program->desc_set_layout_infos[set_index], desc_set_info);
  bfDescriptorSetHandle desc_set  = self->parent->cache_descriptor_set.find(hash_code, *desc_set_info);

  if (!desc_set)
  {
    desc_set = bfShaderProgram_createDescriptorSet(program, set_index);

    for (uint32_t i = 0; i < desc_set_info->num_bindings; ++i)
    {
      const bfDescriptorElementInfo* binding_info = &desc_set_info->bindings[i];

      switch (binding_info->type)
      {
        case BF_DESCRIPTOR_ELEMENT_TEXTURE:
          bfDescriptorSet_setCombinedSamplerTextures(
           desc_set,
           binding_info->binding,
           binding_info->array_element_start,
           (bfTextureHandle*)binding_info->handles,  // NOLINT(clang-diagnostic-cast-qual)
           binding_info->num_handles);
          break;
        case BF_DESCRIPTOR_ELEMENT_BUFFER:
          bfDescriptorSet_setUniformBuffers(
           desc_set,
           binding_info->binding,
           binding_info->offsets,
           binding_info->sizes,
           (bfBufferHandle*)binding_info->handles,  // NOLINT(clang-diagnostic-cast-qual)
           binding_info->num_handles);
          break;
        case BF_DESCRIPTOR_ELEMENT_BUFFER_VIEW:
        case BF_DESCRIPTOR_ELEMENT_DYNAMIC_BUFFER:
        case BF_DESCRIPTOR_ELEMENT_INPUT_ATTACHMENT:
        default:
          assert(!"Not supported yet.");
          break;
      }
    }

    bfDescriptorSet_flushWrites(desc_set);

    self->parent->cache_descriptor_set.insert(hash_code, desc_set, *desc_set_info);
    AddCachedResource(self->parent, &desc_set->super, hash_code);
  }

  bfGfxCmdList_bindDescriptorSets(self, set_index, &desc_set, 1);
  UpdateResourceFrame(&desc_set->super);
}

static void flushPipeline(bfGfxCommandListHandle self)
{
  const uint64_t hash_code = bf::vk::hash(0x0, &self->pipeline_state);

  bfPipelineHandle pl = self->parent->cache_pipeline.find(hash_code, self->pipeline_state);

  if (!pl)
  {
    pl = new bfPipeline();

    bfBaseGfxObject_ctor(&pl->super, BF_GFX_OBJECT_PIPELINE);

    auto& state   = self->pipeline_state;
    auto& ss      = state.state;
    auto& program = state.program;

    VkPipelineShaderStageCreateInfo shader_stages[BF_SHADER_TYPE_MAX];

    for (size_t i = 0; i < program->modules.size; ++i)
    {
      bfShaderModuleHandle shader_module = program->modules.elements[i];
      auto&                shader_stage  = shader_stages[i];
      shader_stage.sType                 = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      shader_stage.pNext                 = nullptr;
      shader_stage.flags                 = 0x0;
      shader_stage.stage                 = bfVkConvertShaderType(shader_module->type);
      shader_stage.module                = shader_module->handle;
      shader_stage.pName                 = shader_module->entry_point;
      shader_stage.pSpecializationInfo   = nullptr;  // https://www.jeremyong.com/c++/vulkan/graphics/rendering/2018/04/20/putting-cpp-to-work-making-abstractions-for-vulkan-specialization-info/
    }

    VkPipelineVertexInputStateCreateInfo vertex_input;
    vertex_input.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.pNext                           = nullptr;
    vertex_input.flags                           = 0x0;
    vertex_input.vertexBindingDescriptionCount   = state.vertex_layout->num_buffer_bindings;
    vertex_input.pVertexBindingDescriptions      = state.vertex_layout->buffer_bindings;
    vertex_input.vertexAttributeDescriptionCount = state.vertex_layout->num_attrib_bindings;
    vertex_input.pVertexAttributeDescriptions    = state.vertex_layout->attrib_bindings;

    VkPipelineInputAssemblyStateCreateInfo input_asm;
    input_asm.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_asm.pNext                  = nullptr;
    input_asm.flags                  = 0x0;
    input_asm.topology               = bfVkConvertTopology((bfDrawMode)state.state.draw_mode);
    input_asm.primitiveRestartEnable = state.state.do_primitive_restart;

    VkPipelineTessellationStateCreateInfo tess;
    tess.sType              = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    tess.pNext              = nullptr;
    tess.flags              = 0x0;
    tess.patchControlPoints = 0;  // TODO: Figure this out.

    // https://erkaman.github.io/posts/tess_opt.html
    // https://computergraphics.stackexchange.com/questions/7955/why-are-tessellation-shaders-disliked

    VkPipelineViewportStateCreateInfo viewport;
    VkViewport                        viewports[1];
    VkRect2D                          scissor_rects[1];

    viewports[0]           = bfVkConvertViewport(&state.viewport);
    scissor_rects[0]       = bfVkConvertScissorRect(&state.scissor_rect);
    viewport.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.pNext         = nullptr;
    viewport.flags         = 0x0;
    viewport.viewportCount = uint32_t(bfCArraySize(viewports));
    viewport.pViewports    = viewports;
    viewport.scissorCount  = uint32_t(bfCArraySize(scissor_rects));
    viewport.pScissors     = scissor_rects;

    VkPipelineRasterizationStateCreateInfo rasterization;
    rasterization.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.pNext                   = nullptr;
    rasterization.flags                   = 0x0;
    rasterization.depthClampEnable        = state.state.do_depth_clamp;
    rasterization.rasterizerDiscardEnable = state.state.do_rasterizer_discard;
    rasterization.polygonMode             = bfVkConvertPolygonMode((bfPolygonFillMode)state.state.fill_mode);
    rasterization.cullMode                = bfVkConvertCullModeFlags(state.state.cull_face);
    rasterization.frontFace               = bfVkConvertFrontFace((bfFrontFace)state.state.front_face);
    rasterization.depthBiasEnable         = state.state.do_depth_bias;
    rasterization.depthBiasConstantFactor = state.depth.bias_constant_factor;
    rasterization.depthBiasClamp          = state.depth.bias_clamp;
    rasterization.depthBiasSlopeFactor    = state.depth.bias_slope_factor;
    rasterization.lineWidth               = state.line_width;

    VkPipelineMultisampleStateCreateInfo multisample;
    multisample.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.pNext                 = nullptr;
    multisample.flags                 = 0x0;
    multisample.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
    multisample.sampleShadingEnable   = state.state.do_sample_shading;
    multisample.minSampleShading      = state.min_sample_shading;
    multisample.pSampleMask           = &state.sample_mask;
    multisample.alphaToCoverageEnable = state.state.do_alpha_to_coverage;
    multisample.alphaToOneEnable      = state.state.do_alpha_to_one;

    const auto ConvertStencilOpState = [](VkStencilOpState& out, uint64_t fail, uint64_t pass, uint64_t depth_fail, uint64_t cmp_op, uint32_t cmp_mask, uint32_t write_mask, uint32_t ref) {
      out.failOp      = bfVkConvertStencilOp((bfStencilOp)fail);
      out.passOp      = bfVkConvertStencilOp((bfStencilOp)pass);
      out.depthFailOp = bfVkConvertStencilOp((bfStencilOp)depth_fail);
      out.compareOp   = bfVkConvertCompareOp((bfCompareOp)cmp_op);
      out.compareMask = cmp_mask;
      out.writeMask   = write_mask;
      out.reference   = ref;
    };

    VkPipelineDepthStencilStateCreateInfo depth_stencil;
    depth_stencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.pNext                 = nullptr;
    depth_stencil.flags                 = 0x0;
    depth_stencil.depthTestEnable       = state.state.do_depth_test;
    depth_stencil.depthWriteEnable      = state.state.do_depth_write;
    depth_stencil.depthCompareOp        = bfVkConvertCompareOp((bfCompareOp)state.state.depth_test_op);
    depth_stencil.depthBoundsTestEnable = state.state.do_depth_bounds_test;
    depth_stencil.stencilTestEnable     = state.state.do_stencil_test;

    ConvertStencilOpState(
     depth_stencil.front,
     ss.stencil_face_front_fail_op,
     ss.stencil_face_front_pass_op,
     ss.stencil_face_front_depth_fail_op,
     ss.stencil_face_front_compare_op,
     ss.stencil_face_front_compare_mask,
     ss.stencil_face_front_write_mask,
     ss.stencil_face_front_reference);

    ConvertStencilOpState(
     depth_stencil.back,
     ss.stencil_face_back_fail_op,
     ss.stencil_face_back_pass_op,
     ss.stencil_face_back_depth_fail_op,
     ss.stencil_face_back_compare_op,
     ss.stencil_face_back_compare_mask,
     ss.stencil_face_back_write_mask,
     ss.stencil_face_back_reference);

    depth_stencil.minDepthBounds = state.depth.min_bound;
    depth_stencil.maxDepthBounds = state.depth.max_bound;

    VkPipelineColorBlendStateCreateInfo color_blend;
    VkPipelineColorBlendAttachmentState color_blend_states[k_bfGfxMaxAttachments];

    const uint32_t num_color_attachments = self->pipeline_state.renderpass->info.subpasses[state.state.subpass_index].num_out_attachment_refs;

    for (uint32_t i = 0; i < num_color_attachments; ++i)
    {
      bfFramebufferBlending&               blend     = state.blending[i];
      VkPipelineColorBlendAttachmentState& clr_state = color_blend_states[i];

      clr_state.blendEnable = blend.color_blend_src != BF_BLEND_FACTOR_NONE && blend.color_blend_dst != BF_BLEND_FACTOR_NONE;

      if (clr_state.blendEnable)
      {
        clr_state.srcColorBlendFactor = bfVkConvertBlendFactor((bfBlendFactor)blend.color_blend_src);
        clr_state.dstColorBlendFactor = bfVkConvertBlendFactor((bfBlendFactor)blend.color_blend_dst);
        clr_state.colorBlendOp        = bfVkConvertBlendOp((bfBlendOp)blend.color_blend_op);
        clr_state.srcAlphaBlendFactor = bfVkConvertBlendFactor((bfBlendFactor)blend.alpha_blend_src);
        clr_state.dstAlphaBlendFactor = bfVkConvertBlendFactor((bfBlendFactor)blend.alpha_blend_dst);
        clr_state.alphaBlendOp        = bfVkConvertBlendOp((bfBlendOp)blend.alpha_blend_op);
      }
      else
      {
        clr_state.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        clr_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        clr_state.colorBlendOp        = VK_BLEND_OP_ADD;
        clr_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        clr_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        clr_state.alphaBlendOp        = VK_BLEND_OP_ADD;
      }

      clr_state.colorWriteMask = bfVkConvertColorMask(blend.color_write_mask);
    }

    color_blend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend.pNext           = nullptr;
    color_blend.flags           = 0x0;
    color_blend.logicOpEnable   = ss.do_logic_op;
    color_blend.logicOp         = bfVkConvertLogicOp((bfLogicOp)ss.logic_op);
    color_blend.attachmentCount = num_color_attachments;
    color_blend.pAttachments    = color_blend_states;
    memcpy(color_blend.blendConstants, state.blend_constants, sizeof(state.blend_constants));

    VkPipelineDynamicStateCreateInfo dynamic_state;
    VkDynamicState                   dynamic_state_storage[9];
    dynamic_state.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.pNext             = nullptr;
    dynamic_state.flags             = 0x0;
    dynamic_state.dynamicStateCount = 0;
    dynamic_state.pDynamicStates    = dynamic_state_storage;

    auto& s = state.state;

    const auto addDynamicState = [&dynamic_state](VkDynamicState* states, uint64_t flag, VkDynamicState vk_state) {
      if (flag)
      {
        states[dynamic_state.dynamicStateCount++] = vk_state;
      }
    };

    addDynamicState(dynamic_state_storage, s.dynamic_viewport, VK_DYNAMIC_STATE_VIEWPORT);
    addDynamicState(dynamic_state_storage, s.dynamic_scissor, VK_DYNAMIC_STATE_SCISSOR);
    addDynamicState(dynamic_state_storage, s.dynamic_line_width, VK_DYNAMIC_STATE_LINE_WIDTH);
    addDynamicState(dynamic_state_storage, s.dynamic_depth_bias, VK_DYNAMIC_STATE_DEPTH_BIAS);
    addDynamicState(dynamic_state_storage, s.dynamic_blend_constants, VK_DYNAMIC_STATE_BLEND_CONSTANTS);
    addDynamicState(dynamic_state_storage, s.dynamic_depth_bounds, VK_DYNAMIC_STATE_DEPTH_BOUNDS);
    addDynamicState(dynamic_state_storage, s.dynamic_stencil_cmp_mask, VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK);
    addDynamicState(dynamic_state_storage, s.dynamic_stencil_write_mask, VK_DYNAMIC_STATE_STENCIL_WRITE_MASK);
    addDynamicState(dynamic_state_storage, s.dynamic_stencil_reference, VK_DYNAMIC_STATE_STENCIL_REFERENCE);

    // TODO(SR): Look into pipeline derivatives?
    //   @PipelineDerivative

    VkGraphicsPipelineCreateInfo pl_create_info;
    pl_create_info.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pl_create_info.pNext               = nullptr;
    pl_create_info.flags               = 0x0;  // @PipelineDerivative
    pl_create_info.stageCount          = program->modules.size;
    pl_create_info.pStages             = shader_stages;
    pl_create_info.pVertexInputState   = &vertex_input;
    pl_create_info.pInputAssemblyState = &input_asm;
    pl_create_info.pTessellationState  = &tess;
    pl_create_info.pViewportState      = &viewport;
    pl_create_info.pRasterizationState = &rasterization;
    pl_create_info.pMultisampleState   = &multisample;
    pl_create_info.pDepthStencilState  = &depth_stencil;
    pl_create_info.pColorBlendState    = &color_blend;
    pl_create_info.pDynamicState       = &dynamic_state;
    pl_create_info.layout              = program->layout;
    pl_create_info.renderPass          = self->pipeline_state.renderpass->handle;
    pl_create_info.subpass             = state.state.subpass_index;
    pl_create_info.basePipelineHandle  = VK_NULL_HANDLE;  // @PipelineDerivative
    pl_create_info.basePipelineIndex   = -1;              // @PipelineDerivative

    // TODO(Shareef): Look into Pipeline caches?
    const VkResult err = vkCreateGraphicsPipelines(
     self->parent->handle,
     VK_NULL_HANDLE,
     1,
     &pl_create_info,
     CUSTOM_ALLOC,
     &pl->handle);
    assert(err == VK_SUCCESS);

    self->parent->cache_pipeline.insert(hash_code, pl, self->pipeline_state);
    AddCachedResource(self->parent, &pl->super, hash_code);
  }

  if (pl != self->pipeline)
  {
    vkCmdBindPipeline(self->handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pl->handle);
    self->dynamic_state_dirty = 0xFFFF;
    self->pipeline            = pl;
  }

  if (self->dynamic_state_dirty & BF_PIPELINE_DYNAMIC_VIEWPORT && self->pipeline_state.state.dynamic_viewport)
  {
    VkViewport viewports = bfVkConvertViewport(&self->pipeline_state.viewport);
    vkCmdSetViewport(self->handle, 0, 1, &viewports);
  }

  if (self->dynamic_state_dirty & BF_PIPELINE_DYNAMIC_SCISSOR && self->pipeline_state.state.dynamic_scissor)
  {
    VkRect2D scissors = bfVkConvertScissorRect(&self->pipeline_state.scissor_rect);
    vkCmdSetScissor(self->handle, 0, 1, &scissors);
  }

  if (self->dynamic_state_dirty & BF_PIPELINE_DYNAMIC_LINE_WIDTH && self->pipeline_state.state.dynamic_line_width)
  {
    vkCmdSetLineWidth(self->handle, self->pipeline_state.line_width);
  }

  if (self->dynamic_state_dirty & BF_PIPELINE_DYNAMIC_DEPTH_BIAS && self->pipeline_state.state.dynamic_depth_bias)
  {
    auto& depth = self->pipeline_state.depth;

    vkCmdSetDepthBias(self->handle, depth.bias_constant_factor, depth.bias_clamp, depth.bias_slope_factor);
  }

  if (self->dynamic_state_dirty & BF_PIPELINE_DYNAMIC_BLEND_CONSTANTS && self->pipeline_state.state.dynamic_blend_constants)
  {
    vkCmdSetBlendConstants(self->handle, self->pipeline_state.blend_constants);
  }

  if (self->dynamic_state_dirty & BF_PIPELINE_DYNAMIC_DEPTH_BOUNDS && self->pipeline_state.state.dynamic_depth_bounds)
  {
    auto& depth = self->pipeline_state.depth;

    vkCmdSetDepthBounds(self->handle, depth.min_bound, depth.max_bound);
  }

  if (self->dynamic_state_dirty & BF_PIPELINE_DYNAMIC_STENCIL_COMPARE_MASK && self->pipeline_state.state.dynamic_stencil_cmp_mask)
  {
    auto& ss = self->pipeline_state.state;

    if (ss.stencil_face_front_compare_mask == ss.stencil_face_back_compare_mask)
    {
      vkCmdSetStencilCompareMask(self->handle, VK_STENCIL_FRONT_AND_BACK, ss.stencil_face_front_compare_mask);
    }
    else
    {
      vkCmdSetStencilCompareMask(self->handle, VK_STENCIL_FACE_FRONT_BIT, ss.stencil_face_front_compare_mask);
      vkCmdSetStencilCompareMask(self->handle, VK_STENCIL_FACE_BACK_BIT, ss.stencil_face_back_compare_mask);
    }
  }

  if (self->dynamic_state_dirty & BF_PIPELINE_DYNAMIC_STENCIL_WRITE_MASK && self->pipeline_state.state.dynamic_stencil_write_mask)
  {
    auto& ss = self->pipeline_state.state;

    if (ss.stencil_face_front_write_mask == ss.stencil_face_back_write_mask)
    {
      vkCmdSetStencilWriteMask(self->handle, VK_STENCIL_FRONT_AND_BACK, ss.stencil_face_front_write_mask);
    }
    else
    {
      vkCmdSetStencilWriteMask(self->handle, VK_STENCIL_FACE_FRONT_BIT, ss.stencil_face_front_write_mask);
      vkCmdSetStencilWriteMask(self->handle, VK_STENCIL_FACE_BACK_BIT, ss.stencil_face_back_write_mask);
    }
  }

  if (self->dynamic_state_dirty & BF_PIPELINE_DYNAMIC_STENCIL_REFERENCE && self->pipeline_state.state.dynamic_stencil_reference)
  {
    auto& ss = self->pipeline_state.state;

    if (ss.stencil_face_front_reference == ss.stencil_face_back_reference)
    {
      vkCmdSetStencilReference(self->handle, VK_STENCIL_FRONT_AND_BACK, ss.stencil_face_front_reference);
    }
    else
    {
      vkCmdSetStencilReference(self->handle, VK_STENCIL_FACE_FRONT_BIT, ss.stencil_face_front_reference);
      vkCmdSetStencilReference(self->handle, VK_STENCIL_FACE_BACK_BIT, ss.stencil_face_back_reference);
    }
  }

  self->dynamic_state_dirty = 0x0;

  UpdateResourceFrame(&pl->super);
}

void bfGfxCmdList_draw(bfGfxCommandListHandle self, uint32_t first_vertex, uint32_t num_vertices)
{
  bfGfxCmdList_drawInstanced(self, first_vertex, num_vertices, 0, 1);
}

void bfGfxCmdList_drawInstanced(bfGfxCommandListHandle self, uint32_t first_vertex, uint32_t num_vertices, uint32_t first_instance, uint32_t num_instances)
{
  flushPipeline(self);
  vkCmdDraw(self->handle, num_vertices, num_instances, first_vertex, first_instance);
}

void bfGfxCmdList_drawIndexed(bfGfxCommandListHandle self, uint32_t num_indices, uint32_t index_offset, int32_t vertex_offset)
{
  bfGfxCmdList_drawIndexedInstanced(self, num_indices, index_offset, vertex_offset, 0, 1);
}

void bfGfxCmdList_drawIndexedInstanced(bfGfxCommandListHandle self, uint32_t num_indices, uint32_t index_offset, int32_t vertex_offset, uint32_t first_instance, uint32_t num_instances)
{
  flushPipeline(self);
  vkCmdDrawIndexed(self->handle, num_indices, num_instances, index_offset, vertex_offset, first_instance);
}

void bfGfxCmdList_executeSubCommands(bfGfxCommandListHandle self, bfGfxCommandListHandle* commands, uint32_t num_commands)
{
  assert(!"Not implemented");
}

void bfGfxCmdList_endRenderpass(bfGfxCommandListHandle self)
{
  auto& render_pass_info = self->pipeline_state.renderpass->info;

  for (std::uint32_t i = 0; i < render_pass_info.num_attachments; ++i)
  {
    render_pass_info.attachments[i].texture->tex_layout = render_pass_info.attachments[i].final_layout;
  }

  vkCmdEndRenderPass(self->handle);
#if 0
  VkMemoryBarrier memoryBarrier = {
   VK_STRUCTURE_TYPE_MEMORY_BARRIER,
   nullptr,
   VK_ACCESS_INDIRECT_COMMAND_READ_BIT |
    VK_ACCESS_INDEX_READ_BIT |
    VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
    VK_ACCESS_UNIFORM_READ_BIT |
    VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
    VK_ACCESS_SHADER_READ_BIT |
    VK_ACCESS_SHADER_WRITE_BIT |
    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
    VK_ACCESS_TRANSFER_READ_BIT |
    VK_ACCESS_TRANSFER_WRITE_BIT |
    VK_ACCESS_HOST_READ_BIT |
    VK_ACCESS_HOST_WRITE_BIT,
   VK_ACCESS_INDIRECT_COMMAND_READ_BIT |
    VK_ACCESS_INDEX_READ_BIT |
    VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
    VK_ACCESS_UNIFORM_READ_BIT |
    VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
    VK_ACCESS_SHADER_READ_BIT |
    VK_ACCESS_SHADER_WRITE_BIT |
    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
    VK_ACCESS_TRANSFER_READ_BIT |
    VK_ACCESS_TRANSFER_WRITE_BIT |
    VK_ACCESS_HOST_READ_BIT |
    VK_ACCESS_HOST_WRITE_BIT};

  vkCmdPipelineBarrier(
   self->handle,
   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,  // srcStageMask
   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,  // dstStageMask
   0x0,
   1,               // memoryBarrierCount
   &memoryBarrier,  // pMemoryBarriers
   0,
   nullptr,
   0,
   nullptr);
#endif
}

void bfGfxCmdList_end(bfGfxCommandListHandle self)
{
  const VkResult err = vkEndCommandBuffer(self->handle);

  if (err)
  {
    assert(false);
  }
}

void bfGfxCmdList_updateBuffer(bfGfxCommandListHandle self, bfBufferHandle buffer, bfBufferSize offset, bfBufferSize size, const void* data)
{
  vkCmdUpdateBuffer(self->handle, buffer->handle, offset, size, data);
}

extern void gfxDestroySwapchain(bfWindowSurface& window);

#include <stdio.h>

void bfGfxCmdList_submit(bfGfxCommandListHandle self)
{
  const VkFence       command_fence = self->fence;
  bfWindowSurface&    window        = *self->window;
  const std::uint32_t frame_index   = bfGfxGetFrameInfo().frame_index;

  VkSemaphore          wait_semaphores[]   = {window.is_image_available[frame_index]};
  VkPipelineStageFlags wait_stages[]       = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};  // What to wait for, like: DO NOT WRITE TO COLOR UNTIL IMAGE IS AVALIBALLE.
  VkSemaphore          signal_semaphores[] = {window.is_render_done[frame_index]};

  VkSubmitInfo submit_info;
  submit_info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.pNext                = NULL;
  submit_info.waitSemaphoreCount   = uint32_t(bfCArraySize(wait_semaphores));
  submit_info.pWaitSemaphores      = wait_semaphores;
  submit_info.pWaitDstStageMask    = wait_stages;
  submit_info.commandBufferCount   = 1;
  submit_info.pCommandBuffers      = &self->handle;
  submit_info.signalSemaphoreCount = uint32_t(bfCArraySize(signal_semaphores));
  submit_info.pSignalSemaphores    = signal_semaphores;

  vkResetFences(self->parent->handle, 1, &command_fence);

  VkResult err = vkQueueSubmit(self->parent->queues[BF_GFX_QUEUE_GRAPHICS], 1, &submit_info, command_fence);

  assert(err == VK_SUCCESS && "bfGfxCmdList_submit: failed to submit the graphics queue");

  VkPresentInfoKHR present_info;
  present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.pNext              = nullptr;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores    = signal_semaphores;
  present_info.swapchainCount     = 1;
  present_info.pSwapchains        = &window.swapchain.handle;
  present_info.pImageIndices      = &window.image_index;
  present_info.pResults           = nullptr;

  err = vkQueuePresentKHR(self->parent->queues[BF_GFX_QUEUE_PRESENT], &present_info);

  if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
  {
    gfxDestroySwapchain(window);
  }
  else
  {
    assert(err == VK_SUCCESS && "GfxContext_submitFrame: failed to present graphics queue");
  }

  window.current_cmd_list = nullptr;
}

// TODO(SR): Make a new file
#include "bf/bf_hash.hpp"

namespace bf::vk
{
  using namespace gfx_hash;

  std::uint64_t hash(std::uint64_t self, const bfPipelineCache* pipeline)
  {
    const auto    num_attachments = pipeline->renderpass->info.subpasses[pipeline->state.subpass_index].num_out_attachment_refs;
    std::uint64_t state_bits[2];

    static_assert(sizeof(state_bits) == sizeof(pipeline->state), "Needs to be same size.");

    std::memcpy(state_bits, &pipeline->state, sizeof(state_bits));

    state_bits[0] &= bfPipelineCache_state0Mask(&pipeline->state);
    state_bits[1] &= bfPipelineCache_state1Mask(&pipeline->state);

    for (std::uint64_t state_bit : state_bits)
    {
      self = hash::addU64(self, state_bit);
    }

    if (!pipeline->state.dynamic_viewport)
    {
      gfx_hash::hash(self, pipeline->viewport);
    }

    if (!pipeline->state.dynamic_scissor)
    {
      gfx_hash::hash(self, pipeline->scissor_rect);
    }

    if (!pipeline->state.dynamic_blend_constants)
    {
      for (float blend_constant : pipeline->blend_constants)
      {
        self = hash::addF32(self, blend_constant);
      }
    }

    if (!pipeline->state.dynamic_line_width)
    {
      self = hash::addF32(self, pipeline->line_width);
    }

    gfx_hash::hash(self, pipeline->depth, pipeline->state);
    self = hash::addF32(self, pipeline->min_sample_shading);
    self = hash::addU64(self, pipeline->sample_mask);
    self = hash::addU32(self, pipeline->state.subpass_index);
    self = hash::addU32(self, num_attachments);

    for (std::uint32_t i = 0; i < num_attachments; ++i)
    {
      gfx_hash::hash(self, pipeline->blending[i]);
    }

    self = hash::addPointer(self, pipeline->program);
    self = hash::addPointer(self, pipeline->renderpass);
    self = hash::addPointer(self, pipeline->vertex_layout);

    return self;
  }

  std::uint64_t hash(std::uint64_t self, bfTextureHandle* attachments, std::size_t num_attachments)
  {
    if (num_attachments)
    {
      self = hash::addS32(self, attachments[0]->image_width);
      self = hash::addS32(self, attachments[0]->image_height);
    }

    for (std::size_t i = 0; i < num_attachments; ++i)
    {
      self = hash::addPointer(self, attachments[i]);
    }

    return self;
  }

  std::uint64_t hash(const bfDescriptorSetLayoutInfo& parent, const bfDescriptorSetInfo* desc_set_info)
  {
    std::uint64_t self = desc_set_info->num_bindings;

    for (uint32_t i = 0; i < desc_set_info->num_bindings; ++i)
    {
      const bfDescriptorElementInfo* binding = &desc_set_info->bindings[i];

      self = hash::addU32(self, binding->binding);
      self = hash::addU32(self, binding->array_element_start);
      self = hash::addU32(self, binding->num_handles);
      self = hash::addU32(self, parent.layout_bindings[i].stageFlags);

      for (uint32_t j = 0; j < binding->num_handles; ++j)
      {
        self = hash::addPointer(self, binding->handles[i]);

        if (binding->type == BF_DESCRIPTOR_ELEMENT_BUFFER)
        {
          self = hash::addU64(self, binding->offsets[j]);
          self = hash::addU64(self, binding->sizes[j]);
        }
      }
    }

    return self;
  }
}  // namespace bf::vk

static constexpr std::uint64_t k_FrontStencilCmpStateMask       = 0b0000000000000000011111111000000000000000000000000000000000000000;
static constexpr std::uint64_t k_FrontStencilWriteStateMask     = 0b0000000001111111100000000000000000000000000000000000000000000000;
static constexpr std::uint64_t k_FrontStencilReferenceStateMask = 0b0111111110000000000000000000000000000000000000000000000000000000;
static constexpr std::uint64_t k_BackStencilCmpStateMask        = 0b0000000000000000000000000000000000000000000000111111110000000000;
static constexpr std::uint64_t k_BackStencilWriteStateMask      = 0b0000000000000000000000000000000000000011111111000000000000000000;
static constexpr std::uint64_t k_BackStencilReferenceStateMask  = 0b0000000000000000000000000000001111111100000000000000000000000000;

uint64_t bfPipelineCache_state0Mask(const bfPipelineState* self)
{
  uint64_t result = 0xFFFFFFFFFFFFFFFF;

  if (self->dynamic_stencil_cmp_mask)
  {
    result &= ~k_FrontStencilCmpStateMask;
  }

  if (self->dynamic_stencil_write_mask)
  {
    result &= ~k_FrontStencilWriteStateMask;
  }

  if (self->dynamic_stencil_reference)
  {
    result &= ~k_FrontStencilReferenceStateMask;
  }

  return result;
}

uint64_t bfPipelineCache_state1Mask(const bfPipelineState* self)
{
  uint64_t result = 0xFFFFFFFFFFFFFFFF;

  if (self->dynamic_stencil_cmp_mask)
  {
    result &= ~k_BackStencilCmpStateMask;
  }

  if (self->dynamic_stencil_write_mask)
  {
    result &= ~k_BackStencilWriteStateMask;
  }

  if (self->dynamic_stencil_reference)
  {
    result &= ~k_BackStencilReferenceStateMask;
  }

  return result;
}
