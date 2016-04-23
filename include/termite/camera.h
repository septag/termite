#pragma once

#include "bx/float4x4_t.h"

namespace termite
{
    inline float4x4_t mtxOrthoOffCenterLH(float l, float r, float b, float t, float zn, float zf)
    {
        float4x4_t m;

        m.col[0][0] = 2.0f / (r - l);
        m.col[0][1] = 0.0f;
        m.col[0][2] = 0.0f;
        m.col[0][3] = (l + r) / (l - r);

        m.col[1][0] = 0.0f;
        m.col[1][1] = 2.0f / (t - b);
        m.col[1][2] = 0.0f;
        m.col[1][3] = (t + b) / (b - t);

        m.col[2][0] = 0.0f;
        m.col[2][1] = 0.0f;
        m.col[2][2] = 1.0f / (zf - zn);
        m.col[2][3] = zn / (zn - zf);

        m.col[3][0] = 0.0f;
        m.col[3][1] = 0.0f;
        m.col[3][2] = 0.0f;
        m.col[3][3] = 1.0f;

        return m;
    }

    inline float4x4_t mtxOrthoLH(float w, float h, float zn, float zf)
    {
        float wHalf = w*0.5f;
        float hHalf = h*0.5f;
        return mtxOrthoOffCenterLH(-wHalf, wHalf, -hHalf, hHalf, zn, zf);
    }
} // namespace st