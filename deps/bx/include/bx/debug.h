/*
 * Copyright 2010-2017 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bx#license-bsd-2-clause
 */

#ifndef BX_DEBUG_H_HEADER_GUARD
#define BX_DEBUG_H_HEADER_GUARD

#include "bx.h"

namespace bx
{
	///
	void debugBreak();

	///
	void debugOutput(const char* _out);

	///
	void debugPrintfVargs(const char* _format, va_list _argList);

	///
	void debugPrintf(const char* _format, ...);

	///
	void debugPrintfData(const void* _data, uint32_t _size, const char* _format, ...);

	///
	struct WriterI* getDebugOut();

} // namespace bx

#if BX_CONFIG_CHECK_ASSERT
#   ifdef BX_ASSERT
#       undef BX_ASSERT
#   endif

#define BX_ASSERT(_condition, _msg, ...) \
    if (!(_condition)) {  \
        bx::debugPrintf(_msg, ##__VA_ARGS__);   \
        bx::debugBreak();   \
    }
#else
#   define BX_ASSERT(_condition, _msg, ...)
#endif

#endif // BX_DEBUG_H_HEADER_GUARD
