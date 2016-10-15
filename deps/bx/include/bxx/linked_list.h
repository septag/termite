#pragma once

namespace bx
{
    template <typename Ty>
    struct ListNode
    {
        ListNode<Ty>* next;
        ListNode<Ty>* prev;
        Ty data;

        explicit ListNode(const Ty& _data) :
            next(nullptr),
            prev(nullptr),
            data(_data)
        {
        }
    };

    template <typename Ty>
    class List
    {
    public:
        List() :
            m_first(nullptr),
            m_last(nullptr)
        { 
        }

        inline void add(ListNode<Ty>* _node)
        {
            _node->next = m_first;
            _node->prev = nullptr;
            if (m_first)
                m_first->prev = _node;
            m_first = _node;
            if (!m_last)
                m_last = _node;
        }

        inline void addToEnd(ListNode<Ty>* _node)
        {
            if (m_last) {
                m_last->next = _node;
                _node->prev = m_last;
                m_last = _node;
            } else {
                m_last = _node;
            }

            if (!m_first)
                m_first = _node;
        }

        inline void remove(ListNode<Ty>* _node)
        {
            if (_node->next)
                _node->next->prev = _node->prev;
            if (_node->prev)
                _node->prev->next = _node->next;
            if (m_first == _node)
                m_first = _node->next;
            if (m_last == _node)
                m_last = _node->prev;
            _node->next = _node->prev = nullptr;
        }

        inline void insert(ListNode<Ty>* _insertAfter, ListNode<Ty>* _node)
        {
            if (_insertAfter->next)
                _insertAfter->next->prev = _node;
            _node->prev = _insertAfter;
            _node->next = _insertAfter->next;
            _insertAfter->next = _node;
        }

        inline bool isEmpty() const
        {
            return m_first == nullptr;
        }

        inline ListNode<Ty>* getFirst() const
        {
            return m_first;
        }

        inline ListNode<Ty>* getLast() const
        {
            return m_last;
        }

    private:
        ListNode<Ty>* m_first;
        ListNode<Ty>* m_last;
    };
}
