#pragma once

#include "bx/bx.h"
#include "bx/fpumath.h"

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

        explicit vec4_t(float _x, float _y, float _z, float _w)
        {
            x = _x;
            y = _y;
            z = _z;
            w = _w;
        }

        explicit vec4_t(const float* _f)
        {
            x = _f[0];
            y = _f[1];
            z = _f[2];
            w = _f[3];
        }
        
        vec4_t()
        {
        }
    };

    union vec2_t
    {
        struct
        {
            float x;
            float y;
        };

        float f[2];

        vec2_t()
        {
        }

        explicit vec2_t(float _x, float _y)
        {
            x = _x;
            y = _y;
        }

        explicit vec2_t(const float* _f)
        {
            x = _f[0];
            y = _f[1];
        }
    };

    union vec2int_t
    {
        struct
        {
            int x;
            int y;
        };

        int n[2];

        vec2int_t()
        {
        }

        explicit vec2int_t(int _x, int _y)
        {
            x = _x;
            y = _y;
        }

        explicit vec2int_t(const int* _n)
        {
            x = _n[0];
            y = _n[1];
        }
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

        vec3_t()
        {
        }

        explicit vec3_t(float _x, float _y, float _z)
        {
            x = _x;
            y = _y;
            z = _z;
        }

        explicit vec3_t(const float* _f)
        {
            x = _f[0];
            y = _f[1];
            z = _f[2];
        }
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

        quat_t()
        {
        }

        explicit quat_t(float _x, float _y, float _z, float _w)
        {
            x = _x;
            y = _y;
            z = _z;
            w = _w;
        }

        explicit quat_t(const float* _f)
        {
            x = _f[0];
            y = _f[1];
            z = _f[2];
            w = _f[3];
        }

        void setIdentity()
        {
            x = y = z = 0;
            w = 1.0f;
        }
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

        mtx4x4_t()
        {
        }

        mtx4x4_t(float _m11, float _m12, float _m13, float _m14,
                 float _m21, float _m22, float _m23, float _m24,
                 float _m31, float _m32, float _m33, float _m34,
                 float _m41, float _m42, float _m43, float _m44)
        {
            m11 = _m11;     m12 = _m12;     m13 = _m13;     m14 = _m14;
            m21 = _m21;     m22 = _m22;     m23 = _m23;     m24 = _m24;
            m31 = _m31;     m32 = _m32;     m33 = _m33;     m34 = _m34;
            m41 = _m41;     m42 = _m42;     m43 = _m43;     m44 = _m44;
        }

        mtx4x4_t(float _m11, float _m12, float _m13, 
                 float _m21, float _m22, float _m23, 
                 float _m31, float _m32, float _m33, 
                 float _m41, float _m42, float _m43)
        {
            m11 = _m11;     m12 = _m12;     m13 = _m13;     m14 = 0;
            m21 = _m21;     m22 = _m22;     m23 = _m23;     m24 = 0;
            m31 = _m31;     m32 = _m32;     m33 = _m33;     m34 = 0;
            m41 = _m41;     m42 = _m42;     m43 = _m43;     m44 = 1.0f;
        }

        mtx4x4_t(const float* _r0, const float* _r1, const float* _r2, const float* _r3)
        {
            m11 = _r0[0];     m12 = _r0[1];     m13 = _r0[2];     m14 = _r0[3];
            m21 = _r1[0];     m22 = _r1[1];     m23 = _r1[2];     m24 = _r1[3];
            m31 = _r2[0];     m32 = _r2[1];     m33 = _r2[2];     m34 = _r2[3];
            m41 = _r3[0];     m42 = _r3[1];     m43 = _r3[2];     m44 = _r3[3];
        }

        mtx4x4_t(const vec4_t& _row0, const vec4_t& _row1, const vec4_t& _row2, const vec4_t& _row3)
        {
            vrow0 = _row0;
            vrow1 = _row1;
            vrow2 = _row2;
            vrow3 = _row3;
        }

        void setIdentity()
        {
            m11 = 1.0f;     m12 = 0;        m13 = 0;        m14 = 0;
            m21 = 0;        m22 = 1.0f;     m23 = 0;        m24 = 0;
            m31 = 0;        m32 = 0;        m33 = 1.0f;     m34 = 0;
            m41 = 0;        m42 = 0;        m43 = 0;        m44 = 1.0f;
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

        mtx3x3_t()
        {
        }

        explicit mtx3x3_t(float _m11, float _m12, float _m13,
                          float _m21, float _m22, float _m23,
                          float _m31, float _m32, float _m33)
        {
            m11 = _m11;     m12 = _m12;     m13 = _m13;   
            m21 = _m21;     m22 = _m22;     m23 = _m23;   
            m31 = _m31;     m32 = _m32;     m33 = _m33;   
        }

        explicit mtx3x3_t(const float* _r0, const float* _r1, const float* _r2)
        {
            m11 = _r0[0];     m12 = _r0[1];     m13 = _r0[2];
            m21 = _r1[0];     m22 = _r1[1];     m23 = _r1[2];
            m31 = _r2[0];     m32 = _r2[1];     m33 = _r2[2];
        }

        explicit mtx3x3_t(const vec3_t& _row0, const vec3_t& _row1, const vec3_t& _row2)
        {
            vrow0 = _row0;
            vrow1 = _row1;
            vrow2 = _row2;
        }

        void setIdentity()
        {
            m11 = 1.0f;     m12 = 0;        m13 = 0;   
            m21 = 0;        m22 = 1.0f;     m23 = 0;   
            m31 = 0;        m32 = 0;        m33 = 1.0f;
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

        aabb_t()
        {
            xmin = ymin = zmin = FLT_MAX;
            xmax = ymax = zmax = -FLT_MAX;
        }

        explicit aabb_t(const vec3_t& _min, const vec3_t& _max)
        {
            vmin = _min;
            vmax = _max;
        }

        explicit aabb_t(const float* _min, const float* _max)
        {
            fmin[0] = _min[0];  fmin[1] = _min[1];  fmin[2] = _min[2];
            fmax[0] = _max[0];  fmax[1] = _max[1];  fmax[2] = _max[2];
        }

        explicit aabb_t(float _xmin, float _ymin, float _zmin, float _xmax, float _ymax, float _zmax)
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

        rect_t()
        {
        }

        explicit rect_t(float _xmin, float _ymin, float _xmax, float _ymax)
        {
            xmin = _xmin;   ymin = _ymin;
            xmax = _xmax;   ymax = _ymax;
        }

        explicit rect_t(const float* _min, const float* _max)
        {
            xmin = _min[0];
            ymin = _min[1];
            xmax = _max[0];
            ymax = _max[1];
        }

        explicit rect_t(const vec2_t& _vmin, const vec2_t& _vmax)
        {
            vmin = _vmin;
            vmax = _vmax;
        }
    };

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

        rectint_t()
        {
        }

        explicit rectint_t(int _xmin, int _ymin, int _xmax, int _ymax)
        {
            xmin = _xmin;   ymin = _ymin;
            xmax = _xmax;   ymax = _ymax;
        }

        explicit rectint_t(const int* _min, const int* _max)
        {
            xmin = _min[0];
            ymin = _min[1];
            xmax = _max[0];
            ymax = _max[1];
        }

        explicit rectint_t(const vec2int_t& _vmin, const vec2int_t& _vmax)
        {
            vmin = _vmin;
            vmax = _vmax;
        }
    };

    union sphere_t
    {
        struct
        {
            float x, y, z;
            float r;
        };

        struct
        {
            vec3_t cp;
            float r;
        };

        float f[4];

        sphere_t()
        {
        }

        explicit sphere_t(const float* _f)
        {
            x = _f[0];  y = _f[1];  z = _f[2];
            r = _f[3];
        }

        explicit sphere_t(float _x, float _y, float _z, float _r)
        {
            x = _x;     y = _y;     z = _z;     r = _r;
        }

        explicit sphere_t(const vec3_t& _cp, float _r)
        {
            cp = _cp;
            r = _r;
        }
    };

    union plane_t
    {
        struct
        {
            float nx, ny, nz;
            float d;
        };

        struct
        {
            vec3_t n;
            float d;
        };

        float f[4];

        plane_t()
        {
        }

        explicit plane_t(const float* _f)
        {
            nx = _f[0];     ny = _f[1];     nz = _f[2];
            d = _f[3];
        }

        explicit plane_t(float _nx, float _ny, float _nz, float _d)
        {
            nx = _nx;       ny = _ny;       nz = _nz;       d = _d;
        }

        explicit plane_t(const vec3_t& _n, float _d)
        {
            n = _n;
            d = _d;
        }
    };

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


} // namespace st
