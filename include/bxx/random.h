#pragma once

#include <ctime>
#include <cstdlib>

namespace bx
{
    inline void randomSeed()
    {
        srand((uint32_t)time(nullptr));
    }

    inline int randomInt(int _min, int _max)
    {
        int r = rand();
        return ((r % (_max - _min + 1)) + _min);
    }

    inline double randomFloat(double _min, double _max)
    {
        double r = (double)rand() / RAND_MAX;
        return r*(_max - _min) + _min;
    }
}
