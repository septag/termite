/*
* Copyright 2016-2017 Sepehr Taghdisian.All rights reserved.
* License: https://github.com/septag/sx#license-bsd-2-clause
*/

#include "bxx/trace_allocator.h"
#include "bx/string.h"
#include "bx/debug.h"

namespace bx
{
    static DefaultAllocator g_traceAlloc;

    TraceAllocator::TraceAllocator(AllocatorI* _alloc, uint32_t _id, uint32_t _poolSize) :
        m_traceTable(HashTableType::Mutable)
    {
        BX_ASSERT(_poolSize > 0, "PoolSize should not be Zero");
        m_id = _id;
        m_size = 0;
        m_numAllocs = 0;
        m_alloc = _alloc;
        m_traceEnabled = _poolSize > 0;
        if (_poolSize) {
            m_tracePool.create(_poolSize, &g_traceAlloc);
            m_traceTable.create(_poolSize, &g_traceAlloc);
        }
    }

    TraceAllocator::~TraceAllocator()
    {
        if (m_traceEnabled) {
            m_tracePool.destroy();
            m_traceTable.destroy();
        }
    }

    uint32_t TraceAllocator::getId() const
    {
        return m_id;
    }

    size_t TraceAllocator::getAllocatedSize() const
    {
        return m_size;
    }

    uint32_t TraceAllocator::getAllocatedCount() const
    {
        return m_numAllocs;
    }

    void* TraceAllocator::realloc(void* _ptr, size_t _size, size_t _align, const char* _file, uint32_t _line)
    {
        if (_size == 0) {
            // Free
            if (m_traceEnabled) {
                int traceIdx = m_traceTable.find((uintptr_t)_ptr);
                if (traceIdx != -1) {
                    TraceAllocator::TraceItem* item = m_traceTable[traceIdx];
                    m_size -= item->size;
                    m_traceList.remove(&item->lnode);
                    m_traceTable.remove(traceIdx);
                    m_tracePool.deleteInstance(item);
                }
            }

            m_alloc->realloc(_ptr, 0, _align, _file, _line);
            --m_numAllocs;
            return NULL;
        } else if (_ptr == NULL) {
            // Malloc
            void* p = m_alloc->realloc(NULL, _size, _align, _file, _line);
            if (p) {
                if (m_traceEnabled) {
                    TraceAllocator::TraceItem* item = m_tracePool.newInstance<>();
#if BX_CONFIG_ALLOCATOR_DEBUG
                    strCopy(item->filename, sizeof(item->filename), _file);
                    item->line = _line;
#endif
                    item->size = _size;

                    m_traceList.addToEnd(&item->lnode);
                    m_traceTable.add((uintptr_t)p, item);
                    m_size += _size;
                }

                ++m_numAllocs;
            }

            return p;
        } else {
            // Realloc
            TraceAllocator::TraceItem* item = NULL;
            if (m_traceEnabled) {
                int traceIdx = m_traceTable.find((uintptr_t)_ptr);
                if (traceIdx != -1) {
                    item = m_traceTable[traceIdx];
                    m_size -= item->size;
                    m_traceTable.remove(traceIdx);
                }
            }

            void* newp = m_alloc->realloc(_ptr, _size, _align, _file, _line);
            if (newp) {
                // Update Trace Item
                if (item) {
#if BX_CONFIG_ALLOCATOR_DEBUG
                    strCopy(item->filename, sizeof(item->filename), _file);
                    item->line = _line;
#endif
                    item->size = _size;
                    m_size += _size;

                    m_traceTable.add((uintptr_t)newp, item);
                }
            }
            return newp;
        }
    }

    uint32_t TraceAllocator::dumpLeaks() const
    {
        if (!m_traceList.isEmpty()) {
            List<TraceAllocator::TraceItem*>::Node* node = m_traceList.getFirst();
            debugPrintf("Found Memory Leaks (AllocatorId = %d): ", m_id);
            uint32_t index = 1;
            while (node) {
                TraceAllocator::TraceItem* item = node->data;
                debugPrintf("\t%d) Size: %d, File: %s, Line: %d", index++, item->size, item->filename, item->size);
                node = node->next;
            }
            return index - 1;
        } 
        return 0;
    }

}
