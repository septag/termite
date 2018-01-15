#pragma once

#include "bx/allocator.h"

namespace tee
{
    bool initMemoryPool(bx::AllocatorI* alloc, size_t pageSize = 0, int maxPagesPerPool = 0);
    void shutdownMemoryPool();

    TEE_API bx::AllocatorI* allocMemPage(uint64_t tag) TEE_THREAD_SAFE;
    TEE_API void freeMemTag(uint64_t tag) TEE_THREAD_SAFE;

    TEE_API size_t getNumMemPages() TEE_THREAD_SAFE;
    TEE_API size_t getMemPoolAllocSize() TEE_THREAD_SAFE;
    TEE_API size_t getMemTagAllocSize(uint64_t tag) TEE_THREAD_SAFE;
    TEE_API int getMemTags(uint64_t* tags, int maxTags, size_t* pageSizes = nullptr) TEE_THREAD_SAFE;


    // Keeps a memory tag, and allocates a new page if the previous page is full
    // IMPORTANT NOTE: When using page allocator, you should not use the common API (see above)
    //                 Instead you should only use the api provided in the page allocator for free and allocate
    //                 Calling freeTag of the API scrambles PageAllocator internal allocator pointer
    class TEE_API PageAllocator : public bx::AllocatorI
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

    struct ImGuiApi;
    TEE_API void debugMemoryPool(ImGuiApi* imgui);
} // namespace tee
