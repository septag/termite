#include "pch.h"

#include "bx/readerwriter.h"
#include "bxx/array.h"
#include "bxx/linear_allocator.h"
#include "memory_pool.h"

#include "gfx_driver.h"
#include "gfx_model.h"
#include "internal.h"

#include "lz4/lz4.h"

#include "../include_common/t3d_format.h"

namespace tee {
    struct ModelInstanceImpl
    {
        struct Pose
        {
            int numJoints;
            mat4_t* mats;         // Final joint matrices
            mat4_t* offsetMats;   // offset matrices
            mat4_t* skinMats;     // Skinning matrices (mtx*offsetMtx)
        };

        ModelInstance i;
        bx::AllocatorI* alloc;
        void* buff; // Buffer allocated for the whole instance object
        Pose pose;
    };

    class ModelLoader : public AssetLibCallbacksI
    {
    public:
        bool loadObj(const MemoryBlock* mem, const AssetParams& params, uintptr_t* obj, bx::AllocatorI* alloc) override;
        void unloadObj(uintptr_t obj, bx::AllocatorI* alloc) override;
        void onReload(AssetHandle handle, bx::AllocatorI* alloc) override;
    };

    struct ModelManager
    {
        bx::AllocatorI* alloc;
        GfxDriver* driver;
        ModelLoader loader;

        ModelManager(bx::AllocatorI* _alloc)
        {
            alloc = _alloc;
            driver = nullptr;
        }
    };

    static ModelManager* gModelMgr = nullptr;

    bool gfx::initModelLoader(GfxDriver* driver, bx::AllocatorI* alloc)
    {
        if (gModelMgr) {
            assert(false);
            return false;
        }

        gModelMgr = BX_NEW(alloc, ModelManager)(alloc);
        if (!gModelMgr)
            return false;

        gModelMgr->driver = driver;

        return true;
    }

    void gfx::shutdownModelLoader()
    {
        if (!gModelMgr)
            return;

        BX_DELETE(gModelMgr->alloc, gModelMgr);
        gModelMgr = nullptr;
    }

    void gfx::registerModelToAssetLib()
    {
        AssetTypeHandle handle;
        handle = asset::registerType("model", &gModelMgr->loader, sizeof(LoadModelParams));
        assert(handle.isValid());
    }

    ModelInstance* gfx::createModelInstance(AssetHandle modelHandle, bx::AllocatorI* alloc)
    {
        BX_ASSERT(gModelMgr, "");
        BX_ASSERT(modelHandle.isValid(), "ModelHandle is not valid");

        GfxDriver* gDriver = getGfxDriver();

        Model* model = asset::getObjPtr<Model>(modelHandle);
        // Estimate final size needed for instance data
        size_t totalSz = sizeof(ModelInstanceImpl) +
            sizeof(VertexBufferHandle)*model->numGeos +
            sizeof(IndexBufferHandle)*model->numGeos +
            sizeof(MaterialHandle)*model->numMtls +
            bx::LinearAllocator::getExtraAllocSize(4);
        void* buff = BX_ALLOC(alloc, totalSz);
        if (!buff)
            return nullptr;
        bx::LinearAllocator lalloc(buff, totalSz);

        ModelInstanceImpl* inst = (ModelInstanceImpl*)BX_ALLOC(&lalloc, sizeof(ModelInstanceImpl));
        if (!inst)
            return nullptr;
        bx::memSet(inst, 0x00, sizeof(ModelInstanceImpl));

        inst->alloc = alloc;
        inst->buff = buff;
        inst->i.modelHandle = modelHandle;
        inst->i.vertexBuffers = (VertexBufferHandle*)BX_ALLOC(&lalloc, sizeof(VertexBufferHandle)*model->numGeos);
        inst->i.indexBuffers = (IndexBufferHandle*)BX_ALLOC(&lalloc, sizeof(IndexBufferHandle)*model->numGeos);
        if (!inst->i.vertexBuffers || !inst->i.indexBuffers) {
            destroyModelInstance(&inst->i);
            return nullptr;
        }

        for (int i = 0, c = model->numGeos; i < c; i++) {
            inst->i.vertexBuffers[i].reset();
            inst->i.indexBuffers[i].reset();
        }

        inst->i.mtls = (MaterialHandle*)BX_ALLOC(&lalloc, sizeof(MaterialHandle)*model->numMtls);
        if (!inst->i.mtls) {
            destroyModelInstance(&inst->i);
            return nullptr;
        }
        for (int i = 0, c = model->numMtls; i < c; i++)
            inst->i.mtls[i].reset();

        // TODO: create skeletal data
        
        // Create gfx objects and Fill data
        for (int i = 0, c = model->numGeos; i < c; i++) {
            const Model::Geometry& geo = model->geos[i];
            if (!model->vbIsDynamic) {
                BX_ASSERT(geo.vertexBuffer.isValid(), "VertexBuffer for static objects should be valid");
                inst->i.vertexBuffers[i] = geo.vertexBuffer;
            } else {
                // Create a new buffer from the vertex data, because dynamic vertex buffers are assumed to be changed
                inst->i.dynVertexBuffers[i] = gDriver->createDynamicVertexBufferMem(
                    gDriver->makeRef(geo.verts, gfx::getDeclSize(&geo.vdecl, geo.numVerts), nullptr, nullptr),
                        geo.vdecl, geo.vbFlags);
            }
            if (!inst->i.vertexBuffers[i].isValid()) {
                destroyModelInstance(&inst->i);
                return nullptr;
            }

            inst->i.indexBuffers[i] = geo.indexBuffer;
        }

        // Create gfx materials
        int mtlIdx = 0;
        for (int i = 0, c = model->numMtls; i < c; i++) {
            inst->i.mtls[i] = gfx::createMaterial(ProgramHandle(), model->mtls[i], alloc);
            if (!inst->i.mtls[i].isValid()) {
                destroyModelInstance(&inst->i);
                return nullptr;
            }
        }

        return &inst->i;
    }

    void gfx::destroyModelInstance(ModelInstance* inst)
    {
        ModelInstanceImpl* mi = (ModelInstanceImpl*)inst;
        BX_ASSERT(mi, "");

        if (mi->i.modelHandle.isValid()) {
            GfxDriver* gDriver = getGfxDriver();
            Model* model = asset::getObjPtr<Model>(mi->i.modelHandle);

            if (mi->i.dynVertexBuffers && model->vbIsDynamic) {
                for (int i = 0, c = model->numGeos; i < c; i++) {
                    if (mi->i.dynVertexBuffers[i].isValid())
                        gDriver->destroyDynamicVertexBuffer(mi->i.dynVertexBuffers[i]);
                }
            }

            for (int i = 0, c = model->numMtls; i < c; i++) {
                if (mi->i.mtls[i].isValid())
                    gfx::destroyMaterial(mi->i.mtls[i]);
            }
        }

        if (mi->alloc) {
            BX_FREE(mi->alloc, mi->buff);
        }
    }

    static void unloadModel(Model* model, bx::AllocatorI* alloc)
    {
        assert(gModelMgr);
        BX_ASSERT(alloc, "");

        if (!model)
            return;

        GfxDriver* gDriver = gModelMgr->driver;

        // 
        if (model->geos) {
            for (int i = 0; i < model->numGeos; i++) {
                if (model->geos[i].verts)
                    BX_FREE(alloc, model->geos[i].verts);
                if (model->geos[i].indices)
                    BX_FREE(alloc, model->geos[i].indices);

                if (model->geos[i].skel) {
                    if (model->geos[i].skel->joints)
                        BX_FREE(alloc, model->geos[i].skel->joints);
                    if (model->geos[i].skel->initPose)
                        BX_FREE(alloc, model->geos[i].skel->initPose);
                    BX_FREE(alloc, model->geos[i].skel);
                }

                if (model->geos[i].vertexBuffer.isValid()) {
                    gDriver->destroyVertexBuffer(model->geos[i].vertexBuffer);
                    model->geos[i].vertexBuffer.reset();
                }

                if (model->geos[i].indexBuffer.isValid()) {
                    gDriver->destroyIndexBuffer(model->geos[i].indexBuffer);
                    model->geos[i].indexBuffer.reset();
                }
            }

            BX_FREE(alloc, model->geos);
        }

        if (model->meshes) {
            for (int i = 0; i < model->numMeshes; i++) {
                if (model->meshes[i].submeshes)
                    BX_FREE(alloc, model->meshes[i].submeshes);
            }
            BX_FREE(alloc, model->meshes);
        }

        if (model->mtls)
            BX_FREE(alloc, model->mtls);

        if (model->nodes)
            BX_FREE(alloc, model->nodes);

        BX_DELETE(alloc, model);
    }

    static bool loadModel10(bx::MemoryReader* data, const t3dHeader& header, const AssetParams& params, uintptr_t* obj,
                            bx::AllocatorI* alloc)
    {
        BX_ASSERT(gModelMgr, "");
        BX_ASSERT(alloc, "");

        bx::Error err;
        LoadModelParams* mparams = (LoadModelParams*)params.userParams;
        GfxDriver* gDriver = gModelMgr->driver;

        // TODO: convert this to a single alloc call
        // Create model
        Model* model = BX_NEW(alloc, Model);
        model->nodes = (Model::Node*)BX_ALLOC(alloc, sizeof(Model::Node)*header.numNodes);
        model->geos = (Model::Geometry*)BX_ALLOC(alloc, sizeof(Model::Geometry)*header.numGeos);
        model->meshes = (Model::Mesh*)BX_ALLOC(alloc, sizeof(Model::Mesh)*header.numMeshes);

        model->numGeos = header.numGeos;
        model->numMeshes = header.numMeshes;
        model->numNodes = header.numNodes;
        model->rootMtx = mat4I();

        // Nodes
        for (int i = 0; i < header.numNodes; i++) {
            t3dNode tnode;
            data->read(&tnode, sizeof(tnode), &err);

            Model::Node& node = model->nodes[i];
            strcpy(node.name, tnode.name);
            node.mesh = tnode.mesh;
            node.parent = tnode.parent;
            node.numChilds = tnode.numChilds;
            node.localMtx = mat4f3(&tnode.xformMtx[0], &tnode.xformMtx[3], &tnode.xformMtx[6], &tnode.xformMtx[9]);

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

            Model::Mesh& mesh = model->meshes[i];
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

        model->vbIsDynamic = (mparams->vbType == LoadModelParams::DynamicVb);
        // Geos
        for (int i = 0; i < header.numGeos; i++) {
            t3dGeometry tgeo;
            data->read(&tgeo, sizeof(tgeo), &err);

            Model::Geometry& geo = model->geos[i];
            geo.numIndices = tgeo.numTris * 3;
            geo.numVerts = tgeo.numVerts;
            geo.vertexBuffer.reset();
            geo.indexBuffer.reset();

            if (tgeo.skel.numJoints) {
                geo.skel = (Model::Skeleton*)BX_ALLOC(alloc, sizeof(Model::Skeleton));

                geo.skel->joints = (Model::Joint*)BX_ALLOC(alloc, sizeof(Model::Joint)*tgeo.skel.numJoints);
                geo.skel->initPose = (mat4_t*)BX_ALLOC(alloc, sizeof(mat4_t)*tgeo.skel.numJoints);

                geo.skel->numJoints = tgeo.skel.numJoints;
                geo.skel->rootMtx = mat4f3(&tgeo.skel.rootMtx[0], &tgeo.skel.rootMtx[3], &tgeo.skel.rootMtx[6],
                                              &tgeo.skel.rootMtx[9]);

                for (int c = 0; c < tgeo.skel.numJoints; c++) {
                    t3dJoint tjoint;
                    data->read(&tjoint, sizeof(tjoint), &err);
                    Model::Joint& joint = geo.skel->joints[c];
                    strcpy(joint.name, tjoint.name);
                    joint.parent = tjoint.parent;
                    joint.offsetMtx = mat4f3(&tjoint.offsetMtx[0], &tjoint.offsetMtx[3], &tjoint.offsetMtx[6],
                                                &tjoint.offsetMtx[9]);
                }

                for (int c = 0; c < tgeo.skel.numJoints; c++) {
                    float mtx[12];
                    data->read(mtx, sizeof(float) * 12, &err);
                    geo.skel->initPose[c] = mat4f3(&mtx[0], &mtx[3], &mtx[6], &mtx[9]);
                }
            } // skeleton

            // Vertex Decl
            t3dVertexAttrib::Enum attribs[t3dVertexAttrib::Count];
            data->read(attribs, sizeof(t3dVertexAttrib::Enum) * tgeo.numAttribs, &err);

            gfx::beginDecl(&geo.vdecl);
            for (int c = 0; c < tgeo.numAttribs; c++) {
                VertexAttrib::Enum att = (VertexAttrib::Enum)attribs[c];
                int num = 0;
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
                    gfx::addAttrib(&geo.vdecl, att, num, type, normalized);
                }
            }
            gfx::endDecl(&geo.vdecl);

            // Indices
            geo.indices = (uint16_t*)BX_ALLOC(alloc, sizeof(uint16_t)*geo.numIndices);
            data->read(geo.indices, sizeof(uint16_t)*geo.numIndices, &err);

            // Verts
            geo.verts = BX_ALLOC(alloc, tgeo.numVerts*geo.vdecl.stride);
            data->read(geo.verts, geo.vdecl.stride*tgeo.numVerts, &err);

            // For now IndexBuffers are always uint16_t
            geo.vbFlags = GfxBufferFlag::None;
            geo.ibFlags = GfxBufferFlag::None;

            // Gpu Buffers
            if (!model->vbIsDynamic) {
                geo.vertexBuffer = gDriver->createVertexBuffer(
                    gDriver->makeRef(geo.verts, gfx::getDeclSize(&geo.vdecl, geo.numVerts), nullptr, nullptr), 
                                     geo.vdecl, geo.vbFlags);
            }

            geo.indexBuffer = gDriver->createIndexBuffer(
                gDriver->makeRef(geo.indices, sizeof(uint16_t)*geo.numIndices, nullptr, nullptr), geo.ibFlags);
            if (!geo.vertexBuffer.isValid() || !geo.indexBuffer.isValid()) {
                unloadModel(model, alloc);
                return false;
            }
        }

        // TODO: Materials


        *obj = uintptr_t(model);
        return true;
    }

    bool ModelLoader::loadObj(const MemoryBlock* mem, const AssetParams& params, uintptr_t* obj, bx::AllocatorI* alloc)
    {
        bx::Error err;
        bx::MemoryReader reader(mem->data, mem->size);

        if (!alloc)
            alloc = gModelMgr->alloc;

        // Read the header
        t3dHeader header;
        reader.read(&header, sizeof(header), &err);

        if (header.sign != T3D_SIGN) {
            TEE_ERROR("Load model failed: Invalid header");
            return false;
        }

        switch (header.version) {
        case T3D_VERSION_10:
            return loadModel10(&reader, header, params, obj, alloc);
        default:
            TEE_ERROR("Load model failed: Invalid version: 0x%x", header.version);
            return false;
        }
    }

    void ModelLoader::unloadObj(uintptr_t obj, bx::AllocatorI* alloc)
    {
        assert(gModelMgr);
        unloadModel((Model*)obj, alloc ? alloc : gModelMgr->alloc);
    }

    void ModelLoader::onReload(AssetHandle handle, bx::AllocatorI* alloc)
    {

    }
} // namespace tee