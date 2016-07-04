#pragma once

#include "bx/bx.h"

#define T3D_SIGN        0x543344	// T3D
#define T3D_VERSION_10	0x312e30	// 1.0

#pragma pack(push, 1)

namespace termite
{
    enum class t3dVertexAttrib : uint16_t
    {
        Position,  // a_position
        Normal,    // a_normal
        Tangent,   // a_tangent
        Bitangent, // a_bitangent
        Color0,    // a_color0
        Color1,    // a_color1
        Indices,   // a_indices
        Weight,    // a_weight
        TexCoord0, // a_texcoord0
        TexCoord1, // a_texcoord1
        TexCoord2, // a_texcoord2
        TexCoord3, // a_texcoord3
        TexCoord4, // a_texcoord4
        TexCoord5, // a_texcoord5
        TexCoord6, // a_texcoord6
        TexCoord7, // a_texcoord7
        Count
    };

    enum class t3dTextureUsage : uint8_t
    {
        Diffuse,
        AO,
        Light,
        Normal,
        Specular,
        Emissive,
        Gloss,
        Reflection,
        Alpha
    };

    struct t3dJoint
    {
        char name[32];
        float offsetMtx[12];
        int parent;
    };

    struct t3dGeometry
    {
        int numTris;
        int numVerts;
        int numAttribs;
        int vertStride;

        struct skel_t
        {
            int numJoints;
            float rootMtx[12];
        } skel;

#if 0
        h3dJoint* joints;
        float* initPose;    // array of 4x3 matrices float[12]
        t3dVertexAttrib* attribs;  
        void* verts;    // each vertex is packed into single struct
        uint16_t* indices;
#endif
    };

    struct t3dSubmesh
    {
        int mtl;
        int startIndex;
        int numIndices;
    };

    struct t3dMesh
    {
        int geo;
        int numSubmeshes;

#if 0
        t3dSubmesh* submeshes;
#endif
    };

    struct t3dTexture
    {
        t3dTextureUsage usage;
        char filepath[256];
    };

    struct t3dMaterial
    {
        float diffuse[3];
        float specular[3];
        float ambient[3];
        float emissive[3];
        float specExp;
        float specIntensity;
        float opacity;
        int numTextures;

#if 0
        t3dTexture* textures;
#endif
    };

    struct t3dNode
    {
        char name[32];
        int mesh;
        int parent;
        int numChilds;
        float xformMtx[12];
        float aabbMin[3];
        float aabbMax[3];

#if 0
        int* childs;
#endif
    };

    struct t3dMetablock
    {
        char name[32];  // ex. "Materials"
        int stride; // bytes to step into next meta block, -1 if no block exists
    };

    struct t3dHeader
    {
        uint32_t sign;
        uint32_t version;

        int numNodes;
        int numMeshes;
        int numGeos;
        int reserved1;
        int reserved2;
        int64_t metaOffset;

#if 0
        t3dNode* nodes;
        t3dMesh* meshes;
        t3dGeometry* geos;
#endif
    };

#pragma pack(pop)

} // namespace termite
