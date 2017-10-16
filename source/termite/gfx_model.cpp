#include "pch.h"

#include "bx/readerwriter.h"
#include "bxx/array.h"
#include "memory_pool.h"

#include "gfx_model.h"

#include "../include_common/t3d_format.h"

using namespace termite;

struct ModelInstanceImpl
{
    struct Pose
    {
        int numJoints;
        mtx4x4_t* mtxs; // Final joint matrices
        mtx4x4_t* offsetMtxs;   // offset matrices
        mtx4x4_t* skinMtxs; // Skinning matrices (mtx*offsetMtx)
    };

    struct Material
    {
        ProgramHandle program;
    };
};

struct ModelImpl
{
    Model m;
    VertexBufferHandle* vertexBuffers;     // 1 for each geometry
    IndexBufferHandle* indexBuffers;       // 1 for each geometry

    ModelImpl()
    {
        bx::memSet(&m, 0x00, sizeof(m));
        vertexBuffers = nullptr;
        indexBuffers = nullptr;
    }
};

class ModelLoader : public ResourceCallbacksI
{
public:
    bool loadObj(const MemoryBlock* mem, const ResourceTypeParams& params, uintptr_t* obj, bx::AllocatorI* alloc) override;
    void unloadObj(uintptr_t obj, bx::AllocatorI* alloc) override;
    void onReload(ResourceHandle handle, bx::AllocatorI* alloc) override;
};

struct ModelManager
{
    bx::AllocatorI* alloc;
    GfxDriverApi* driver;
    ModelLoader loader;

    PageAllocator allocStub;

    ModelManager(bx::AllocatorI* _alloc) :
        allocStub(MODELS_MEMORY_TAG)
    {
        alloc = _alloc;
        driver = nullptr;
    }
};

static ModelManager* g_modelMgr = nullptr;

result_t termite::initModelLoader(GfxDriverApi* driver, bx::AllocatorI* alloc)
{
    if (g_modelMgr) {
        assert(false);
        return T_ERR_ALREADY_INITIALIZED;
    }

    g_modelMgr = BX_NEW(alloc, ModelManager)(alloc);
    if (!g_modelMgr)
        return T_ERR_OUTOFMEM;

    g_modelMgr->driver = driver;

    return 0;
}

void termite::shutdownModelLoader()
{
    if (!g_modelMgr)
        return;

    BX_DELETE(g_modelMgr->alloc, g_modelMgr);
    g_modelMgr = nullptr;
}

void termite::registerModelToResourceLib()
{
    ResourceTypeHandle handle;
    handle = registerResourceType("model", &g_modelMgr->loader, sizeof(LoadModelParams));
    assert(handle.isValid());
}

ModelInstance* termite::createModelInstance(Model* model)
{
    assert(g_modelMgr);

    return nullptr;
}

VertexBufferHandle termite::getModelVertexBuffer(Model* model, int index)
{
    return ((ModelImpl*)model)->vertexBuffers[index];
}

IndexBufferHandle termite::getModelIndexBuffer(Model* model, int index)
{
    return ((ModelImpl*)model)->indexBuffers[index];
}

static void unloadModel(ModelImpl* model)
{
    assert(g_modelMgr);

    if (!model)
        return;

    GfxDriverApi* driver = g_modelMgr->driver;

    if (model->indexBuffers) {
        for (int i = 0, c = model->m.numGeos; i < c; i++) {
            if (model->indexBuffers[i].isValid())
                driver->destroyIndexBuffer(model->indexBuffers[i]);
        }
    }

    if (model->vertexBuffers) {
        for (int i = 0, c = model->m.numGeos; i < c; i++) {
            if (model->vertexBuffers[i].isValid())
                driver->destroyVertexBuffer(model->vertexBuffers[i]);
        }
    }
}

static bool loadModel10(bx::MemoryReader* data, const t3dHeader& header, const ResourceTypeParams& params, uintptr_t* obj)
{
    assert(g_modelMgr);

    bx::Error err;

    GfxDriverApi* driver = g_modelMgr->driver;
    bx::AllocatorI* alloc = &g_modelMgr->allocStub;

    // Create model
    ModelImpl* model = BX_NEW(alloc, ModelImpl);
    model->m.nodes = (Model::Node*)BX_ALLOC(alloc, sizeof(Model::Node)*header.numNodes);
    model->m.geos = (Model::Geometry*)BX_ALLOC(alloc, sizeof(Model::Geometry)*header.numGeos);
    model->m.meshes = (Model::Mesh*)BX_ALLOC(alloc, sizeof(Model::Mesh)*header.numMeshes);
    
    model->m.numGeos = header.numGeos;
    model->m.numMeshes = header.numMeshes;
    model->m.numNodes = header.numNodes;
    model->m.rootMtx = mtx4x4Ident();

    // Nodes
    for (int i = 0; i < header.numNodes; i++) {
        t3dNode tnode;
        data->read(&tnode, sizeof(tnode), &err);

        Model::Node& node = model->m.nodes[i];
        strcpy(node.name, tnode.name);
        node.mesh = tnode.mesh;
        node.parent = tnode.parent;
        node.numChilds = tnode.numChilds;
        node.localMtx = mtx4x4fv3(&tnode.xformMtx[0], &tnode.xformMtx[3], &tnode.xformMtx[6], &tnode.xformMtx[9]);
        
        if (tnode.numChilds) {
            node.childs = (int*)BX_ALLOC(alloc, sizeof(int)*tnode.numChilds);
            data->read(node.childs, sizeof(int)*tnode.numChilds, &err);
        } else {
            node.childs = nullptr;
        }
    }

    // Meshes
    for (int i = 0; i < header.numMeshes; i++) {
        t3dMesh tmesh;
        data->read(&tmesh, sizeof(tmesh), &err);

        Model::Mesh& mesh = model->m.meshes[i];
        mesh.numSubmeshes = tmesh.numSubmeshes;
        mesh.geo = tmesh.geo;

        mesh.submeshes = (Model::Submesh*)BX_ALLOC(alloc, sizeof(Model::Submesh)*tmesh.numSubmeshes);
        
        for (int c = 0; c < mesh.numSubmeshes; c++) {
            t3dSubmesh tsubmesh;
            data->read(&tsubmesh, sizeof(tsubmesh), &err);

            Model::Submesh& submesh = mesh.submeshes[c];
            submesh.mtl = tsubmesh.mtl;
            submesh.numIndices = tsubmesh.numIndices;
            submesh.startIndex = tsubmesh.startIndex;
        }
    }

    // Geos
    for (int i = 0; i < header.numGeos; i++) {
        t3dGeometry tgeo;
        data->read(&tgeo, sizeof(tgeo), &err);

        Model::Geometry& geo = model->m.geos[i];
        geo.numIndices = tgeo.numTris * 3;
        geo.numVerts = tgeo.numVerts;
        
        if (tgeo.skel.numJoints) {
            geo.skel = (Model::Skeleton*)BX_ALLOC(alloc, sizeof(Model::Skeleton));

            geo.skel->joints = (Model::Joint*)BX_ALLOC(alloc, sizeof(Model::Joint)*tgeo.skel.numJoints);
            geo.skel->initPose = (mtx4x4_t*)BX_ALLOC(alloc, sizeof(mtx4x4_t)*tgeo.skel.numJoints);

            geo.skel->numJoints = tgeo.skel.numJoints;
            geo.skel->rootMtx = mtx4x4fv3(&tgeo.skel.rootMtx[0], &tgeo.skel.rootMtx[3], &tgeo.skel.rootMtx[6],
                                          &tgeo.skel.rootMtx[9]);

            for (int c = 0; c < tgeo.skel.numJoints; c++) {
                t3dJoint tjoint;
                data->read(&tjoint, sizeof(tjoint), &err);
                Model::Joint& joint = geo.skel->joints[c];
                strcpy(joint.name, tjoint.name);
                joint.parent = tjoint.parent;
                joint.offsetMtx = mtx4x4fv3(&tjoint.offsetMtx[0], &tjoint.offsetMtx[3], &tjoint.offsetMtx[6],
                                            &tjoint.offsetMtx[9]);
            }

            for (int c = 0; c < tgeo.skel.numJoints; c++) {
                float mtx[12];
                data->read(mtx, sizeof(float) * 12, &err);
                geo.skel->initPose[c] = mtx4x4fv3(&mtx[0], &mtx[3], &mtx[6], &mtx[9]);
            }
        } // skeleton

        // Vertex Decl
        vdeclBegin(&geo.vdecl);
        for (int c = 0; c < tgeo.numAttribs; c++) {
            t3dVertexAttrib::Enum tatt;
            data->read(&tatt, sizeof(tatt), &err);
            VertexAttrib::Enum att = (VertexAttrib::Enum)tatt;
            int num;
            VertexAttribType::Enum type;
            bool normalized = false;

            switch (att) {
            case VertexAttrib::Position:
            case VertexAttrib::Normal:
            case VertexAttrib::Tangent:
            case VertexAttrib::Bitangent:
                num = 3;
                type = VertexAttribType::Float;
                break;
            case VertexAttrib::Color0:
                num = 4;
                type = VertexAttribType::Uint8;
                normalized = true;
                break;
            case VertexAttrib::TexCoord1:
            case VertexAttrib::TexCoord0:
            case VertexAttrib::TexCoord2:
            case VertexAttrib::TexCoord3:
                num = 2;
                type = VertexAttribType::Float;
                break;
            case VertexAttrib::Indices:
                num = 4;
                type = VertexAttribType::Uint8;
                break;
            case VertexAttrib::Weight:
                num = 4;
                type = VertexAttribType::Float;
                break;
            default:
                num = 0;
                type = VertexAttribType::Count;
                break;
            }

            if (num) {
                vdeclAdd(&geo.vdecl, att, num, type, normalized);
            }
        }
        vdeclEnd(&geo.vdecl);

        // Indices
        geo.indices = (uint16_t*)BX_ALLOC(alloc, sizeof(uint16_t)*geo.numIndices);
        data->read(geo.indices, sizeof(uint16_t)*geo.numIndices, &err);

        // Verts
        geo.verts = BX_ALLOC(alloc, tgeo.numVerts*geo.vdecl.stride);
        data->read(geo.verts, geo.vdecl.stride*tgeo.numVerts, &err);
    }

    // Create Gfx buffers
    model->vertexBuffers = (VertexBufferHandle*)BX_ALLOC(alloc, sizeof(VertexBufferHandle)*header.numGeos);
    model->indexBuffers = (IndexBufferHandle*)BX_ALLOC(alloc, sizeof(IndexBufferHandle)*header.numGeos);

    for (int i = 0; i < header.numGeos; i++) {
        model->vertexBuffers[i].reset();
        model->indexBuffers[i].reset();
    }

    for (int i = 0; i < header.numGeos; i++) {
        const Model::Geometry& geo = model->m.geos[i];
        model->vertexBuffers[i] = driver->createVertexBuffer(
            driver->makeRef(geo.verts, geo.numVerts*geo.vdecl.stride, nullptr, nullptr), geo.vdecl, GpuBufferFlag::None);
        model->indexBuffers[i] = driver->createIndexBuffer(
            driver->makeRef(geo.indices, sizeof(uint16_t)*geo.numIndices, nullptr, nullptr), GpuBufferFlag::None);
        if (!model->vertexBuffers[i].isValid() || !model->indexBuffers[i].isValid()) {
            unloadModel(model);
            return false;
        }
    }
    
    *obj = uintptr_t(model);
    return true;
}

bool ModelLoader::loadObj(const MemoryBlock* mem, const ResourceTypeParams& params, uintptr_t* obj, bx::AllocatorI* alloc)
{
    bx::Error err;
    bx::MemoryReader reader(mem->data, mem->size);

    // Read the header
    t3dHeader header;
    reader.read(&header, sizeof(header), &err);

    if (header.sign != T3D_SIGN) {
        T_ERROR("Load model failed: Invalid header");
        return false;
    }

    switch (header.version) {
    case T3D_VERSION_10:
        return loadModel10(&reader, header, params, obj);
    default:
        T_ERROR("Load model failed: Invalid version: 0x%x", header.version);
        return false;
    }
}

void ModelLoader::unloadObj(uintptr_t obj, bx::AllocatorI* alloc)
{
    assert(g_modelMgr);

    unloadModel((ModelImpl*)obj);
}

void ModelLoader::onReload(ResourceHandle handle, bx::AllocatorI* alloc)
{

}
