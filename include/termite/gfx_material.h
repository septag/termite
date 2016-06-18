#pragma once

#include "gfx_defines.h"

#define MAX_MATERIAL_VARS 32

namespace termite
{
    struct mtlLibrary;
    class gfxDriverI;

    T_HANDLE(mtlHandle);

    class mtlDecl
    {
    public:
        struct Var
        {
            char name[32];
            gfxUniformType type;
            int num;
        };

    public:
        mtlDecl();
        
        mtlDecl& begin();
        mtlDecl& add(const char* name, gfxUniformType type, int num = 1);
        void end();

    private:
        Var m_vars[MAX_MATERIAL_VARS];
        int m_count;
    };

    TERMITE_API mtlLibrary* mtlCreateLibrary(bx::AllocatorI* alloc, gfxDriverI* driver);
    TERMITE_API void mtlDestroyLibrary(mtlLibrary* lib);

    TERMITE_API mtlHandle mtlCreate(mtlLibrary* lib);
    TERMITE_API void mtlDestroy(mtlLibrary* lib, mtlHandle handle);
} // namespace termite
