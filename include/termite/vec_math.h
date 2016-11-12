#pragma once

#include "bx/bx.h"
#include "bx/fpumath.h"
#include <float.h>
#include <cassert>

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

        inline vec4_t() {}
        explicit inline vec4_t(float _v) : x(_v), y(_v), z(_v), w(_v) {}
        inline vec4_t(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
        explicit inline vec4_t(const float* _f) : x(_f[0]), y(_f[1]), z(_f[2]), w(_f[3]) {}
    };

    union vec2_t
    {
        struct
        {
            float x;
            float y;
        };

        float f[2];

        inline vec2_t() {}
        explicit inline vec2_t(float _v) : x(_v), y(_v) {}
        explicit inline vec2_t(const float* _f) : x(_f[0]), y(_f[1]) {}
        inline vec2_t(float _x, float _y) : x(_x), y(_y) {}
    };

    union vec3_t
    {
        struct
        {
            float x;
            float y;
            float z;
        };

        float f[3];

        inline vec3_t() {}
        explicit inline vec3_t(float _v) : x(_v), y(_v), z(_v) {}
        inline vec3_t(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
        explicit inline vec3_t(const float* _f) : x(_f[0]), y(_f[1]), z(_f[2]) {}
    };

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

        inline color_t() : n(0) {}
        inline color_t(uint32_t _n) : n(_n) {}
        inline color_t& operator=(uint32_t _n) { this->n = _n; return *this; }
        inline color_t(uint8_t _r, uint8_t _g, uint8_t _b, uint8_t _a = 255) : r(_r), g(_g), b(_b), a(_a) {}
        inline operator uint32_t() const { return n; }
    };

    union vec2i_t
    {
        struct
        {
            int x;
            int y;
        };

        int n[2];

        inline vec2i_t() {}
        explicit inline vec2i_t(int _v) : x(_v), y(_v) {}
        explicit inline vec2i_t(const int* _n) : x(_n[0]), y(_n[1]) {}
        inline vec2i_t(int _x, int _y) : x(_x), y(_y) {}
    };

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

        inline quat_t() {}
        inline quat_t(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
        explicit inline quat_t(const float* _f) : x(_f[0]), y(_f[1]), z(_f[2]), w(_f[3]) {}
    };

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

        inline mtx4x4_t() {}
        inline mtx4x4_t(float _m11, float _m12, float _m13, float _m14,
                        float _m21, float _m22, float _m23, float _m24,
                        float _m31, float _m32, float _m33, float _m34,
                        float _m41, float _m42, float _m43, float _m44)
        {
            m11 = _m11;     m12 = _m12;     m13 = _m13;     m14 = _m14;
            m21 = _m21;     m22 = _m22;     m23 = _m23;     m24 = _m24;
            m31 = _m31;     m32 = _m32;     m33 = _m33;     m34 = _m34;
            m41 = _m41;     m42 = _m42;     m43 = _m43;     m44 = _m44;
        }

        inline mtx4x4_t(const float* _r0, const float* _r1, const float* _r2, const float* _r3)
        {
            m11 = _r0[0];     m12 = _r0[1];     m13 = _r0[2];     m14 = _r0[3];
            m21 = _r1[0];     m22 = _r1[1];     m23 = _r1[2];     m24 = _r1[3];
            m31 = _r2[0];     m32 = _r2[1];     m33 = _r2[2];     m34 = _r2[3];
            m41 = _r3[0];     m42 = _r3[1];     m43 = _r3[2];     m44 = _r3[3];
        }

        inline mtx4x4_t(const vec4_t& _row0, const vec4_t& _row1, const vec4_t& _row2, const vec4_t& _row3)
        {
            mtx4x4_t(_row0.f, _row1.f, _row2.f, _row3.f);
        }
    };

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

        inline mtx3x3_t() {}

        inline mtx3x3_t(float _m11, float _m12, float _m13,
                        float _m21, float _m22, float _m23,
                        float _m31, float _m32, float _m33)
        {
            m11 = _m11;     m12 = _m12;     m13 = _m13;
            m21 = _m21;     m22 = _m22;     m23 = _m23;
            m31 = _m31;     m32 = _m32;     m33 = _m33;
        }

        inline mtx3x3_t(const float* _r0, const float* _r1, const float* _r2)
        {
            m11 = _r0[0];     m12 = _r0[1];     m13 = _r0[2];
            m21 = _r1[0];     m22 = _r1[1];     m23 = _r1[2];
            m31 = _r2[0];     m32 = _r2[1];     m33 = _r2[2];
        }

        inline mtx3x3_t(const vec3_t& _row0, const vec3_t& _row1, const vec3_t& _row2)
        {
            mtx3x3_t(_row0.f, _row1.f, _row2.f);
        }
    };

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

        inline aabb_t() 
        {
            xmin = ymin = zmin = FLT_MAX;
            xmax = ymax = zmax = -FLT_MAX;
        }

        inline aabb_t(const vec3_t& _min, const vec3_t& _max)
        {
            vmin = _min;
            vmax = _max;
        }

        inline aabb_t(const float* _min, const float* _max)
        {
            fmin[0] = _min[0];  fmin[1] = _min[1];  fmin[2] = _min[2];
            fmax[0] = _max[0];  fmax[1] = _max[1];  fmax[2] = _max[2];
        }

        inline aabb_t(float _xmin, float _ymin, float _zmin, float _xmax, float _ymax, float _zmax)
        {
            xmin = _xmin;   ymin = _ymin;   zmin = _zmin;
            xmax = _xmax;   ymax = _ymax;   zmax = _zmax;
        }
    };

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

        inline rect_t()
        {
            xmin = ymin = FLT_MAX;
            xmax = ymax = -FLT_MAX;
        }

        inline rect_t(float _xmin, float _ymin, float _xmax, float _ymax)
        {
            xmin = _xmin;   ymin = _ymin;
            xmax = _xmax;   ymax = _ymax;
        }

        inline rect_t(const float* _min, const float* _max)
        {
            xmin = _min[0];
            ymin = _min[1];
            xmax = _max[0];
            ymax = _max[1];
        }

        inline rect_t(const vec2_t& _vmin, const vec2_t& _vmax)
        {
            vmin = _vmin;
            vmax = _vmax;
        }
    };

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

        inline recti_t()
        {
            xmin = ymin = INT_MAX;
            xmax = ymax = -INT_MAX;
        }

        inline recti_t(int _xmin, int _ymin, int _xmax, int _ymax)
        {
            xmin = _xmin;   ymin = _ymin;
            xmax = _xmax;   ymax = _ymax;
        }

        inline recti_t(const int* _min, const int* _max)
        {
            xmin = _min[0];
            ymin = _min[1];
            xmax = _max[0];
            ymax = _max[1];
        }

        inline recti_t(const vec2i_t& _vmin, const vec2i_t& _vmax)
        {
            vmin = _vmin;
            vmax = _vmax;
        }
    };

    union sphere_t
    {
        struct
        {
            float x, y, z, r;
        };

        vec3_t center;
        float f[4];

        inline sphere_t() {}
        inline sphere_t(const float* _f)
        {
            x = _f[0];  y = _f[1];  z = _f[2];
            r = _f[3];
        }

        inline sphere_t(float _x, float _y, float _z, float _r)
        {
            x = _x;     y = _y;     z = _z;     r = _r;
        }

        inline sphere_t(const vec3_t& _cp, float _r)
        {
            center = _cp;        r = _r;
        }
    };

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

        inline plane_t() {}
        inline plane_t(const float* _f)
        {
            nx = _f[0];     ny = _f[1];     nz = _f[2];
            d = _f[3];
        }

        inline plane_t(float _nx, float _ny, float _nz, float _d)
        {
            nx = _nx;       ny = _ny;       nz = _nz;       d = _d;
        }

        inline plane_t(const vec3_t& _n, float _d)
        {
            n = _n;        d = _d;
        }
    };

    // Quaternion
    inline quat_t quatIdent()
    {
        return quat_t(0, 0, 0, 1.0f);
    }

    // mtx4x4
    inline mtx4x4_t mtx4x4f3(float _m11, float _m12, float _m13,
                             float _m21, float _m22, float _m23,
                             float _m31, float _m32, float _m33,
                             float _m41, float _m42, float _m43)
    {
        return mtx4x4_t(_m11, _m12, _m13, 0,
                        _m21, _m22, _m23, 0,
                        _m31, _m32, _m33, 0,
                        _m41, _m42, _m43, 1.0f);
    }

    inline mtx4x4_t mtx4x4fv3(const float* _r0, const float* _r1, const float* _r2, const float* _r3)
    {
        return mtx4x4_t(_r0[0], _r0[1], _r0[2], 0,
                        _r1[0], _r1[1], _r1[2], 0,
                        _r2[0], _r2[1], _r2[2], 0,
                        _r3[0], _r3[1], _r3[2], 1.0f);
    }

    inline mtx4x4_t mtx4x4Ident()
    {
        return mtx4x4_t(1.0f, 0, 0, 0,
                        0, 1.0f, 0, 0,
                        0, 0, 1.0f, 0,
                        0, 0, 0, 1.0f);
    }
    
    inline mtx3x3_t mtx3x3Ident()
    {
        return mtx3x3_t(1.0f, 0, 0,
                        0, 1.0f, 0,
                        0, 0, 1.0f);
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
        if (rc1.xmax < rc2.xmin || rc1.xmin > rc2.xmax || rc1.ymax < rc2.ymin || rc1.ymin > rc2.ymax)
            return false;
        return true;
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

        return aabb_t(vmin, vmax);
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
        return vec3_t((index & 1) ? box.vmax.x : box.vmin.x,
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
        return rect_t(_x, _y, _x + _width, _y + _height);
    }

    // Color
    inline color_t colorRGBAf(float r, float g, float b, float a = 1.0f)
    {
        uint8_t _r = uint8_t(r * 255.0f);
        uint8_t _g = uint8_t(g * 255.0f);
        uint8_t _b = uint8_t(b * 255.0f);
        uint8_t _a = uint8_t(a * 255.0f);
        return color_t(_r, _g, _b, _a);
    }

    inline color_t colorRGBAfv(const float* _f)
    {
        uint8_t _r = uint8_t(_f[0] * 255.0f);
        uint8_t _g = uint8_t(_f[1] * 255.0f);
        uint8_t _b = uint8_t(_f[2] * 255.0f);
        uint8_t _a = uint8_t(_f[3] * 255.0f);
        return color_t(_r, _g, _b, _a);
    }

    inline color_t colorPremultiplyAlpha(color_t color, float alpha)
    {
        float _alpha = float((color.n >> 24) & 0xff) / 255.0f;
        float premulAlpha = alpha * _alpha;
        return color_t(((uint32_t(premulAlpha * 255.0f) & 0xff) << 24) | (color.n & 0xffffff));
    }

    inline vec4_t colorToVec4(color_t c)
    {
        float rcp = 1.0f / 255.0f;
        return vec4_t(
            float(c.n & 0xff) * rcp,
            float((c.n >> 8) & 0xff) * rcp,
            float((c.n >> 16) & 0xff) * rcp,
            float((c.n >> 24) & 0xff) * rcp);
    }

    inline vec4_t colorToLinear(const vec4_t& c)
    {
        return vec4_t(c.x*c.x, c.y*c.y, c.z*c.z, c.w*c.w);
    }

    // Operators
    inline vec2_t operator+(const vec2_t& a, const vec2_t& b)
    {
        return vec2_t(a.x + b.x, a.y + b.y);       
    }

    inline vec2_t operator-(const vec2_t& a, const vec2_t& b)
    {
        return vec2_t(a.x - b.x, a.y - b.y);
    }

    inline vec2_t operator*(const vec2_t& v, float k)
    {
        return vec2_t(v.x*k, v.y*k);
    }

    inline vec2_t operator*(float k, const vec2_t& v)
    {
        return vec2_t(v.x*k, v.y*k);
    }

    inline vec2_t operator*(const vec2_t& v0, const vec2_t& v1)
    {
        return vec2_t(v0.x*v1.x, v0.y*v1.y);
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
