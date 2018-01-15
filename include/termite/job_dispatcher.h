#pragma once

#include "bx/allocator.h"

namespace tee
{
    // Note: Important - Cannot use and should not use Mutexes inside this callback
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
    
    TEE_API JobHandle dispatchSmallJobs(const JobDesc* jobs, uint16_t numJobs) TEE_THREAD_SAFE;
    TEE_API JobHandle dispatchBigJobs(const JobDesc* jobs, uint16_t numJobs) TEE_THREAD_SAFE;

    /// Waits on job until all sub-tasks are finished, Deletes after wait automatically
    TEE_API void waitAndDeleteJob(JobHandle handle) TEE_THREAD_SAFE;

    /// This api is used for non-blocking mode, check if job is done, then delete it
    TEE_API bool isJobDone(JobHandle handle) TEE_THREAD_SAFE;
    TEE_API void deleteJob(JobHandle handle) TEE_THREAD_SAFE;
	TEE_API uint8_t getNumWorkerThreads();    
} // namespace tee


