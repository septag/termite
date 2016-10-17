#pragma once

#include "bx/allocator.h"

namespace bx
{
    template <typename Ty>
    class Stack
    {
    public:
        struct Node
        {
            Node* down;
            Ty data;

            explicit Node(const Ty& _value) :
                down(nullptr),
                data(_value)
            {
            }
        };

    public:
        Stack() :
            m_head(nullptr)
        {
        }

        inline void push(Node* _node)
        {
            _node->down = m_head;
            m_head = _node;
        }

        inline bool pop(Ty* pData)
        {
            if (m_head) {
                Node* node = m_head;
                m_head = node->down;
                node->down = nullptr;
                *pData = node->data;
                return true;
            } else {
                return false;
            }
        }

        inline bool peek(Ty* pData) const
        {
            if (m_head) {
                *pData = m_head->data;
                return true;
            } else {
                return false;
            }            
        }

        inline bool isEmpty() const
        {
            return m_head == nullptr;
        }

        inline const Node* getHead() const
        {
            return m_head;
        }

    private:
        Node* m_head;
    };
}   // namespace bx