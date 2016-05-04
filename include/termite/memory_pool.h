#pragma once

#include "bx/allocator.h"

namespace termite
{
    result_t memInit(bx::AllocatorI* alloc, size_t pageSize = 0, int maxPagesPerPool = 0);
    void memShutdown();

    TERMITE_API bx::AllocatorI* memAllocPage(uint32_t tag) T_THREAD_SAFE;
    TERMITE_API void memFreeTag(uint32_t tag) T_THREAD_SAFE;
    TERMITE_API size_t memGetNumPages() T_THREAD_SAFE;
    TERMITE_API size_t memGetAllocSize() T_THREAD_SAFE;
    TERMITE_API size_t memGetTagSize(uint32_t tag) T_THREAD_SAFE;
} // namespace st
