#pragma once

#include <algorithm>
#include "bx/allocator.h"

#include "rapidjson/allocators.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/document.h"

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
        static const bool kNeedFree = true;
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

    typedef MemoryPoolAllocator<BxAllocatorStatic> BxsAllocator;
    typedef GenericDocument<UTF8<>, MemoryPoolAllocator<BxAllocatorStatic>, BxAllocatorStatic> BxsDocument;
    typedef GenericValue<UTF8<>, MemoryPoolAllocator<BxAllocatorStatic>> BxsValue;
    typedef GenericStringBuffer<UTF8<>, BxAllocatorStatic> BxsStringBuffer;
    
    template <typename _T> 
    void getFloatArray(const _T& jvalue, float* f, int num)
    {
        assert(jvalue.IsArray());
        num = std::min<int>(jvalue.Size(), num);
        for (int i = 0; i < num; i++)
            f[i] = jvalue[i].GetFloat();
    }

    template <typename _T>
    inline void getIntArray(const _T& jvalue, int* n, int num)
    {
        assert(jvalue.IsArray());
        num = std::min<int>(jvalue.Size(), num);
        for (int i = 0; i < num; i++)
            n[i] = jvalue[i].GetInt();
    }

    template <typename _T, typename _AllocT>
    _T createFloatArray(const float* f, int num, _AllocT& alloc)
    {
        _T value(kArrayType);
        for (int i = 0; i < num; i++)
            value.PushBack(_T(f[i]).Move(), alloc);
        return value;
    }

    template <typename _T, typename _AllocT>
    _T createIntArray(const int* n, int num, _AllocT& alloc)
    {
        _T value(kArrayType);
        for (int i = 0; i < num; i++)
            value.PushBack(_T(n[i]).Move(), alloc);
        return value;
    }
}
