#pragma once

#include "bx/bx.h"
#include <cstdlib>

namespace bx
{

#if BX_COMPILER_MSVC
#   include <rpc.h>
static void generateUUID(char uuidStr[37])
{
    UUID guid;
    UuidCreate(&guid);

    bx::snprintf(uuidStr, 37, "%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
                 guid.Data1, guid.Data2, guid.Data3,
                 guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
                 guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
}
#else
#   include <uuid/uuid.h>
static void generateUUID(char uuidStr[37])
{
    uuid_t uid;
    uuid_unparse(uid, uuidStr);
}
#endif

}   // namespace: bx
