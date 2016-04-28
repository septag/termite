#pragma once

#include "bx/allocator.h"

namespace bx
{
    template <typename Ty>
    struct StackNode
    {
        StackNode<Ty>* down;
        Ty data;

        StackNode()
        {
            down = nullptr;
        }
    };

    template <typename Ty>
    void pushStackNode(StackNode<Ty>** _ref, StackNode<Ty>* _node, const Ty& _data)
    {
        _node->down = *_ref;
        *_ref = _node;
        _node->data = _data;
    }

    template <typename Ty>
    Ty popStack(StackNode<Ty>** _ref)
    {
        StackNode<Ty>* node = *_ref;
        if (node) {
            *_ref = node->down;
            node->down = nullptr;
        }
        return node->data;
    }

    template <typename Ty>
    Ty peekStack(StackNode<Ty>* _ref)
    {
        return _ref->data;
    }
}   // namespace bx