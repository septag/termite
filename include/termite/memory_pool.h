#pragma once

#include "bx/allocator.h"

namespace termite
{
    result_t memInit(bx::AllocatorI* alloc, size_t pageSize = 0, int maxPagesPerPool = 0);
    void memShutdown();

    TERMITE_API bx::AllocatorI* memAllocPage(uint64_t tag) T_THREAD_SAFE;
    TERMITE_API void memFreeTag(uint64_t tag) T_THREAD_SAFE;
    TERMITE_API size_t memGetNumPages() T_THREAD_SAFE;
    TERMITE_API size_t memGetAllocSize() T_THREAD_SAFE;
    TERMITE_API size_t memGetTagSize(uint64_t tag) T_THREAD_SAFE;

    // This is a pool helper allocator
    // Keeps a memory tag, and allocates a new page if the previous page is full
    class memPageAllocator : public bx::AllocatorI
    {
    public:
        memPageAllocator(uint64_t tag)
        {
            m_tag = tag;
            m_linAlloc = nullptr;
        }

        virtual ~memPageAllocator()
        {
        }

        void* realloc(void* _ptr, size_t _size, size_t _align, const char* _file, uint32_t _line) override
        {
            if (!m_linAlloc) {
                m_linAlloc = memAllocPage(m_tag);
                if (!m_linAlloc)
                    return nullptr;
            }

            void* p = m_linAlloc->realloc(_ptr, _size, _align, _file, _line);
            if (!p) {
                m_linAlloc = memAllocPage(m_tag);
                return m_linAlloc->realloc(_ptr, _size, _align, _file, _line);
            }
            return p;
        }

    private:
        uint64_t m_tag;
        bx::AllocatorI* m_linAlloc;
    };
} // namespace st
