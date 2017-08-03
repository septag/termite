#pragma once

#include "bx/bx.h"

#include "resource_lib.h"

namespace termite
{
    struct Lang;

    TERMITE_API const char* getText(Lang* lang, const char* strId);

    void registerLangToResourceLib();    
}

