#pragma once

#include "termite/tmath.h"

namespace tee
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
            return vec3(v.x, v.y, v.z);
        case ZAxis::Up:
            return vec3(v.x, v.z, v.y);
        case ZAxis::GL:
            return vec3(v.x, v.y, -v.z);
        default:
            return vec3(0, 0, 0);
        }
    }

    inline void saveMtx(const mat4_t& m, float f[12])
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
            return quaternion(q.x, q.y, q.z, q.w);
        case ZAxis::Up:
            return quaternion(q.x, q.y, q.z, q.w);   // TODO: this is wrong, we have to flip the rotation axis
        case ZAxis::GL:
            return quaternion(-q.x, -q.y, q.z, q.w);
        default:
            return quaternionI();
        }
    }    

    inline mat4_t convertMtx(const aiMatrix4x4& m, ZAxis zaxis)
    {
        switch (zaxis) {
        case ZAxis::Unknown:
            return mat4(m.a1, m.b1, m.c1,
                        m.a2, m.b2, m.c2,
                        m.a3, m.b3, m.c3,
                        m.a4, m.b4, m.c4);
        case ZAxis::GL:
            return mat4(m.a1, m.b1, -m.c1,
                        m.a2, m.b2, -m.c2,
                        -m.a3, -m.b3, m.c3,
                        m.a4, m.b4, -m.c4);
        case ZAxis::Up:
        {
            mat4_t zup = mat4(1.0f, 0, 0,
                              0, 0, 1.0f,
                              0, 1.0f, 0,
                              0, 0, 0);
            mat4_t r = mat4(m.a1, m.b1, -m.c1,
                            m.a2, m.b2, -m.c2,
                            -m.a3, -m.b3, m.c3,
                            m.a4, m.b4, -m.c4);
            return r * zup;
        }
        }
        return mat4_t();
    }

} // namespace tee
