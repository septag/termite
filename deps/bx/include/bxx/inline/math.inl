#ifndef BXX_MATH_H_HEADER_GUARD
#   error "This inline file must only be included from math.h"
#endif

#include "bx/debug.h"

namespace bx {
    inline void mat3Translate(float* result, float x, float y)
    {
        bx::memSet(result, 0x00, sizeof(float) * 9);
        result[0] = 1.0f;
        result[4] = 1.0f;
        result[6] = x;
        result[7] = y;
        result[8] = 1.0f;
    }


    inline void mat3Rotate(float* result, float theta)
    {
        bx::memSet(result, 0x00, sizeof(float) * 9);
        float c = cos(theta);
        float s = sin(theta);
        result[0] = c;     result[1] = -s;
        result[3] = s;     result[4] = c;
        result[8] = 1.0f;
    }

    inline void mat3Scale(float* result, float sx, float sy)
    {
        bx::memSet(result, 0x00, sizeof(float) * 9);
        result[0] = sx;
        result[4] = sy;
        result[8] = 1.0f;
    }

    inline void vec3MulMtxXYZ(float* __restrict _result, const float* __restrict _vec, const float* __restrict _mat)
    {
        _result[0] = _vec[0] * _mat[0] + _vec[1] * _mat[4] + _vec[2] * _mat[8];
        _result[1] = _vec[0] * _mat[1] + _vec[1] * _mat[5] + _vec[2] * _mat[9];
        _result[2] = _vec[0] * _mat[2] + _vec[1] * _mat[6] + _vec[2] * _mat[10];
    }

    inline void mat3Compose(float* result, float x, float y, float angle)
    {
        bx::memSet(result, 0x00, sizeof(float) * 9);
        float c = bx::cos(angle);
        float s = bx::sin(angle);
        result[0] = c;     result[1] = -s;
        result[3] = s;     result[4] = c;
        result[6] = x;     result[7] = y;
        result[8] = 1.0f;
    }

    inline void mat3Decompose(const float* mat, float* translation, float* rotation)
    {
        translation[0] = mat[6];
        translation[1] = mat[7];

        *rotation = bx::atan2(mat[3], mat[4]);
    }

    inline void vec2MulMat3(float* __restrict _result, const float* __restrict _vec, const float* __restrict _mat)
    {
        _result[0] = _vec[0] * _mat[0] + _vec[1] * _mat[3] + _mat[6];
        _result[1] = _vec[0] * _mat[1] + _vec[1] * _mat[4] + _mat[7];
    }

    inline void vec3MulMat3(float* __restrict _result, const float* __restrict _vec, const float* __restrict _mat)
    {
        _result[0] = _vec[0] * _mat[0] + _vec[1] * _mat[3] + _vec[2] * _mat[6];
        _result[1] = _vec[0] * _mat[1] + _vec[1] * _mat[4] + _vec[2] * _mat[7];
        _result[2] = _vec[0] * _mat[2] + _vec[1] * _mat[5] + _vec[2] * _mat[8];
    }

    inline float vec2Dot(const float* __restrict _a, const float* __restrict _b)
    {
        return _a[0]*_b[0] + _a[1]*_b[1];
    }

    inline float vec2Length(const float* _a)
    {
        return bx::sqrt(vec2Dot(_a, _a));
    }

    inline float vec2Norm(float* __restrict _result, const float* __restrict _a)
    {
        const float len = vec2Length(_a);
        if (len >= kNearZero) {
            const float invLen = 1.0f/len;
            _result[0] = _a[0] * invLen;
            _result[1] = _a[1] * invLen;
        }
        return len;
    }

    inline void vec2Min(float* __restrict _result, const float* __restrict _a, const float* __restrict _b)
    {
        _result[0] = bx::min(_a[0], _b[0]);
        _result[1] = bx::min(_a[1], _b[1]);
    }

    inline void vec2Max(float* __restrict _result, const float* __restrict _a, const float* __restrict _b)
    {
        _result[0] = bx::max(_a[0], _b[0]);
        _result[1] = bx::max(_a[1], _b[1]);
    }

    inline void vec2Lerp(float* __restrict _result, const float* __restrict _a, const float* __restrict _b, float _t)
    {
        _result[0] = bx::lerp(_a[0], _b[0], _t);
        _result[1] = bx::lerp(_a[1], _b[1], _t);
    }

    inline vec4_t::vec4_t(float _x, float _y, float _z, float _w)
    {
        x = _x;    y = _y;    z = _z;    w = _w;
    }

    inline vec4_t::vec4_t(const float* _f)
    {
        x = _f[0];     y = _f[1];     z = _f[2];     w = _f[3];
    }

    BX_CONST_FUNC inline vec4_t vec4_splat(float _n)
    {
        vec4_t v;
        v.x = v.y = v.z = v.w = _n;
        return v;
    }

    inline vec2_t::vec2_t(float _x, float _y)
    {
        x = _x;        y = _y;
    }

    inline vec2_t::vec2_t(const float* _f)
    {
        x = _f[0];     y = _f[1];
    }

    BX_CONST_FUNC inline vec2_t vec2_splat(float _n)
    {
        vec2_t v;
        v.x = v.y = _n;
        return v;
    }

    inline vec3_t::vec3_t(float _x, float _y, float _z)
    {
        x = _x;        y = _y;        z = _z;
    }

    inline vec3_t::vec3_t(const float* _f)
    {
        x = _f[0];     y = _f[1];     z = _f[3];
    }

    BX_CONST_FUNC inline vec3_t vec3_splat(float _n)
    {
        vec3_t v;
        v.x = v.y = v.z = _n;
        return v;
    }

    inline ucolor_t::ucolor_t(uint8_t _r, uint8_t _g, uint8_t _b, uint8_t _a) :
        r(_r), g(_g), b(_b), a(_a)
    {
    }

    inline ucolor_t::ucolor_t(uint32_t _n) : n(_n)
    {
    }

    inline ucolor_t::ucolor_t(float _r, float _g, float _b, float _a) :
        ucolor_t(uint8_t(_r * 255.0f), uint8_t(_g * 255.0f), uint8_t(_b * 255.0f), uint8_t(_a * 255.0f))
    {
    }

    BX_CONST_FUNC inline ucolor_t ucolor_inv(uint32_t _n)
    {
        ucolor_t c(_n);
        bx::xchg<uint8_t>(c.r, c.a);
        bx::xchg<uint8_t>(c.g, c.b);
        return c;
    }

    inline ivec2_t::ivec2_t(const int* _n)
    {
        x = _n[0];     y = _n[1];
    }

    inline ivec2_t::ivec2_t(int _x, int _y)
    {
        x = _x;        y = _y;
    }

    // Quaternion
    inline quat_t::quat_t(float _x, float _y, float _z, float _w)
    {
        x = _x;        y = _y;        z = _z;        w = _w;
    }

    inline quat_t::quat_t(const float* _f)
    {
        x = _f[0];     y = _f[1];     z = _f[2];     w = _f[3];
    }

    inline mat4_t::mat4_t(float _m11, float _m12, float _m13, float _m14,
                          float _m21, float _m22, float _m23, float _m24,
                          float _m31, float _m32, float _m33, float _m34,
                          float _m41, float _m42, float _m43, float _m44)
    {
        m11 = _m11;     m12 = _m12;     m13 = _m13;     m14 = _m14;
        m21 = _m21;     m22 = _m22;     m23 = _m23;     m24 = _m24;
        m31 = _m31;     m32 = _m32;     m33 = _m33;     m34 = _m34;
        m41 = _m41;     m42 = _m42;     m43 = _m43;     m44 = _m44;
    }

    inline mat4_t::mat4_t(const float* _r1, const float* _r2, const float* _r3, const float* _r4)
    {
        m11 = _r1[0];     m12 = _r1[1];     m13 = _r1[2];     m14 = _r1[3];
        m21 = _r2[0];     m22 = _r2[1];     m23 = _r2[2];     m24 = _r2[3];
        m31 = _r3[0];     m32 = _r3[1];     m33 = _r3[2];     m34 = _r3[3];
        m41 = _r4[0];     m42 = _r4[1];     m43 = _r4[2];     m44 = _r4[3];
    }

    inline mat4_t::mat4_t(const vec4_t& _row1, const vec4_t& _row2, const vec4_t& _row3, const vec4_t& _row4) :
        mat4_t(_row1.f, _row2.f, _row3.f, _row4.f)
    {        
    }

    inline mat3_t::mat3_t(float _m11, float _m12, float _m13,
                          float _m21, float _m22, float _m23,
                          float _m31, float _m32, float _m33)
    {
        m11 = _m11;     m12 = _m12;     m13 = _m13;
        m21 = _m21;     m22 = _m22;     m23 = _m23;
        m31 = _m31;     m32 = _m32;     m33 = _m33;
    }

    inline mat3_t::mat3_t(const float* _r1, const float* _r2, const float* _r3)
    {
        m11 = _r1[0];     m12 = _r1[1];     m13 = _r1[2];
        m21 = _r2[0];     m22 = _r2[1];     m23 = _r2[2];
        m31 = _r3[0];     m32 = _r3[1];     m33 = _r3[2];
    }

    inline mat3_t::mat3_t(const vec3_t& _row1, const vec3_t& _row2, const vec3_t& _row3) :
        mat3_t(_row1.f, _row2.f, _row3.f)
    {        
    }

    inline aabb_t::aabb_t(const vec3_t& _min, const vec3_t& _max)
    {
        vmin = _min;
        vmax = _max;
    }

    inline aabb_t::aabb_t(const float* _min, const float* _max)
    {
        fmin[0] = _min[0];  fmin[1] = _min[1];  fmin[2] = _min[2];
        fmax[0] = _max[0];  fmax[1] = _max[1];  fmax[2] = _max[2];
    }

    inline aabb_t::aabb_t(float _xmin, float _ymin, float _zmin, float _xmax, float _ymax, float _zmax)
    {
        xmin = _xmin;   ymin = _ymin;   zmin = _zmin;
        xmax = _xmax;   ymax = _ymax;  zmax = _zmax;
    }

    inline rect_t::rect_t(float _xmin, float _ymin, float _xmax, float _ymax)
    {
        xmin = _xmin;   ymin = _ymin;
        xmax = _xmax;   ymax = _ymax;
    }

    inline rect_t::rect_t(const float* _min, const float* _max)
    {
        xmin = _min[0];
        ymin = _min[1];
        xmax = _max[0];
        ymax = _max[1];
    }

    inline rect_t::rect_t(const vec2_t& _vmin, const vec2_t& _vmax)
    {
        vmin = _vmin;
        vmax = _vmax;
    }

    BX_CONST_FUNC inline rect_t rectwh(float _x, float _y, float _width, float _height)
    {
        return rect_t(_x, _y, _x + _width, _y + _height);
    }

    inline irect_t::irect_t(int _xmin, int _ymin, int _xmax, int _ymax)
    {
        xmin = _xmin;   ymin = _ymin;
        xmax = _xmax;   ymax = _ymax;
    }

    inline irect_t::irect_t(const int* _min, const int* _max)
    {
        xmin = _min[0];
        ymin = _min[1];
        xmax = _max[0];
        ymax = _max[1];
    }

    inline irect_t::irect_t(const ivec2_t& _vmin, const ivec2_t& _vmax)
    {
        vmin = _vmin;
        vmax = _vmax;
    }

    BX_CONST_FUNC inline irect_t irectwh(int _x, int _y, int _width, int _height)
    {
        return irect_t(_x, _y, _x + _width, _y + _height);
    }

    inline sphere_t::sphere_t(const float* _f)
    {
        x = _f[0];  y = _f[1];  z = _f[2];
        r = _f[3];
    }

    inline sphere_t::sphere_t(float _x, float _y, float _z, float _r)
    {
        x = _x;     y = _y;     z = _z;     r = _r;
    }

    inline sphere_t::sphere_t(const vec3_t& _cp, float _r)
    {
        center = _cp;        r = _r;
    }

    inline plane_t::plane_t(const float* _f)
    {
        nx = _f[0];     ny = _f[1];     nz = _f[2];
        d = _f[3];
    }

    inline plane_t::plane_t(float _nx, float _ny, float _nz, float _d)
    {
        nx = _nx;       ny = _ny;       nz = _nz;       d = _d;
    }

    inline plane_t::plane_t(const vec3_t& _n, float _d)
    {
        n = _n;        d = _d;
    }

    // mtx4x4
    BX_CONST_FUNC inline mat4_t mat4_splat(float _m11, float _m12, float _m13,
                                           float _m21, float _m22, float _m23,
                                           float _m31, float _m32, float _m33,
                                           float _m41, float _m42, float _m43)
    {
        return mat4_t(_m11, _m12, _m13, 0,
                      _m21, _m22, _m23, 0,
                      _m31, _m32, _m33, 0,
                      _m41, _m42, _m43, 1.0f);
    }

    BX_CONST_FUNC inline mat4_t mat4_splat(const float* _r1, const float* _r2, const float* _r3, const float* _r4)
    {
        return mat4_t(_r1[0], _r1[1], _r1[2], 0,
                      _r2[0], _r2[1], _r2[2], 0,
                      _r3[0], _r3[1], _r3[2], 0,
                      _r4[0], _r4[1], _r4[2], 1.0f);
    }

    BX_CONST_FUNC inline mat4_t mat4_splat(const mat3_t& m)
    {
        return mat4_splat(m.m11, m.m12, m.m13,
                          m.m21, m.m22, m.m23,
                          0, 0, 1.0,
                          m.m31, m.m32, m.m33);
    }


    // Operators
    BX_CONST_FUNC inline vec2_t operator+(const vec2_t& a, const vec2_t& b)
    {
        return vec2_t(a.x + b.x, a.y + b.y);
    }

    BX_CONST_FUNC inline vec2_t operator-(const vec2_t& a, const vec2_t& b)
    {
        return vec2_t(a.x - b.x, a.y - b.y);
    }

    BX_CONST_FUNC inline vec2_t operator*(const vec2_t& v, float k)
    {
        return vec2_t(v.x*k, v.y*k);
    }

    BX_CONST_FUNC inline ivec2_t operator+(const ivec2_t& a, const ivec2_t& b)
    {
        return ivec2_t(a.x + b.x, a.y + b.y);
    }

    BX_CONST_FUNC inline ivec2_t operator-(const ivec2_t& a, const ivec2_t& b)
    {
        return ivec2_t(a.x - b.x, a.y - b.y);
    }

    BX_CONST_FUNC inline ivec2_t operator*(const ivec2_t& v, int k)
    {
        return ivec2_t(v.x*k, v.y*k);
    }

    BX_CONST_FUNC inline ivec2_t operator/(const ivec2_t& v, int k)
    {
        return ivec2_t(v.x/k, v.y/k);
    }

    BX_CONST_FUNC inline vec2_t operator*(float k, const vec2_t& v)
    {
        return vec2_t(v.x*k, v.y*k);
    }

    BX_CONST_FUNC inline vec2_t operator*(const vec2_t& v0, const vec2_t& v1)
    {
        return vec2_t(v0.x*v1.x, v0.y*v1.y);
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

    inline void aabb_t::addPoint(const vec3_t& pt)
    {
        bx::vec3Min(fmin, fmin, pt.f);
        bx::vec3Max(fmax, fmax, pt.f);
    }

    inline vec3_t aabb_t::getCorner(int index) const
    {
        BX_ASSERT(index < 8, "index out of bounds");
        return vec3_t((index & 1) ? vmax.x : vmin.x,
                      (index & 2) ? vmax.y : vmin.y,
                      (index & 4) ? vmax.z : vmin.z);
    }

    inline void rect_t::addPoint(const vec2_t& pt)
    {
        bx::vec2Min(vmin.f, vmin.f, pt.f);
        bx::vec2Max(vmax.f, vmax.f, pt.f);
    }

    inline bool rect_t::testPoint(const vec2_t& pt) const
    {
        if (pt.x < xmin || pt.y < ymin || pt.x > xmax || pt.y > ymax)
            return false;
        return true;
    }

    inline bool rect_t::testCircle(const vec2_t& center, float radius) const
    {
        float wHalf = (xmax - xmin)*0.5f;
        float hHalf = (ymax - ymin)*0.5f;

        float dx = bx::abs((xmin + wHalf) - center.x);
        float dy = bx::abs((ymin + hHalf) - center.y);
        if (dx > (radius + wHalf) || dy > (radius + hHalf))
            return false;

        return true;
    }

    inline float rect_t::getWidth() const
    {
        return xmax - xmin;
    }

    inline float rect_t::getHeight() const
    {
        return ymax - ymin;
    }

    inline vec2_t rect_t::getSize() const
    {
        return vec2_t(xmax - xmin, ymax - ymin);
    }

    inline vec2_t rect_t::getCenter() const
    {
        return (vmin + vmax) * 0.5f;
    }

    inline int irect_t::getWidth() const
    {
        return xmax - xmin;
    }

    inline int irect_t::getHeight() const
    {
        return ymax - ymin;
    }

    inline ivec2_t irect_t::getSize() const
    {
        return ivec2_t(xmax - xmin, ymax - ymin);
    }

    inline ivec2_t irect_t::getCenter() const
    {
        return (vmin + vmax) / 2;
    }

} //namespace bx