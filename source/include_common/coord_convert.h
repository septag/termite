#pragma once

#include "termite/vec_math.h"

namespace termite
{
    enum class ZAxis
    {
        Unknown = 0,
        Up, // Z is Up
        GL  // Z is inverted
    };

    inline vec3_t convertVec3(const aiVector3D& v, ZAxis zaxis)
    {
        switch (zaxis) {
        case ZAxis::Unknown:
            return vec3_t(v.x, v.y, v.z);
        case ZAxis::Up:
            return vec3_t(v.x, v.z, v.y);
        case ZAxis::GL:
            return vec3_t(v.x, v.y, -v.z);
        default:
            return vec3_t();
        }
    }

    inline void saveMtx(const mtx4x4_t& m, float f[12])
    {
        f[0] = m.f[0];    f[1] = m.f[1];    f[2] = m.f[2];
        f[3] = m.f[4];    f[4] = m.f[5];    f[5] = m.f[6];
        f[6] = m.f[8];    f[7] = m.f[9];    f[8] = m.f[10];
        f[9] = m.f[12];   f[10] = m.f[13];  f[11] = m.f[14];
    }

    inline quat_t convertQuat(const aiQuaternion& q, ZAxis zaxis)
    {
        switch (zaxis)  {
        case ZAxis::Unknown:
            return quat_t(q.x, q.y, q.z, q.w);
        case ZAxis::Up:
            return quat_t(q.x, q.y, q.z, q.w);   // TODO: this is wrong, we have to flip the rotation axis
        case ZAxis::GL:
            return quat_t(-q.x, -q.y, q.z, q.w);
        default:
            return quatIdent();
        }
    }    

    inline mtx4x4_t convertMtx(const aiMatrix4x4& m, ZAxis zaxis)
    {
        switch (zaxis) {
        case ZAxis::Unknown:
            return mtx4x4f3(m.a1, m.b1, m.c1,
                           m.a2, m.b2, m.c2,
                           m.a3, m.b3, m.c3,
                           m.a4, m.b4, m.c4);
        case ZAxis::GL:
            return mtx4x4f3(m.a1, m.b1, -m.c1,
                           m.a2, m.b2, -m.c2,
                           -m.a3, -m.b3, m.c3,
                           m.a4, m.b4, -m.c4);
        case ZAxis::Up:
        {
            mtx4x4_t zup = mtx4x4f3(1.0f, 0, 0,
                                    0, 0, 1.0f,
                                    0, 1.0f, 0,
                                    0, 0, 0);
            mtx4x4_t r = mtx4x4f3(m.a1, m.b1, -m.c1,
                                  m.a2, m.b2, -m.c2,
                                  -m.a3, -m.b3, m.c3,
                                  m.a4, m.b4, -m.c4);
            return r * zup;
        }
        }
        return mtx4x4_t();
    }

} // namespace termite
