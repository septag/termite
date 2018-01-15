#pragma once

#include "bx/bx.h"

#include "assetlib.h"
#include "gfx_material.h"

namespace tee
{
    struct LoadModelParams
    {
        enum VertexBufferType
        {
            StaticVb = 0,
            DynamicVb
        };

        VertexBufferType vbType;
        float resize;

        LoadModelParams()
        {
            vbType = VertexBufferType::StaticVb;
            resize = 1.0f;
        }
    };

    //
    struct ModelInstance
    {
        AssetHandle modelHandle;
        union {
            VertexBufferHandle* vertexBuffers;     // 1-1 to geometries
            DynamicVertexBufferHandle* dynVertexBuffers;
        };
        IndexBufferHandle* indexBuffers;       // 1-1 to geometries
        MaterialHandle* mtls;                  // 1-1 to materials in model
    };

    // This is the actual Descriptor for every model
    // With this data, you can create model instances and perform any model operations
    struct Model
    {
        struct Node
        {
            char name[32];
            mat4_t localMtx;
            int parent;
            int mesh;
            int numChilds;
            int* childs;
            aabb_t bb;
        };

        struct Submesh
        {
            int mtl;
            int startIndex;
            int numIndices;
        };

        struct Mesh
        {
            int geo;
            int numSubmeshes;
            Submesh* submeshes;
        };

        struct Joint
        {
            char name[32];
            mat4_t offsetMtx;
            int parent;
        };

        struct Skeleton
        {
            mat4_t rootMtx;
            int numJoints;
            Joint* joints;
            mat4_t* initPose;
        };

        struct Geometry
        {
            int numVerts;
            int numIndices;
            VertexDecl vdecl;

            GfxBufferFlag::Bits vbFlags;// VertexBuffer creation flags
            void* verts;                // Vertex buffer (on cpu)

            GfxBufferFlag::Bits ibFlags; // Index buffer creation flags
            union {
                uint16_t* indices;      
                uint32_t* indicesUint;
            };
            Skeleton* skel;

            // We keep these two buffers for caching, If buffer type is static these buffers are not changed so we don't
            // need any more extra buffers created for every instance
            VertexBufferHandle vertexBuffer;     // 1 for each geometry
            IndexBufferHandle indexBuffer;       // 1 for each geometry
        };

        int numNodes;
        int numGeos;
        int numMeshes;
        int numMtls;

        mat4_t rootMtx;

        Node* nodes;
        Geometry* geos;
        Mesh* meshes;
        MaterialDecl* mtls;
        bool vbIsDynamic;
    };

    namespace gfx {
        TEE_API ModelInstance* createModelInstance(AssetHandle modelHandle, bx::AllocatorI* alloc);
        TEE_API void destroyModelInstance(ModelInstance* inst);
    }

} // namespace tee

