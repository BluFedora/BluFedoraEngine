#pragma once

#include "bifrost/data_structures/bifrost_variant.hpp"  // Variant<Ts...>
#include "bifrost_standard_renderer.hpp"

namespace bifrost
{
  static constexpr int k_DebugRendererNumLinesInBatch   = 256;
  static constexpr int k_DebugRendererNumVerticesInLine = 6;
  static constexpr int k_DebugRendererLineBatchSize     = k_DebugRendererNumLinesInBatch * k_DebugRendererNumVerticesInLine;

  struct VertexDebugLine final
  {
    Vector3f  curr_pos;
    Vector3f  next_pos;
    Vector3f  prev_pos;
    bfColor4u color;
    float     direction;
    float     thickness;
  };

  class DebugRenderer final
  {
    using DebugVertexBuffer = VertexDebugLine[k_DebugRendererLineBatchSize];

    struct BufferLink final
    {
      MultiBuffer<DebugVertexBuffer> gpu_buffer;
      BufferLink*                    next;
      int                            vertices_left;

      VertexDebugLine* currentVertex()
      {
        return &(*gpu_buffer.currentElement())[numVertices()];
      }

      int numVertices() const
      {
        return k_DebugRendererLineBatchSize - vertices_left;
      }
    };

    struct DrawLine final
    {
      Vector3f  a;
      Vector3f  b;
      bfColor4u color;
    };

    struct DrawAABB final
    {
      Vector3f  center;
      Vector3f  extents;
      bfColor4u color;
    };

    struct DrawCommand final  // NOLINT(hicpp-member-init)
    {
      float                       duration;
      Variant<DrawLine, DrawAABB> data;

      template<typename T>
      void init(float in_duration, T&& in_data)
      {
        duration = in_duration;
        data.set<T>(std::forward<T>(in_data));
      }
    };

   private:
    StandardRenderer*       m_Gfx;
    BufferLink*             m_LineBufferPool;
    List<DrawCommand>       m_DepthDrawCommands;
    List<DrawCommand>       m_OverlayDrawCommands;
    Array<BufferLink*>      m_LineBuffers[2];  // world, overlay
    bfShaderModuleHandle    m_DbgOverlay[2];   // vertex, fragment
    bfShaderModuleHandle    m_DbgWorld[2];     // vertex, fragment
    bfShaderProgramHandle   m_Shaders[2];      // world, overlay
    bfVertexLayoutSetHandle m_DbgVertexLayout;

   public:
    explicit DebugRenderer(IMemoryManager& memory);

    void init(StandardRenderer& renderer);

    inline void update(float delta_time)
    {
      updateDrawCommands(m_DepthDrawCommands, delta_time);
      updateDrawCommands(m_OverlayDrawCommands, delta_time);
    }

    void addLine(const Vector3f& a, const Vector3f& b, const bfColor4u& color, float duration = 0.0f, bool is_overlay = false);
    void addAABB(const Vector3f& center, const Vector3f& extents, const bfColor4u& color, float duration = 0.0f, bool is_overlay = false);

    void draw(bfGfxCommandListHandle command_list, Camera& camera, const bfGfxFrameInfo& frame_info, bool overlay);

    void deinit();

   private:
    List<DrawCommand>& grabCommandList(bool is_overlay);
    BufferLink*        grabFreeLink(const bfGfxFrameInfo& frame_info);
    void               clearLineBuffer(Array<BufferLink*>& buffer_link_list);
    void               updateDrawCommands(List<DrawCommand>& list, float delta_time);
    void               addVertices(Array<BufferLink*>& buffer, const Vector3f& a, const Vector3f& b, const bfColor4u& color, const bfGfxFrameInfo& frame_info);
    void               addTriangle(Array<BufferLink*>& buffer, const VertexDebugLine& a, const VertexDebugLine& b, const VertexDebugLine& c, const bfGfxFrameInfo& frame_info);
    IMemoryManager&    memory() const { return m_DepthDrawCommands.memory(); }
  };
}  // namespace bifrost
