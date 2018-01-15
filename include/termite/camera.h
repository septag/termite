#pragma once

#include "gfx_utils.h"

namespace tee
{
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

    struct Camera;
    struct Camera2D;

    // Camera
    TEE_API void camInit(Camera* cam, float fov = 60.0f, float fnear = 0.1f, float ffar = 100.0f);
    TEE_API void camLookAt(Camera* cam, const vec3_t pos, const vec3_t lookat);
    TEE_API void camCalcFrustumCorners(const Camera* cam, vec3_t result[8], float aspectRatio, 
                                           float nearOverride = 0, float farOverride = 0);
    TEE_API void camCalcFrustumPlanes(plane_t result[CameraPlane::Count], const mat4_t& viewProjMtx);
    TEE_API void camPitch(Camera* cam, float pitch);
    TEE_API void camYaw(Camera* cam, float yaw);
    TEE_API void camPitchYaw(Camera* cam, float pitch, float yaw);
    TEE_API void camRoll(Camera* cam, float roll);
    TEE_API void camForward(Camera* cam, float fwd);
    TEE_API void camStrafe(Camera* cam, float strafe);
    TEE_API mat4_t camViewMtx(const Camera* cam);
    TEE_API mat4_t camProjMtx(const Camera* cam, float aspectRatio);

    // Camera2D
    TEE_API void cam2dInit(Camera2D* cam, float refWidth, float refHeight, 
                               DisplayPolicy::Enum policy, float zoom = 1.0f, const vec2_t pos = vec2(0, 0));
    TEE_API void cam2dPan(Camera2D* cam, vec2_t pan);
    TEE_API void cam2dZoom(Camera2D* cam, float zoom);
    TEE_API mat4_t cam2dViewMtx(const Camera2D& cam);
    TEE_API mat4_t cam2dProjMtx(const Camera2D& cam);
    TEE_API rect_t cam2dGetRect(const Camera2D& cam);

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

        inline void init(float _fov = 60.0f, float _fnear = 0.1f, float _ffar = 100.0f)
        {
            camInit(this, _fov, _fnear, _ffar);
        }

        inline void lookAt(const vec3_t _pos, const vec3_t _lookat)
        {
            camLookAt(this, _pos, _lookat);
        }

        inline void calcFrustumCorners(vec3_t _result[8], float _aspectRatio, float _nearOverride = 0, float _farOverride = 0) const
        {
            camCalcFrustumCorners(this, _result, _aspectRatio, _nearOverride, _farOverride);
        }

        inline void calcFrustumPlanes(plane_t _result[CameraPlane::Count], const mat4_t& _viewProjMtx) const
        {
            camCalcFrustumPlanes(_result, _viewProjMtx);
        }

        inline void rotatePitch(float _pitch)
        {
            camPitch(this, _pitch);
        }

        inline void rotateYaw(float _yaw)
        {
            camYaw(this, _yaw);
        }

        inline void rotatePitchYaw(float _pitch, float _yaw)
        {
            camPitchYaw(this, _pitch, _yaw);
        }

        inline void rotateRoll(float _roll)
        {
            camRoll(this, _roll);
        }

        inline void moveForward(float _fwd)
        {
            camForward(this, _fwd);
        }

        inline void moveStrafe(float _strafe)
        {
            camStrafe(this, _strafe);
        }

        inline mat4_t getViewMtx() const
        {
            return camViewMtx(this);
        }

        inline mat4_t getProjMtx(float _aspectRatio) const
        {
            return camProjMtx(this, _aspectRatio);
        }
    };

    struct Camera2D
    {
        vec2_t pos;
        float zoom;
        float zoomPercentOffset;
        float refWidth;
        float refHeight;
        DisplayPolicy::Enum policy;

        inline void init(float _refWidth, float _refHeight, DisplayPolicy::Enum _policy, 
                         float _zoom = 1.0f, const vec2_t _pos = vec2(0, 0))
        {
            cam2dInit(this, _refWidth, _refHeight, _policy, _zoom, _pos);
        }

        inline void pan(vec2_t _pan)
        {
            cam2dPan(this, _pan);
        }

        inline void setZoom(float _zoom)
        {
            cam2dZoom(this, _zoom);
        }

        inline float getZoom() const
        {
            return zoom + zoom*zoomPercentOffset;
        }

        inline mat4_t getViewMtx() const
        {
            return cam2dViewMtx(*this);
        }

        inline mat4_t getProjMtx() const
        {
            return cam2dProjMtx(*this);
        }

        inline rect_t getRect() const
        {
            return cam2dGetRect(*this);
        }
    };
} // namespace tee