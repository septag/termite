#include "pch.h"
#include "math_util.h"

#include <alloca.h>

bool termite::generateWhiteNoise(FloatMatrix* whiteNoise, int width, int height)
{
    if (!whiteNoise->create(width, height))
        return false;

    int num = width*height;
    for (int i = 0; i < num; i++)
        whiteNoise->mtx[i] = getRandomFloatUniform(0.0f, 1.0f);

    return true;
}

bool termite::generateSmoothNoise(FloatMatrix* smoothNoise, const FloatMatrix* baseNoise, int octave)
{
    int width = baseNoise->width;
    int height = baseNoise->height;

    if (!smoothNoise->create(width, height))
        return false;

    int samplePeriod = 1 << octave;
    float sampleFreq = 1.0f / samplePeriod;

    for (int i = 0; i < width; i++) {
        int sample_i0 = (i / samplePeriod) * samplePeriod;
        int sample_i1 = (sample_i0 + samplePeriod) % width; // wrap
        float horzBlend = float(i - sample_i0) * sampleFreq;

        for (int j = 0; j < height; j++) {
            int sample_j0 = (j / samplePeriod) * samplePeriod;
            int sample_j1 = (sample_j0 + samplePeriod) % height; // wrap
            float vertBlend = float(j - sample_j0) * sampleFreq;

            // blend two top corners
            float top = bx::flerp(baseNoise->get(sample_i0, sample_j0), baseNoise->get(sample_i1, sample_j0), horzBlend);
            float bottom = bx::flerp(baseNoise->get(sample_i0, sample_j1), baseNoise->get(sample_i1, sample_j1), horzBlend);

            smoothNoise->set(i, j, bx::flerp(top, bottom, vertBlend));
        }
    }

    return true;
}

bool termite::generatePerlinNoise(FloatMatrix* perlinNoise, const FloatMatrix* baseNoise, int octaveCount,
                                  float persistance, bx::AllocatorI* alloc)
{
    int width = baseNoise->width;
    int height = baseNoise->height;

    FloatMatrix* buff = (FloatMatrix*)alloca(sizeof(FloatMatrix)*octaveCount);
    FloatMatrix** smoothNoise = (FloatMatrix**)alloca(sizeof(FloatMatrix*)*octaveCount);
    if (!smoothNoise)
        return false;
    for (int i = 0; i < octaveCount; i++) {
        smoothNoise[i] = new(buff + i) FloatMatrix(alloc);
        generateSmoothNoise(smoothNoise[i], baseNoise, i);
    }

    if (!perlinNoise->create(width, height))
        return false;
    float amplitude = 1.0f;
    float totalAmplitude = 0;

    for (int octave = octaveCount - 1; octave >= 0; octave--) {
        amplitude *= persistance;
        totalAmplitude += amplitude;

        for (int j = 0; j < height; j++) {
            for (int i = 0; i < width; i++) {
                float f = perlinNoise->get(i, j);
                perlinNoise->set(i, j, f + smoothNoise[octave]->get(i, j)*amplitude);
            }
        }
    }

    // normalize
    int total = width*height;
    for (int i = 0; i < total; i++)
        perlinNoise->mtx[i] /= totalAmplitude;

    for (int i = 0; i < octaveCount; i++)
        smoothNoise[i]->destroy();

    return true;
}

float termite::normalDist(float x, float mean, float stdDev)
{
    float variance = stdDev*stdDev;
    float var2x = 2.0f*variance;
    float f = 1.0f / bx::fsqrt(var2x*bx::pi);
    float p = x - mean;
    float ep = -(p*p) / var2x;

    return f*expf(ep);
}

