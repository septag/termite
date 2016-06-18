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
        gfxProgramHandle program;
    };
};

struct ModelImpl
{
    gfxModel m;
    gfxVertexBufferHandle* vertexBuffers;     // 1 for each geometry
    gfxIndexBufferHandle* indexBuffers;       // 1 for each geometry

    ModelImpl()
    {
        memset(&m, 0x00, sizeof(m));
        vertexBuffers = nullptr;
        indexBuffers = nullptr;
    }
};

class ModelLoader : public dsResourceCallbacksI
{
public:
    bool loadObj(const MemoryBlock* mem, const dsResourceTypeParams& params, uintptr_t* obj) override;
    void unloadObj(uintptr_t obj) override;
    void onReload(dsDataStore* ds, dsResourceHandle handle) override;
    uintptr_t getDefaultAsyncObj() override;
};

struct ModelManager
{
    bx::AllocatorI* alloc;
    gfxDriverI* driver;
    ModelLoader loader;

    memPageAllocator allocStub;

    ModelManager(bx::AllocatorI* _alloc) :
        allocStub(MODELS_MEMORY_TAG)
    {
        alloc = _alloc;
        driver = nullptr;
    }
};

static ModelManager* g_modelMgr = nullptr;

result_t termite::mdlInitLoader(gfxDriverI* driver, bx::AllocatorI* alloc)
{
    if (g_modelMgr) {
        assert(false);
        return T_ERR_ALREADY_INITIALIZED;
    }

    g_modelMgr = BX_NEW(alloc, ModelManager)(alloc);
    if (!g_modelMgr)
        return T_ERR_OUTOFMEM;

    g_modelMgr->driver = driver;

    return T_OK;
}

void termite::mdlShutdownLoader()
{
    if (!g_modelMgr)
        return;

    BX_DELETE(g_modelMgr->alloc, g_modelMgr);
    g_modelMgr = nullptr;
}

void termite::mdlRegisterToDatastore(dsDataStore* ds)
{
    dsResourceTypeHandle handle;
    handle = dsRegisterResourceType(ds, "model", &g_modelMgr->loader, sizeof(gfxModelLoadParams));
    assert(T_ISVALID(handle));
}

gfxModelInstance* termite::mdlCreateInstance(gfxModel* model)
{
    assert(g_modelMgr);

    return nullptr;
}

gfxVertexBufferHandle termite::mdlGetVertexBuffer(gfxModel* model, int index)
{
    return ((ModelImpl*)model)->vertexBuffers[index];
}

gfxIndexBufferHandle termite::mdlGetIndexBuffer(gfxModel* model, int index)
{
    return ((ModelImpl*)model)->indexBuffers[index];
}

static void unloadModel(ModelImpl* model)
{
    assert(g_modelMgr);
    gfxDriverI* driver = g_modelMgr->driver;

    if (model->indexBuffers) {
        for (int i = 0, c = model->m.numGeos; i < c; i++) {
            if (T_ISVALID(model->indexBuffers[i]))
                driver->destroyIndexBuffer(model->indexBuffers[i]);
        }
    }

    if (model->vertexBuffers) {
        for (int i = 0, c = model->m.numGeos; i < c; i++) {
            if (T_ISVALID(model->vertexBuffers[i]))
                driver->destroyVertexBuffer(model->vertexBuffers[i]);
        }
    }
}

static bool loadModel10(bx::MemoryReader* data, const t3dHeader& header, const dsResourceTypeParams& params, uintptr_t* obj)
{
    assert(g_modelMgr);

    bx::Error err;

    gfxDriverI* driver = g_modelMgr->driver;
    bx::AllocatorI* alloc = &g_modelMgr->allocStub;

    // Create model
    ModelImpl* model = BX_NEW(alloc, ModelImpl);
    model->m.nodes = (gfxModel::Node*)BX_ALLOC(alloc, sizeof(gfxModel::Node)*header.numNodes);
    model->m.geos = (gfxModel::Geometry*)BX_ALLOC(alloc, sizeof(gfxModel::Geometry)*header.numGeos);
    model->m.meshes = (gfxModel::Mesh*)BX_ALLOC(alloc, sizeof(gfxModel::Mesh)*header.numMeshes);
    
    model->m.numGeos = header.numGeos;
    model->m.numMeshes = header.numMeshes;
    model->m.numNodes = header.numNodes;
    model->m.rootMtx = mtx4x4Ident();

    // Nodes
    for (int i = 0; i < header.numNodes; i++) {
        t3dNode tnode;
        data->read(&tnode, sizeof(tnode), &err);

        gfxModel::Node& node = model->m.nodes[i];
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

        gfxModel::Mesh& mesh = model->m.meshes[i];
        mesh.numSubmeshes = tmesh.numSubmeshes;
        mesh.geo = tmesh.geo;

        mesh.submeshes = (gfxModel::Submesh*)BX_ALLOC(alloc, sizeof(gfxModel::Submesh)*tmesh.numSubmeshes);
        
        for (int c = 0; c < mesh.numSubmeshes; c++) {
            t3dSubmesh tsubmesh;
            data->read(&tsubmesh, sizeof(tsubmesh), &err);

            gfxModel::Submesh& submesh = mesh.submeshes[c];
            submesh.mtl = tsubmesh.mtl;
            submesh.numIndices = tsubmesh.numIndices;
            submesh.startIndex = tsubmesh.startIndex;
        }
    }

    // Geos
    for (int i = 0; i < header.numGeos; i++) {
        t3dGeometry tgeo;
        data->read(&tgeo, sizeof(tgeo), &err);

        gfxModel::Geometry& geo = model->m.geos[i];
        geo.numIndices = tgeo.numTris * 3;
        geo.numVerts = tgeo.numVerts;
        
        if (tgeo.skel.numJoints) {
            geo.skel = (gfxModel::Skeleton*)BX_ALLOC(alloc, sizeof(gfxModel::Skeleton));

            geo.skel->joints = (gfxModel::Joint*)BX_ALLOC(alloc, sizeof(gfxModel::Joint)*tgeo.skel.numJoints);
            geo.skel->initPose = (mtx4x4_t*)BX_ALLOC(alloc, sizeof(mtx4x4_t)*tgeo.skel.numJoints);

            geo.skel->numJoints = tgeo.skel.numJoints;
            geo.skel->rootMtx = mtx4x4fv3(&tgeo.skel.rootMtx[0], &tgeo.skel.rootMtx[3], &tgeo.skel.rootMtx[6],
                                          &tgeo.skel.rootMtx[9]);

            for (int c = 0; c < tgeo.skel.numJoints; c++) {
                t3dJoint tjoint;
                data->read(&tjoint, sizeof(tjoint), &err);
                gfxModel::Joint& joint = geo.skel->joints[c];
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
        geo.vdecl.begin();
        for (int c = 0; c < tgeo.numAttribs; c++) {
            t3dVertexAttrib tatt;
            data->read(&tatt, sizeof(tatt), &err);
            gfxAttrib att = (gfxAttrib)tatt;
            int num;
            gfxAttribType type;
            bool normalized = false;

            switch (att) {
            case gfxAttrib::Position:
            case gfxAttrib::Normal:
            case gfxAttrib::Tangent:
            case gfxAttrib::Bitangent:
                num = 3;
                type = gfxAttribType::Float;
                break;
            case gfxAttrib::Color0:
                num = 4;
                type = gfxAttribType::Uint8;
                normalized = true;
                break;
            case gfxAttrib::TexCoord1:
            case gfxAttrib::TexCoord0:
            case gfxAttrib::TexCoord2:
            case gfxAttrib::TexCoord3:
                num = 2;
                type = gfxAttribType::Float;
                break;
            case gfxAttrib::Indices:
                num = 4;
                type = gfxAttribType::Uint8;
                break;
            case gfxAttrib::Weight:
                num = 4;
                type = gfxAttribType::Float;
                break;
            default:
                num = 0;
                type = gfxAttribType::Count;
                break;
            }

            if (num)
                geo.vdecl.add(att, num, type, normalized);
        }
        geo.vdecl.end();

        // Indices
        geo.indices = (uint16_t*)BX_ALLOC(alloc, sizeof(uint16_t)*geo.numIndices);
        data->read(geo.indices, sizeof(uint16_t)*geo.numIndices, &err);

        // Verts
        geo.verts = BX_ALLOC(alloc, tgeo.numVerts*geo.vdecl.stride);
        data->read(geo.verts, geo.vdecl.stride*tgeo.numVerts, &err);
    }

    // Create Gfx buffers
    model->vertexBuffers = (gfxVertexBufferHandle*)BX_ALLOC(alloc, sizeof(gfxVertexBufferHandle)*header.numGeos);
    model->indexBuffers = (gfxIndexBufferHandle*)BX_ALLOC(alloc, sizeof(gfxIndexBufferHandle)*header.numGeos);

    for (int i = 0; i < header.numGeos; i++) {
        model->vertexBuffers[i] = T_INVALID_HANDLE;
        model->indexBuffers[i] = T_INVALID_HANDLE;
    }

    for (int i = 0; i < header.numGeos; i++) {
        const gfxModel::Geometry& geo = model->m.geos[i];
        model->vertexBuffers[i] = driver->createVertexBuffer(driver->makeRef(geo.verts, geo.numVerts*geo.vdecl.stride),
                                                             geo.vdecl);
        model->indexBuffers[i] = driver->createIndexBuffer(driver->makeRef(geo.indices, sizeof(uint16_t)*geo.numIndices));
        if (!T_ISVALID(model->vertexBuffers[i]) || !T_ISVALID(model->indexBuffers[i])) {
            unloadModel(model);
            return false;
        }
    }
    
    *obj = uintptr_t(model);
    return true;
}

bool ModelLoader::loadObj(const MemoryBlock* mem, const dsResourceTypeParams& params, uintptr_t* obj)
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

void ModelLoader::unloadObj(uintptr_t obj)
{
    assert(g_modelMgr);

    unloadModel((ModelImpl*)obj);
}

void ModelLoader::onReload(dsDataStore* ds, dsResourceHandle handle)
{

}

uintptr_t ModelLoader::getDefaultAsyncObj()
{
    return 0;
}
