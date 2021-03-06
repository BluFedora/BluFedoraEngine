//
// Author: Shareef Abdoul-Raheem
// Sprite Vertex Shader
//
#version 450

layout(location = 0) in vec4 in_Position;
layout(location = 1) in vec4 in_Normal;
layout(location = 2) in vec4 in_Color;
layout(location = 3) in vec2 in_UV;


//
// Camera Uniform Layout
//
layout(std140, set = 0, binding = 0) uniform u_Set0
{
  mat4  u_CameraProjection;
  mat4  u_CameraInvViewProjection;
  mat4  u_CameraViewProjection;
  mat4  u_CameraView;
  vec3  u_CameraForward;
  float u_Time;
  vec3  u_CameraPosition;
  float u_CameraAspect;
  vec3  u_CameraAmbient;
};

mat4 Camera_getViewMatrix()
{
  return u_CameraView;
}
// #include "assets/shaders/standard/object.ubo.glsl"

layout(location = 0) out vec3 frag_WorldNormal;
layout(location = 1) out vec3 frag_Color;
layout(location = 2) out vec2 frag_UV;

void main()
{
  vec4 object_position = vec4(in_Position.xyz, 1.0);
  vec4 clip_position   = u_CameraViewProjection * object_position;

  // frag_WorldNormal = mat3(u_NormalModel) * in_Normal.xyz;
  frag_WorldNormal = in_Normal.xyz;
  frag_Color       = in_Color.rgb;
  frag_UV          = in_UV;

  gl_Position = clip_position;
}
