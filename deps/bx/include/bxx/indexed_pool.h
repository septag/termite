// Original Author: Ali Salehi - https://github.com/lordhippo
// Adapted for BX by septag
#pragma once

#include "bx/bx.h"
#include "bx/allocator.h"
#include <cassert>
#include <utility>

#define BX_INDEXED_POOL_MAX_BUFFERS 4

namespace bx
{
    class IndexedPool
    {
        BX_CLASS(IndexedPool
            , NO_COPY
            , NO_ASSIGNMENT
        );

    public:
        IndexedPool();
        bool create(const uint32_t* itemSizes, int numBuffers, uint16_t maxItems, uint16_t growSize, AllocatorI* alloc);
        void destroy();
        ~IndexedPool();

        uint16_t newHandle();
        void freeHandle(uint16_t handle);

        void* getData(int bufferIdx);
        void* getHandleData(int bufferIdx, uint16_t handle);
        uint16_t getCount() const
        {
            return m_partition;
        }

		const uint16_t* getIndices() const
		{
			return m_indices;
		}

        uint16_t indexAt(uint16_t index) const
        {
            assert(index < m_partition);
            return m_indices[index];
        }

        template<typename _T>
        inline _T* getData(int bufferIdx)
        {
            return (_T*)getData(bufferIdx);
        }

        template<typename _T>
        inline _T* getHandleData(int bufferIdx, uint16_t handle)
        {
            return (_T*)getHandleData(bufferIdx, handle);
        }

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

    inline IndexedPool::IndexedPool()
    {
        m_alloc = nullptr;
        m_indices = nullptr;
        m_revIndices = nullptr;
        memset(m_buffers, 0x00, sizeof(uint8_t*)*BX_INDEXED_POOL_MAX_BUFFERS);
        memset(m_itemSizes, 0x00, sizeof(uint32_t)*BX_INDEXED_POOL_MAX_BUFFERS);
        m_maxItems = 0;
        m_partition = 0;
        m_numBuffers = 0;
    }

    inline bool IndexedPool::create(const uint32_t* itemSizes, int numBuffers, uint16_t maxItems, uint16_t growSize, AllocatorI* alloc)
    {
        assert(numBuffers <= BX_INDEXED_POOL_MAX_BUFFERS);
        assert(numBuffers > 0);

        m_alloc = alloc;
        m_maxItems = maxItems;
        m_partition = 0;
        m_growSize = growSize;
        m_numBuffers = numBuffers;
        memcpy(m_itemSizes, itemSizes, sizeof(uint32_t)*numBuffers);

        size_t totalSize = 2 * sizeof(uint16_t)*maxItems;
        for (int i = 0; i < numBuffers; i++) {
            totalSize += itemSizes[i] * maxItems;
        }
        
        uint8_t* buff = (uint8_t*)BX_ALLOC(alloc, totalSize);
        if (!buff)
            return false;

        m_indices = (uint16_t*)buff;       buff += sizeof(uint16_t)*maxItems;
        m_revIndices = (uint16_t*)buff;    buff += sizeof(uint16_t)*maxItems;

        for (int i = 0; i < numBuffers; i++) {
            m_buffers[i] = buff;                   
            buff += maxItems*itemSizes[i];
        }

        for (uint16_t i = 0; i < maxItems; ++i) {
            m_indices[i] = i;
            m_revIndices[i] = i;
        }

        return true;
    }

    inline void IndexedPool::destroy()
    {
        if (m_indices) {
            assert(m_alloc);
            BX_FREE(m_alloc, m_indices);
        }

        m_indices = m_revIndices = nullptr;
        m_alloc = nullptr;
        m_maxItems = m_partition = 0;
        memset(m_buffers, 0x00, sizeof(uint8_t*)*BX_INDEXED_POOL_MAX_BUFFERS);
        memset(m_itemSizes, 0x00, sizeof(uint32_t)*BX_INDEXED_POOL_MAX_BUFFERS);
    }

    inline IndexedPool::~IndexedPool()
    {
        assert(m_indices == nullptr);
        assert(m_revIndices == nullptr);
    }

    inline uint16_t IndexedPool::newHandle()
    {
        // Grow buffer if needed
        if (m_partition == m_maxItems) {
            uint8_t* buff = (uint8_t*)m_indices;
            uint16_t prevMax = m_maxItems;
            m_maxItems += m_growSize;
            size_t totalSize = 2 * sizeof(uint16_t)*m_maxItems;
            for (int i = 0; i < m_numBuffers; i++)
                totalSize += m_itemSizes[i] * m_maxItems;

            buff = (uint8_t*)BX_REALLOC(m_alloc, buff, totalSize);
            if (!buff)
                return UINT16_MAX;

            uint8_t* startBuff = buff;
            uint16_t* indices = (uint16_t*)buff;      buff += sizeof(uint16_t)*m_maxItems;
            uint16_t* revIndices = (uint16_t*)buff;   buff += sizeof(uint16_t)*m_maxItems;

            for (int i = 0; i < m_numBuffers; i++) {
                m_buffers[i] = buff;
                buff += m_itemSizes[i]*m_maxItems;
            }

            memcpy(indices, startBuff, sizeof(uint16_t)*prevMax);
            memcpy(revIndices, startBuff + sizeof(uint16_t)*prevMax, sizeof(uint16_t)*prevMax);

            for (uint16_t i = prevMax, c = m_maxItems; i < c; i++) {
                indices[i] = i;
                revIndices[i] = i;
            }

            m_indices = indices;
            m_revIndices = revIndices;
        }

        return m_indices[m_partition++];
    }

    inline void IndexedPool::freeHandle(uint16_t handle)
    {
        assert(handle < m_maxItems);

        uint16_t freeIndex = m_revIndices[handle];
        uint16_t moveIndex = m_partition - 1;

        uint16_t freeHdl = handle;
        uint16_t moveHdl = m_indices[m_partition - 1];

        assert(freeIndex < m_partition);

        std::swap(m_indices[freeIndex], m_indices[moveIndex]);
        std::swap(m_revIndices[freeHdl], m_revIndices[moveHdl]);
        m_partition--;
    }

    inline void* IndexedPool::getData(int bufferIdx)
    {
        assert(bufferIdx < m_numBuffers);
        return m_buffers[bufferIdx];
    }

    inline void* IndexedPool::getHandleData(int bufferIdx, uint16_t handle)
    {
        assert(bufferIdx < m_numBuffers);
        assert(handle < m_maxItems);
        return m_buffers[bufferIdx] + handle*m_itemSizes[bufferIdx];
    }

} // namespace bx