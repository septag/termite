#pragma once

namespace bx {
    inline bool HandlePool::create(const uint32_t itemSize, uint16_t maxItems, uint16_t growSize, AllocatorI* alloc)
    {
        return create(&itemSize, 1, maxItems, growSize, alloc);
    }

    inline uint16_t HandlePool::handleAt(uint16_t index) const
    {
        assert(index < m_partition);
        return m_indices[index];
    }

    inline void* HandlePool::getData(int bufferIdx)
    {
        assert(bufferIdx < m_numBuffers);
        return m_buffers[bufferIdx];
    }

    inline void* HandlePool::getHandleData(int bufferIdx, uint16_t handle)
    {
        assert(bufferIdx < m_numBuffers);
        assert(handle < m_maxItems);
        return m_buffers[bufferIdx] + handle*m_itemSizes[bufferIdx];
    }

    template<typename _T>
    _T* HandlePool::getHandleData(int bufferIdx, uint16_t handle)
    {
        return (_T*)getHandleData(bufferIdx, handle);
    }

    template<typename _T>
    _T* HandlePool::getData(int bufferIdx)
    {
        return (_T*)getData(bufferIdx);
    }
}