#pragma once

#include "bx/allocator.h"
#include "bx/cpu.h"

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
    class SpScUnboundedQueueAlloc
    {
        BX_CLASS(SpScUnboundedQueueAlloc
                 , NO_COPY
                 , NO_ASSIGNMENT
                 );

    public:
        explicit SpScUnboundedQueueAlloc(AllocatorI* _alloc)
            : m_first(BX_NEW(_alloc, Node))
            , m_divider(m_first)
            , m_last(m_first)
            , m_alloc(_alloc)
        {
        }

        ~SpScUnboundedQueueAlloc()
        {
            while (NULL != m_first) {
                Node* node = m_first;
                m_first = node->m_next;
                BX_DELETE(m_alloc, node);
            }
        }

        void push(const Ty& _value) // producer only
        {
            m_last->m_next = BX_NEW(m_alloc, Node)(_value);
            atomicExchangePtr((void**)&m_last, m_last->m_next);
            while (m_first != m_divider) {
                Node* node = m_first;
                m_first = m_first->m_next;
                BX_DELETE(m_alloc, node);
            }
        }

        const Ty& peek() const // consumer only
        {
            if (m_divider != m_last) {
                Ty& val = m_divider->m_next->m_value;
                return val;
            }

            return NULL;
        }

        Ty pop() // consumer only
        {
            if (m_divider != m_last) {
                Ty& val = m_divider->m_next->m_value;
                atomicExchangePtr((void**)&m_divider, m_divider->m_next);
                return val;
            }

            return Ty();
        }

    private:
        template <typename Tn> 
        struct Node_t
        {
            Node_t() : m_next(NULL)
            {
            }

            Node_t(const Tn& _value) 
                : m_value(_value)
                , m_next(NULL)
            {
            }

            Tn m_value;
            Node_t<Tn>* m_next;
        };

        typedef Node_t<Ty> Node;

        AllocatorI* m_alloc;
        Node* m_first;
        Node* m_divider;
        Node* m_last;
    };

}   // namespace: bx