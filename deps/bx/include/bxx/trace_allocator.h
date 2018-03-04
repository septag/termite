/*
* Copyright 2016-2017 Sepehr Taghdisian.All rights reserved.
* License: https://github.com/septag/sx#license-bsd-2-clause
*/

#pragma once

#include "bx/allocator.h"
#include "hash_table.h"
#include "linked_list.h"
#include "pool.h"
#include "bxx/lock.h"

namespace bx
{
    /// Trace allocator: Routes all allocations to other allocators
    ///                  They also can track memory allocations and leaks
    ///                  Saves an Id and sizes per allocation
    class TraceAllocator : public AllocatorI
    {
        BX_CLASS(TraceAllocator
                 , NO_COPY
                 , NO_ASSIGNMENT
        );

    public:
        struct TraceItem
        {
            size_t size;
            char filename[32];
            int line;
            bx::List<TraceItem*>::Node lnode;

            TraceItem() :
                size(0),
                line(0),
                lnode(this)
            {
                filename[0] = 0;
            }
        };

    private:
        uint32_t m_id;
        AllocatorI* m_alloc;

        bool m_traceEnabled;
        Pool<TraceItem> m_tracePool;
        HashTable<TraceItem*, uintptr_t> m_traceTable;
        List<TraceItem*> m_traceList;
        List<TraceItem*>::Node* m_traceNode;

        uintptr_t m_size;
        uint32_t m_numAllocs;

        bx::Lock m_lock;

    public:
        /// @param[in] _alloc ProxyAllocator that the calls should pass through
        /// @param[in] _id Memory Id
        /// @param[in] _tracePoolSize If it's Zero, tracing will be disabled, other values enables tracing
        TraceAllocator(AllocatorI* _alloc, uint32_t _id, uint32_t _tracePoolSize = 0);

        virtual ~TraceAllocator();

        uint32_t getId() const;

        size_t getAllocatedSize() const;
        uint32_t getAllocatedCount() const;

        void* realloc(void* _ptr, size_t _size, size_t _align, const char* _file, uint32_t _line) override;

        const TraceItem* getFirstLeak();
        const TraceItem* getNextLeak();
    };
}   // namespace bx
