#include "bxx/uuid.h"
#include "bx/string.h"

namespace bx {
#if BX_PLATFORM_WINDOWS || BX_PLATFORM_WINRT || BX_PLATFORM_XBOXONE
#   pragma comment(lib, "Rpcrt4.lib")
#   include <rpc.h>
    void generateUUID(char uuidStr[37])
    {
        UUID guid;
        UuidCreate(&guid);

        bx::snprintf(uuidStr, 37, "%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
                     guid.Data1, guid.Data2, guid.Data3,
                     guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
                     guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    }
#elif BX_PLATFORM_OSX || BX_PLATFORM_IOS
#   include <CoreFoundation/CFUUID.h>
    void generateUUID(char uuidStr[37])
    {
        auto guid = CFUUIDCreate(nullptr);
        auto bytes = CFUUIDGetUUIDBytes(guid);
        CFRelease(guid);

        bx::snprintf(uuidStr, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                     bytes.byte0, bytes.byte1, bytes.byte2, bytes.byte3, bytes.byte4, bytes.byte5, bytes.byte6,
                     bytes.byte7, bytes.byte8, bytes.byte9, bytes.byte10, bytes.byte11, bytes.byte12, bytes.byte13,
                     bytes.byte14, bytes.byte15);
    }
#elif BX_PLATFORM_LINUX
#   include <uuid/uuid.h>
    void generateUUID(char uuidStr[37])
    {
        uuid_t uid;
        uuid_generate(uid);
        uuid_unparse(uid, uuidStr);
    }
#elif BX_PLATFORM_ANDROID
	#include <stdio.h>
	#include <string.h>

    void generateUUID(char uuidStr[37])
    {
    	FILE* f = fopen("/proc/sys/kernel/random/uuid", "rt");
    	if (f) {
			char uuid[40];
			fgets(uuid, sizeof(uuid), f);
			fclose(f);
			char* nl = strchr(uuid, '\n');
			if (nl)
				*nl = 0;
			bx::strCopy(uuidStr, 37, uuid);
		} else {
			uuidStr[0] = 0;
		}
	}
#else
#  error "Not Implemented"
#endif

}
