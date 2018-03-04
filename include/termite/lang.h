#pragma once

#include "bx/bx.h"

namespace tee
{
    struct Lang;

    namespace lang {
        TEE_API const char* getText(Lang* lang, const char* strId, float* pScale = nullptr);
    }
}

