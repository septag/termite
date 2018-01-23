#include "pch.h"
#include "termite/rapidjson.h"

namespace tee {

    void json::HeapAllocator::SetAlloc(bx::AllocatorI* alloc)
    {
        Alloc = alloc;
    }

    void json::HeapAllocator::Free(void *ptr)
    {
        BX_FREE(Alloc, ptr);
    }

    void* json::HeapAllocator::Realloc(void* originalPtr, size_t originalSize, size_t newSize)
    {
        BX_UNUSED(originalSize);
        if (newSize == 0) {
            BX_FREE(Alloc, originalPtr);
            return NULL;
        }
        return BX_REALLOC(Alloc, originalPtr, newSize);
    }

    void* json::HeapAllocator::Malloc(size_t size)
    {
        return BX_ALLOC(Alloc, size);
    }

    json::HeapAllocator::HeapAllocator()
    {
        BX_ASSERT(HeapAllocator::Alloc, "Static Allocator should be defined before use");
    }

    bx::AllocatorI* json::HeapAllocator::Alloc = nullptr;


    json::StackAllocator::StackAllocator()
    {
        BX_ASSERT(false, "NoFree allocator should not be constructed");
    }

    json::StackAllocator::StackAllocator(bx::AllocatorI* alloc) : m_alloc(alloc)
    {

    }

    void* json::StackAllocator::Malloc(size_t size)
    {
        return BX_ALLOC(m_alloc, size);
    }

    void* json::StackAllocator::Realloc(void* originalPtr, size_t originalSize, size_t newSize)
    {
        BX_UNUSED(originalSize);
        if (newSize == 0) {
            BX_FREE(m_alloc, originalPtr);
            return NULL;
        }
        return BX_REALLOC(m_alloc, originalPtr, newSize);
    }

} // namespace tee
 