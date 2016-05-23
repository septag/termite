#pragma once

#include "bx/bx.h"
#include "bx/fpumath.h"
#include <float.h>

// Add some 2D transform functions to bx
namespace bx
{
    inline void mtx3x3Translate(float* result, float x, float y)
    {
        memset(result, 0x00, sizeof(float) * 9);
        result[0] = 1.0f;
        result[4] = 1.0f;
        result[6] = x;
        result[7] = y;
        result[8] = 1.0f;
    }

    inline void mtx3x3Rotate(float* result, float theta)
    {
        memset(result, 0x00, sizeof(float) * 9);
        float c = fcos(theta);
        float s = fsin(theta);
        result[0] = c;     result[1] = -s;
        result[3] = s;     result[4] = c;
        result[8] = 1.0f;
    }

    inline void mtx3x3Scale(float* result, float sx, float sy)
    {
        memset(result, 0x00, sizeof(float) * 9);
        result[0] = sx;
        result[4] = sy;
        result[8] = 1.0f;
    }

    inline void vec2MulMtx3x3(float* __restrict _result, const float* __restrict _vec, const float* __restrict _mat)
    {
        _result[0] = _vec[0] * _mat[0] + _vec[1] * _mat[3] + _mat[6];
        _result[1] = _vec[0] * _mat[1] + _vec[1] * _mat[4] + _mat[7];
        _result[2] = _vec[0] * _mat[2] + _vec[1] * _mat[5] + _mat[8];
    }

    inline void vec3MulMtx3x3(float* __restrict _result, const float* __restrict _vec, const float* __restrict _mat)
    {
        _result[0] = _vec[0] * _mat[0] + _vec[1] * _mat[3] + _vec[2] * _mat[6];
        _result[1] = _vec[0] * _mat[1] + _vec[1] * _mat[4] + _vec[2] * _mat[7];
        _result[2] = _vec[0] * _mat[2] + _vec[1] * _mat[5] + _vec[2] * _mat[8];
    }

    inline void mtx3x3Mul(float* __restrict _result, const float* __restrict _a, const float* __restrict _b)
    {
        vec3MulMtx3x3(&_result[0], &_a[0], _b);
        vec3MulMtx3x3(&_result[3], &_a[3], _b);
        vec3MulMtx3x3(&_result[6], &_a[6], _b);
    }

    // Reference: http://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/
    inline void quatMtx(float* _result, const float* mtx)
    {
        float trace = mtx[0] + mtx[5] + mtx[10];
        if (trace > 0.00001f) {
            float s = 0.5f / sqrtf(trace + 1.0f);
            _result[3] = 0.25f / s;
            _result[0] = (mtx[9] - mtx[6]) * s;
            _result[1] = (mtx[2] - mtx[8]) * s;
            _result[2] = (mtx[4] - mtx[1]) * s;
        } else {
            if (mtx[0] > mtx[5] && mtx[0] > mtx[10]) {
                float s = 2.0f * sqrtf(1.0f + mtx[0] - mtx[5] - mtx[10]);
                _result[3] = (mtx[9] - mtx[6]) / s;
                _result[0] = 0.25f * s;
                _result[1] = (mtx[1] + mtx[4]) / s;
                _result[2] = (mtx[2] + mtx[8]) / s;
            } else if (mtx[5] > mtx[10]) {
                float s = 2.0f * sqrtf(1.0f + mtx[5] - mtx[0] - mtx[10]);
                _result[3] = (mtx[2] - mtx[8]) / s;
                _result[0] = (mtx[1] + mtx[4]) / s;
                _result[1] = 0.25f * s;
                _result[2] = (mtx[6] + mtx[9]) / s;
            } else {
                float s = 2.0f * sqrtf(1.0f + mtx[10] - mtx[0] - mtx[5]);
                _result[3] = (mtx[4] - mtx[1]) / s;
                _result[0] = (mtx[2] + mtx[8]) / s;
                _result[1] = (mtx[6] + mtx[9]) / s;
                _result[2] = 0.25f * s;
            }
        }
    }
}   // namespace bx


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

    inline vec4_t vec4f(float _x, float _y, float _z, float _w)
    {
        vec4_t v;
        v.x = _x;        v.y = _y;        v.z = _z;        v.w = _w;
        return v;
    }

    inline vec4_t vec4fv(const float* _f)
    {
        vec4_t v;
        v.x = _f[0];        v.y = _f[1];        v.z = _f[2];        v.w = _f[3];
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

    inline vec2_t vec2f(float _x, float _y)
    {
        vec2_t v;
        v.x = _x;
        v.y = _y;
        return v;
    }

    inline vec2_t vec2fv(const float* _f)
    {
        vec2_t v;
        v.x = _f[0];
        v.y = _f[1];
        return v;
    }

    union vec2int_t
    {
        struct
        {
            int x;
            int y;
        };

        int n[2];
    };

    inline vec2int_t vec2i(int _x, int _y)
    {
        vec2int_t v;
        v.x = _x;        v.y = _y;
        return v;
    }

    inline vec2int_t vec2iv(const int* _n)
    {
        vec2int_t v;
        v.x = _n[0];        v.y = _n[1];
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

    inline vec3_t vec3f(float _x, float _y, float _z)
    {
        vec3_t v;
        v.x = _x;        v.y = _y;        v.z = _z;
        return v;
    }

    inline vec3_t vec3fv(const float* _f)
    {
        vec3_t v;
        v.x = _f[0];        v.y = _f[1];        v.z = _f[2];
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

    inline quat_t quatf(float _x, float _y, float _z, float _w)
    {
        quat_t q;
        q.x = _x;        q.y = _y;        q.z = _z;        q.w = _w;
        return q;
    }

    inline quat_t quatfv(const float* _f)
    {
        quat_t q;
        q.x = _f[0];        q.y = _f[1];        q.z = _f[2];        q.w = _f[3];
        return q;
    }

    inline quat_t quatIdent()
    {
        quat_t q;
        q.x = q.y = q.z = 0;
        q.w = 1.0f;
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

    inline mtx4x4_t mtx4x4f3(float _m11, float _m12, float _m13,
                             float _m21, float _m22, float _m23,
                             float _m31, float _m32, float _m33,
                             float _m41, float _m42, float _m43)
    {
        mtx4x4_t m;
        m.m11 = _m11;     m.m12 = _m12;     m.m13 = _m13;     m.m14 = 0;
        m.m21 = _m21;     m.m22 = _m22;     m.m23 = _m23;     m.m24 = 0;
        m.m31 = _m31;     m.m32 = _m32;     m.m33 = _m33;     m.m34 = 0;
        m.m41 = _m41;     m.m42 = _m42;     m.m43 = _m43;     m.m44 = 1.0f;
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

    inline mtx4x4_t mtx4x4fv3(const float* _r0, const float* _r1, const float* _r2, const float* _r3)
    {
        mtx4x4_t m;
        m.m11 = _r0[0];     m.m12 = _r0[1];     m.m13 = _r0[2];     m.m14 = 0;
        m.m21 = _r1[0];     m.m22 = _r1[1];     m.m23 = _r1[2];     m.m24 = 0;
        m.m31 = _r2[0];     m.m32 = _r2[1];     m.m33 = _r2[2];     m.m34 = 0;
        m.m41 = _r3[0];     m.m42 = _r3[1];     m.m43 = _r3[2];     m.m44 = 1.0f;
        return m;
    }

    inline mtx4x4_t mtx4x4v(const vec4_t& _row0, const vec4_t& _row1, const vec4_t& _row2, const vec4_t& _row3)
    {
        mtx4x4_t m;
        m.vrow0 = _row0;        m.vrow1 = _row1;        m.vrow2 = _row2;        m.vrow3 = _row3;
        return m;
    }

    inline mtx4x4_t mtx4x4Ident()
    {
        mtx4x4_t m;
        m.m11 = 1.0f;     m.m12 = 0;        m.m13 = 0;        m.m14 = 0;
        m.m21 = 0;        m.m22 = 1.0f;     m.m23 = 0;        m.m24 = 0;
        m.m31 = 0;        m.m32 = 0;        m.m33 = 1.0f;     m.m34 = 0;
        m.m41 = 0;        m.m42 = 0;        m.m43 = 0;        m.m44 = 1.0f;
        return m;
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
        mtx3x3_t m;
        m.vrow0 = _row0;
        m.vrow1 = _row1;
        m.vrow2 = _row2;
        return m;
    }

    inline mtx3x3_t mtx3x3Ident()
    {
        mtx3x3_t m;
        m.m11 = 1.0f;     m.m12 = 0;        m.m13 = 0;
        m.m21 = 0;        m.m22 = 1.0f;     m.m23 = 0;
        m.m31 = 0;        m.m32 = 0;        m.m33 = 1.0f;
        return m;
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

    inline aabb_t aabb()
    {
        aabb_t a;
        a.xmin = a.ymin = a.zmin = FLT_MAX;
        a.xmax = a.ymax = a.zmax = -FLT_MAX;
        return a;
    }

    inline aabb_t aabbv(const vec3_t& _min, const vec3_t& _max)
    {
        aabb_t a;
        a.vmin = _min;
        a.vmax = _max;
        return a;
    }

    inline aabb_t aabbfv(const float* _min, const float* _max)
    {
        aabb_t a;
        a.fmin[0] = _min[0];  a.fmin[1] = _min[1];  a.fmin[2] = _min[2];
        a.fmax[0] = _max[0];  a.fmax[1] = _max[1];  a.fmax[2] = _max[2];
        return a;
    }

    inline aabb_t aabbf(float _xmin, float _ymin, float _zmin, float _xmax, float _ymax, float _zmax)
    {
        aabb_t a;
        a.xmin = _xmin;   a.ymin = _ymin;   a.zmin = _zmin;
        a.xmax = _xmax;   a.ymax = _ymax;   a.zmax = _zmax;
        return a;
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

    inline rect_t rectf(float _xmin, float _ymin, float _xmax, float _ymax)
    {
        rect_t r;
        r.xmin = _xmin;   r.ymin = _ymin;
        r.xmax = _xmax;   r.ymax = _ymax;
        return r;
    }

    inline rect_t rectfwh(float _x, float _y, float _width, float _height)
    {
        rect_t r;
        r.xmin = _x;                     r.ymin = _y;
        r.xmax = _x + _width;            r.ymax = _y + _height;
        return r;
    }

    inline rect_t rectfv(const float* _min, const float* _max)
    {
        rect_t r;
        r.xmin = _min[0];
        r.ymin = _min[1];
        r.xmax = _max[0];
        r.ymax = _max[1];
        return r;
    }

    inline rect_t rectv(const vec2_t& _vmin, const vec2_t& _vmax)
    {
        rect_t r;
        r.vmin = _vmin;
        r.vmax = _vmax;
        return r;
    }

    union rectint_t
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
            vec2int_t vmin;
            vec2int_t vmax;
        };

        int n[4];
    };

    inline rectint_t rectinti(int _xmin, int _ymin, int _xmax, int _ymax)
    {
        rectint_t r;
        r.xmin = _xmin;   r.ymin = _ymin;
        r.xmax = _xmax;   r.ymax = _ymax;
        return r;
    }

    inline rectint_t rectintiv(const int* _min, const int* _max)
    {
        rectint_t r;
        r.xmin = _min[0];
        r.ymin = _min[1];
        r.xmax = _max[0];
        r.ymax = _max[1];
        return r;
    }

    inline rectint_t rectintv(const vec2int_t& _vmin, const vec2int_t& _vmax)
    {
        rectint_t r;
        r.vmin = _vmin;
        r.vmax = _vmax;
        return r;
    }

    union sphere_t
    {
        struct
        {
            float x, y, z, r;
        };

        struct
        {
            vec3_t cp;
        };

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

    inline sphere_t spherev(const vec3_t& _cp, float _r)
    {
        sphere_t s;
        s.cp = _cp;        s.r = _r;
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

    inline plane_t planev(const vec3_t& _n, float _d)
    {
        plane_t p;
        p.n = _n;        p.d = _d;
        return p;
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

    inline uint32_t rgbaUint(float rgba[4])
    {
        uint8_t r = uint8_t(rgba[0]*255.0f);
        uint8_t g = uint8_t(rgba[1]*255.0f);
        uint8_t b = uint8_t(rgba[2]*255.0f);
        uint8_t a = uint8_t(rgba[3]*255.0f);
        return (uint32_t(r) << 24) | (uint32_t(g) << 16) | (uint32_t(b) << 8) | uint32_t(a);
    }

    typedef uint32_t color_t;

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

    // Color
    inline color_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
    {
        return (uint32_t(a) << 24) | (uint32_t(b) << 16) | (uint32_t(g) << 8) | uint32_t(r);
    }

    inline color_t rgbaf(float r, float g, float b, float a = 1.0f)
    {
        uint8_t _r = uint8_t(r * 255.0f);
        uint8_t _g = uint8_t(g * 255.0f);
        uint8_t _b = uint8_t(b * 255.0f);
        uint8_t _a = uint8_t(a * 255.0f);
        return (uint32_t(_a) << 24) | (uint32_t(_b) << 16) | (uint32_t(_g) << 8) | uint32_t(_r);
    }

    inline color_t premultiplyAlpha(color_t color, float alpha)
    {
        float _alpha = float((color >> 24) & 0xff);
        float premulAlpha = alpha * (_alpha / 255.0f);
        return ((uint32_t(premulAlpha * 255.0f) & 0xff) << 24) | (color & 0xffffff);
    }

    // Operators
    inline vec2_t operator+(const vec2_t& a, const vec2_t& b)
    {
        return vec2f(a.x + b.x, a.y + b.y);       
    }

    inline vec2_t operator-(const vec2_t& a, const vec2_t& b)
    {
        return vec2f(a.x - b.x, a.x - b.y);
    }

    inline vec2_t operator*(const vec2_t& v, float k)
    {
        return vec2f(v.x*k, v.y*k);
    }

    inline vec2_t operator*(float k, const vec2_t& v)
    {
        return vec2f(v.x*k, v.y*k);
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
} // namespace st
