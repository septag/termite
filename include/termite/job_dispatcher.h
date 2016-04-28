#pragma once

#include "bx/allocator.h"

namespace termite
{
    typedef void(*jobCallback)(int jobIndex, void* userParam);

    enum class jobPriority : int
    {
        High = 0,
        Normal,
        Low,

        Count
    };

    struct jobDesc
    {
        jobCallback callback;
        jobPriority priority;
        void* userParam;

        explicit jobDesc(jobCallback _callback, void* _userParam = nullptr, jobPriority _priority = jobPriority::Normal)
        {
            callback = _callback;
            userParam = _userParam;
            priority = _priority;
        }
    };

    typedef volatile int32_t jobCounter;
    typedef jobCounter* jobHandle;
    
    TERMITE_API jobHandle jobDispatchSmall(const jobDesc* jobs, uint16_t numJobs) T_THREAD_SAFE;
    TERMITE_API jobHandle jobDispatchBig(const jobDesc* jobs, uint16_t numJobs) T_THREAD_SAFE;
    TERMITE_API void jobWait(jobHandle handle) T_THREAD_SAFE;

    int jobInit(bx::AllocatorI* alloc, 
                uint16_t maxSmallFibers = 0, uint32_t smallFiberStackSize = 0, 
                uint16_t maxBigFibers = 0, uint32_t bigFiberStackSize = 0,
                bool lockThreadsToCores = true, uint8_t numWorkerThreads = UINT8_MAX);
    void jobShutdown();
    
} // namespace st


