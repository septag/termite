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

    struct TEE_API Camera
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

        Camera();
        void init(float _fov = 60.0f, float _fnear = 0.1f, float _ffar = 100.0f);
        void lookAt(const vec3_t _pos, const vec3_t _lookat);
        void calcFrustumCorners(vec3_t _result[8], float _aspectRatio, float _nearOverride = 0, float _farOverride = 0) const;
        void calcFrustumPlanes(plane_t _result[CameraPlane::Count], const mat4_t& _viewProjMtx) const;
        void rotatePitch(float _pitch);
        void rotateYaw(float _yaw);
        void rotatePitchYaw(float _pitch, float _yaw);
        void rotateRoll(float _roll);
        void moveForward(float _fwd);
        void moveStrafe(float _strafe);
        mat4_t getViewMat() const;
        mat4_t getProjMat(float _aspectRatio) const;

    private:
        void updateRotation();
    };

    struct TEE_API Camera2D
    {
        vec2_t pos;
        float scale;
        float refWidth;
        float refHeight;
        DisplayPolicy::Enum policy;

        Camera2D();
        void init(float _refWidth, float _refHeight, DisplayPolicy::Enum _policy, 
                  float _zoom = 1.0f, const vec2_t _pos = vec2(0, 0));
        void pan(vec2_t _pan);
        void zoom(float _zoom);
        mat4_t getViewMat() const;
        mat4_t getProjMat() const;
        rect_t getViewRect() const;
    };

    // API to be used in plugins
    namespace tmath {
        TEE_API void cam2dInit(Camera2D* cam, float refWidth, float refHeight, DisplayPolicy::Enum policy, float zoom, const vec2_t pos);
        TEE_API void cam2dPan(Camera2D* cam, vec2_t pan);
        TEE_API void cam2dZoom(Camera2D* cam, float zoom);
        TEE_API mat4_t cam2dGetViewMat(const Camera2D* cam);
        TEE_API mat4_t cam2dGetProjMat(const Camera2D* cam);
        TEE_API rect_t cam2dGetViewRect(const Camera2D* cam);
    }
} // namespace tee