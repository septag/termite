#pragma once

#ifndef BXX_MATH_H_HEADER_GUARD
#define BXX_MATH_H_HEADER_GUARD

#include "bx/math.h"

namespace bx
{
    void mat3Translate(float* result, float x, float y);

    void mat3Rotate(float* result, float theta);

    void mat3Scale(float* result, float sx, float sy);

    void vec3MulMtxXYZ(float* __restrict _result, const float* __restrict _vec, const float* __restrict _mat);

    void mat3Compose(float* result, float x, float y, float angle);

    void mat3Decompose(const float* mat, float* translation, float* rotation);

    void vec2MulMat3(float* __restrict _result, const float* __restrict _vec, const float* __restrict _mat);

    void vec3MulMat3(float* __restrict _result, const float* __restrict _vec, const float* __restrict _mat);

    void mat3Mul(float* __restrict _result, const float* __restrict _a, const float* __restrict _b);

    void quatFromMtx(float* _result, const float* mtx);

    float vec2Dot(const float* __restrict _a, const float* __restrict _b);

    float vec2Length(const float* _a);

    float vec2Norm(float* __restrict _result, const float* __restrict _a);

    void vec2Min(float* __restrict _result, const float* __restrict _a, const float* __restrict _b);

    void vec2Max(float* __restrict _result, const float* __restrict _a, const float* __restrict _b);

    void vec2Lerp(float* __restrict _result, const float* __restrict _a, const float* __restrict _b, float _t);

    // Math Primitives
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

        static const vec4_t Zero;
        static const vec4_t Up;
        static const vec4_t Right;
        static const vec4_t Forward;

        vec4_t() {}
        explicit vec4_t(float _x, float _y, float _z, float _w);
        explicit vec4_t(const float* _f);
    };

    union vec2_t
    {
        struct
        {
            float x;
            float y;
        };

        float f[2];

        static const vec2_t Zero;
        static const vec2_t Right;
        static const vec2_t Up;

        vec2_t() {}
        explicit vec2_t(float _x, float _y);
        explicit vec2_t(const float* _f);
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

        static const vec3_t Zero;
        static const vec3_t Right;
        static const vec3_t Up;
        static const vec3_t Forward;

        vec3_t() {}
        explicit vec3_t(float _x, float _y, float _z);
        explicit vec3_t(const float* _f);
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

        static const ucolor_t White;
        static const ucolor_t Black;
        static const ucolor_t Red;
        static const ucolor_t Green;
        static const ucolor_t Blue;
        static const ucolor_t Yellow;
        static const ucolor_t Cyan;
        static const ucolor_t Magenta;

        ucolor_t() {}
        explicit ucolor_t(uint8_t _r, uint8_t _g, uint8_t _b, uint8_t _a = 255);
        explicit ucolor_t(uint32_t _n);
        explicit ucolor_t(float _r, float _g, float _b, float _a = 1.0f);
    };


    union ivec2_t
    {
        struct
        {
            int x;
            int y;
        };

        int n[2];

        static const ivec2_t Zero;
        static const ivec2_t Up;
        static const ivec2_t Right;

        ivec2_t() {}
        explicit ivec2_t(int _x, int _y);
        explicit ivec2_t(const int* _n);
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

        static const quat_t Ident;

        quat_t() {};
        explicit quat_t(float _x, float _y, float _z, float _w);
        explicit quat_t(const float* _f);
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
            float r1[3];
            float r2[3];
            float r3[3];
        };

        struct
        {
            vec3_t vrow1;
            vec3_t vrow2;
            vec3_t vrow3;
        };

        float f[9];

        static const mat3_t Zero;
        static const mat3_t Ident;

        mat3_t() {}
        explicit mat3_t(float _m11, float _m12, float _m13,
                        float _m21, float _m22, float _m23,
                        float _m31, float _m32, float _m33);
        explicit mat3_t(const float* _r1, const float* _r2, const float* _r3);
        explicit mat3_t(const vec3_t& _row1, const vec3_t& _row2, const vec3_t& _row3);
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
            vec4_t vrow1;
            vec4_t vrow2;
            vec4_t vrow3;
            vec4_t vrow4;
        };

        float f[16];

        static const mat4_t Zero;
        static const mat4_t Ident;

        mat4_t() {}
        explicit mat4_t(float _m11, float _m12, float _m13, float _m14,
                        float _m21, float _m22, float _m23, float _m24,
                        float _m31, float _m32, float _m33, float _m34,
                        float _m41, float _m42, float _m43, float _m44);
        explicit mat4_t(const float* _r1, const float* _r2, const float* _r3, const float* _r4);
        explicit mat4_t(const vec4_t& _row1, const vec4_t& _row2, const vec4_t& _row3, const vec4_t& _row4);
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

        static const aabb_t Null;

        aabb_t() {}
        explicit aabb_t(const vec3_t& _min, const vec3_t& _max);
        explicit aabb_t(const float* _min, const float* _max);
        explicit aabb_t(float _xmin, float _ymin, float _zmin, float _xmax, float _ymax, float _zmax);

        void addPoint(const vec3_t& pt);
        vec3_t getCorner(int index) const;
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

        static const rect_t Null;

        void addPoint(const vec2_t& pt);
        bool testPoint(const vec2_t& pt) const;
        bool testCircle(const vec2_t& center, float radius) const;
        float getWidth() const;
        float getHeight() const;
        vec2_t getSize() const;
        vec2_t getCenter() const;

        rect_t() {}
        explicit rect_t(float _xmin, float _ymin, float _xmax, float _ymax);
        explicit rect_t(const float* _min, const float* _max);
        explicit rect_t(const vec2_t& _vmin, const vec2_t& _vmax);
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

        static const irect_t Null;

        int getWidth() const;
        int getHeight() const;
        ivec2_t getSize() const;
        ivec2_t getCenter() const;

        irect_t() {}
        explicit irect_t(const ivec2_t& _vmin, const ivec2_t& _vmax);
        explicit irect_t(int _x, int _y, int _width, int _height);
        explicit irect_t(const int* _min, const int* _max);
    };

    union sphere_t
    {
        struct
        {
            float x, y, z, r;
        };

        vec3_t center;
        float f[4];

        static const sphere_t Zero;

        sphere_t() {}
        explicit sphere_t(float _x, float _y, float _z, float _r);
        explicit sphere_t(const vec3_t& _cp, float _r);
        explicit sphere_t(const float* _f);
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

        static const plane_t Up;
        static const plane_t Forward;
        static const plane_t Right;

        plane_t() {}
        explicit plane_t(const float* _f);
        explicit plane_t(float _nx, float _ny, float _nz, float _d);
        explicit plane_t(const vec3_t& _n, float _d);
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    BX_CONST_FUNC vec4_t vec4_splat(float n);
    BX_CONST_FUNC vec2_t vec2_splat(float n);
    BX_CONST_FUNC vec3_t vec3_splat(float n);
    BX_CONST_FUNC ucolor_t ucolor_inv(uint32_t n);
    BX_CONST_FUNC rect_t rectwh(float _x, float _y, float _width, float _height);
    BX_CONST_FUNC irect_t irectwh(int _x, int _y, int _width, int _height);
    BX_CONST_FUNC mat4_t mat4_splat(float _m11, float _m12, float _m13,
                                    float _m21, float _m22, float _m23,
                                    float _m31, float _m32, float _m33,
                                    float _m41, float _m42, float _m43);
    BX_CONST_FUNC mat4_t mat4_splat(const float* _r1, const float* _r2, const float* _r3, const float* _r4);
    BX_CONST_FUNC mat4_t mat4_splat(const mat3_t& m);

    // Operators
    BX_CONST_FUNC vec2_t operator+(const vec2_t& a, const vec2_t& b);
    BX_CONST_FUNC vec2_t operator-(const vec2_t& a, const vec2_t& b);
    BX_CONST_FUNC vec2_t operator*(const vec2_t& v, float k);
    BX_CONST_FUNC ivec2_t operator+(const ivec2_t& a, const ivec2_t& b);
    BX_CONST_FUNC ivec2_t operator-(const ivec2_t& a, const ivec2_t& b);
    BX_CONST_FUNC ivec2_t operator*(const ivec2_t& v, int k);
    BX_CONST_FUNC ivec2_t operator/(const ivec2_t& v, int k);
    BX_CONST_FUNC vec2_t operator*(float k, const vec2_t& v);
    BX_CONST_FUNC vec2_t operator*(const vec2_t& v0, const vec2_t& v1);
    BX_CONST_FUNC vec3_t operator+(const vec3_t& v1, const vec3_t& v2);
    BX_CONST_FUNC vec3_t operator-(const vec3_t& v1, const vec3_t& v2);
    BX_CONST_FUNC vec3_t operator*(const vec3_t& v, float k);
    BX_CONST_FUNC vec3_t operator*(float k, const vec3_t& v);
    BX_CONST_FUNC mat4_t operator*(const mat4_t& a, const mat4_t& b);
    BX_CONST_FUNC mat3_t operator*(const mat3_t& a, const mat3_t& b);
    BX_CONST_FUNC quat_t operator*(const quat_t& a, const quat_t& b);

}   // namespace bx

#include "inline/math.inl"

#endif