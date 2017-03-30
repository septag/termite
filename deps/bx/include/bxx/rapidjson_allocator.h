#pragma once

#include "bx/allocator.h"

#include "rapidjson/allocators.h"
#include "rapidjson/stringbuffer.h"

namespace rapidjson
{
    class BxAllocatorNoFree
    {
    private:
        bx::AllocatorI* m_alloc;

    public:
        static const bool kNeedFree = false;

        BxAllocatorNoFree()
        {
            assert(false);
        }

        explicit BxAllocatorNoFree(bx::AllocatorI* alloc) : m_alloc(alloc)
        {
        }

        void* Malloc(size_t size) 
        {
            return BX_ALLOC(m_alloc, size);
        }

        void* Realloc(void* originalPtr, size_t originalSize, size_t newSize) 
        {
            BX_UNUSED(originalSize);
            if (newSize == 0) {
                BX_FREE(m_alloc, originalPtr);
                return NULL;
            }
            return BX_REALLOC(m_alloc, originalPtr, newSize);
        }

        static void Free(void *ptr) {}
    };

    class BxAllocatorStatic
    {
    public:
        static const bool kNeedFree = false;
        static bx::AllocatorI* Alloc;

        BxAllocatorStatic()
        {
            assert(BxAllocatorStatic::Alloc);
        }

        void* Malloc(size_t size)
        {
            return BX_ALLOC(Alloc, size);
        }

        void* Realloc(void* originalPtr, size_t originalSize, size_t newSize)
        {
            BX_UNUSED(originalSize);
            if (newSize == 0) {
                BX_FREE(Alloc, originalPtr);
                return NULL;
            }
            return BX_REALLOC(Alloc, originalPtr, newSize);
        }

        static void Free(void *ptr) 
        {
            BX_FREE(Alloc, ptr);
        }
    };

    typedef MemoryPoolAllocator<BxAllocatorNoFree> BxAllocator;
    typedef GenericDocument<UTF8<>, MemoryPoolAllocator<BxAllocatorNoFree>, BxAllocatorNoFree> BxDocument;
    typedef GenericValue<UTF8<>, MemoryPoolAllocator<BxAllocatorNoFree>> BxValue;
    typedef GenericStringBuffer<UTF8<>, BxAllocatorNoFree> BxStringBuffer;
    
    inline void getFloatArray(const BxValue& jvalue, float* f, int num)
    {
        assert(jvalue.IsArray());
        num = std::min<int>(jvalue.Size(), num);
        for (int i = 0; i < num; i++)
            f[i] = jvalue[i].GetFloat();
    }

    inline void getIntArray(const BxValue& jvalue, int* n, int num)
    {
        assert(jvalue.IsArray());
        num = std::min<int>(jvalue.Size(), num);
        for (int i = 0; i < num; i++)
            n[i] = jvalue[i].GetInt();
    }

    inline BxValue createFloatArray(const float* f, int num, BxDocument::AllocatorType& alloc)
    {
        BxValue value(kArrayType);
        for (int i = 0; i < num; i++)
            value.PushBack(BxValue(f[i]).Move(), alloc);
        return value;
    }

    inline BxValue createIntArray(const int* n, int num, BxDocument::AllocatorType& alloc)
    {
        BxValue value(kArrayType);
        for (int i = 0; i < num; i++)
            value.PushBack(BxValue(n[i]).Move(), alloc);
        return value;
    }
}
