#include "pch.h"
#include "camera.h"

namespace tee {

    Camera::Camera()
    {
        forward = right = up = pos = vec3_t::Zero;
    }

    void Camera::init(float _fov /*= 60.0f*/, float _fnear /*= 0.1f*/, float _ffar /*= 100.0f*/)
    {
        right = vec3(1.0f, 0, 0);
        up = vec3(0, 1.0f, 0);
        forward = vec3(0, 0, 1.0f);

        fov = _fov;
        fnear = _fnear;
        ffar = _ffar;
        quat = quaternionI();
    }

    void Camera::lookAt(const vec3_t _pos, const vec3_t _lookat)
    {
        forward = _lookat - _pos;
        bx::vec3Norm(forward.f, forward.f);

        bx::vec3Cross(right.f, vec3_t::Up.f, forward.f);
        bx::vec3Norm(right.f, right.f);

        bx::vec3Cross(up.f, forward.f, right.f);

        mat4_t m = mat4f3(right.f, up.f, forward.f, vec3(0, 0, 0).f);

        pos = _pos;
        bx::quatFromMtx(quat.f, m.f);

        float euler[3];
        bx::quatToEuler(euler, quat.f);
        pitch = euler[0];
        yaw = euler[1];
    }

    void Camera::calcFrustumCorners(vec3_t _result[8], float _aspectRatio, float _nearOverride /*= 0*/, float _farOverride /*= 0*/) const
    {
        const float ff = _farOverride != 0.0f ? _farOverride : ffar;
        const float fn = _nearOverride != 0.0f ? _nearOverride : fnear;
        const float fovRad = bx::toRad(fov);

        vec3_t xaxis = right;
        vec3_t yaxis = up;
        vec3_t zaxis = forward;

        float nearPlaneHeight = bx::tan(fovRad * 0.5f) * fn;
        float nearPlaneWidth = nearPlaneHeight * _aspectRatio;

        float farPlaneHeight = bx::tan(fovRad * 0.5f) * ff;
        float farPlaneWidth = farPlaneHeight * _aspectRatio;

        // Far/Near planes
        vec3_t centerNear = zaxis*fn + pos;
        vec3_t centerFar = zaxis*ff + pos;

        // Scaled axises
        vec3_t xNearScaled = xaxis*nearPlaneWidth;
        vec3_t xFarScaled = xaxis*farPlaneWidth;
        vec3_t yNearScaled = yaxis*nearPlaneHeight;
        vec3_t yFarScaled = yaxis*farPlaneHeight;

        // Near quad
        _result[0] = centerNear - (xNearScaled + yNearScaled);
        _result[1] = centerNear - (xNearScaled - yNearScaled);
        _result[2] = centerNear + (xNearScaled + yNearScaled);
        _result[3] = centerNear + (xNearScaled - yNearScaled);

        // Far quad
        _result[4] = centerFar - (xFarScaled + yFarScaled);
        _result[5] = centerFar - (xFarScaled - yFarScaled);
        _result[6] = centerFar + (xFarScaled + yFarScaled);
        _result[7] = centerFar + (xFarScaled - yFarScaled);
    }

    void Camera::calcFrustumPlanes(plane_t _result[CameraPlane::Count], const mat4_t& _viewProjMtx) const
    {
        const mat4_t vp = _viewProjMtx;
        _result[0] = plane(vp.m14 + vp.m11, vp.m24 + vp.m21, vp.m34 + vp.m31, vp.m44 + vp.m41);
        _result[1] = plane(vp.m14 - vp.m11, vp.m24 - vp.m21, vp.m34 - vp.m31, vp.m44 - vp.m41);
        _result[2] = plane(vp.m14 - vp.m12, vp.m24 - vp.m22, vp.m34 - vp.m32, vp.m44 - vp.m42);
        _result[3] = plane(vp.m14 + vp.m12, vp.m24 + vp.m22, vp.m34 + vp.m32, vp.m44 + vp.m42);
        _result[4] = plane(vp.m13, vp.m23, vp.m33, vp.m43);
        _result[5] = plane(vp.m14 - vp.m13, vp.m24 - vp.m23, vp.m34 - vp.m33, vp.m44 - vp.m43);

        // Normalize _result
        for (int i = 0; i < int(CameraPlane::Count); i++) {
            plane_t& p = _result[i];
            vec3_t nd = vec3(p.nx, p.ny, p.nz);

            float nlen_inv = 1.0f / bx::vec3Length(nd.f);
            p.nx *= nlen_inv;
            p.ny *= nlen_inv;
            p.nz *= nlen_inv;
            p.d *= nlen_inv;
        }
    }

    void Camera::updateRotation()
    {
        mat4_t m;
        bx::mtxQuat(m.f, quat.f);
        right = vec3(m.m11, m.m12, m.m13);
        up = vec3(m.m21, m.m22, m.m23);
        forward = vec3(m.m31, m.m32, m.m33);
    }

    void Camera::rotatePitch(float _pitch)
    {
        pitch += _pitch;
        quat_t q1, q2;
        bx::quatRotateAxis(q1.f, vec3(0, 1.0f, 0).f, yaw);
        bx::quatRotateAxis(q2.f, vec3(1.0f, 0, 0).f, pitch);
        bx::quatMul(quat.f, q2.f, q1.f);

        updateRotation();
    }

    void Camera::rotatePitch(float _pitch, float _min, float _max)
    {
        pitch += _pitch;
        pitch = bx::clamp(pitch, _min, _max);
        quat_t q1, q2;
        bx::quatRotateAxis(q1.f, vec3(0, 1.0f, 0).f, yaw);
        bx::quatRotateAxis(q2.f, vec3(1.0f, 0, 0).f, pitch);
        bx::quatMul(quat.f, q2.f, q1.f);

        updateRotation();
    }

    void Camera::rotateYaw(float _yaw)
    {
        yaw += _yaw;

        quat_t q1, q2;
        bx::quatRotateAxis(q1.f, vec3(0, 1.0f, 0).f, yaw);
        bx::quatRotateAxis(q2.f, vec3(1.0f, 0, 0).f, pitch);
        bx::quatMul(quat.f, q2.f, q1.f);

        updateRotation();
    }

    void Camera::rotatePitchYaw(float _pitch, float _yaw)
    {
        pitch += _pitch;
        yaw += _yaw;

        quat_t q1, q2;
        bx::quatRotateAxis(q1.f, vec3(0, 1.0f, 0).f, _yaw);
        bx::quatRotateAxis(q2.f, vec3(1.0f, 0, 0).f, _pitch);
        bx::quatMul(quat.f, q2.f, q1.f);

        updateRotation();
    }

    void Camera::rotateRoll(float _roll)
    {
        quat_t q;
        bx::quatRotateAxis(q.f, vec3(0, 0, 1.0f).f, _roll);
        quat_t qa = quat;
        bx::quatMul(quat.f, qa.f, q.f);

        updateRotation();
    }

    void Camera::setPitch(float _pitch)
    {
        pitch = _pitch;
        quat_t q1, q2;
        bx::quatRotateAxis(q1.f, vec3(0, 1.0f, 0).f, yaw);
        bx::quatRotateAxis(q2.f, vec3(1.0f, 0, 0).f, pitch);
        bx::quatMul(quat.f, q2.f, q1.f);

        updateRotation();
    }

    void Camera::setYaw(float _yaw)
    {
        yaw = _yaw;
        quat_t q1, q2;
        bx::quatRotateAxis(q1.f, vec3(0, 1.0f, 0).f, yaw);
        bx::quatRotateAxis(q2.f, vec3(1.0f, 0, 0).f, pitch);
        bx::quatMul(quat.f, q2.f, q1.f);

        updateRotation();
    }

    void Camera::moveForward(float _fwd)
    {
        pos = pos + forward*_fwd;
    }

    void Camera::moveStrafe(float _strafe)
    {
        pos = pos + right*_strafe;
    }

    mat4_t Camera::getViewMat() const
    {
        return mat4(right.x, up.x, forward.x,
                    right.y, up.y, forward.y,
                    right.z, up.z, forward.z,
                    -bx::vec3Dot(right.f, pos.f), -bx::vec3Dot(up.f, pos.f), -bx::vec3Dot(forward.f, pos.f));
    }

    mat4_t Camera::getProjMat(float _aspectRatio) const
    {
        float xscale = 1.0f /bx::tan(bx::toRad(fov)*0.5f);
        float yscale = _aspectRatio*xscale;
        float zf = ffar;
        float zn = fnear;

        return mat4(xscale, 0, 0, 0,
                    0, yscale, 0, 0,
                    0, 0, zf / (zf - zn), 1.0f,
                    0, 0, zn*zf / (zn - zf), 0);
    }

    Camera2D::Camera2D()
    {
        refWidth = refHeight = 0;
        scale = 1.0f;
        pos = vec2_t::Zero;
        policy = DisplayPolicy::FitToHeight;
    }

    void Camera2D::init(float _refWidth, float _refHeight, DisplayPolicy::Enum _policy, float _zoom, const vec2_t _pos)
    {
        refWidth = _refWidth;
        refHeight = _refHeight;
        scale = _zoom;
        pos = _pos;
        policy = _policy;
    }

    void Camera2D::pan(vec2_t _pan)
    {
        pos = pos + _pan;
    }

    void Camera2D::zoom(float _zoom)
    {
        scale = _zoom;
    }

    mat4_t Camera2D::getViewMat() const
    {
        return mat4(1.0f, 0, 0,
                    0, 1.0f, 0,
                    0, 0, 1.0f,
                    -pos.x, -pos.y, 0);
    }

    static vec2_t calcCam2dHalfSize(const Camera2D& cam)
    {
        float zoom = cam.scale;
        float s = 1.0f / zoom;

        // keep the ratio in scale of 1.0
        float hw, hh;
        float ratio = cam.refWidth / cam.refHeight;
        if (cam.policy == DisplayPolicy::FitToHeight) {
            hw = 0.5f;
            hh = hw / ratio;
        } else if (cam.policy == DisplayPolicy::FitToWidth) {
            hh = 0.5f;
            hw = ratio * hh;
        } else {
            hh = 0.5f;
            hw = 0.5f;
        }

        return vec2(hw*s, hh*s);
    }

    mat4_t Camera2D::getProjMat() const
    {
        mat4_t projMtx;
        vec2_t halfSize = calcCam2dHalfSize(*this);
        bx::mtxOrtho(projMtx.f, -halfSize.x, halfSize.x, -halfSize.y, halfSize.y, 0, 1.0f, 0, false);
        return projMtx;
    }

    rect_t Camera2D::getViewRect() const
    {
        vec2_t halfSize = calcCam2dHalfSize(*this);
        return rect(-halfSize.x + pos.x, -halfSize.y + pos.y, halfSize.x + pos.x, halfSize.y + pos.y);
    }

    ///
    void tmath::cam2dInit(Camera2D* cam, float refWidth, float refHeight, DisplayPolicy::Enum policy, float _zoom, const vec2_t pos)
    {
        cam->init(refWidth, refHeight, policy, _zoom, pos);
    }

    void tmath::cam2dPan(Camera2D* cam, vec2_t _pan)
    {
        cam->pan(_pan);
    }

    void tmath::cam2dZoom(Camera2D* cam, float _zoom)
    {
        cam->zoom(_zoom);
    }

    mat4_t tmath::cam2dGetViewMat(const Camera2D* cam)
    {
        return cam->getViewMat();
    }

    mat4_t tmath::cam2dGetProjMat(const Camera2D* cam)
    {
        return cam->getProjMat();
    }

    rect_t tmath::cam2dGetViewRect(const Camera2D* cam)
    {
        return cam->getViewRect();
    }

} // namespace tee