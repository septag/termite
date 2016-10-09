#pragma once

#include "bx/allocator.h"
#include "bx/cpu.h"
#include "pool.h"

namespace bx
{
    template <typename Ty>
    struct QueueNode
    {
        QueueNode<Ty>* next;
        Ty data;

        QueueNode()
        {
            next = nullptr;
        }
    };

    template <typename Ty> 
    void pushQueueNode(QueueNode<Ty>** _ref, QueueNode<Ty>* _node, const Ty& data)
    {
        if (*_ref) {
            QueueNode<Ty>* last = *_ref;
            while (last->next)
                last = last->next;
            last->next = _node;
        } else {
            *_ref = _node;
        }
        _node->next = nullptr;
        _node->data = data;
    }

    template <typename Ty>
    Ty popQueue(QueueNode<Ty>** _ref)
    {
        QueueNode<Ty>* item = *_ref;
        if (*_ref) {
            *_ref =(*_ref)->next;
            item->next = nullptr;
        } 
        return item->data;
    }

    template <typename Ty> 
    Ty peekQueue(QueueNode<Ty>* _ref)
    {
        return _ref->data;
    }

    // SpScQueueAlloc and container
    // http://drdobbs.com/article/print?articleId=210604448&siteSectionName=
    template <typename Ty>
    class SpScUnboundedQueuePool
    {
        BX_CLASS(SpScUnboundedQueuePool
                 , NO_COPY
                 , NO_ASSIGNMENT
                 );
    public:
        struct Node
        {
            Node() : next(NULL)
            {
            }

            Node(const Ty& _value) 
                : value(_value)
                , next(NULL)
            {
            }

            Ty value;
            Node* next;
        };

    public:
        explicit SpScUnboundedQueuePool(bx::Pool<Node>* _pool)
            :
              m_pool(_pool)
            , m_first(_pool->newInstance())
            , m_divider(m_first)
            , m_last(m_first)

        {
        }

        ~SpScUnboundedQueuePool()
        {
            while (NULL != m_first) {
                Node* node = m_first;
                m_first = node->next;
                m_pool->deleteInstance(node);
            }
        }

        void push(const Ty& _value) // producer only
        {
            m_last->next = m_pool->newInstance();
            m_last->next->value = _value;
            atomicExchangePtr((void**)&m_last, m_last->next);
            while (m_first != m_divider) {
                Node* node = m_first;
                m_first = m_first->next;
                m_pool->deleteInstance(node);
            }
        }

        const Ty& peek() const // consumer only
        {
            if (m_divider != m_last) {
                Ty& val = m_divider->next->value;
                return val;
            }

            return NULL;
        }

        bool pop(Ty* val) // consumer only
        {
            if (m_divider != m_last) {
                *val = m_divider->next->value;
                atomicExchangePtr((void**)&m_divider, m_divider->next);
                return true;
            }

            return false;
        }

    private:
        bx::Pool<Node>* m_pool;
        Node* m_first;
        Node* m_divider;
        Node* m_last;
    };

}   // namespace: bx
