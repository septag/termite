/*
* Copyright 2016-2017 Sepehr Taghdisian.All rights reserved.
* License: https://github.com/septag/sx#license-bsd-2-clause
*/

#include "bxx/trace_allocator.h"
#include "bx/string.h"
#include "bx/debug.h"
#include "bxx/path.h"

namespace bx
{
    static DefaultAllocator g_traceAlloc;

    TraceAllocator::TraceAllocator(AllocatorI* _alloc, uint32_t _id, uint32_t _tracePoolSize) :
        m_traceTable(HashTableType::Mutable)
    {
        m_id = _id;
        m_size = 0;
        m_numAllocs = 0;
        m_alloc = _alloc;
        m_traceEnabled = _tracePoolSize > 0;
        m_traceNode = nullptr;
        if (_tracePoolSize) {
            m_tracePool.create(_tracePoolSize, &g_traceAlloc);
            m_traceTable.create(_tracePoolSize, &g_traceAlloc);
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
            if (_ptr) {
                m_lock.lock();
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
                --m_numAllocs;
                m_lock.unlock();

                m_alloc->realloc(_ptr, 0, _align, _file, _line);
            }
            return nullptr;
        } else if (_ptr == nullptr) {
            // Malloc
            void* p = m_alloc->realloc(nullptr, _size, _align, _file, _line);
            if (p) {
                m_lock.lock();
                if (m_traceEnabled) {
                    TraceAllocator::TraceItem* item = m_tracePool.newInstance<>();
#if BX_CONFIG_ALLOCATOR_DEBUG
                    Path path(_file);   
                    strCopy(item->filename, sizeof(item->filename), path.getFilenameFull().cstr());
                    item->line = _line;
#endif
                    item->size = _size;

                    m_traceList.addToEnd(&item->lnode);
                    m_traceTable.add((uintptr_t)p, item);
                    m_size += _size;
                }

                ++m_numAllocs;
                m_lock.unlock();
            }

            return p;
        } else {
            // Realloc
            TraceAllocator::TraceItem* item = nullptr;
            if (m_traceEnabled) {
                m_lock.lock();
                int traceIdx = m_traceTable.find((uintptr_t)_ptr);
                if (traceIdx != -1) {
                    item = m_traceTable[traceIdx];
                    m_size -= item->size;
                    m_traceTable.remove(traceIdx);
                }
                m_lock.unlock();
            }

            void* newp = m_alloc->realloc(_ptr, _size, _align, _file, _line);
            if (newp && item) {
                m_lock.lock();
#if BX_CONFIG_ALLOCATOR_DEBUG
                Path path(_file);
                strCopy(item->filename, sizeof(item->filename), path.getFilenameFull().cstr());
                item->line = _line;
#endif
                item->size = _size;
                m_size += _size;

                m_traceTable.add((uintptr_t)newp, item);
                m_lock.unlock();
            }
            return newp;
        }
    }

    const TraceAllocator::TraceItem* TraceAllocator::getFirstLeak()
    {
        m_traceNode = m_traceList.getFirst();
        return m_traceNode ? m_traceNode->data : nullptr;;
    }

    const TraceAllocator::TraceItem* TraceAllocator::getNextLeak()
    {
        m_traceNode = m_traceNode->next;
        return m_traceNode ? m_traceNode->data : nullptr;
    }
}
