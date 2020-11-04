#ifndef BIFROST_CAMERA_H
#define BIFROST_CAMERA_H

#include "bifrost_mat4x4.h"      /* Mat4x4       */
#include "bifrost_math_export.h" /* BIFROST_MATH_API */
#include "bifrost_vec2.h"        /* Vec2i        */
#include "bifrost_vec3.h"        /* Vec3f, Rectf */

#ifdef __cplusplus
extern "C" {
#endif
typedef enum CameraMode_t
{
  BIFROST_CAMERA_MODE_ORTHOGRAPHIC,
  BIFROST_CAMERA_MODE_FRUSTRUM,
  BIFROST_CAMERA_MODE_PRESPECTIVE,
  BIFROST_CAMERA_MODE_PRESPECTIVE_INFINITY

} CameraMode;

typedef struct CameraModeParams_t
{
  CameraMode mode;

  union
  {
    // NOTE(Shareef):
    //   Used by:
    //     BIFROST_CAMERA_MODE_ORTHOGRAPHIC
    //     BIFROST_CAMERA_MODE_FRUSTRUM
    //   Units = Arbitrary World Space Units
    Rectf orthographic_bounds;

    struct
    {
      // NOTE(Shareef):
      //   Used by:
      //     BIFROST_CAMERA_MODE_PRESPECTIVE
      //     BIFROST_CAMERA_MODE_PRESPECTIVE_INFINITY
      //   Units = Degrees
      float field_of_view_y;

      // NOTE(Shareef):
      //   Used by:
      //     BIFROST_CAMERA_MODE_PRESPECTIVE
      //     BIFROST_CAMERA_MODE_PRESPECTIVE_INFINITY
      //   Units = Its just a ratio of width / height
      float aspect_ratio;
    };
  };

  // NOTE(Shareef):
  //   Units = Arbitrary World Space Units
  float near_plane;
  // NOTE(Shareef):
  //   Ignored by:
  //     BIFROST_CAMERA_MODE_PRESPECTIVE_INFINITY
  float far_plane;

} CameraModeParams;

typedef Vec3f Plane;

typedef struct bfCameraFrustum_t
{
  Plane planes[6];

} bfCameraFrustum;

void bfCameraFrustum_set(bfCameraFrustum* self, const Mat4x4* view_proj);

typedef struct BifrostCamera_t
{
  Vec3f            position;
  Vec3f            forward;
  Vec3f            up;
  Vec3f            _worldUp;
  Vec3f            _right;
  float            _yaw;    // Radians
  float            _pitch;  // Radians
  CameraModeParams camera_mode;
  Mat4x4           proj_cache;
  Mat4x4           view_cache;
  Mat4x4           inv_proj_cache;       // The inverse cached for 3D picking.
  Mat4x4           inv_view_cache;       // The inverse cached for 3D picking.
  Mat4x4           inv_view_proj_cache;  //
  int              needs_update[2];      //   [0] - for proj_cache.
                                         //   [1] - for view_cache.

} BifrostCamera;

BF_MATH_API void  Camera_init(BifrostCamera* cam, const Vec3f* pos, const Vec3f* world_up, float yaw, float pitch);
BF_MATH_API void  Camera_update(BifrostCamera* cam);
BF_MATH_API void  bfCamera_openGLProjection(const BifrostCamera* cam, Mat4x4* out_projection);
BF_MATH_API void  Camera_move(BifrostCamera* cam, const Vec3f* dir, float amt);
BF_MATH_API void  Camera_moveLeft(BifrostCamera* cam, float amt);
BF_MATH_API void  Camera_moveRight(BifrostCamera* cam, float amt);
BF_MATH_API void  Camera_moveUp(BifrostCamera* cam, float amt);
BF_MATH_API void  Camera_moveDown(BifrostCamera* cam, float amt);
BF_MATH_API void  Camera_moveForward(BifrostCamera* cam, float amt);
BF_MATH_API void  Camera_moveBackward(BifrostCamera* cam, float amt);
BF_MATH_API void  Camera_addPitch(BifrostCamera* cam, float amt);
BF_MATH_API void  Camera_addYaw(BifrostCamera* cam, float amt);
BF_MATH_API void  Camera_mouse(BifrostCamera* cam, float offsetx, float offsety);
BF_MATH_API void  Camera_setFovY(BifrostCamera* cam, float value);
BF_MATH_API void  Camera_onResize(BifrostCamera* cam, uint width, uint height);
BF_MATH_API void  Camera_setProjectionModified(BifrostCamera* cam);
BF_MATH_API void  Camera_setViewModified(BifrostCamera* cam);
BF_MATH_API Vec3f Camera_castRay(BifrostCamera* cam, Vec2i screen_space, Vec2i screen_size);

// 'New' API

BF_MATH_API void bfCamera_setPosition(BifrostCamera* cam, const Vec3f* pos);

/* Ray API */

typedef struct bfRay3D_t
{
  Vec3f origin;              /* Required                   */
  Vec3f direction;           /* Required                   */
  Vec3f inv_direction;       /* Derived From direction     */
  int   inv_direction_signs; /* Derived From inv_direction */

} bfRay3D;

typedef struct bfRayCastResult_t
{
  int   did_hit;  /* Check this to se if the ray hit anything.      */
  float min_time; /* Set To An Undefined Value if did_hit is false. */
  float max_time; /* Set To An Undefined Value if did_hit is false. */

} bfRayCastResult;

BF_MATH_API bfRay3D         bfRay3D_make(Vec3f origin, Vec3f direction);
BF_MATH_API bfRayCastResult bfRay3D_intersectsAABB(const bfRay3D* ray, Vec3f aabb_min, Vec3f aabb_max);

#ifdef __cplusplus
}
#endif

#endif /* BIFROST_CAMERA_H */