#pragma once

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


}   // namespace: bx