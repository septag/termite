#pragma once

#include "bx/allocator.h"
#include "bx/cpu.h"
#include "pool.h"

namespace bx
{
    template <typename Ty>
    class Queue
    {
    public:
        struct Node
        {
            Node* next;
            Ty data;

            explicit Node(const Ty& _data) :
                next(nullptr),
                data(_data)
            {
            }
        };

    public:
        Queue() :
            m_first(nullptr),
            m_last(nullptr)
        {
        }

        inline void push(Node* _node)
        {
            if (m_last)
                m_last->next = _node;
            m_last = _node;

            if (!m_first)
                m_first = _node;
            _node->next = nullptr;
        }

        inline bool pop(Ty* pData)
        {
            if (m_first) {
                Node* first = m_first;
                if (m_last == first)
                    m_last = nullptr;

                m_first = first->next;
                first->next = nullptr;
                *pData = first->data;
                return true;
            } else {
                return false;
            }
        }

        inline bool peek(Ty* pData)
        {
            BX_ASSERT(pData);
            if (m_first) {
                *pData = m_first->data;
                return true;
            } else {
                return false;
            }
        }

        inline bool isEmpty() const
        {
            return m_first == nullptr;
        }

        inline const Node* getFirst() const
        {
            return m_first;
        }

        inline const Node* getLast() const
        {
            return m_last;
        }

    private:
        Node* m_first;
        Node* m_last;
    };

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
