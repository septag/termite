/*
 * Copyright 2010-2018 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bx#license-bsd-2-clause
 */

#ifndef BX_DEBUG_H_HEADER_GUARD
#define BX_DEBUG_H_HEADER_GUARD

#include "string.h"

namespace bx
{
	///
	void debugBreak();

	///
	void debugOutput(const char* _out);

	///
	void debugOutput(const StringView& _str);

	///
	void debugPrintfVargs(const char* _format, va_list _argList);

	///
	void debugPrintf(const char* _format, ...);

	///
	void debugPrintfData(const void* _data, uint32_t _size, const char* _format, ...);

	///
	struct WriterI* getDebugOut();

} // namespace bx

#if BX_ENABLE_ASSERTS
#   ifdef BX_ASSERT
#       undef BX_ASSERT
#   endif

#   if !BX_COMPILER_MSVC
#       define BX_ASSERT_1(_cond)  if (!(_cond)) { bx::debugBreak(); }
#       define BX_ASSERT_2(_cond, _arg1) if (!(_cond)) { bx::debugOutput(_arg1); bx::debugBreak(); }
#       define BX_ASSERT_3(_cond, _arg1, _arg2) if (!(_cond)) { bx::debugPrintf(_arg1, _arg2); bx::debugBreak(); }
#       define BX_ASSERT_4(_cond, _arg1, _arg2, _arg3) if (!(_cond)) { bx::debugPrintf(_arg1, _arg2, _arg3); bx::debugBreak(); }

#       define BX_GET_ASSERT_MACRO(_1, _2, _3, _4, _NAME, ...) _NAME
#       define BX_ASSERT(...) BX_GET_ASSERT_MACRO(__VA_ARGS__, BX_ASSERT_4, BX_ASSERT_3, BX_ASSERT_2, BX_ASSERT_1)(__VA_ARGS__);
#   else
#       define BX_ASSERT(_cond, ...)  if (!(_cond)) { bx::debugBreak();  }
#   endif
#else
#   define BX_ASSERT(...)
#endif

#endif // BX_DEBUG_H_HEADER_GUARD
