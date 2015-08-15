#pragma once

namespace bx
{
    union Vec2
    {
        struct  
        {
            float x;
            float y;
        };

        float f[2];

        Vec2()  {}
        Vec2(float _x, float _y)
        {
            x = _x;
            y = _y;
        }
    };

    union Vec3
    {
        struct  
        {
            float x;
            float y;
            float z;
        };

        float f[3];

        Vec3()  {}
        Vec3(float _x, float _y, float _z)
        {
            x = _x;
            y = _y;
            z = _z;
        }
    };

    union Vec4
    {
        struct
        {
            float x;
            float y;
            float z;
            float w;
        };

        float f[4];

        Vec4()      { }
        Vec4(float _x, float _y, float _z, float _w)
        {
            x = _x;
            y = _y;
            z = _z;
            w = _w;
        }
    };

    union Quat
    {
        struct 
        {
            float x;
            float y;
            float z;
            float w;
        };

        float f[4];

        Quat(float _x, float _y, float _z, float _w)
        {
            x = _x;
            y = _y;
            z = _z;
            w = _w;
        }
    };

    union Matrix3
    {
        struct
        {
            Vec3 x;
            Vec3 y;
            Vec3 z;
        };

        float f[9];
    };

    union Matrix
    {
        struct
        {
            Vec4 x;
            Vec4 y;
            Vec4 z;
            Vec4 t;
        };

        float f[16];
    };

    inline Vec2* vec2Set(Vec2* _r, float _x, float _y)
    {
        _r->x = _x;
        _r->y = _y;
        return _r;
    }

    inline Vec3* vec3Set(Vec3* _r, float _x, float _y, float _z)
    {
        _r->x = _x;
        _r->y = _y;
        _r->z = _z;
        return _r;
    }

    inline Vec4* vec4Set(Vec4* _r, float _x, float _y, float _z, float _w)
    {
        _r->x = _x;
        _r->y = _y;
        _r->z = _z;
        _r->w = _w;
        return _r;
    }

    inline Quat* quatSet(Quat* _r, float _x, float _y, float _z, float _w)
    {
        _r->x = _x;
        _r->y = _y;
        _r->z = _z;
        _r->w = _w;
        return _r;
    }

    inline Matrix3* mtx3Set(Matrix3* _r,
                            float _xx, float _xy, float _xz,
                            float _yx, float _yy, float _yz,
                            float _zx, float _zy, float _zz)
    {
        _r->x.x = _xx;      _r->x.y = _xy;      _r->x.z = _xz;
        _r->y.x = _yx;      _r->y.y = _yy;      _r->y.z = _yz;
        _r->z.x = _zx;      _r->z.y = _zy;      _r->z.z = _zz;
        return _r;        
    }

    inline Matrix* mtxSet(Matrix* _r,
                          float _xx, float _xy, float _xz, float _xw,
                          float _yx, float _yy, float _yz, float _yw,
                          float _zx, float _zy, float _zz, float _zw)
    {
        _r->x.x = _xx;      _r->x.y = _xy;      _r->x.z = _xz;      _r->x.w = _xw;
        _r->y.x = _yx;      _r->y.y = _yy;      _r->y.z = _yz;      _r->y.w = _yw;
        _r->z.x = _zx;      _r->z.y = _zy;      _r->z.z = _zz;      _r->z.w = _zw;
        return _r;
    }
}   // namespace: bx