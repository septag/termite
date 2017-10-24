#pragma once

#include "bx/allocator.h"

namespace termite
{
    result_t initMemoryPool(bx::AllocatorI* alloc, size_t pageSize = 0, int maxPagesPerPool = 0);
    void shutdownMemoryPool();

    TERMITE_API bx::AllocatorI* allocMemPage(uint64_t tag) T_THREAD_SAFE;
    TERMITE_API void freeMemTag(uint64_t tag) T_THREAD_SAFE;

    TERMITE_API size_t getNumMemPages() T_THREAD_SAFE;
    TERMITE_API size_t getMemPoolAllocSize() T_THREAD_SAFE;
    TERMITE_API size_t getMemTagAllocSize(uint64_t tag) T_THREAD_SAFE;
    TERMITE_API int getMemTags(uint64_t* tags, int maxTags, size_t* pageSizes = nullptr) T_THREAD_SAFE;


    // Keeps a memory tag, and allocates a new page if the previous page is full
    // IMPORTANT NOTE: When using page allocator, you should not use the common API (see above)
    //                 Instead you should only use the api provided in the page allocator for free and allocate
    //                 Calling freeTag of the API scrambles PageAllocator internal allocator pointer
    class TERMITE_API PageAllocator : public bx::AllocatorI
    {
    public:
        PageAllocator(uint64_t tag) :
            m_tag(tag),
            m_linAlloc(nullptr)
        {
        }

        virtual ~PageAllocator()
        {
        }

        void* realloc(void* _ptr, size_t _size, size_t _align, const char* _file, uint32_t _line) override;

        void free()
        {
            freeMemTag(m_tag);
            m_linAlloc = nullptr;
        }

    private:
        uint64_t m_tag;
        bx::AllocatorI* m_linAlloc;
    };

    struct ImGuiApi_v0;
    TERMITE_API void debugMemoryPool(ImGuiApi_v0* imgui);
} // namespace termite
