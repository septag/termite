#pragma once

#include "bx/allocator.h"

namespace termite
{
    typedef void(*JobCallback)(int jobIndex, void* userParam);

    struct JobPriority
    {
        enum Enum
        {
            High = 0,
            Normal,
            Low,
            Count
        };
    };

    struct JobDesc
    {
        JobCallback callback;
        JobPriority::Enum priority;
        void* userParam;

        JobDesc()
        {
            callback = nullptr;
            priority = JobPriority::Normal;
            userParam = nullptr;
        }

        explicit JobDesc(JobCallback _callback, void* _userParam = nullptr, JobPriority::Enum _priority = JobPriority::Normal)
        {
            callback = _callback;
            userParam = _userParam;
            priority = _priority;
        }
    };

    typedef volatile int32_t JobCounter;
    typedef JobCounter* JobHandle;
    
    TERMITE_API JobHandle dispatchSmallJobs(const JobDesc* jobs, uint16_t numJobs) T_THREAD_SAFE;
    TERMITE_API JobHandle dispatchBigJobs(const JobDesc* jobs, uint16_t numJobs) T_THREAD_SAFE;
    TERMITE_API void waitJobs(JobHandle handle) T_THREAD_SAFE;

    result_t initJobDispatcher(bx::AllocatorI* alloc, 
                               uint16_t maxSmallFibers = 0, uint32_t smallFiberStackSize = 0,
                               uint16_t maxBigFibers = 0, uint32_t bigFiberStackSize = 0,
                               bool lockThreadsToCores = true, uint8_t numWorkerThreads = UINT8_MAX);
    void shutdownJobDispatcher();
	uint8_t getNumWorkerThreads();    
} // namespace termite


