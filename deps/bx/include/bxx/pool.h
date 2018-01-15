#pragma once

#include "../bx/allocator.h"

namespace bx
{
    template <typename Ty>
    class FixedPool
    {
    public:
        FixedPool();
        ~FixedPool();

        bool create(int _bucketSize, AllocatorI* _alloc);
        void destroy();

        template <typename ... _ConstructorParams>
        Ty* newInstance(_ConstructorParams ... params)
        {
            if (m_index > 0)
                return new(m_ptrs[--m_index]) Ty(params ...);
            else
                return nullptr;
        }

        void deleteInstance(Ty* _inst);
        void clear();
        int getMaxItems() const {  return m_maxItems;    }

    private:
        AllocatorI* m_alloc;
        Ty* m_buffer;
        Ty** m_ptrs;
        int m_maxItems;
        int m_index;
    };

    template <typename Ty>
    class Pool 
    {
    public:
        Pool();
        ~Pool();
        
        bool create(int _bucketSize, AllocatorI* _alloc);
        void destroy();

        template <typename ... _ConstructorParams>
        Ty* newInstance(_ConstructorParams ... params)
        {
            Bucket* b = m_firstBucket;
            while (b) {
                if (b->iter > 0)
                    return BX_PLACEMENT_NEW(b->ptrs[--b->iter], Ty)(params ...);
                b = b->next;
            }

            b = createBucket();
            if (!b)
                return nullptr;

            return BX_PLACEMENT_NEW(b->ptrs[--b->iter], Ty)(params ...);
        }

        void deleteInstance(Ty* _inst);

        void clear();
        int getLeakCount() const;
        bool owns(Ty* _ptr);

    private:
        struct Bucket
        {
            Bucket* prev;
            Bucket* next;
            uint8_t* buffer;
            Ty** ptrs;
            int iter;
        };

    private:
        Bucket* createBucket();
        void destroyBucket(Bucket* bucket);

    private:
        AllocatorI* m_alloc;
        int m_maxItemsPerBucket;
        int m_numBuckets;
        Bucket* m_firstBucket;
    };
}

#include "inline/pool.inl"