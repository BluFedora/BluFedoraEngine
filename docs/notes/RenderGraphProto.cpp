enum class BufferReadUsage
{
  COMPUTE_SSBO,
  // VK_ACCESS_SHADER_READ_BIT
  // VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
  COMPUTE_UNIFORM,
  // VK_ACCESS_UNIFORM_READ_BIT
  // VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
  GRAPHICS_VERTEX,
  // VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
  // VK_PIPELINE_STAGE_VERTEX_INPUT_BIT
  GRAPHICS_INDEX,
  // VK_ACCESS_INDEX_READ_BIT
  // VK_PIPELINE_STAGE_VERTEX_INPUT_BIT
  GRAPHICS_UNIFORM,
  // VK_ACCESS_UNIFORM_READ_BIT
  // VK_PIPELINE_STAGE_VERTEX_SHADER_BIT or VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
  GRAPHICS_DRAW_INDIRECT,
  // VK_ACCESS_INDIRECT_COMMAND_READ_BIT
  // VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT
};

enum class BufferWriteUsage
{
  COMPUTE_SSBO,
  // VK_ACCESS_SHADER_WRITE_BIT
  // VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
  COMPUTE_IMAGE,
  // VK_ACCESS_SHADER_WRITE_BIT
  // VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
  // VK_IMAGE_LAYOUT_GENERAL
};

enum class ImageReadUsage
{
  // GRAPHICS_FRAGMENT_*
  // VK_ACCESS_SHADER_READ_BIT
  // VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
  GRAPHICS_FRAGMENT_SAMPLE,
  // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
  GRAPHICS_FRAGMENT_SAMPLE_DEPTH_STENCIL_RW,
  // VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL
  GRAPHICS_FRAGMENT_SAMPLE_DEPTH_STENCIL_WR,
  // VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL
  GRAPHICS_FRAGMENT_SAMPLE_DEPTH_STENCIL_RR,
  // VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
  GRAPHICS_FRAGMENT_STORAGE_IMAGE,
  // VK_IMAGE_LAYOUT_GENERAL

  // Repeat for GRAPHICS_VERTEX
  // VK_PIPELINE_STAGE_VERTEX_INPUT_BIT

  // Compute needs a layout of VK_IMAGE_LAYOUT_GENERAL
};

enum class ImageWriteUsage
{
  // The only layout available:
  // VK_IMAGE_LAYOUT_GENERAL

  // You can do stuff in Vertex, and Fragment, and Compute Shaders.

  COMPUTE_WRITE,
  // VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
  // VK_ACCESS_SHADER_WRITE_BIT
  // VK_IMAGE_LAYOUT_GENERAL -> X

  GRAPHICS_FRAGMENT_WRITE,
  // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
  // VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
  // VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL -> X

  GRAPHCICS_DEPTH_STENCIL,
  // VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
  // VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
  // VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
  GRAPHCICS_STENCIL,
  // VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL
  GRAPHCICS_DEPTH,
  // VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL
};
