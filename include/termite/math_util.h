#pragma once

#include "bx/allocator.h"
#include "bx/fpumath.h"

namespace termite
{
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
            memset(this->mtx, 0x00, sizeof(Ty)*width*height);

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


    TERMITE_API bool generateWhiteNoise(FloatMatrix* whiteNoise, int width, int height);
    TERMITE_API bool generateSmoothNoise(FloatMatrix* smoothNoise, const FloatMatrix* baseNoise, int octave);
    TERMITE_API bool generatePerlinNoise(FloatMatrix* perlinNoise, const FloatMatrix* baseNoise, int octaveCount, 
                                         float persistance, bx::AllocatorI* alloc);
    TERMITE_API float normalDist(float x, float mean, float stdDev);


    // Reference: http://stackoverflow.com/questions/707370/clean-efficient-algorithm-for-wrapping-integers-in-c
    inline int iwrap(int kX, int const kLowerBound, int const kUpperBound)
    {
        int range_size = kUpperBound - kLowerBound + 1;

        if (kX < kLowerBound)
            kX += range_size * ((kLowerBound - kX) / range_size + 1);

        return kLowerBound + (kX - kLowerBound) % range_size;
    }

    inline float fwrapRange(float x, float vmin, float vmax)
    {
        vmax -= vmin;
        x = fmod(x, vmax);
        return x + vmin;
    }

    inline int iclamp(int n, const int _min, const int _max)
    {
        if (n < _min)
            return _min;
        else if (n > _max)
            return _max;
        else
            return n;
    }

    inline float falign(float value, float size)
    {
        return value - bx::fabsolute(bx::fmod(value, size));
    }

    // Line fgain, but goes up to 1.0 and then back to 0
    inline float fwave(float _time, float _gain)
    {
        if (_time < 0.5f) {
            return bx::fbias(_time * 2.0f, _gain);
        } else {
            return 1.0f - bx::fbias(_time * 2.0f - 1.0f, 1.0f - _gain);
        }
    }
} // namespace termite

