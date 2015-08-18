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

    inline Vec2 vec2(float _x, float _y)
    {
        Vec2 r;
        r.x = _x;       r.y = _y;
        return r;
    }

    inline Vec2* vec2Set(Vec2* _r, float _x, float _y)
    {
        _r->x = _x;
        _r->y = _y;
        return _r;
    }

    inline Vec3 vec3(float _x, float _y, float _z)
    {
        Vec3 r;
        r.x = _x;   r.y = _y;   r.z = _z;
        return r;
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

    inline Quat quat(float _x, float _y, float _z, float _w)
    {
        Quat r;
        r.x = _x;   r.y = _y;   r.z = _z;   r.w = _w;
        return r;
    }

    inline Quat* quatSet(Quat* _r, float _x, float _y, float _z, float _w)
    {
        _r->x = _x;
        _r->y = _y;
        _r->z = _z;
        _r->w = _w;
        return _r;
    }

    inline Matrix3 mtx3(float _xx, float _xy, float _xz,
                        float _yx, float _yy, float _yz,
                        float _zx, float _zy, float _zz)
    {
        Matrix3 r;
        r.x.x = _xx;      r.x.y = _xy;      r.x.z = _xz;
        r.y.x = _yx;      r.y.y = _yy;      r.y.z = _yz;
        r.z.x = _zx;      r.z.y = _zy;      r.z.z = _zz;
        return r;
    }

    inline Matrix3 mtx3v(const Vec3& _x, const Vec3& _y, const Vec3& _z)
    {
        Matrix3 r;
        r.x.x = _x.x;      r.x.y = _x.y;      r.x.z = _x.z;
        r.y.x = _y.x;      r.y.y = _y.y;      r.y.z = _y.z;
        r.z.x = _z.x;      r.z.y = _z.y;      r.z.z = _z.z;
        return r;
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

    inline Matrix mtx(float _xx, float _xy, float _xz, float _xw,
                      float _yx, float _yy, float _yz, float _yw,
                      float _zx, float _zy, float _zz, float _zw)
    {
        Matrix r;
        r.x.x = _xx;      r.x.y = _xy;      r.x.z = _xz;      r.x.w = _xw;
        r.y.x = _yx;      r.y.y = _yy;      r.y.z = _yz;      r.y.w = _yw;
        r.z.x = _zx;      r.z.y = _zy;      r.z.z = _zz;      r.z.w = _zw;
        return r;
    }

    inline Matrix mtxv(const Vec4& _x, const Vec4& _y, const Vec4& _z, const Vec4& _w)
    {
        Matrix r;
        r.x.x = _x.x;      r.x.y = _x.y;      r.x.z = _x.z;      r.x.w = _x.w;
        r.y.x = _y.x;      r.y.y = _y.y;      r.y.z = _y.z;      r.y.w = _y.w;
        r.z.x = _z.x;      r.z.y = _z.y;      r.z.z = _z.z;      r.z.w = _z.w;
        return r;
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

    //
    inline int imin(int _a, int _b)
    {
        return _a < _b ? _a : _b;
    }

    inline int imax(int _a, int _b)
    {
        return _a < _b ? _a : _b;
    }

    inline int imin3(int _a, int _b, int _c)
    {
        return imin(_a, imin(_b, _c));
    }

    inline int imax3(int _a, int _b, int _c)
    {
        return imax(_a, imax(_b, _c));
    }

    inline int iclamp(int _a, int _min, int _max)
    {
        return imin(imax(_a, _min), _max);
    }

}   // namespace: bx