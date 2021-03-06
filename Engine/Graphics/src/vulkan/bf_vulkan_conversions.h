#ifndef BF_VULKAN_CONVERSIONS_H
#define BF_VULKAN_CONVERSIONS_H

#include "bf/bf_gfx_api.h"

#include <vulkan/vulkan.h>

#if __cplusplus
extern "C" {
#endif

VkFormat              bfVkConvertFormat(bfGfxImageFormat format);
VkImageLayout         bfVkConvertImgLayout(bfGfxImageLayout layout);
VkSampleCountFlags    bfVkConvertSampleFlags(/* bfGfxSampleFlags */ uint32_t flags);
VkSampleCountFlagBits bfVkConvertSampleCount(bfGfxSampleFlags bit);
VkClearValue          bfVKConvertClearColor(const bfClearValue* color);
VkIndexType           bfVkConvertIndexType(bfGfxIndexType idx_type);
VkShaderStageFlagBits bfVkConvertShaderType(bfShaderType type);
VkShaderStageFlagBits bfVkConvertShaderStage(bfShaderStageBits flags);
VkPrimitiveTopology   bfVkConvertTopology(bfDrawMode draw_mode);
VkViewport            bfVkConvertViewport(const bfViewport* viewport);
VkRect2D              bfVkConvertScissorRect(const bfScissorRect* viewport);
VkPolygonMode         bfVkConvertPolygonMode(bfPolygonFillMode polygon_mode);
VkCullModeFlags       bfVkConvertCullModeFlags(/* bfCullFaceFlags */ uint32_t cull_face_flags);
VkFrontFace           bfVkConvertFrontFace(bfFrontFace front_face);
VkFormat              bfVkConvertVertexFormatAttrib(bfGfxVertexFormatAttribute vertex_format_attrib);
VkFlags               bfVkConvertBufferUsageFlags(/* bfBufferUsageFlags */ uint16_t flags);
VkFlags               bfVkConvertBufferPropertyFlags(/* bfBufferPropertyFlags */ uint16_t flags);
VkImageType           bfVkConvertTextureType(bfTextureType type);
VkFilter              bfVkConvertSamplerFilterMode(bfTexSamplerFilterMode mode);
VkSamplerAddressMode  bfVkConvertSamplerAddressMode(bfTexSamplerAddressMode mode);
VkCompareOp           bfVkConvertCompareOp(bfCompareOp op);
VkStencilOp           bfVkConvertStencilOp(bfStencilOp op);
VkLogicOp             bfVkConvertLogicOp(bfLogicOp op);
VkBlendFactor         bfVkConvertBlendFactor(bfBlendFactor factor);
VkBlendOp             bfVkConvertBlendOp(bfBlendOp factor);
VkFlags               bfVkConvertColorMask(/* bfColorMask */ uint16_t flags);
VkPipelineStageFlags  bfVkConvertPipelineStageFlags(bfGfxPipelineStageBits flags);
VkAccessFlags         bfVkConvertAccessFlags(bfGfxAccessFlagsBits flags);
uint32_t              bfConvertQueueIndex(const uint32_t queue_list[BF_GFX_QUEUE_MAX], bfGfxQueueType type);

VkImageView bfCreateImageView(
 VkDevice           device,
 VkImage            image,
 VkImageViewType    view_type,
 VkFormat           format,
 VkImageAspectFlags aspect_flags,
 uint32_t           base_mip_level,
 uint32_t           base_array_layer,
 uint32_t           mip_levels,
 uint32_t           layer_count);

VkImageView bfCreateImageView2D(
 VkDevice           device,
 VkImage            image,
 VkFormat           format,
 VkImageAspectFlags aspect_flags,
 uint32_t           mip_levels);

#if __cplusplus
}
#endif

#endif /* BIFROST_VULKAN_CONVERSIONS_H */
