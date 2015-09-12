#pragma once

#include "bx/bx.h"
#include <cstdlib>

namespace bx
{

#if _WIN32
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
#elif !__APPLE__ && !_WIN32
#   include <uuid/uuid.h>
static void generateUUID(char uuidStr[37])
{
    uuid_t uid;
    uuid_generate(uid);
    uuid_unparse(uid, uuidStr);
}
#elif __APPLE__
#	include <CoreFoundation/CFUUID.h>

static void generateUUID(char uuidStr[37])
{
	auto guid = CFUUIDCreate(nullptr);
	auto bytes = CFUUIDGetUUIDBytes(guid);
	CFRelease(guid);

	bx::snprintf(uuidStr, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		bytes.byte0, bytes.byte1, bytes.byte2, bytes.byte3, bytes.byte4, bytes.byte5, bytes.byte6,
		bytes.byte7, bytes.byte8, bytes.byte9, bytes.byte10, bytes.byte11, bytes.byte12, bytes.byte13,
		bytes.byte14, bytes.byte15);
}
#endif

}   // namespace: bx
