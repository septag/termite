/*
 * Copyright 2010-2016 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bx#license-bsd-2-clause
 */

#ifndef BX_TIMER_H_HEADER_GUARD
#define BX_TIMER_H_HEADER_GUARD

#include "bx.h"

#if BX_PLATFORM_ANDROID || BX_PLATFORM_LINUX
#	include <time.h> // clock, clock_gettime
#elif BX_PLATFORM_EMSCRIPTEN
#	include <emscripten.h>
#elif BX_PLATFORM_WINDOWS || BX_PLATFORM_XBOXONE || BX_PLATFORM_WINRT
#   define WIN32_LEAN_AND_MEAN
#	include <windows.h>
#   undef WIN32_LEAN_AND_MEAN
#elif BX_PLATFORM_OSX || BX_PLATFORM_IOS
#   include <mach/mach_time.h>
#else
#	include <sys/time.h> // gettimeofday
#endif // BX_PLATFORM_

namespace bx
{
	inline int64_t getHPCounter()
	{
#if BX_PLATFORM_WINDOWS || BX_PLATFORM_XBOX360 || BX_PLATFORM_XBOXONE || BX_PLATFORM_WINRT
		LARGE_INTEGER li;
		// Performance counter value may unexpectedly leap forward
		// http://support.microsoft.com/kb/274323
		QueryPerformanceCounter(&li);
		return li.QuadPart;
#elif BX_PLATFORM_ANDROID || BX_PLATFORM_LINUX
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		int64_t i64 = now.tv_sec*INT64_C(1000000000) + now.tv_nsec;
        return i64;
#elif BX_PLATFORM_OSX || BX_PLATFORM_IOS
        return mach_absolute_time();
#elif BX_PLATFORM_EMSCRIPTEN
		return int64_t(1000.0f * emscripten_get_now() );
#else
		struct timeval now;
		gettimeofday(&now, 0);
		int64_t i64 = now.tv_sec*INT64_C(1000000) + now.tv_usec;
        return i64;
#endif // BX_PLATFORM_
	}

	inline double getHPFrequency()
	{
#if BX_PLATFORM_WINDOWS || BX_PLATFORM_XBOX360 || BX_PLATFORM_XBOXONE || BX_PLATFORM_WINRT
		LARGE_INTEGER li;
		QueryPerformanceFrequency(&li);
		return (double)li.QuadPart;
#elif BX_PLATFORM_ANDROID || BX_PLATFORM_LINUX
        timespec res;
        clock_getres(CLOCK_MONOTONIC, &res);
        return (double)res.tv_nsec * 1e9;
#elif BX_PLATFORM_OSX || BX_PLATFORM_IOS
        struct mach_timebase_info tmbase;
        mach_timebase_info(&tmbase);
        return ((double)tmbase.numer / (double)tmbase.denom)*1e9;
#elif BX_PLATFORM_EMSCRIPTEN
		return INT64_C(1000000);
#else
		return INT64_C(1000000);
#endif // BX_PLATFORM_
	}

} // namespace bx

#endif // BX_TIMER_H_HEADER_GUARD
