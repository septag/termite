#include "pch.h"
#include "gfx_material.h"
#include "gfx_driver.h"
#include "gfx_texture.h"

#include "bxx/array.h"
#include "bxx/hash_table.h"
#include "bxx/handle_pool.h"

#include "tinystl/hash.h"

#include "internal.h"

#define MAX_MATERIAL_ITEMS 256

namespace tee {
    struct MaterialLib;

    struct MaterialTexture
    {
        TextureFlag::Bits flags;
        AssetHandle texHandle;
        uint8_t stage;
    };

    struct Material
    {
        MaterialLib* mtlLib;                       // Owner material lib
        bx::AllocatorI* alloc;
        ProgramHandle prog;
        size_t nameHashes[MAX_MATERIAL_VARS];
        uint32_t offsets[MAX_MATERIAL_VARS];       // Offsets to data buffer for each value
        int uniformIds[MAX_MATERIAL_VARS];        // index to MaterialLib->uniforms
        uint32_t dataSize;
        uint32_t dataHash;                         // Hash of all data inside the material
        int numValues;      
        uint8_t* data;                             // All material data
    };

    struct MaterialUniform
    {
        UniformHandle handle;
        UniformType::Enum type;
        uint16_t num;
    };

    struct MaterialLib
    {
        bx::AllocatorI* alloc;
        bx::HandlePool mtls;
        GfxDriver* driver;

        bx::Array<size_t> uniformNameHashes;
        bx::Array<MaterialUniform> uniforms;
        bx::Array<bx::String32> uniformNames;
        int numUniforms;

        MaterialLib(bx::AllocatorI* _alloc) : alloc(_alloc)
        {
            driver = nullptr;
            numUniforms = 0;
        }
    };

    static MaterialLib* gMtlLib = nullptr;

    bool gfx::initMaterialLib(bx::AllocatorI* alloc, GfxDriver* driver)
    {
        BX_ASSERT(!gMtlLib, "MaterialLib is initialized previously");

        gMtlLib = BX_NEW(alloc, MaterialLib)(alloc);
        if (!gMtlLib)
            return false;
        MaterialLib* lib = gMtlLib;
        lib->driver = driver;

        uint32_t itemSz[] = { sizeof(Material) };
        if (!lib->mtls.create(itemSz, BX_COUNTOF(itemSz), MAX_MATERIAL_ITEMS, MAX_MATERIAL_ITEMS, alloc)) {
            return false;
        }

        if (!lib->uniformNameHashes.create(32, 64, alloc) ||
            !lib->uniforms.create(32, 64, alloc) ||
            !lib->uniformNames.create(32, 64, alloc)) 
        {
            return false;
        }

        return true;
    }

    void gfx::shutdownMaterialLib()
    {
        if (!gMtlLib)
            return;
        MaterialLib* lib = gMtlLib;

        if (lib->alloc) {
            lib->uniformNameHashes.destroy();
            lib->uniforms.destroy();
            lib->uniformNames.destroy();

            lib->mtls.destroy();

            BX_DELETE(lib->alloc, gMtlLib);
        }
        gMtlLib = nullptr;
    }

    MaterialHandle gfx::createMaterial(ProgramHandle prog, const MaterialDecl& decl, bx::AllocatorI* dataAlloc)
    {
        auto getUniformSize = [](UniformType::Enum type, int num)->uint32_t {
            switch (type) {
            case UniformType::Int1:
                // Int type is actually texture sampler
                return sizeof(MaterialUniform);
            case UniformType::Vec4:
                return sizeof(vec4_t)*num;
            case UniformType::Mat3:
                return sizeof(float)*num*9;
            case UniformType::Mat4:
                return sizeof(mat4_t)*num;
            default:
                return 0;
            }
        };
        MaterialLib* lib = gMtlLib;
        bx::AllocatorI* alloc = dataAlloc ? dataAlloc : lib->alloc;

        MaterialHandle handle = MaterialHandle(lib->mtls.newHandle());
        if (!handle.isValid())
            return handle;

        Material* mtl = lib->mtls.getHandleData<Material>(0, handle);
        memset(mtl, 0x00, sizeof(Material));
        mtl->mtlLib = lib;
        mtl->alloc = alloc;
        mtl->prog = prog;
        mtl->numValues = decl.count;

        uint32_t dataOffset = 0;

        // GPU binding 
        // Check decls and add new uniform values if required
        for (int i = 0, ic = decl.count; i < ic; i++) {
            size_t hash = tinystl::hash_string(decl.names[i], strlen(decl.names[i]));
            int foundIdx = -1;
            for (int k = 0, kc = lib->numUniforms; k < kc; k++) {
                if (lib->uniformNameHashes[k] != hash)
                    continue;
                foundIdx = k;
                break;
            }

            if (foundIdx != -1) {
                mtl->uniformIds[i] = foundIdx;
            } else {
                // Create new uniform
                int numUniforms = ++lib->numUniforms;

                *lib->uniformNameHashes.push() = hash;
                MaterialUniform* mu = lib->uniforms.push();
                *lib->uniformNames.push() = decl.names[i];

                mu->handle = lib->driver->createUniform(decl.names[i], decl.types[i], decl.arrayCounts[i]);
                mu->type = decl.types[i];
                mu->num = decl.arrayCounts[i];

                mtl->uniformIds[i] = numUniforms - 1;
            }

            // Calculate data size needed to hold all uniform data
            mtl->nameHashes[i] = hash;

            uint32_t uniformSize = getUniformSize(decl.types[i], decl.arrayCounts[i]);
            mtl->offsets[i] = dataOffset;
            dataOffset += uniformSize;
            mtl->dataSize += uniformSize;
        }

        // Data creation and binding
        mtl->data = (uint8_t*)BX_ALLOC(alloc, mtl->dataSize);
        bx::memSet(mtl->data, 0x00, mtl->dataSize);

        // Set initial values
        uint8_t stageId = 0;
        for (int i = 0; i < decl.count; i++) {
            if (decl.initValues[i]) {
                if (decl.types[i] == UniformType::Vec4)
                    gfx::setMtlValue(handle, decl.names[i], decl.initData[i].v);
                else if (decl.types[i] == UniformType::Int1)
                    gfx::setMtlTexture(handle, decl.names[i], stageId++, decl.initData[i].t);
            }
        }

        return handle;
    }

    void gfx::destroyMaterial(MaterialHandle handle)
    {
        MaterialLib* lib = gMtlLib;
        Material* mtl = lib->mtls.getHandleData<Material>(0, handle);

        bx::AllocatorI* alloc = mtl->alloc;
        if (mtl->data)
            BX_FREE(alloc, mtl->data);
       
        lib->mtls.freeHandle(handle);
    }

    void gfx::destroyMaterialUniforms()
    {
        MaterialLib* lib = gMtlLib;
        GfxDriver* gDriver = gMtlLib->driver;

        for (int i = 0; i < lib->numUniforms; i++) {
            UniformHandle& handle = lib->uniforms[i].handle;
            if (handle.isValid()) {
                gDriver->destroyUniform(handle);
                handle = UniformHandle();
            }
        }
    }

    bool gfx::createMaterialUniforms(GfxDriver* driver)
    {
        MaterialLib* lib = gMtlLib;
        lib->driver = driver;
        
        // Recreate all uniform handles
        for (int i = 0, c = lib->numUniforms; i < c; i++) {
            BX_ASSERT(!lib->uniforms[i].handle.isValid(), "");

            lib->uniforms[i].handle = driver->createUniform(lib->uniformNames[i].cstr(), 
                                                            lib->uniforms[i].type,
                                                            lib->uniforms[i].num);
            if (!lib->uniforms[i].handle.isValid())
                return false;
        }

        return true;
    }

    void gfx::applyMaterial(MaterialHandle handle)
    {
        MaterialLib* lib = gMtlLib;
        Material* mtl = lib->mtls.getHandleData<Material>(0, handle);

        // Apply all uniform values
        GfxDriver* gDriver = lib->driver;
        for (int i = 0, c = mtl->numValues; i < c; i++) {
            if (lib->uniforms[i].type != UniformType::Int1) {
                gDriver->setUniform(lib->uniforms[mtl->uniformIds[i]].handle,
                                    mtl->data + mtl->offsets[i],
                                    lib->uniforms[i].num);
            } else {
                MaterialTexture* mt = (MaterialTexture*)(mtl->data + mtl->offsets[i]);
                gDriver->setTexture(mt->stage, lib->uniforms[mtl->uniformIds[i]].handle, 
                                    asset::getObjPtr<Texture>(mt->texHandle)->handle, mt->flags);
            }                                    
        }
    }

    static int findMtlValue(Material* mtl, const char* name)
    {
        size_t hash = tinystl::hash_string(name, strlen(name));
        for (int i = 0, c = mtl->numValues; i < c; i++) {
            if (hash != mtl->nameHashes[i])
                continue;
            return i;
        }
        return -1;
    }

    void gfx::setMtlValue(MaterialHandle handle, const char* name, const vec4_t& v)
    {
        MaterialLib* lib = gMtlLib;
        Material* mtl = lib->mtls.getHandleData<Material>(0, handle);

        int index = findMtlValue(mtl, name);
        if (index != -1) {
            BX_ASSERT(lib->uniforms[mtl->uniformIds[index]].type == UniformType::Vec4, "Type is invalid");
            BX_ASSERT(lib->uniforms[mtl->uniformIds[index]].num == 1, "Array count should be %d", lib->uniforms[mtl->uniformIds[index]].num);
            vec4_t* dest = (vec4_t*)(mtl->data + mtl->offsets[index]);
            *dest = v;
        }
    }

    void gfx::setMtlValue(MaterialHandle handle, const char* name, const mat3_t* mats, uint16_t num)
    {
        MaterialLib* lib = gMtlLib;
        Material* mtl = lib->mtls.getHandleData<Material>(0, handle);

        int index = findMtlValue(mtl, name);
        if (index != -1) {
            BX_ASSERT(lib->uniforms[mtl->uniformIds[index]].type == UniformType::Mat3, "Type is invalid");
            BX_ASSERT(lib->uniforms[mtl->uniformIds[index]].num == num, "Array count should be %d", lib->uniforms[mtl->uniformIds[index]].num);
            bx::memCopy(mtl->data + mtl->offsets[index], mats, sizeof(mat3_t)*num);
        }
    }

    void gfx::setMtlValue(MaterialHandle handle, const char* name, const mat3_t& mat)
    {
        MaterialLib* lib = gMtlLib;
        Material* mtl = lib->mtls.getHandleData<Material>(0, handle);

        int index = findMtlValue(mtl, name);
        if (index != -1) {
            BX_ASSERT(lib->uniforms[mtl->uniformIds[index]].type == UniformType::Mat3, "Type is invalid");
            BX_ASSERT(lib->uniforms[mtl->uniformIds[index]].num == 1, "Array count should be %d", lib->uniforms[mtl->uniformIds[index]].num);

            bx::memCopy(mtl->data + mtl->offsets[index], mat.f, sizeof(mat3_t));
        }
    }

    void gfx::setMtlValue(MaterialHandle handle, const char* name, const mat4_t* mats, uint16_t num)
    {
        MaterialLib* lib = gMtlLib;
        Material* mtl = lib->mtls.getHandleData<Material>(0, handle);

        int index = findMtlValue(mtl, name);
        if (index != -1) {
            BX_ASSERT(lib->uniforms[mtl->uniformIds[index]].type == UniformType::Mat4, "Type is invalid");
            BX_ASSERT(lib->uniforms[mtl->uniformIds[index]].num == num, "Array count should be %d", lib->uniforms[mtl->uniformIds[index]].num);

            bx::memCopy(mtl->data + mtl->offsets[index], mats, sizeof(mat4_t)*num);
        }
    }

    void gfx::setMtlValue(MaterialHandle handle, const char* name, const mat4_t& mat)
    {
        MaterialLib* lib = gMtlLib;
        Material* mtl = lib->mtls.getHandleData<Material>(0, handle);

        int index = findMtlValue(mtl, name);
        if (index != -1) {
            BX_ASSERT(lib->uniforms[mtl->uniformIds[index]].type == UniformType::Mat4, "Type is invalid");
            BX_ASSERT(lib->uniforms[mtl->uniformIds[index]].num == 1, "Array count should be %d", lib->uniforms[mtl->uniformIds[index]].num);

            bx::memCopy(mtl->data + mtl->offsets[index], mat.f, sizeof(mat4_t));
        }
    }

    void gfx::setMtlValue(MaterialHandle handle, const char* name, const vec4_t* vs, uint16_t num)
    {
        MaterialLib* lib = gMtlLib;
        Material* mtl = lib->mtls.getHandleData<Material>(0, handle);

        int index = findMtlValue(mtl, name);
        if (index != -1) {
            BX_ASSERT(lib->uniforms[mtl->uniformIds[index]].type == UniformType::Vec4, "Type is invalid");
            BX_ASSERT(lib->uniforms[mtl->uniformIds[index]].num == num, "Array count should be %d", lib->uniforms[mtl->uniformIds[index]].num);

            bx::memCopy(mtl->data + mtl->offsets[index], vs, sizeof(vec4_t)*num);
        }
    }

    void gfx::setMtlTexture(MaterialHandle handle, const char* name, uint8_t stage, AssetHandle texHandle, 
                            TextureFlag::Bits flags)
    {
        MaterialLib* lib = gMtlLib;
        Material* mtl = lib->mtls.getHandleData<Material>(0, handle);

        int index = findMtlValue(mtl, name);
        if (index != -1) {
            BX_ASSERT(lib->uniforms[mtl->uniformIds[index]].type == UniformType::Int1, "Type is invalid");

            MaterialTexture* mt = (MaterialTexture*)(mtl->data + mtl->offsets[index]);
            mt->texHandle = texHandle;
            mt->stage = stage;
            mt->flags = flags;
        }
    }
} // namespace tee

