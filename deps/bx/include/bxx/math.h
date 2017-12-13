#pragma once

#include "bx/math.h"

namespace bx
{
    inline void mtx3x3Translate(float* result, float x, float y)
    {
        bx::memSet(result, 0x00, sizeof(float) * 9);
        result[0] = 1.0f;
        result[4] = 1.0f;
        result[6] = x;
        result[7] = y;
        result[8] = 1.0f;
    }

    inline void mtx3x3Rotate(float* result, float theta)
    {
        bx::memSet(result, 0x00, sizeof(float) * 9);
        float c = fcos(theta);
        float s = fsin(theta);
        result[0] = c;     result[1] = -s;
        result[3] = s;     result[4] = c;
        result[8] = 1.0f;
    }

    inline void mtx3x3Scale(float* result, float sx, float sy)
    {
        bx::memSet(result, 0x00, sizeof(float) * 9);
        result[0] = sx;
        result[4] = sy;
        result[8] = 1.0f;
    }

    inline void vec3MulMtxRot(float* __restrict _result, const float* __restrict _vec, const float* __restrict _mat)
    {
        _result[0] = _vec[0] * _mat[0] + _vec[1] * _mat[4] + _vec[2] * _mat[8];
        _result[1] = _vec[0] * _mat[1] + _vec[1] * _mat[5] + _vec[2] * _mat[9];
        _result[2] = _vec[0] * _mat[2] + _vec[1] * _mat[6] + _vec[2] * _mat[10];
    }

    inline void mtx3x3Compose(float* result, float x, float y, float angle)
    {
        bx::memSet(result, 0x00, sizeof(float) * 9);
        float c = bx::fcos(angle);
        float s = fsin(angle);
        result[0] = c;     result[1] = -s;
        result[3] = s;     result[4] = c;
        result[6] = x;     result[7] = y;
        result[8] = 1.0f;
    }

    inline void mtx3x3Decompose(const float* mat, float* translation, float* rotation)
    {
        translation[0] = mat[6];
        translation[1] = mat[7];

        *rotation = bx::fatan2(mat[3], mat[4]);
    }

    inline void vec2MulMtx3x3(float* __restrict _result, const float* __restrict _vec, const float* __restrict _mat)
    {
        _result[0] = _vec[0] * _mat[0] + _vec[1] * _mat[3] + _mat[6];
        _result[1] = _vec[0] * _mat[1] + _vec[1] * _mat[4] + _mat[7];
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
            float s = 0.5f / bx::fsqrt(trace + 1.0f);
            _result[3] = 0.25f / s;
            _result[0] = (mtx[9] - mtx[6]) * s;
            _result[1] = (mtx[2] - mtx[8]) * s;
            _result[2] = (mtx[4] - mtx[1]) * s;
        } else {
            if (mtx[0] > mtx[5] && mtx[0] > mtx[10]) {
                float s = 2.0f * bx::fsqrt(1.0f + mtx[0] - mtx[5] - mtx[10]);
                _result[3] = (mtx[9] - mtx[6]) / s;
                _result[0] = 0.25f * s;
                _result[1] = (mtx[1] + mtx[4]) / s;
                _result[2] = (mtx[2] + mtx[8]) / s;
            } else if (mtx[5] > mtx[10]) {
                float s = 2.0f * bx::fsqrt(1.0f + mtx[5] - mtx[0] - mtx[10]);
                _result[3] = (mtx[2] - mtx[8]) / s;
                _result[0] = (mtx[1] + mtx[4]) / s;
                _result[1] = 0.25f * s;
                _result[2] = (mtx[6] + mtx[9]) / s;
            } else {
                float s = 2.0f * bx::fsqrt(1.0f + mtx[10] - mtx[0] - mtx[5]);
                _result[3] = (mtx[4] - mtx[1]) / s;
                _result[0] = (mtx[2] + mtx[8]) / s;
                _result[1] = (mtx[6] + mtx[9]) / s;
                _result[2] = 0.25f * s;
            }
        }
    }

    inline float vec2Dot(const float* __restrict _a, const float* __restrict _b)
    {
        return _a[0]*_b[0] + _a[1]*_b[1];
    }

    inline float vec2Length(const float* _a)
    {
        return fsqrt(vec2Dot(_a, _a));
    }

    inline float vec2Norm(float* __restrict _result, const float* __restrict _a)
    {
        const float len = vec2Length(_a);
        const float invLen = 1.0f/len;
        _result[0] = _a[0] * invLen;
        _result[1] = _a[1] * invLen;
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
        _result[0] = flerp(_a[0], _b[0], _t);
        _result[1] = flerp(_a[1], _b[1], _t);
    }

}