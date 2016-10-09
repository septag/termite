#pragma once

#include "bx/bx.h"

#define TSPRITE_SIGN 0x54535052
#define TSPRITE_VERSION 0x312e30

#pragma pack(push, 1)

namespace termite
{
    struct tsSprite
    {
        float tx0, ty0;
        float tx1, ty1;
        uint32_t tag; // 0 or hashed name value
    };

    struct tsAnimation
    {
        char name[32];
        int fps;
        int startFrame;
        int endFrame;       
    };

    struct tsHeader
    {
        uint32_t sign;
        uint32_t version;

        char textureFilepath[128];
        int numSprites;
        int numAnims;
#if 0
        tsSprite* sprites;
        tsAnimation* anims;
#endif
    };
}

#pragma pack(pop)