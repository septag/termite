#pragma once

#include "bx/bx.h"
#include "bx/fpumath.h"

#if !BX_COMPILER_MSVC
#  include <float.h>
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


} // namespace st
