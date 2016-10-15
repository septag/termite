#include "pch.h"
#include "gfx_material.h"

#include "bxx/array.h"
#include "bxx/hash_table.h"
#include "bxx/indexed_pool.h"

using namespace termite;

struct MaterialUniform
{
    char name[32];
    UniformHandle handle;
};

struct Material
{
    MaterialUniform* uniforms;
};

namespace termite
{
    struct MaterialLib
    {
        bx::IndexedPool materials;
    };

    MaterialDecl::MaterialDecl()
    {
        memset(m_vars, 0x00, sizeof(m_vars));
        m_count = 0;
    }

    MaterialDecl& MaterialDecl::begin()
    {
        m_count = 0;
        return *this;
    }

    MaterialDecl& MaterialDecl::add(const char* name, UniformType type, int num)
    {
        assert(num >= 1);

        if (m_count < MAX_MATERIAL_VARS) {
            Var& v = m_vars[m_count++];
            bx::strlcpy(v.name, name, sizeof(name));
            v.type = type;
            v.num = num;
        }

        return *this;
    }

    void MaterialDecl::end()
    {
    }

} // namespace termite

MaterialLib* termite::createMaterialLib(bx::AllocatorI* alloc, GfxDriverApi* driver)
{
    return nullptr;
}

void termite::destroyMaterialLib(MaterialLib* lib)
{

}

MaterialHandle termite::createMaterial(MaterialLib* lib)
{
    return MaterialHandle();
}

void termite::destroyMaterial(MaterialLib* lib, MaterialHandle handle)
{

}
