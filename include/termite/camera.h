#pragma once

#include "vec_math.h"
#include "gfx_utils.h"

namespace termite
{
    struct Camera
    {
        vec3_t forward;
        vec3_t right;
        vec3_t up;
        vec3_t pos;

        quat_t quat;
        float ffar;
        float fnear;
        float fov;

        float pitch;
        float yaw;
    };

    struct CameraPlane
    {
        enum Enum
        {
            Left = 0,
            Right,
            Top,
            Bottom,
            Near,
            Far,

            Count
        };
    };

    TERMITE_API void camInit(Camera* cam, float fov = 60.0f, float fnear = 0.1f, float ffar = 100.0f);
    TERMITE_API void camLookAt(Camera* cam, const vec3_t pos, const vec3_t lookat);
    TERMITE_API void camCalcFrustumCorners(Camera* cam, vec3_t result[8], float aspectRatio, 
                                           float nearOverride = 0, float farOverride = 0);
    TERMITE_API void camCalcFrustumPlanes(plane_t result[CameraPlane::Count], const mtx4x4_t& viewProjMtx);
    TERMITE_API void camPitch(Camera* cam, float pitch);
    TERMITE_API void camYaw(Camera* cam, float yaw);
    TERMITE_API void camPitchYaw(Camera* cam, float pitch, float yaw);
    TERMITE_API void camRoll(Camera* cam, float roll);
    TERMITE_API void camForward(Camera* cam, float fwd);
    TERMITE_API void camStrafe(Camera* cam, float strafe);
    TERMITE_API mtx4x4_t camViewMtx(Camera* cam);
    TERMITE_API mtx4x4_t camProjMtx(Camera* cam, float aspectRatio);

    struct Camera2D
    {
        vec2_t pos;
        float zoom;
        float refWidth;
        float refHeight;
        DisplayPolicy::Enum policy;
    };

    TERMITE_API void cam2dInit(Camera2D* cam, float refWidth, float refHeight, 
                               DisplayPolicy::Enum policy, float zoom = 1.0f, const vec2_t pos = vec2f(0, 0));
    TERMITE_API void cam2dPan(Camera2D* cam, vec2_t pan);
    TERMITE_API void cam2dZoom(Camera2D* cam, float zoom);
    TERMITE_API mtx4x4_t cam2dViewMtx(const Camera2D& cam);
    TERMITE_API mtx4x4_t cam2dProjMtx(const Camera2D& cam);
    TERMITE_API rect_t cam2dGetRect(const Camera2D& cam);
} // namespace termite