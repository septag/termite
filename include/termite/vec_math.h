#pragma once

#include "bx/bx.h"
#include "bx/platform.h"
#include "bx/fpumath.h"
#include <float.h>
#include <cassert>

#if BX_COMPILER_CLANG || BX_COMPILER_GCC
#  include <limits.h>
#endif

namespace termite
{
    union vec4_t
    {
        struct
        {
            float x;
            float y;
            float z;
            float w;
        };

        float f[4];
    };

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

    union vec2_t
    {
        struct
        {
            float x;
            float y;
        };

        float f[2];
    };

    inline vec2_t vec2f(float x, float y)
    {
        vec2_t v;
        v.x = x;        v.y = y;
        return v;
    }

    inline vec2_t vec2fv(const float* f)
    {
        vec2_t v;
        v.x = f[0];     v.y = f[1];
        return v;
    }

    union vec3_t
    {
        struct
        {
            float x;
            float y;
            float z;
        };

        float f[3];
    };

    inline vec3_t vec3f(float x, float y, float z)
    {
        vec3_t v;
        v.x = x;        v.y = y;        v.z = z;
        return v;
    }

    inline vec3_t vec3fv(const float* f)
    {
        vec3_t v;
        v.x = f[0];     v.y = f[1];     v.z = f[3];
        return v;
    }

    union color_t
    {
        struct
        {
            uint8_t r;
            uint8_t g;
            uint8_t b;
            uint8_t a;
        };

        uint32_t n;
    };

    inline color_t color4u(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
    {
        color_t c;
        c.r = r;    c.g = g;    c.b = b;    c.a = a;
        return c;
    }

    inline color_t color1n(uint32_t n)
    {
        color_t c;
        c.n = n;
        return c;
    }

    union vec2i_t
    {
        struct
        {
            int x;
            int y;
        };

        int n[2];
    };

    inline vec2i_t vec2iv(const int* n)
    {
        vec2i_t v;
        v.x = n[0];     v.y = n[1];
        return v;
    }

    inline vec2i_t vec2i(int x, int y)
    {
        vec2i_t v;
        v.x = x;        v.y = y;
        return v;
    }

    union quat_t
    {
        struct
        {
            float x;
            float y;
            float z;
            float w;
        };

        float f[4];
    };

    inline quat_t quatf(float x, float y, float z, float w)
    {
        quat_t q;
        q.x = x;        q.y = y;        q.z = z;        q.w = w;
        return q;
    }

    inline quat_t quatfv(const float* f)
    {
        quat_t q;
        q.x = f[0];     q.y = f[1];     q.z = f[2];     q.w = f[3];
        return q;
    }

    union mtx4x4_t
    {
        struct
        {
            float m11, m12, m13, m14;
            float m21, m22, m23, m24;
            float m31, m32, m33, m34;
            float m41, m42, m43, m44;
        };

        struct
        {
            float r0[4];
            float r1[4];
            float r2[4];
            float r3[4];
        };

        struct
        {
            vec4_t vrow0;
            vec4_t vrow1;
            vec4_t vrow2;
            vec4_t vrow3;
        };

        float f[16];
    };

    inline mtx4x4_t mtx4x4f(float _m11, float _m12, float _m13, float _m14,
                            float _m21, float _m22, float _m23, float _m24,
                            float _m31, float _m32, float _m33, float _m34,
                            float _m41, float _m42, float _m43, float _m44)
    {
        mtx4x4_t m;
        m.m11 = _m11;     m.m12 = _m12;     m.m13 = _m13;     m.m14 = _m14;
        m.m21 = _m21;     m.m22 = _m22;     m.m23 = _m23;     m.m24 = _m24;
        m.m31 = _m31;     m.m32 = _m32;     m.m33 = _m33;     m.m34 = _m34;
        m.m41 = _m41;     m.m42 = _m42;     m.m43 = _m43;     m.m44 = _m44;
        return m;
    }

    inline mtx4x4_t mtx4x4fv(const float* _r0, const float* _r1, const float* _r2, const float* _r3)
    {
        mtx4x4_t m;
        m.m11 = _r0[0];     m.m12 = _r0[1];     m.m13 = _r0[2];     m.m14 = _r0[3];
        m.m21 = _r1[0];     m.m22 = _r1[1];     m.m23 = _r1[2];     m.m24 = _r1[3];
        m.m31 = _r2[0];     m.m32 = _r2[1];     m.m33 = _r2[2];     m.m34 = _r2[3];
        m.m41 = _r3[0];     m.m42 = _r3[1];     m.m43 = _r3[2];     m.m44 = _r3[3];
        return m;
    }

    inline mtx4x4_t mtx4x4v(const vec4_t& _row0, const vec4_t& _row1, const vec4_t& _row2, const vec4_t& _row3)
    {
        return mtx4x4fv(_row0.f, _row1.f, _row2.f, _row3.f);
    }

    union mtx3x3_t
    {
        struct
        {
            float m11, m12, m13;
            float m21, m22, m23;
            float m31, m32, m33;
        };

        struct
        {
            float r0[3];
            float r1[3];
            float r2[3];
        };

        struct
        {
            vec3_t vrow0;
            vec3_t vrow1;
            vec3_t vrow2;
        };

        float f[9];
    };


    inline mtx3x3_t mtx3x3f(float _m11, float _m12, float _m13,
                            float _m21, float _m22, float _m23,
                            float _m31, float _m32, float _m33)
    {
        mtx3x3_t m;
        m.m11 = _m11;     m.m12 = _m12;     m.m13 = _m13;
        m.m21 = _m21;     m.m22 = _m22;     m.m23 = _m23;
        m.m31 = _m31;     m.m32 = _m32;     m.m33 = _m33;
        return m;
    }

    inline mtx3x3_t mtx3x3fv(const float* _r0, const float* _r1, const float* _r2)
    {
        mtx3x3_t m;
        m.m11 = _r0[0];     m.m12 = _r0[1];     m.m13 = _r0[2];
        m.m21 = _r1[0];     m.m22 = _r1[1];     m.m23 = _r1[2];
        m.m31 = _r2[0];     m.m32 = _r2[1];     m.m33 = _r2[2];
        return m;
    }

    inline mtx3x3_t mtx3x3v(const vec3_t& _row0, const vec3_t& _row1, const vec3_t& _row2)
    {
        return mtx3x3fv(_row0.f, _row1.f, _row2.f);
    }

    union aabb_t
    {
        struct
        {
            float xmin, ymin, zmin;
            float xmax, ymax, zmax;
        };

        struct
        {
            float fmin[3];
            float fmax[3];
        };

        struct
        {
            vec3_t vmin;
            vec3_t vmax;
        };

        float f[6];
    };


    inline aabb_t aabbEmpty()
    {
        aabb_t aabb;
        aabb.xmin = aabb.ymin = aabb.zmin = FLT_MAX;
        aabb.xmax = aabb.ymax = aabb.zmax = -FLT_MAX;
        return aabb;
    }

    inline aabb_t aabbv(const vec3_t& _min, const vec3_t& _max)
    {
        aabb_t aabb;
        aabb.vmin = _min;
        aabb.vmax = _max;
        return aabb;
    }

    inline aabb_t aabbfv(const float* _min, const float* _max)
    {
        aabb_t aabb;
        aabb.fmin[0] = _min[0];  aabb.fmin[1] = _min[1];  aabb.fmin[2] = _min[2];
        aabb.fmax[0] = _max[0];  aabb.fmax[1] = _max[1];  aabb.fmax[2] = _max[2];
        return aabb;
    }

    inline aabb_t aabbf(float _xmin, float _ymin, float _zmin, float _xmax, float _ymax, float _zmax)
    {
        aabb_t aabb;
        aabb.xmin = _xmin;   aabb.ymin = _ymin;   aabb.zmin = _zmin;
        aabb.xmax = _xmax;   aabb.ymax = _ymax;  aabb.zmax = _zmax;
        return aabb;
    }

    union rect_t
    {
        struct
        {
            float xmin, ymin;
            float xmax, ymax;
        };

        struct
        {
            float left, top;
            float right, bottom;
        };

        struct
        {
            vec2_t vmin;
            vec2_t vmax;
        };

        float f[4];
    };

    inline rect_t rectEmpty()
    {
        rect_t rc;
        rc.xmin = rc.ymin = FLT_MAX;
        rc.xmax = rc.ymax = -FLT_MAX;
        return rc;
    }

    inline rect_t rectf(float _xmin, float _ymin, float _xmax, float _ymax)
    {
        rect_t rc;
        rc.xmin = _xmin;   rc.ymin = _ymin;
        rc.xmax = _xmax;   rc.ymax = _ymax;
        return rc;
    }

    inline rect_t rectfv(const float* _min, const float* _max)
    {
        rect_t rc;
        rc.xmin = _min[0];
        rc.ymin = _min[1];
        rc.xmax = _max[0];
        rc.ymax = _max[1];
        return rc;
    }

    inline rect_t rectv(const vec2_t& _vmin, const vec2_t& _vmax)
    {
        rect_t rc;
        rc.vmin = _vmin;
        rc.vmax = _vmax;
        return rc;
    }

    union recti_t
    {
        struct
        {
            int xmin, ymin;
            int xmax, ymax;
        };

        struct
        {
            int left, top;
            int right, bottom;
        };

        struct
        {
            vec2i_t vmin;
            vec2i_t vmax;
        };

        int n[4];
    };

    inline recti_t rectiEmpty()
    {
        recti_t rc;
        rc.xmin = rc.ymin = INT_MAX;
        rc.xmax = rc.ymax = -INT_MAX;
        return rc;
    }

    inline recti_t recti(int _xmin, int _ymin, int _xmax, int _ymax)
    {
        recti_t rc;
        rc.xmin = _xmin;   rc.ymin = _ymin;
        rc.xmax = _xmax;   rc.ymax = _ymax;
        return rc;
    }

    inline recti_t rectin(const int* _min, const int* _max)
    {
        recti_t rc;
        rc.xmin = _min[0];
        rc.ymin = _min[1];
        rc.xmax = _max[0];
        rc.ymax = _max[1];
        return rc;
    }

    inline recti_t rectiv(const vec2i_t& _vmin, const vec2i_t& _vmax)
    {
        recti_t rc;
        rc.vmin = _vmin;
        rc.vmax = _vmax;
        return rc;
    }

    union sphere_t
    {
        struct
        {
            float x, y, z, r;
        };

        vec3_t center;
        float f[4];
    };

    inline sphere_t spherefv(const float* _f)
    {
        sphere_t s;
        s.x = _f[0];  s.y = _f[1];  s.z = _f[2];
        s.r = _f[3];
        return s;
    }

    inline sphere_t spheref(float _x, float _y, float _z, float _r)
    {
        sphere_t s;
        s.x = _x;     s.y = _y;     s.z = _z;     s.r = _r;
        return s;
    }

    inline sphere_t sphereCenterRadius(const vec3_t& _cp, float _r)
    {
        sphere_t s;
        s.center = _cp;        s.r = _r;
        return s;
    }

    union plane_t
    {
        struct
        {
            float nx, ny, nz, d;
        };

        struct
        {
            vec3_t n;
        };

        float f[4];
    };

    inline plane_t planefv(const float* _f)
    {
        plane_t p;
        p.nx = _f[0];     p.ny = _f[1];     p.nz = _f[2];
        p.d = _f[3];
        return p;
    }

    inline plane_t planef(float _nx, float _ny, float _nz, float _d)
    {
        plane_t p;
        p.nx = _nx;       p.ny = _ny;       p.nz = _nz;       p.d = _d;
        return p;
    }

    inline plane_t planePointDist(const vec3_t& _n, float _d)
    {
        plane_t p;
        p.n = _n;        p.d = _d;
        return p;
    }

    // Quaternion
    inline quat_t quatIdent()
    {
        return quatf(0, 0, 0, 1.0f);
    }

    // mtx4x4
    inline mtx4x4_t mtx4x4f3(float _m11, float _m12, float _m13,
                             float _m21, float _m22, float _m23,
                             float _m31, float _m32, float _m33,
                             float _m41, float _m42, float _m43)
    {
        return mtx4x4f(_m11, _m12, _m13, 0,
                       _m21, _m22, _m23, 0,
                       _m31, _m32, _m33, 0,
                       _m41, _m42, _m43, 1.0f);
    }

    inline mtx4x4_t mtx4x4fv3(const float* _r0, const float* _r1, const float* _r2, const float* _r3)
    {
        return mtx4x4f(_r0[0], _r0[1], _r0[2], 0,
                       _r1[0], _r1[1], _r1[2], 0,
                       _r2[0], _r2[1], _r2[2], 0,
                       _r3[0], _r3[1], _r3[2], 1.0f);
    }

    inline mtx4x4_t mtx4x4From3x3(const mtx3x3_t& m)
    {
        return mtx4x4f3(m.m11, m.m12, m.m13,
                        m.m21, m.m22, m.m23,
                        0, 0, 1.0,
                        m.m31, m.m32, m.m33);


    }

    inline mtx4x4_t mtx4x4Ident()
    {
        return mtx4x4f(1.0f, 0, 0, 0,
                       0, 1.0f, 0, 0,
                       0, 0, 1.0f, 0,
                       0, 0, 0, 1.0f);
    }
    
    inline mtx3x3_t mtx3x3Ident()
    {
        return mtx3x3f(1.0f, 0, 0,
                       0, 1.0f, 0,
                       0, 0, 1.0f);
    }

    inline mtx3x3_t mtx3x3From4x4(const mtx4x4_t& m)
    {
        return mtx3x3f(m.m11, m.m12, m.m13,
                       m.m12, m.m22, m.m23,
                       m.m41, m.m42, m.m43);
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

        float dx = bx::fabsolute((rc.xmin + wHalf) - center.x);
        float dy = bx::fabsolute((rc.ymin + hHalf) - center.y);
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

    // aabb_t functions
    inline aabb_t aabbTransform(const aabb_t& b, const mtx4x4_t& mtx)
    {
        vec3_t vmin;
        vec3_t vmax;

        /* start with translation part */
        vec4_t t(mtx.vrow3);
        
        vmin.x = vmax.x = t.x;
        vmin.y = vmax.y = t.y;
        vmin.z = vmax.z = t.z;

        if (mtx.m11 > 0.0f) {
            vmin.x += mtx.m11 * b.vmin.x;
            vmax.x += mtx.m11 * b.vmax.x;
        } else {
            vmin.x += mtx.m11 * b.vmax.x;
            vmax.x += mtx.m11 * b.vmin.x;
        }

        if (mtx.m12 > 0.0f) {
            vmin.y += mtx.m12 * b.vmin.x;
            vmax.y += mtx.m12 * b.vmax.x;
        } else {
            vmin.y += mtx.m12 * b.vmax.x;
            vmax.y += mtx.m12 * b.vmin.x;
        }

        if (mtx.m13 > 0.0f) {
            vmin.z += mtx.m13 * b.vmin.x;
            vmax.z += mtx.m13 * b.vmax.x;
        } else {
            vmin.z += mtx.m13 * b.vmax.x;
            vmax.z += mtx.m13 * b.vmin.x;
        }

        if (mtx.m21 > 0.0f) {
            vmin.x += mtx.m21 * b.vmin.y;
            vmax.x += mtx.m21 * b.vmax.y;
        } else {
            vmin.x += mtx.m21 * b.vmax.y;
            vmax.x += mtx.m21 * b.vmin.y;
        }

        if (mtx.m22 > 0.0f) {
            vmin.y += mtx.m22 * b.vmin.y;
            vmax.y += mtx.m22 * b.vmax.y;
        } else {
            vmin.y += mtx.m22 * b.vmax.y;
            vmax.y += mtx.m22 * b.vmin.y;
        }

        if (mtx.m23 > 0.0f) {
            vmin.z += mtx.m23 * b.vmin.y;
            vmax.z += mtx.m23 * b.vmax.y;
        } else {
            vmin.z += mtx.m23 * b.vmax.y;
            vmax.z += mtx.m23 * b.vmin.y;
        }

        if (mtx.m31 > 0.0f) {
            vmin.x += mtx.m31 * b.vmin.z;
            vmax.x += mtx.m31 * b.vmax.z;
        } else {
            vmin.x += mtx.m31 * b.vmax.z;
            vmax.x += mtx.m31 * b.vmin.z;
        }

        if (mtx.m32 > 0.0f) {
            vmin.y += mtx.m32 * b.vmin.z;
            vmax.y += mtx.m32 * b.vmax.z;
        } else {
            vmin.y += mtx.m32 * b.vmax.z;
            vmax.y += mtx.m32 * b.vmin.z;
        }

        if (mtx.m33 > 0.0f) {
            vmin.z += mtx.m33 * b.vmin.z;
            vmax.z += mtx.m33 * b.vmax.z;
        } else {
            vmin.z += mtx.m33 * b.vmax.z;
            vmax.z += mtx.m33 * b.vmin.z;
        }

        return aabbv(vmin, vmax);
    }

    inline void aabbPushPoint(aabb_t* rb, const vec3_t& pt)
    {
        bx::vec3Min(rb->fmin, rb->fmin, pt.f);
        bx::vec3Max(rb->fmax, rb->fmax, pt.f);
    }    

    /*
    *            6                                7
    *              ------------------------------
    *             /|                           /|
    *            / |                          / |
    *           /  |                         /  |
    *          /   |                        /   |
    *         /    |                       /    |
    *        /     |                      /     |
    *       /      |                     /      |
    *      /       |                    /       |
    *     /        |                   /        |
    *  2 /         |                3 /         |
    *   /----------------------------/          |
    *   |          |                 |          |
    *   |          |                 |          |      +Y
    *   |        4 |                 |          |
    *   |          |-----------------|----------|      |
    *   |         /                  |         /  5    |
    *   |        /                   |        /        |       +Z
    *   |       /                    |       /         |
    *   |      /                     |      /          |     /
    *   |     /                      |     /           |    /
    *   |    /                       |    /            |   /
    *   |   /                        |   /             |  /
    *   |  /                         |  /              | /
    *   | /                          | /               |/
    *   |/                           |/                ----------------- +X
    *   ------------------------------
    *  0                              1
    */
    inline vec3_t aabbGetCorner(const aabb_t& box, int index)
    {
        assert(index < 8);
        return vec3f((index & 1) ? box.vmax.x : box.vmin.x,
                     (index & 2) ? box.vmax.y : box.vmin.y,
                     (index & 4) ? box.vmax.z : box.vmin.z);
    }

    inline void mtxProjPlane(mtx4x4_t* r, const vec3_t planeNorm)
    {
        memset(r, 0x00, sizeof(mtx4x4_t));

        r->m11 = 1.0f - planeNorm.x*planeNorm.x;
        r->m22 = 1.0f - planeNorm.y*planeNorm.y;
        r->m33 = 1.0f - planeNorm.z*planeNorm.z;

        r->m12 = r->m21 = -planeNorm.x*planeNorm.y;
        r->m13 = r->m31 = -planeNorm.x*planeNorm.z;
        r->m23 = r->m32 = -planeNorm.y*planeNorm.z;

        r->m44 = 1.0f;
    }

    // Rect
    inline rect_t rectwh(float _x, float _y, float _width, float _height)
    {
        return rectf(_x, _y, _x + _width, _y + _height);
    }

    // Color
    inline color_t colorRGBAf(float r, float g, float b, float a = 1.0f)
    {
        uint8_t _r = uint8_t(r * 255.0f);
        uint8_t _g = uint8_t(g * 255.0f);
        uint8_t _b = uint8_t(b * 255.0f);
        uint8_t _a = uint8_t(a * 255.0f);
        return color4u(_r, _g, _b, _a);
    }

    inline color_t colorRGBAfv(const float* _f)
    {
        uint8_t _r = uint8_t(_f[0] * 255.0f);
        uint8_t _g = uint8_t(_f[1] * 255.0f);
        uint8_t _b = uint8_t(_f[2] * 255.0f);
        uint8_t _a = uint8_t(_f[3] * 255.0f);
        return color4u(_r, _g, _b, _a);
    }

    inline color_t colorPremultiplyAlpha(color_t color, float alpha)
    {
        float _alpha = float((color.n >> 24) & 0xff) / 255.0f;
        float premulAlpha = alpha * _alpha;
        return color1n(((uint32_t(premulAlpha * 255.0f) & 0xff) << 24) | (color.n & 0xffffff));
    }

    inline vec4_t colorToVec4(color_t c)
    {
        float rcp = 1.0f / 255.0f;
        return vec4f(
            float(c.n & 0xff) * rcp,
            float((c.n >> 8) & 0xff) * rcp,
            float((c.n >> 16) & 0xff) * rcp,
            float((c.n >> 24) & 0xff) * rcp);
    }

    inline vec4_t colorToLinear(const vec4_t& c)
    {
        return vec4f(c.x*c.x, c.y*c.y, c.z*c.z, c.w*c.w);
    }

    // Operators
    inline vec2_t operator+(const vec2_t& a, const vec2_t& b)
    {
        return vec2f(a.x + b.x, a.y + b.y);       
    }

    inline vec2_t operator-(const vec2_t& a, const vec2_t& b)
    {
        return vec2f(a.x - b.x, a.y - b.y);
    }

    inline vec2_t operator*(const vec2_t& v, float k)
    {
        return vec2f(v.x*k, v.y*k);
    }

    inline vec2_t operator*(float k, const vec2_t& v)
    {
        return vec2f(v.x*k, v.y*k);
    }

    inline vec2_t operator*(const vec2_t& v0, const vec2_t& v1)
    {
        return vec2f(v0.x*v1.x, v0.y*v1.y);
    }

    inline vec3_t operator+(const vec3_t& v1, const vec3_t& v2)
    {
        vec3_t r;
        bx::vec3Add(r.f, v1.f, v2.f);
        return r;
    }

    inline vec3_t operator-(const vec3_t& v1, const vec3_t& v2)
    {
        vec3_t r;
        bx::vec3Sub(r.f, v1.f, v2.f);
        return r;
    }

    inline vec3_t operator*(const vec3_t& v, float k)
    {
        vec3_t r;
        bx::vec3Mul(r.f, v.f, k);
        return r;
    }

    inline vec3_t operator*(float k, const vec3_t& v)
    {
        vec3_t r;
        bx::vec3Mul(r.f, v.f, k);
        return r;
    }

    inline mtx4x4_t operator*(const mtx4x4_t& a, const mtx4x4_t& b)
    {
        mtx4x4_t r;
        bx::mtxMul(r.f, a.f, b.f);
        return r;
    }

    inline mtx3x3_t operator*(const mtx3x3_t& a, const mtx3x3_t& b)
    {
        mtx3x3_t r;
        bx::mtx3x3Mul(r.f, a.f, b.f);
        return r;
    }

    inline quat_t operator*(const quat_t& a, const quat_t& b)
    {
        quat_t r;
        bx::quatMul(r.f, a.f, b.f);
        return r;
    }

} // namespace termite
