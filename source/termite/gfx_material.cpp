#include "pch.h"
#include "gfx_material.h"

#include "bxx/array.h"
#include "bxx/hash_table.h"

using namespace termite;

struct MaterialUniform
{
    char name[32];
    gfxUniformHandle handle;
};

struct Material
{
    MaterialUniform* uniforms;
};

namespace termite
{
    struct mtlLibrary
    {
        bx::ArrayWithPop<Material> mtls;
    };

    mtlDecl::mtlDecl()
    {
        memset(m_vars, 0x00, sizeof(m_vars));
        m_count = 0;
    }

    mtlDecl& mtlDecl::begin()
    {
        m_count = 0;
        return *this;
    }

    mtlDecl& mtlDecl::add(const char* name, gfxUniformType type, int num)
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

    void mtlDecl::end()
    {
    }

} // namespace termite

mtlLibrary* termite::mtlCreateLibrary(bx::AllocatorI* alloc, gfxDriverI* driver)
{
    return nullptr;
}

void termite::mtlDestroyLibrary(mtlLibrary* lib)
{

}

mtlHandle termite::mtlCreate(mtlLibrary* lib)
{
    return T_INVALID_HANDLE;
}

void termite::mtlDestroy(mtlLibrary* lib, mtlHandle handle)
{

}
