#pragma once

namespace bx
{
    class StackAllocator : public AllocatorI
    {
        BX_CLASS(StackAllocator
                 , NO_COPY
                 , NO_ASSIGNMENT
                 );
    public:
        StackAllocator(void* _ptr, size_t _size)
        {
            m_offset = 0;
            m_size = _size;
            m_ptr = (uint8_t*)_ptr;
        }

        virtual ~StackAllocator()
        {
        }

        virtual void* realloc(void* _ptr, size_t _size, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
        {
            if (_size) {
                if (m_offset + _size > m_size)
                    return nullptr;

                void* p = m_ptr + m_offset;
                m_offset += _size;
                return p;
            }
            return nullptr;
        }

    private:
        size_t m_offset;
        size_t m_size;
        uint8_t* m_ptr;
    };
}