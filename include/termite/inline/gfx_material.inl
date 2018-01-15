#pragma once

namespace tee {
    namespace gfx {
        inline void beginMtlDecl(MaterialDecl* decl)
        {
            decl->count = 0;
        }

        inline void addMtlDeclAttrib(MaterialDecl* decl, const char* name, UniformType::Enum type, uint16_t num)
        {
            if (decl->count < MAX_MATERIAL_VARS) {
                BX_ASSERT(name, "");

                int index = decl->count;
                decl->names[index] = name;
                decl->types[index] = type;
                decl->arrayCounts[index] = num;
                decl->initValues[index] = false;
                ++decl->count;
            } else {
                BX_ASSERT(false, "Material Vars should not exceed %d", MAX_MATERIAL_VARS);
            }
        }

        inline void endMtlDecl(MaterialDecl* decl)
        {
        }

        inline void setMtlDeclInitData(MaterialDecl* decl, const vec4_t& v)
        {
            int index = decl->count - 1;
            BX_ASSERT(index >= 0, "First add an attrib, then set data");
            decl->initValues[index] = true;
            decl->initData[index].v = v;
        }

        inline void setMtlDeclInitData(MaterialDecl* decl, AssetHandle aHandle)
        {
            int index = decl->count - 1;
            BX_ASSERT(index >= 0, "First add an attrib, then set data");
            decl->initValues[index] = true;
            decl->initData[index].t = aHandle;
        }
    }
}