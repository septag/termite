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

        virtual ~LinearAllocator()
        {
        }

        void init(void* _ptr, size_t _size)
        {
            m_offset = 0;
            m_size = _size;
            m_ptr = (uint8_t*)_ptr;
        }

        void* realloc(void* _ptr, size_t _size, size_t _align, const char* _file, uint32_t _line) override
        {
            if (_size) {
                _align = _align < 8 ? 8 : _align;
                struct Header
                {
                    uint32_t size;
                    uint8_t padding;
                };

                size_t total = _size + sizeof(Header) + _align;

                // Allocate memory (with header)
                if (m_offset + total > m_size)
                    return nullptr;     // Not enough memory in the buffer
                uint8_t* ptr = m_ptr + m_offset;
                uint8_t* aligned = (uint8_t*)bx::alignPtr(ptr, sizeof(Header), _align);
                Header* header = (Header*)(aligned - sizeof(Header));
                header->size = uint32_t(_size);
                header->padding = uint8_t(aligned - ptr);

                m_offset += total;

                // Realloc emulation
                if (_ptr) {
                    Header* prevHeader = (Header*)_ptr - 1;
                    size_t prevsize = prevHeader->size;
                    memcpy(aligned, _ptr, _size > prevsize ? prevsize : _size);
                }

                return aligned;
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