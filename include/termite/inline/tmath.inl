#pragma once

#include "bx/platform.h"
#include "bxx/math.h"
#include <math.h>
#include <float.h>

#if BX_COMPILER_CLANG || BX_COMPILER_GCC
#  include <limits.h>
#endif

namespace tee {
    BX_CONST_FUNC inline vec4_t vec4(float x, float y, float z, float w)
    {
        vec4_t v;
        v.x = x;    v.y = y;    v.z = z;    v.w = w;
        return v;
    }

    BX_CONST_FUNC inline vec4_t vec4(const float* f)
    {
        vec4_t v;
        v.x = f[0];     v.y = f[1];     v.z = f[2];     v.w = f[3];
        return v;
    }

    BX_CONST_FUNC inline vec4_t vec4_splat(float n)
    {
        vec4_t v;
        v.x = v.y = v.z = v.w = n;
        return v;
    }

    BX_CONST_FUNC inline vec2_t vec2(float x, float y)
    {
        vec2_t v;
        v.x = x;        v.y = y;
        return v;
    }

    BX_CONST_FUNC inline vec2_t vec2(const float* f)
    {
        vec2_t v;
        v.x = f[0];     v.y = f[1];
        return v;
    }

    BX_CONST_FUNC inline vec2_t vec2_splat(float n)
    {
        vec2_t v;
        v.x = v.y = n;
        return v;
    }

    BX_CONST_FUNC inline vec3_t vec3(float x, float y, float z)
    {
        vec3_t v;
        v.x = x;        v.y = y;        v.z = z;
        return v;
    }

    BX_CONST_FUNC inline vec3_t vec3(const float* f)
    {
        vec3_t v;
        v.x = f[0];     v.y = f[1];     v.z = f[3];
        return v;
    }

    BX_CONST_FUNC inline vec3_t vec3_splat(float n)
    {
        vec3_t v;
        v.x = v.y = v.z = n;
        return v;
    }

    BX_CONST_FUNC inline ucolor_t ucolor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        ucolor_t c;
        c.r = r;    c.g = g;    c.b = b;    c.a = a;
        return c;
    }

    BX_CONST_FUNC inline ucolor_t ucolor(uint32_t n)
    {
        ucolor_t c;
        c.n = n;
        return c;
    }

    BX_CONST_FUNC inline ucolor_t ucolorf(float r, float g, float b, float a)
    {
        return ucolor(uint8_t(r * 255.0f), uint8_t(g * 255.0f), uint8_t(b * 255.0f), uint8_t(a * 255.0f));
    }

    BX_CONST_FUNC inline ucolor_t ucolor_inv(uint32_t n)
    {
        ucolor_t c;
        c.n = n;
        bx::xchg<uint8_t>(c.r, c.a);
        bx::xchg<uint8_t>(c.g, c.b);
        return c;
    }

    BX_CONST_FUNC inline ivec2_t ivec2(const int* n)
    {
        ivec2_t v;
        v.x = n[0];     v.y = n[1];
        return v;
    }

    BX_CONST_FUNC inline ivec2_t ivec2(int x, int y)
    {
        ivec2_t v;
        v.x = x;        v.y = y;
        return v;
    }

    // Quaternion
    BX_CONST_FUNC inline quat_t quaternion(float x, float y, float z, float w)
    {
        quat_t q;
        q.x = x;        q.y = y;        q.z = z;        q.w = w;
        return q;
    }

    BX_CONST_FUNC inline quat_t quaternion(const float* f)
    {
        quat_t q;
        q.x = f[0];     q.y = f[1];     q.z = f[2];     q.w = f[3];
        return q;
    }


    BX_CONST_FUNC inline quat_t quaternionI()
    {
        return quaternion(0, 0, 0, 1.0f);
    }

    BX_CONST_FUNC inline mat4_t mat4(float _m11, float _m12, float _m13, float _m14,
                                     float _m21, float _m22, float _m23, float _m24,
                                     float _m31, float _m32, float _m33, float _m34,
                                     float _m41, float _m42, float _m43, float _m44)
    {
        mat4_t m;
        m.m11 = _m11;     m.m12 = _m12;     m.m13 = _m13;     m.m14 = _m14;
        m.m21 = _m21;     m.m22 = _m22;     m.m23 = _m23;     m.m24 = _m24;
        m.m31 = _m31;     m.m32 = _m32;     m.m33 = _m33;     m.m34 = _m34;
        m.m41 = _m41;     m.m42 = _m42;     m.m43 = _m43;     m.m44 = _m44;
        return m;
    }

    BX_CONST_FUNC inline mat4_t mat4(const float* _r0, const float* _r1, const float* _r2, const float* _r3)
    {
        mat4_t m;
        m.m11 = _r0[0];     m.m12 = _r0[1];     m.m13 = _r0[2];     m.m14 = _r0[3];
        m.m21 = _r1[0];     m.m22 = _r1[1];     m.m23 = _r1[2];     m.m24 = _r1[3];
        m.m31 = _r2[0];     m.m32 = _r2[1];     m.m33 = _r2[2];     m.m34 = _r2[3];
        m.m41 = _r3[0];     m.m42 = _r3[1];     m.m43 = _r3[2];     m.m44 = _r3[3];
        return m;
    }

    BX_CONST_FUNC inline mat4_t mat4(const vec4_t& _row0, const vec4_t& _row1, const vec4_t& _row2, const vec4_t& _row3)
    {
        return mat4(_row0.f, _row1.f, _row2.f, _row3.f);
    }

    BX_CONST_FUNC inline mat3_t mat3(float _m11, float _m12, float _m13,
                                     float _m21, float _m22, float _m23,
                                     float _m31, float _m32, float _m33)
    {
        mat3_t m;
        m.m11 = _m11;     m.m12 = _m12;     m.m13 = _m13;
        m.m21 = _m21;     m.m22 = _m22;     m.m23 = _m23;
        m.m31 = _m31;     m.m32 = _m32;     m.m33 = _m33;
        return m;
    }

    BX_CONST_FUNC inline mat3_t mat3(const float* _r0, const float* _r1, const float* _r2)
    {
        mat3_t m;
        m.m11 = _r0[0];     m.m12 = _r0[1];     m.m13 = _r0[2];
        m.m21 = _r1[0];     m.m22 = _r1[1];     m.m23 = _r1[2];
        m.m31 = _r2[0];     m.m32 = _r2[1];     m.m33 = _r2[2];
        return m;
    }

    BX_CONST_FUNC inline mat3_t mat3(const vec3_t& _row0, const vec3_t& _row1, const vec3_t& _row2)
    {
        return mat3(_row0.f, _row1.f, _row2.f);
    }

    BX_CONST_FUNC inline aabb_t aabb(const vec3_t& _min, const vec3_t& _max)
    {
        aabb_t aabb;
        aabb.vmin = _min;
        aabb.vmax = _max;
        return aabb;
    }

    BX_CONST_FUNC inline aabb_t aabb(const float* _min, const float* _max)
    {
        aabb_t aabb;
        aabb.fmin[0] = _min[0];  aabb.fmin[1] = _min[1];  aabb.fmin[2] = _min[2];
        aabb.fmax[0] = _max[0];  aabb.fmax[1] = _max[1];  aabb.fmax[2] = _max[2];
        return aabb;
    }

    BX_CONST_FUNC inline aabb_t aabb(float _xmin, float _ymin, float _zmin, float _xmax, float _ymax, float _zmax)
    {
        aabb_t aabb;
        aabb.xmin = _xmin;   aabb.ymin = _ymin;   aabb.zmin = _zmin;
        aabb.xmax = _xmax;   aabb.ymax = _ymax;  aabb.zmax = _zmax;
        return aabb;
    }

    BX_CONST_FUNC inline rect_t rect(float _xmin, float _ymin, float _xmax, float _ymax)
    {
        rect_t rc;
        rc.xmin = _xmin;   rc.ymin = _ymin;
        rc.xmax = _xmax;   rc.ymax = _ymax;
        return rc;
    }

    BX_CONST_FUNC inline rect_t rect(const float* _min, const float* _max)
    {
        rect_t rc;
        rc.xmin = _min[0];
        rc.ymin = _min[1];
        rc.xmax = _max[0];
        rc.ymax = _max[1];
        return rc;
    }

    BX_CONST_FUNC inline rect_t rect(const vec2_t& _vmin, const vec2_t& _vmax)
    {
        rect_t rc;
        rc.vmin = _vmin;
        rc.vmax = _vmax;
        return rc;
    }

    BX_CONST_FUNC inline rect_t rectwh(float _x, float _y, float _width, float _height)
    {
        return rect(_x, _y, _x + _width, _y + _height);
    }

    BX_CONST_FUNC inline irect_t irect(int _xmin, int _ymin, int _xmax, int _ymax)
    {
        irect_t rc;
        rc.xmin = _xmin;   rc.ymin = _ymin;
        rc.xmax = _xmax;   rc.ymax = _ymax;
        return rc;
    }

    BX_CONST_FUNC inline irect_t irect(const int* _min, const int* _max)
    {
        irect_t rc;
        rc.xmin = _min[0];
        rc.ymin = _min[1];
        rc.xmax = _max[0];
        rc.ymax = _max[1];
        return rc;
    }

    BX_CONST_FUNC inline irect_t irect(const ivec2_t& _vmin, const ivec2_t& _vmax)
    {
        irect_t rc;
        rc.vmin = _vmin;
        rc.vmax = _vmax;
        return rc;
    }

    BX_CONST_FUNC inline irect_t irectwh(int _x, int _y, int _width, int _height)
    {
        return irect(_x, _y, _x + _width, _y + _height);
    }

    BX_CONST_FUNC inline sphere_t sphere(const float* _f)
    {
        sphere_t s;
        s.x = _f[0];  s.y = _f[1];  s.z = _f[2];
        s.r = _f[3];
        return s;
    }

    BX_CONST_FUNC inline sphere_t sphere(float _x, float _y, float _z, float _r)
    {
        sphere_t s;
        s.x = _x;     s.y = _y;     s.z = _z;     s.r = _r;
        return s;
    }

    BX_CONST_FUNC inline sphere_t sphere(const vec3_t& _cp, float _r)
    {
        sphere_t s;
        s.center = _cp;        s.r = _r;
        return s;
    }

    BX_CONST_FUNC inline plane_t plane(const float* _f)
    {
        plane_t p;
        p.nx = _f[0];     p.ny = _f[1];     p.nz = _f[2];
        p.d = _f[3];
        return p;
    }

    BX_CONST_FUNC inline plane_t plane(float _nx, float _ny, float _nz, float _d)
    {
        plane_t p;
        p.nx = _nx;       p.ny = _ny;       p.nz = _nz;       p.d = _d;
        return p;
    }

    BX_CONST_FUNC inline plane_t plane(const vec3_t& _n, float _d)
    {
        plane_t p;
        p.n = _n;        p.d = _d;
        return p;
    }

    // mtx4x4
    BX_CONST_FUNC inline mat4_t mat4(float _m11, float _m12, float _m13,
                                     float _m21, float _m22, float _m23,
                                     float _m31, float _m32, float _m33,
                                     float _m41, float _m42, float _m43)
    {
        return mat4(_m11, _m12, _m13, 0,
                    _m21, _m22, _m23, 0,
                    _m31, _m32, _m33, 0,
                    _m41, _m42, _m43, 1.0f);
    }

    BX_CONST_FUNC inline mat4_t mat4f3(const float* _r0, const float* _r1, const float* _r2, const float* _r3)
    {
        return mat4(_r0[0], _r0[1], _r0[2], 0,
                    _r1[0], _r1[1], _r1[2], 0,
                    _r2[0], _r2[1], _r2[2], 0,
                    _r3[0], _r3[1], _r3[2], 1.0f);
    }

    BX_CONST_FUNC inline mat4_t mat4(const mat3_t& m)
    {
        return mat4(m.m11, m.m12, m.m13,
                    m.m21, m.m22, m.m23,
                    0, 0, 1.0,
                    m.m31, m.m32, m.m33);


    }

    BX_CONST_FUNC inline mat3_t mat3(const mat4_t& m)
    {
        return mat3(m.m11, m.m12, m.m13,
                    m.m12, m.m22, m.m23,
                    m.m41, m.m42, m.m43);
    }

    // Operators
    BX_CONST_FUNC inline vec2_t operator+(const vec2_t& a, const vec2_t& b)
    {
        return vec2(a.x + b.x, a.y + b.y);
    }

    BX_CONST_FUNC inline vec2_t operator-(const vec2_t& a, const vec2_t& b)
    {
        return vec2(a.x - b.x, a.y - b.y);
    }

    BX_CONST_FUNC inline vec2_t operator*(const vec2_t& v, float k)
    {
        return vec2(v.x*k, v.y*k);
    }

    BX_CONST_FUNC inline vec2_t operator*(float k, const vec2_t& v)
    {
        return vec2(v.x*k, v.y*k);
    }

    BX_CONST_FUNC inline vec2_t operator*(const vec2_t& v0, const vec2_t& v1)
    {
        return vec2(v0.x*v1.x, v0.y*v1.y);
    }

    BX_CONST_FUNC inline vec3_t operator+(const vec3_t& v1, const vec3_t& v2)
    {
        vec3_t r;
        bx::vec3Add(r.f, v1.f, v2.f);
        return r;
    }

    BX_CONST_FUNC inline vec3_t operator-(const vec3_t& v1, const vec3_t& v2)
    {
        vec3_t r;
        bx::vec3Sub(r.f, v1.f, v2.f);
        return r;
    }

    BX_CONST_FUNC inline vec3_t operator*(const vec3_t& v, float k)
    {
        vec3_t r;
        bx::vec3Mul(r.f, v.f, k);
        return r;
    }

    BX_CONST_FUNC inline vec3_t operator*(float k, const vec3_t& v)
    {
        vec3_t r;
        bx::vec3Mul(r.f, v.f, k);
        return r;
    }

    BX_CONST_FUNC inline mat4_t operator*(const mat4_t& a, const mat4_t& b)
    {
        mat4_t r;
        bx::mtxMul(r.f, a.f, b.f);
        return r;
    }

    BX_CONST_FUNC inline mat3_t operator*(const mat3_t& a, const mat3_t& b)
    {
        mat3_t r;
        bx::mat3Mul(r.f, a.f, b.f);
        return r;
    }

    BX_CONST_FUNC inline quat_t operator*(const quat_t& a, const quat_t& b)
    {
        quat_t r;
        bx::quatMul(r.f, a.f, b.f);
        return r;
    }

    BX_CONST_FUNC inline mat4_t mat4I()
    {
        return mat4(1.0f, 0, 0, 0,
                    0, 1.0f, 0, 0,
                    0, 0, 1.0f, 0,
                    0, 0, 0, 1.0f);
    }

    BX_CONST_FUNC inline mat3_t mat3I()
    {
        return mat3(1.0f, 0, 0,
                    0, 1.0f, 0,
                    0, 0, 1.0f);
    }

    namespace tmath {

        // Reference: http://stackoverflow.com/questions/707370/clean-efficient-algorithm-for-wrapping-integers-in-c
        inline int iwrap(int kX, int const kLowerBound, int const kUpperBound)
        {
            int range_size = kUpperBound - kLowerBound + 1;

            if (kX < kLowerBound)
                kX += range_size * ((kLowerBound - kX) / range_size + 1);

            return kLowerBound + (kX - kLowerBound) % range_size;
        }

        inline float fwrap(float x, float vmin, float vmax)
        {
            vmax -= vmin;
            x = bx::mod(x, vmax);
            return x + vmin;
        }

        inline float falign(float value, float size)
        {
            return value - bx::abs(bx::mod(value, size));
        }

        // Line fgain, but goes up to 1.0 and then back to 0
        inline float fwave(float _time, float _gain)
        {
            if (_time < 0.5f) {
                return bx::bias(_time * 2.0f, _gain);
            } else {
                return 1.0f - bx::bias(_time * 2.0f - 1.0f, 1.0f - _gain);
            }
        }

        // Like fwave, but with saw-tooth like shape (drops fast after 0.5)
        inline float fwaveSharp(float _time, float _gain)
        {
            if (_time < 0.5f) {
                return bx::bias(_time * 2.0f, _gain);
            } else {
                return 1.0f - bx::bias(_time * 2.0f - 1.0f, _gain);
            }
        }

        inline vec2_t bezierCubic(const vec2_t pts[4], float t)
        {
            float ti = 1.0f - t;
            float ti2 = ti*ti;
            float ti3 = ti2*ti;

            vec2_t p = pts[0]*ti3 + pts[1]*3.0f*ti2*t + pts[2]*3.0f*ti*t*t + pts[3]*t*t*t;
            return p;
        }

        inline vec2_t bezierQuadric(const vec2_t pts[3], float t)
        {
            float ti = 1.0f - t;

            vec2_t p = pts[0]*ti*ti + pts[1]*2.0f*ti*t + pts[2]*t*t;
            return p;
        }

        inline vec4_t vec4f(float x, float y, float z, float w)
        {
            vec4_t v;
            v.x = x;    v.y = y;    v.z = z;    v.w = w;
            return v;
        }

        inline vec4_t vec4fv(const float* f)
        {
            vec4_t v;
            v.x = f[0];     v.y = f[1];     v.z = f[2];     v.w = f[3];
            return v;
        }

        // Rect
        inline bool rectTestPoint(const rect_t& rc, const vec2_t& pt)
        {
            if (pt.x < rc.xmin || pt.y < rc.ymin || pt.x > rc.xmax || pt.y > rc.ymax)
                return false;
            return true;
        }

        inline bool rectTestCircle(const rect_t& rc, const vec2_t& center, float radius)
        {
            float wHalf = (rc.xmax - rc.xmin)*0.5f;
            float hHalf = (rc.ymax - rc.ymin)*0.5f;

            float dx = bx::abs((rc.xmin + wHalf) - center.x);
            float dy = bx::abs((rc.ymin + hHalf) - center.y);
            if (dx > (radius + wHalf) || dy > (radius + hHalf))
                return false;

            return true;
        }

        inline bool rectTestRect(const rect_t& rc1, const rect_t& rc2)
        {
            return !(rc1.xmax < rc2.xmin || rc1.xmin > rc2.xmax || rc1.ymax < rc2.ymin || rc1.ymin > rc2.ymax);
        }

        inline void rectPushPoint(rect_t* rc, const vec2_t& pt)
        {
            bx::vec2Min(rc->vmin.f, rc->vmin.f, pt.f);
            bx::vec2Max(rc->vmax.f, rc->vmax.f, pt.f);
        }

        inline void aabbPushPoint(aabb_t* rb, const vec3_t& pt)
        {
            bx::vec3Min(rb->fmin, rb->fmin, pt.f);
            bx::vec3Max(rb->fmax, rb->fmax, pt.f);
        }

        inline vec3_t aabbGetCorner(const aabb_t& box, int index)
        {
            BX_ASSERT(index < 8);
            return vec3((index & 1) ? box.vmax.x : box.vmin.x,
                (index & 2) ? box.vmax.y : box.vmin.y,
                        (index & 4) ? box.vmax.z : box.vmin.z);
        }

        inline void mtxProjPlane(mat4_t* r, const vec3_t planeNorm)
        {
            bx::memSet(r, 0x00, sizeof(mat4_t));

            r->m11 = 1.0f - planeNorm.x*planeNorm.x;
            r->m22 = 1.0f - planeNorm.y*planeNorm.y;
            r->m33 = 1.0f - planeNorm.z*planeNorm.z;

            r->m12 = r->m21 = -planeNorm.x*planeNorm.y;
            r->m13 = r->m31 = -planeNorm.x*planeNorm.z;
            r->m23 = r->m32 = -planeNorm.y*planeNorm.z;

            r->m44 = 1.0f;
        }

        // Color
        inline ucolor_t colorPremultiplyAlpha(ucolor_t color, float alpha)
        {
            float _alpha = float((color.n >> 24) & 0xff) / 255.0f;
            float premulAlpha = alpha * _alpha;
            return ucolor(((uint32_t(premulAlpha * 255.0f) & 0xff) << 24) | (color.n & 0xffffff));
        }

        inline vec4_t ucolorToVec4(ucolor_t c)
        {
            float rcp = 1.0f / 255.0f;
            return vec4(
                float(c.n & 0xff) * rcp,
                float((c.n >> 8) & 0xff) * rcp,
                float((c.n >> 16) & 0xff) * rcp,
                float((c.n >> 24) & 0xff) * rcp);
        }

        inline vec4_t fcolorToLinear(const vec4_t& c)
        {
            return vec4(c.x*c.x, c.y*c.y, c.z*c.z, c.w*c.w);
        }

    }   // namespace math
}   // namespace tee