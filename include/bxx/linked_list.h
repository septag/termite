#pragma once

namespace bx
{
    template <typename Ty>
    struct ListNode
    {
        ListNode<Ty>* next;
        ListNode<Ty>* prev;
        Ty data;

        ListNode()
        {
            next = prev = nullptr;
        }
    };

    template <typename Ty> 
    void addListNode(ListNode<Ty>** _ref, ListNode<Ty>* _node, const Ty& _data)
    {
        _node->next = *_ref;
        _node->prev = nullptr;
        if (*_ref)
            (*_ref)->prev = _node;
        *_ref = _node;
        _node->data = _data;
    }

    template <typename Ty> 
    void addListNodeToEnd(ListNode<Ty>** _ref, ListNode<Ty>* _node, const Ty& _data)
    {
        if (*_ref)     {
            // Move to last node
            ListNode<Ty>* last = *_ref;
            while (last->next)
                last = last->next;
            last->next = _node;
            _node->prev = last;
            _node->next = nullptr;
        }    else    {
            *_ref = _node;
            _node->prev = _node->next = nullptr;
        }

        _node->data = _data;
    }

    template <typename Ty> 
    void removeListNode(ListNode<Ty>** _ref, ListNode<Ty>* _node)
    {
       if (_node->next)
           _node->next->prev = _node->prev;
       if (_node->prev)
           _node->prev->next = _node->next;
       if (*_ref == _node)
           *_ref = _node->next;
       _node->next = _node->prev = nullptr;
    }

    template <typename Ty> 
    void insertListNode(ListNode<Ty>* _insertAfter, ListNode<Ty>* _node, const Ty& _data)
    {
        if (_insertAfter->next)
            _insertAfter->next->prev = _node;
        _node->prev = _insertAfter;
        _node->next = _insertAfter->next;
        _insertAfter->next = _node;
        _node->data = _data;
    }
}
