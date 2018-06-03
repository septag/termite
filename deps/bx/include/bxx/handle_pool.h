#pragma once

#ifndef BX_HANDLE_POOL_H_
#   define BX_HANDLE_POOL_H_

#include "bx/bx.h"
#include "bx/allocator.h"

#define BX_INDEXED_POOL_MAX_BUFFERS 4

namespace bx
{
    // Sparse Handle pool
    // No memory moving of main data will happen, but cache coherency will be damaged in large quantities
    // Original Author: Ali Salehi - https://github.com/lordhippo
    // Adapted for BX by septag
    class HandlePool
    {
        BX_CLASS(HandlePool
            , NO_COPY
            , NO_ASSIGNMENT
        );

    public:
        HandlePool();

        bool create(const uint32_t* itemSizes, int numBuffers, uint16_t maxItems, uint16_t growSize, AllocatorI* alloc);
        bool create(const uint32_t itemSize, uint16_t maxItems, uint16_t growSize, AllocatorI* alloc);

        void destroy();

        ~HandlePool();

        uint16_t newHandle();

        void freeHandle(uint16_t handle);

        void* getData(int bufferIdx);

        void* getHandleData(int bufferIdx, uint16_t handle);

        uint16_t getCount() const  { return m_partition; }
		const uint16_t* getIndices() const { return m_indices; }

        uint16_t handleAt(uint16_t index) const;

        template<typename _T>
        _T* getData(int bufferIdx);

        template<typename _T>
        _T* getHandleData(int bufferIdx, uint16_t handle);

    private:
        AllocatorI* m_alloc;
        uint16_t* m_indices;
        uint16_t* m_revIndices;
        uint8_t* m_buffers[BX_INDEXED_POOL_MAX_BUFFERS];
        uint32_t m_itemSizes[BX_INDEXED_POOL_MAX_BUFFERS];
        int m_numBuffers;
        uint16_t m_maxItems;
        uint16_t m_growSize;
        uint16_t m_partition;
    };
} // namespace bx

#include "inline/handle_pool.inl"

#endif
