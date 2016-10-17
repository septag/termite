#pragma once

namespace bx
{
    template <typename Ty>
    class List
    {
    public:
        struct Node
        {
            Node* next;
            Node* prev;
            Ty data;

            explicit Node(const Ty& _data) :
                next(nullptr),
                prev(nullptr),
                data(_data)
            {
            }
        };

    public:
        List() :
            m_first(nullptr),
            m_last(nullptr)
        { 
        }

        inline void add(Node* _node)
        {
            _node->next = m_first;
            _node->prev = nullptr;
            if (m_first)
                m_first->prev = _node;
            m_first = _node;
            if (!m_last)
                m_last = _node;
        }

        inline void addToEnd(Node* _node)
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

        inline void remove(Node* _node)
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

        inline void insert(Node* _insertAfter, Node* _node)
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

        inline Node* getFirst() const
        {
            return m_first;
        }

        inline Node* getLast() const
        {
            return m_last;
        }

    private:
        Node* m_first;
        Node* m_last;
    };
}
