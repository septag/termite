#pragma once

#include "../bx/allocator.h"

namespace bx
{
    class LinearAllocator : public AllocatorI
    {
        BX_CLASS(LinearAllocator
                 , NO_COPY
                 , NO_ASSIGNMENT
                 );
    public:
        LinearAllocator(void* _ptr, size_t _size)
        {
            m_offset = 0;
            m_size = _size;
            m_ptr = (uint8_t*)_ptr;
        }

        void init(void* _ptr, size_t _size)
        {
            m_offset = 0;
            m_size = _size;
            m_ptr = (uint8_t*)_ptr;
        }

        virtual ~LinearAllocator()
        {
        }

        virtual void* realloc(void* _ptr, size_t _size, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
        {
            if (_size) {
                if (m_offset + _size + sizeof(size_t) > m_size)
                    return nullptr;

                void* p = m_ptr + m_offset + sizeof(uint32_t);
                *((uint32_t*)((uint8_t*)p - sizeof(uint32_t))) = uint32_t(_size);
                m_offset += _size + sizeof(uint32_t);

                if (_ptr) {
                    size_t prevsize = *((uint32_t*)((uint8_t*)_ptr - sizeof(uint32_t)));
                    memcpy(p, _ptr, _size > prevsize ? prevsize : _size);
                }

                return p;
            }
            return nullptr;
        }

        void reset()
        {
            m_offset = 0;
        }

    private:
        size_t m_offset;
        size_t m_size;
        uint8_t* m_ptr;
    };
}