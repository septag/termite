#pragma once

#include "bx/bx.h"

#include "gfx_driver.h"
#include "resource_lib.h"
#include "vec_math.h"
#include "gfx_material.h"

#define MODELS_MEMORY_TAG 0x463fd3c75716947e 

namespace termite
{
    struct LoadModelParams
    {
        float resize;
    };

    struct ModelInstance
    {
        ResourceHandle modelHandle;
    };

    struct Model
    {
        struct Node
        {
            char name[32];
            mtx4x4_t localMtx;
            int parent;
            int mesh;
            int numChilds;
            int* childs;
            aabb_t bb;
        };

        struct Material
        {
            MaterialHandle handle;
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
            mtx4x4_t offsetMtx;
            int parent;
        };

        struct Skeleton
        {
            mtx4x4_t rootMtx;
            int numJoints;
            Joint* joints;
            mtx4x4_t* initPose;
        };

        struct Geometry
        {
            int numVerts;
            int numIndices;
            VertexDecl vdecl;
            void* verts;
            uint16_t* indices;
            Skeleton* skel;
        };

        int numNodes;
        int numGeos;
        int numMaterials;
        int numMeshes;

        mtx4x4_t rootMtx;

        Node* nodes;
        Geometry* geos;
        Material* mtls;
        Mesh* meshes;
    };

    result_t initModelLoader(GfxDriverApi* driver, bx::AllocatorI* alloc);
    void shutdownModelLoader();

    void registerModelToResourceLib(ResourceLib* resLib);

    ModelInstance* createModelInstance(Model* model);

    TERMITE_API VertexBufferHandle getModelVertexBuffer(Model* model, int index);
    TERMITE_API IndexBufferHandle getModelIndexBuffer(Model* model, int index);

} // namespace termite

