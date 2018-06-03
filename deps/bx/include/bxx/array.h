#pragma once

#include "../bx/allocator.h"
#include <cassert>
#include <functional>

namespace bx
{
    template <typename Ty>
    class Array
    {
    public:
        typedef std::function<bool(const Ty& value)> FindMatchFunc;

    public:
        Array();
        ~Array();

        bool create(int _initCount, int _growCount, AllocatorI* _alloc);
        void destroy();

        Ty* push();
        Ty* pushMany(int _count);
        Ty* pop();

        int getCount() const    {   return m_numItems;  }
        Ty* getBuffer() const   {   return m_buff;      }
        void clear()            {   m_numItems = 0;     }
        Ty* detach(int* _count, AllocatorI** _palloc = nullptr);

        int find(const Ty& _item) const;
        int find(FindMatchFunc matchFn) const;

        Ty* itemPtr(int _index)  {  
            BX_ASSERT(_index < m_numItems); return &m_buff[_index];   
        }
        Ty& item(int _index) {
            BX_ASSERT(_index < m_numItems); return m_buff[_index];
        }
        const Ty& operator[](int _index) const   {   BX_ASSERT(_index < m_numItems);  return m_buff[_index]; }
        Ty& operator[](int _index) { BX_ASSERT(_index < m_numItems);  return m_buff[_index]; }

    private:
        AllocatorI* m_alloc;
        Ty* m_buff;
        int m_numItems;
        int m_maxItems;
        int m_numExpand;
    };

    // Implementation
    template <typename Ty>
    Array<Ty>::Array()
    {
        m_alloc = nullptr;
        m_buff = nullptr;
        m_numItems = 0;
        m_maxItems = 0;
        m_numExpand = 0;
    }

    template <typename Ty>
    Array<Ty>::~Array()
    {
        BX_ASSERT(m_buff == nullptr);
    }

    template <typename Ty>
    bool Array<Ty>::create(int _initCount, int _growCount, AllocatorI* _alloc)
    {
        BX_ASSERT(_alloc);

        m_buff = (Ty*)BX_ALLOC(_alloc, _initCount*sizeof(Ty));
        if (!m_buff)
            return false;
        m_alloc = _alloc;
        m_maxItems = _initCount;
        m_numItems = 0;
        m_numExpand = _growCount ? _growCount : _initCount;
        return true;
    }

    template <typename Ty>
    void Array<Ty>::destroy()
    {
        if (!m_alloc)
            return;

        if (m_buff)
            BX_FREE(m_alloc, m_buff);
        m_buff = nullptr;
        m_maxItems = 0;
        m_numItems = 0;
        m_numExpand = 0;

        m_alloc = nullptr;
    }

    template <typename Ty>
    Ty* Array<Ty>::push()
    {
        if (m_numItems == m_maxItems)    {
            int newsz = m_numExpand + m_maxItems;
            m_buff = (Ty*)BX_REALLOC(m_alloc, m_buff, sizeof(Ty)*newsz);
            if (!m_buff)
                return nullptr;
            m_maxItems = newsz;
        }

        Ty* item = &m_buff[m_numItems++];
        return item;
    }

    template <typename Ty>
    Ty* bx::Array<Ty>::pop()
    {
        if (m_numItems > 0) {
            return &m_buff[--m_numItems];
        } else {
            return nullptr;
        }
    }

    template <typename Ty>
    Ty* Array<Ty>::pushMany(int _count)
    {
        if (m_maxItems < m_numItems + _count) {
            int newsz = BX_ALIGN_MASK((_count + m_numItems), m_numExpand-1);
            m_buff = (Ty*)BX_REALLOC(m_alloc, m_buff, sizeof(Ty)*newsz);
            if (!m_buff)
                return nullptr;
            m_maxItems = newsz;
        }

        Ty* items = &m_buff[m_numItems];
        m_numItems += _count;
        return items;
    }

    template <typename Ty>
    int Array<Ty>::find(const Ty& _item) const
    {
        for (int i = 0, c = m_numItems; i < c; i++)  {
            if (_item != m_buff[i])
                continue;
            else
                return i;
        }
        return -1;
    }

    template <typename Ty> 
    Ty* Array<Ty>::detach(int* _count, AllocatorI** _palloc)
    {
        *_count = m_numItems;
        Ty* buff = m_buff;
        if (_palloc)
            *_palloc = m_alloc;

        m_buff = nullptr;
        m_numItems = 0;
        m_maxItems = 0;
        m_numExpand = 0;
        m_alloc = nullptr;
        return buff;
    }

    template <typename Ty>
    int bx::Array<Ty>::find(FindMatchFunc matchFn) const
    {
        for (int i = 0, c = m_numItems; i < c; i++) {
            if (!matchFn(m_buff[i]))
                continue;
            else
                return i;
        }
        return -1;
    }
}
