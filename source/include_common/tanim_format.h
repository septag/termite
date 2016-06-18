#pragma once

#include "bx/bx.h"

#define TANIM_SIGN 0x54414e4d   // TANM
#define TANIM_VERSION 0x312e30  // 1.0

#pragma pack(push, 1)

namespace termite
{
    struct taChannel
    {
        char bindto[32];

#if 0
        float* poss;    // float[4] array + scale
        float* rots;    // float[4] array
#endif
    };

    struct taClip
    {
        char name[32];
        int start;
        int end;
        int looped;
    };

    struct taMetablock
    {
        char name[32];  // ex. "Clips"
        int stride;     // bytes to step into next meta block
    };

    struct taHeader
    {
        uint32_t sign;
        uint32_t version;

        int fps;
        int numFrames;
        int numChannels;
        int hasScale;
        int64_t metaOffset;

#if 0
        taChannel* channels;
#endif
    };
} // namespace termite

#pragma pack(pop)
