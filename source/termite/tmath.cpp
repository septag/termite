#include "pch.h"
#include "tmath.h"

#include <alloca.h>

// TODO: Remove this in the next project

namespace tee {
    bool tmath::whiteNoise(FloatMatrix* whiteNoise, int width, int height)
    {
        if (!whiteNoise->create(width, height))
            return false;

        int num = width*height;
        for (int i = 0; i < num; i++)
            whiteNoise->mtx[i] = getRandomFloatUniform(0.0f, 1.0f);

        return true;
    }

    bool tmath::smoothNoise(FloatMatrix* smoothNoise, const FloatMatrix* baseNoise, int octave)
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
                float top = bx::lerp(baseNoise->get(sample_i0, sample_j0), baseNoise->get(sample_i1, sample_j0), horzBlend);
                float bottom = bx::lerp(baseNoise->get(sample_i0, sample_j1), baseNoise->get(sample_i1, sample_j1), horzBlend);

                smoothNoise->set(i, j, bx::lerp(top, bottom, vertBlend));
            }
        }

        return true;
    }

    bool tmath::perlinNoise(FloatMatrix* perlinNoise, const FloatMatrix* baseNoise, int octaveCount,
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
            tmath::smoothNoise(smoothNoise[i], baseNoise, i);
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

    float tmath::normalDist(float x, float mean, float stdDev)
    {
        float variance = stdDev*stdDev;
        float var2x = 2.0f*variance;
        float f = 1.0f / bx::sqrt(var2x*bx::kPi);
        float p = x - mean;
        float ep = -(p*p) / var2x;

        return f*bx::exp2(ep);
    }

    aabb_t tmath::aabbTransform(const aabb_t& b, const mat4_t& mtx)
    {
        vec3_t vmin;
        vec3_t vmax;

        /* start with translation part */
        vec4_t t(mtx.vrow3);

        vmin.x = vmax.x = t.x;
        vmin.y = vmax.y = t.y;
        vmin.z = vmax.z = t.z;

        if (mtx.m11 > 0.0f) {
            vmin.x += mtx.m11 * b.vmin.x;
            vmax.x += mtx.m11 * b.vmax.x;
        } else {
            vmin.x += mtx.m11 * b.vmax.x;
            vmax.x += mtx.m11 * b.vmin.x;
        }

        if (mtx.m12 > 0.0f) {
            vmin.y += mtx.m12 * b.vmin.x;
            vmax.y += mtx.m12 * b.vmax.x;
        } else {
            vmin.y += mtx.m12 * b.vmax.x;
            vmax.y += mtx.m12 * b.vmin.x;
        }

        if (mtx.m13 > 0.0f) {
            vmin.z += mtx.m13 * b.vmin.x;
            vmax.z += mtx.m13 * b.vmax.x;
        } else {
            vmin.z += mtx.m13 * b.vmax.x;
            vmax.z += mtx.m13 * b.vmin.x;
        }

        if (mtx.m21 > 0.0f) {
            vmin.x += mtx.m21 * b.vmin.y;
            vmax.x += mtx.m21 * b.vmax.y;
        } else {
            vmin.x += mtx.m21 * b.vmax.y;
            vmax.x += mtx.m21 * b.vmin.y;
        }

        if (mtx.m22 > 0.0f) {
            vmin.y += mtx.m22 * b.vmin.y;
            vmax.y += mtx.m22 * b.vmax.y;
        } else {
            vmin.y += mtx.m22 * b.vmax.y;
            vmax.y += mtx.m22 * b.vmin.y;
        }

        if (mtx.m23 > 0.0f) {
            vmin.z += mtx.m23 * b.vmin.y;
            vmax.z += mtx.m23 * b.vmax.y;
        } else {
            vmin.z += mtx.m23 * b.vmax.y;
            vmax.z += mtx.m23 * b.vmin.y;
        }

        if (mtx.m31 > 0.0f) {
            vmin.x += mtx.m31 * b.vmin.z;
            vmax.x += mtx.m31 * b.vmax.z;
        } else {
            vmin.x += mtx.m31 * b.vmax.z;
            vmax.x += mtx.m31 * b.vmin.z;
        }

        if (mtx.m32 > 0.0f) {
            vmin.y += mtx.m32 * b.vmin.z;
            vmax.y += mtx.m32 * b.vmax.z;
        } else {
            vmin.y += mtx.m32 * b.vmax.z;
            vmax.y += mtx.m32 * b.vmin.z;
        }

        if (mtx.m33 > 0.0f) {
            vmin.z += mtx.m33 * b.vmin.z;
            vmax.z += mtx.m33 * b.vmax.z;
        } else {
            vmin.z += mtx.m33 * b.vmax.z;
            vmax.z += mtx.m33 * b.vmin.z;
        }

        return aabb(vmin, vmax);
    }

    bool tmath::projectToScreen(vec2_t* result, const vec3_t point, const irect_t& viewport, const mat4_t& viewProjMtx)
    {
        float w = float(viewport.xmax - viewport.xmin);
        float h = float(viewport.ymax - viewport.ymin);
        float wh = w*0.5f;
        float hh = h*0.5f;

        vec4_t proj;
        bx::vec4MulMtx(proj.f, vec4(point.x, point.y, point.z, 1.0f).f, viewProjMtx.f);
        bx::vec3Mul(proj.f, proj.f, 1.0f / proj.w);     proj.w = 1.0f;

        float x = bx::floor(proj.x*wh + wh + 0.5f);
        float y = bx::floor(-proj.y*hh + hh + 0.5f);

        *result = vec2(x, y);

        // ZCull
        if (proj.z < 0.0f || proj.z > 1.0f)
            return false;
        return true;
    }

    const vec4_t vec4_t::Zero = vec4(0, 0, 0, 1.0f);
    const vec4_t vec4_t::Up = vec4(0, 1.0f, 0, 1.0f);
    const vec4_t vec4_t::Right = vec4(1.0f, 0, 0, 1.0f);
    const vec4_t vec4_t::Forward = vec4(0, 0, 1.0f, 1.0f);

    const vec2_t vec2_t::Zero = vec2(0, 0);
    const vec2_t vec2_t::Right = vec2(1.0f, 0);
    const vec2_t vec2_t::Up = vec2(0, 1.0f);

    const vec3_t vec3_t::Zero = vec3(0, 0, 0);
    const vec3_t vec3_t::Right = vec3(1.0f, 0, 0);
    const vec3_t vec3_t::Up = vec3(0, 1.0f, 0);
    const vec3_t vec3_t::Forward = vec3(0, 0, 1.0f);

    const ucolor_t ucolor_t::White = ucolor(0xffffffff);
    const ucolor_t ucolor_t::Black = ucolor(0xff000000);
    const ucolor_t ucolor_t::Red = ucolor(0xff0000ff);
    const ucolor_t ucolor_t::Green = ucolor(0xff00ff00);
    const ucolor_t ucolor_t::Blue = ucolor(0xffff0000);
    const ucolor_t ucolor_t::Yellow = ucolor(0xff00ffff);
    const ucolor_t ucolor_t::Cyan = ucolor(0xffffff00);

    const ivec2_t ivec2_t::Zero = ivec2(0, 0);
    const ivec2_t ivec2_t::Up = ivec2(0, 1);
    const ivec2_t ivec2_t::Right = ivec2(1, 0);

    const quat_t quat_t::Ident = quaternionI();

    const mat3_t mat3_t::Zero = mat3(0, 0, 0, 0, 0, 0, 0, 0, 0);
    const mat3_t mat3_t::Ident = mat3I();

    const mat4_t mat4_t::Zero = mat4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    const mat4_t mat4_t::Ident = mat4I();

    const aabb_t aabb_t::Null = aabb(FLT_MAX, FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX);
    const rect_t rect_t::Null = rect(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);
    const irect_t irect_t::Null = irect(INT_MAX, INT_MAX, -INT_MAX, -INT_MAX);
    
}   // namespace tee