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

} // namespace termite

