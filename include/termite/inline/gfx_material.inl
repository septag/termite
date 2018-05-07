#pragma once

#include "bx/debug.h"

namespace tee {
    namespace gfx {
        inline void beginMtlDecl(MaterialDecl* decl)
        {
            decl->count = 0;
        }

        inline int addMtlDeclAttrib(MaterialDecl* decl, const char* name, UniformType::Enum type, uint16_t num)
        {
            if (decl->count < MAX_MATERIAL_VARS) {
                BX_ASSERT(name, "");

                int index = decl->count;
                decl->names[index] = name;
                decl->types[index] = type;
                decl->arrayCounts[index] = num;
                decl->initTypes[index] = MaterialDecl::InitTypeNone;
                ++decl->count;
                return index;
            } else {
                BX_ASSERT(false, "Material Vars should not exceed %d", MAX_MATERIAL_VARS);
                return -1;
            }
        }

        inline void endMtlDecl(MaterialDecl* decl)
        {
        }

        inline void setMtlDeclInitData(MaterialDecl* decl, int index, const vec4_t& v)
        {
            BX_ASSERT(index >= 0 && index < decl->count, "out of bounds index");
            decl->initTypes[index] = MaterialDecl::InitTypeVector;
            decl->initData[index].v = v;
        }

        inline void setMtlDeclInitData(MaterialDecl* decl, int index, AssetHandle aHandle)
        {
            BX_ASSERT(index >= 0 && index < decl->count, "out of bounds index");
            decl->initTypes[index] = MaterialDecl::InitTypeTextureResource;
            decl->initData[index].t = aHandle;
        }


        inline void setMtlDeclInitData(MaterialDecl* decl, int index, TextureHandle tHandle)
        {
            BX_ASSERT(index >= 0 && index < decl->count, "out of bounds index");
            decl->initTypes[index] = MaterialDecl::InitTypeTextureHandle;
            decl->initData[index].th = tHandle;
        }

        inline int findMtlAttrib(const MaterialDecl* decl, const char* name)
        {
            for (int i = 0, c = decl->count; i < c; i++) {
                if (bx::strCmp(decl->names[i], name) != 0)
                    continue;
                return i;
            }
            return -1;
        }
    }
}