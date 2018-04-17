#pragma once

#include "bx/bx.h"
#include "bx/allocator.h"
#include <assert.h>

#ifndef TEE_API
#   define TEE_API
#endif

namespace tee
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

    union vec2_t
    {
        struct
        {
            float x;
            float y;
        };

        float f[2];
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
    };

    union ucolor_t
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


    union ivec2_t
    {
        struct
        {
            int x;
            int y;
        };

        int n[2];
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
    };

    union mat3_t
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

    union mat4_t
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

    union irect_t
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
            ivec2_t vmin;
            ivec2_t vmax;
        };

        int n[4];
    };

    union sphere_t
    {
        struct
        {
            float x, y, z, r;
        };

        vec3_t center;
        float f[4];
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
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    vec4_t vec4(float x, float y, float z, float w);
    vec4_t vec4(const float* f);

    vec2_t vec2(float x, float y);
    vec2_t vec2(const float* f);

    vec3_t vec3(float x, float y, float z);
    vec3_t vec3(const float* f);

    ucolor_t ucolor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    ucolor_t ucolor(uint32_t n);
    ucolor_t ucolorSwizzle(uint32_t n);
    ucolor_t ucolorf(float r, float g, float b, float a = 1.0f);

    ivec2_t ivec2(const int* n);
    ivec2_t ivec2(int x, int y);

    quat_t quaternionI();
    quat_t quaternion(float x, float y, float z, float w);
    quat_t quaternion(const float* f);

    mat4_t mat4(float _m11, float _m12, float _m13, float _m14,
                float _m21, float _m22, float _m23, float _m24,
                float _m31, float _m32, float _m33, float _m34,
                float _m41, float _m42, float _m43, float _m44);
    mat4_t mat4(const float* _r0, const float* _r1, const float* _r2, const float* _r3);
    mat4_t mat4(const vec4_t& _row0, const vec4_t& _row1, const vec4_t& _row2, const vec4_t& _row3);

    mat3_t mat3(float _m11, float _m12, float _m13,
                float _m21, float _m22, float _m23,
                float _m31, float _m32, float _m33);
    mat3_t mat3(const float* _r0, const float* _r1, const float* _r2);
    mat3_t mat3(const vec3_t& _row0, const vec3_t& _row1, const vec3_t& _row2);

    aabb_t aabbZero();
    aabb_t aabb(const vec3_t& _min, const vec3_t& _max);
    aabb_t aabb(const float* _min, const float* _max);
    aabb_t aabb(float _xmin, float _ymin, float _zmin, float _xmax, float _ymax, float _zmax);

    rect_t rectZero();
    rect_t rect(float _xmin, float _ymin, float _xmax, float _ymax);
    rect_t rect(const float* _min, const float* _max);
    rect_t rect(const vec2_t& _vmin, const vec2_t& _vmax);
    rect_t rectwh(float _x, float _y, float _width, float _height);

    irect_t irect();
    irect_t irect(int _xmin, int _ymin, int _xmax, int _ymax);
    irect_t irect(const int* _min, const int* _max);
    irect_t irect(const ivec2_t& _vmin, const ivec2_t& _vmax);

    sphere_t sphere(const float* _f);
    sphere_t sphere(float _x, float _y, float _z, float _r);
    sphere_t sphere(const vec3_t& _cp, float _r);

    plane_t plane(const float* _f);
    plane_t plane(float _nx, float _ny, float _nz, float _d);
    plane_t plane(const vec3_t& _n, float _d);

    mat4_t mat4(float _m11, float _m12, float _m13,
                float _m21, float _m22, float _m23,
                float _m31, float _m32, float _m33,
                float _m41, float _m42, float _m43);
    mat4_t mat4f3(const float* _r0, const float* _r1, const float* _r2, const float* _r3);
    mat4_t mat4(const mat3_t& m);

    mat4_t mat4I();
    mat3_t mat3I();
    mat3_t mat3(const mat4_t& m);

    // Operators
    inline vec2_t operator+(const vec2_t& a, const vec2_t& b);
    inline vec2_t operator-(const vec2_t& a, const vec2_t& b);
    inline vec2_t operator*(const vec2_t& v, float k);
    inline vec2_t operator*(float k, const vec2_t& v);
    inline vec2_t operator*(const vec2_t& v0, const vec2_t& v1);
    inline vec3_t operator+(const vec3_t& v1, const vec3_t& v2);
    inline vec3_t operator-(const vec3_t& v1, const vec3_t& v2);
    inline vec3_t operator*(const vec3_t& v, float k);
    inline vec3_t operator*(float k, const vec3_t& v);
    inline mat4_t operator*(const mat4_t& a, const mat4_t& b);
    inline mat3_t operator*(const mat3_t& a, const mat3_t& b);
    inline quat_t operator*(const quat_t& a, const quat_t& b);

    template <typename Ty>
    struct Matrix
    {
        int width;
        int height;
        Ty* mtx;
        bx::AllocatorI* alloc;

        explicit Matrix(bx::AllocatorI* _alloc)
        {
            mtx = nullptr;
            width = height = 0;
            alloc = _alloc;
        }

        ~Matrix()
        {
            assert(mtx == nullptr);
        }

        bool create(int _width, int _height)
        {
            assert(_width > 0);
            assert(_height > 0);
            assert(this->mtx == nullptr);

            this->mtx = (Ty*)BX_ALLOC(alloc, _width*_height * sizeof(Ty));
            if (!this->mtx)
                return false;

            this->width = _width;
            this->height = _height;
            bx::memSet(this->mtx, 0x00, sizeof(Ty)*width*height);

            return true;
        }

        void destroy()
        {
            if (this->mtx) {
                BX_FREE(this->alloc, this->mtx);
                this->mtx = nullptr;
            }
        }

        void set(int x, int y, const Ty& f)
        {
            assert(x < width);
            assert(y < height);
            mtx[x + width*y] = f;
        }

        const Ty& get(int x, int y) const
        {
            return mtx[x + width*y];
        }
    };
    typedef Matrix<float> FloatMatrix;

    namespace tmath {
        TEE_API bool whiteNoise(FloatMatrix* whiteNoise, int width, int height);
        TEE_API bool smoothNoise(FloatMatrix* smoothNoise, const FloatMatrix* baseNoise, int octave);
        TEE_API bool perlinNoise(FloatMatrix* perlinNoise, const FloatMatrix* baseNoise, int octaveCount,
                                 float persistance, bx::AllocatorI* alloc);
        TEE_API float normalDist(float x, float mean, float stdDev);
        TEE_API aabb_t aabbTransform(const aabb_t& b, const mat4_t& mtx);
        TEE_API bool projectToScreen(vec2_t* result, const vec3_t point, const irect_t& viewport, const mat4_t& viewProjMtx);

        // Reference: http://stackoverflow.com/questions/707370/clean-efficient-algorithm-for-wrapping-integers-in-c
        int iwrap(int kX, int const kLowerBound, int const kUpperBound);
        float fwrap(float x, float vmin, float vmax);
        int iclamp(int n, const int _min, const int _max);
        float falign(float value, float size);
        // Line fgain, but goes up to 1.0 and then back to 0
        float fwave(float _time, float _gain);
        // Like fwave, but with saw-tooth like shape (drops fast after 0.5)
        float fwaveSharp(float _time, float _gain);
        vec2_t bezierCubic(const vec2_t pts[4], float t);
        vec2_t bezierQuadric(const vec2_t pts[3], float t);

        bool rectTestPoint(const rect_t& rc, const vec2_t& pt);
        bool rectTestCircle(const rect_t& rc, const vec2_t& center, float radius);
        bool rectTestRect(const rect_t& rc1, const rect_t& rc2);
        void rectPushPoint(rect_t* rc, const vec2_t& pt);
        void aabbPushPoint(aabb_t* rb, const vec3_t& pt);

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
        vec3_t aabbGetCorner(const aabb_t& box, int index);

        void mtxProjPlane(mat4_t* r, const vec3_t planeNorm);

        ucolor_t colorPremultiplyAlpha(ucolor_t color, float alpha);

        vec4_t ucolorToVec4(ucolor_t c);

        vec4_t fcolorToLinear(const vec4_t& c);
    }

} // namespace tee

#include "inline/tmath.inl"
