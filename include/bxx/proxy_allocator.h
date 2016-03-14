#pragma once

#include "../bx/allocator.h"
#include "../bx/platform.h"
#include "../bx/cpu.h"

namespace bx
{
    // Proxy allocator routes all allocations to other allocators
    // Saves an Id and sizes per allocation
    class ProxyAllocator : public AllocatorI
    {
        BX_CLASS(StackAllocator
                 , NO_COPY
                 , NO_ASSIGNMENT
                 );
    private:
        uint32_t m_id;
        AllocatorI* m_alloc;
        volatile uintptr_t m_size;

    public:
        ProxyAllocator(AllocatorI* _alloc, uint32_t _id)
        {
            m_id = _id;
            m_size = 0;
            m_alloc = _alloc;
        }

        virtual ~ProxyAllocator()
        {
        }

        uint32_t getId() const
        {
            return m_id;
        }

        size_t getAllocatedSize() const
        {
            return m_size;
        }

        virtual void* realloc(void* _ptr, size_t _size, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
        {
            if (_size == 0) {
                // free
                uint8_t* ptr = (uint8_t*)_ptr - sizeof(size_t);
                size_t curSize = *((size_t*)ptr);
                bx::atomicFetchAndSub<uintptr_t>(&m_size, curSize);
                m_alloc->realloc(ptr, 0, _align, _file, _line);
                return NULL;
            } else if (_ptr == NULL) {
                // malloc
                uint8_t* p = (uint8_t*)m_alloc->realloc(NULL, _size + sizeof(size_t), _align, _file, _line);
                if (p) {
                    *((size_t*)p) = _size;  // save size
                    bx::atomicAddAndFetch<uintptr_t>(&m_size, _size);
                    return p + sizeof(size_t);
                } else {
                    return NULL;
                }
            } else {
                // realloc
                uint8_t* ptr = (uint8_t*)_ptr - sizeof(size_t);
                size_t curSize = *((size_t*)ptr);
                bx::atomicFetchAndSub<uintptr_t>(&m_size, curSize);
                uint8_t* newp = (uint8_t*)m_alloc->realloc(ptr, _size + sizeof(size_t), _align, _file, _line);
                if (newp) {
                    *((size_t*)newp) = _size;  // save size
                    bx::atomicAddAndFetch<uintptr_t>(&m_size, _size);
                    return newp + sizeof(size_t);
                } else {
                    return NULL;
                }
            }
        }
    };
}   // namespace bx