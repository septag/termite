#pragma once

#include "types.h"
#include "gfx_defines.h"

#define MAX_MATERIAL_VARS 32

namespace termite
{
    struct MaterialLib;
    struct GfxDriverApi;

    struct MaterialT {};
    typedef PhantomType<uint16_t, MaterialT, UINT16_MAX> MaterialHandle;

    class MaterialDecl
    {
    public:
        struct Var
        {
            char name[32];
            UniformType type;
            int num;
        };

    public:
        MaterialDecl();
        
        MaterialDecl& begin();
        MaterialDecl& add(const char* name, UniformType type, int num = 1);
        void end();

    private:
        Var m_vars[MAX_MATERIAL_VARS];
        int m_count;
    };

    TERMITE_API MaterialLib* createMaterialLib(bx::AllocatorI* alloc, GfxDriverApi* driver);
    TERMITE_API void destroyMaterialLib(MaterialLib* lib);

    TERMITE_API MaterialHandle createMaterial(MaterialLib* lib);
    TERMITE_API void destroyMaterial(MaterialLib* lib, MaterialHandle handle);
} // namespace termite
