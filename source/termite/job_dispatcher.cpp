#include "pch.h"

#include "job_dispatcher.h"
#include "internal.h"

#include "bx/cpu.h"
#include "bx/thread.h"
#include "bx/uint32_t.h"
#include "bx/string.h"
#include "bxx/pool.h"
#include "bxx/stack.h"
#include "bxx/linked_list.h"
#include "bxx/lock.h"

#include "fcontext/fcontext.h"

#include <mutex>
#include <thread>

using namespace tee;

#define DEFAULT_MAX_SMALL_FIBERS 128
#define DEFAULT_MAX_BIG_FIBERS 32
#define DEFAULT_SMALL_STACKSIZE 65536   // 64kb
#define DEFAULT_BIG_STACKSIZE 524288   // 512kb
#define MAX_WAIT_STACKS 32
#define WAIT_STACK_SIZE 8192    // 8kb

class FiberPool;

struct Fiber
{
    typedef bx::List<Fiber*>::Node LNode;

    uint32_t ownerThread;    // by default, owner thread is 0, which indicates that thread owns this fiber
                             // If we wait on a job (fiber), owner thread gets a valid value
    uint16_t jobIndex;
    uint16_t stackIndex;
    JobCounter* counter;
    JobCounter* waitCounter;
    fcontext_t context;
    FiberPool* ownerPool;

    LNode lnode;

    JobCallback callback;
    JobPriority::Enum priority;
    void* userData;

    Fiber() :
        lnode(this)
    {
    }
};

class FiberPool
{
private:
    bx::AllocatorI* m_alloc;

    Fiber* m_fibers;
    Fiber** m_ptrs;
    fcontext_stack_t* m_stacks;

    uint16_t m_maxFibers;
    int32_t m_index;

    bx::Lock m_lock;

public:
    FiberPool();
    bool create(uint16_t maxFibers, uint32_t stackSize, bx::AllocatorI* alloc);
    void destroy();

    Fiber* newFiber(JobCallback callbackFn, void* userData, uint16_t index, JobPriority::Enum priority, FiberPool* pool,
                    JobCounter* counter);
    void deleteFiber(Fiber* fiber);

    inline uint16_t getMax() const
    {
        return m_maxFibers;
    }
};

struct ThreadData
{
    Fiber* running;     // Current running fiber
    fcontext_stack_t stacks[MAX_WAIT_STACKS];
    int stackIdx;
    bool main;
    uint32_t threadId;   

    ThreadData()
    {
        running = nullptr;
        stackIdx = 0;
        main = false;
        threadId = 0;
        bx::memSet(stacks, 0x00, sizeof(stacks));
    }
};

struct CounterContainer
{
    JobCounter counter;
};

struct JobDispatcher
{
    bx::AllocatorI* alloc;
    bx::Thread** threads;
    uint8_t numThreads;
    FiberPool smallFibers;
    FiberPool bigFibers;

    bx::List<Fiber*> waitList[JobPriority::Count];  // 3 lists for each priority
    bx::Lock jobLock;
    bx::Lock counterLock;
    bx::TlsData threadData;
    volatile int32_t stop;

    fcontext_stack_t mainStack;
    bx::FixedPool<CounterContainer> counterPool;
    JobCounter dummyCounter;

    bx::Semaphore semaphore;

    JobDispatcher()
    {
        alloc = nullptr;
        threads = nullptr;
        numThreads = 0;
        stop = 0;
        dummyCounter = 0;
        bx::memSet(&mainStack, 0x00, sizeof(mainStack));
    }
};

static JobDispatcher* gDispatcher = nullptr;

FiberPool::FiberPool()
{
    m_fibers = nullptr;
    m_stacks = nullptr;
    m_maxFibers = 0;
    m_index = 0;
    m_ptrs = nullptr;
    m_alloc = nullptr;
}

bool FiberPool::create(uint16_t maxFibers, uint32_t stackSize, bx::AllocatorI* alloc)
{
    m_alloc = alloc;

    // Create pool structure
    size_t totalSize =
        sizeof(Fiber)*maxFibers +
        sizeof(Fiber*)*maxFibers +
        sizeof(fcontext_stack_t)*maxFibers;

    uint8_t* buff = (uint8_t*)BX_ALLOC(alloc, totalSize);
    if (!buff)
        return false;
    bx::memSet(buff, 0x00, totalSize);

    m_fibers = (Fiber*)buff;
    buff += sizeof(Fiber)*maxFibers;
    m_ptrs = (Fiber**)buff;
    buff += sizeof(Fiber*)*maxFibers;
    m_stacks = (fcontext_stack_t*)buff;

    for (uint16_t i = 0; i < maxFibers; i++)
        m_ptrs[maxFibers - i - 1] = &m_fibers[i];
    m_maxFibers = maxFibers;
    m_index = maxFibers;

    // Create contexts and their stack memories
    for (uint16_t i = 0; i < maxFibers; i++) {
        m_stacks[i] = create_fcontext_stack(stackSize);
        m_fibers[i].stackIndex = i;
        if (!m_stacks[i].sptr)
            return false;
    }

    return true;
}

void FiberPool::destroy()
{
    for (uint16_t i = 0; i < m_maxFibers; i++) {
        if (m_stacks[i].sptr)
            destroy_fcontext_stack(&m_stacks[i]);
    }

    // Free the whole buffer (context+ptrs+stacks)
    if (m_fibers)
        BX_FREE(m_alloc, m_fibers);
}

static ThreadData* createThreadData(bx::AllocatorI* alloc, uint32_t threadId, bool main)
{
    ThreadData* data = BX_NEW(alloc, ThreadData);
    if (!data)
        return nullptr;
    data->main = main;
    data->threadId = threadId;
    bx::memSet(data->stacks, 0x00, sizeof(fcontext_stack_t)*MAX_WAIT_STACKS);

    for (int i = 0; i < MAX_WAIT_STACKS; i++) {
        data->stacks[i] = create_fcontext_stack(WAIT_STACK_SIZE);
        if (!data->stacks[i].sptr)
            return nullptr;
    }

    return data;
}

static void destroyThreadData(ThreadData* data, bx::AllocatorI* alloc)
{
    for (int i = 0; i < MAX_WAIT_STACKS; i++) {
        if (data->stacks[i].sptr)
            destroy_fcontext_stack(&data->stacks[i]);
    }
    BX_DELETE(alloc, data);
}

static fcontext_stack_t* pushWaitStack(ThreadData* data)
{
    if (data->stackIdx == MAX_WAIT_STACKS)
        return nullptr;
    return &data->stacks[data->stackIdx++];
}

static fcontext_stack_t* popWaitStack(ThreadData* data)
{
    if (data->stackIdx == 0)
        return nullptr;
    return &data->stacks[--data->stackIdx];
}

static void fiberCallback(fcontext_transfer_t transfer)
{
    Fiber* fiber = (Fiber*)transfer.data;
    ThreadData* data = (ThreadData*)gDispatcher->threadData.get();

    data->running = fiber;

    // Call user task callback
    fiber->callback(fiber->jobIndex, fiber->userData);

    // Job is finished
    bx::atomicFetchAndSub(fiber->counter, 1);

    data->running = nullptr;

    // Delete the fiber
    fiber->ownerPool->deleteFiber(fiber);

    // Go back
    jump_fcontext(transfer.ctx, transfer.data);
}

Fiber* FiberPool::newFiber(JobCallback callbackFn, void* userData, uint16_t index, JobPriority::Enum priority, 
                           FiberPool* pool, JobCounter* counter)
{
    bx::LockScope lk(m_lock);
    if (m_index > 0) {
        Fiber* fiber = BX_PLACEMENT_NEW(m_ptrs[--m_index], Fiber);
        fiber->ownerThread = 0;
        fiber->context = make_fcontext(m_stacks[fiber->stackIndex].sptr, m_stacks[fiber->stackIndex].ssize, fiberCallback);
        fiber->callback = callbackFn;
        fiber->userData = userData;
        fiber->jobIndex = index;
        fiber->waitCounter = &gDispatcher->dummyCounter;
        fiber->counter = counter;
        fiber->priority = priority;
        fiber->ownerPool = pool;
        return fiber;
    } else {
        return nullptr;
    }
}

void FiberPool::deleteFiber(Fiber* fiber)
{
    bx::LockScope lk(m_lock);
    assert(m_index != m_maxFibers);
    m_ptrs[m_index++] = fiber;
}

static void jobPusherCallback(fcontext_transfer_t transfer)
{
    ThreadData* data = (ThreadData*)transfer.data;

    while (!gDispatcher->stop) {
        // Wait for a job to be placed in the job queue
        if (!data->main)
            gDispatcher->semaphore.wait();     // Decreases list counter on continue

        Fiber* fiber = nullptr;
        bool listNotEmpty = false;
        gDispatcher->jobLock.lock();
        {
            for (int i = 0; i < JobPriority::Count && !fiber; i++) {
                bx::List<Fiber*>& list = gDispatcher->waitList[i];
                Fiber::LNode* node = list.getFirst();
                while (node) {
                    listNotEmpty = true;
                    Fiber* f = node->data;
                    if (*f->waitCounter == 0) {     // Fiber is not waiting for any child tasks
                        if (f->ownerThread == 0 || f->ownerThread == data->threadId) {
                            // Job is ready to run, pull it from the wait list
                            fiber = f;
                            list.remove(node);
                            break;
                        }
                    }
                    node = node->next;
                }
            }
        }
        gDispatcher->jobLock.unlock();

        if (fiber) {
            // If jobPusher is called within another 'Wait' call, get back to it's context, which should be the same working thread
            // Else just run the job from beginning
            if (fiber->ownerThread) {
                fiber->ownerThread = 0;
                jump_fcontext(transfer.ctx, transfer.data);
            } else {
                jump_fcontext(fiber->context, fiber);
            }
        } else if (!data->main && listNotEmpty) {
            gDispatcher->semaphore.post(); // Increase the list counter because we didn't pull any jobs
        }

        // In the main thread, we have to quit the loop and get back to the caller
        if (data->main)
            break;
    }

    // Back to main thread func
    jump_fcontext(transfer.ctx, transfer.data);
}

static JobHandle dispatch(const JobDesc* jobs, uint16_t numJobs, FiberPool* pool) TEE_THREAD_SAFE
{
    // Get dispatcher counter to assign to jobs
    ThreadData* data = (ThreadData*)gDispatcher->threadData.get();

    // Get a counter
    gDispatcher->counterLock.lock();
    JobCounter* counter = &gDispatcher->counterPool.newInstance()->counter;
    gDispatcher->counterLock.unlock();
    if (!counter) {
        BX_WARN("Exceeded maximum jobCounters (Max = %d)", gDispatcher->counterPool.getMaxItems());
        return nullptr;
    }

    // Create N Fibers/Job
    uint32_t count = 0;
    Fiber** fibers = (Fiber**)alloca(sizeof(Fiber*)*numJobs);
    assert(fibers);

    for (uint16_t i = 0; i < numJobs; i++) {
        Fiber* fiber = pool->newFiber(jobs[i].callback, jobs[i].userParam, i, jobs[i].priority, pool, counter);
        if (fiber) {
            fibers[i] = fiber;
            count++;
        } else {
            BX_WARN("Exceeded maximum jobs (Max = %d)", pool->getMax());
        }
    }

    if (count > 0) {
        *counter = count;
        assert(data);       // Must be called within main or job_dispatcher threads only
        if (data->running)
            data->running->waitCounter = counter;

        gDispatcher->jobLock.lock();
        for (uint32_t i = 0; i < count; i++)
            gDispatcher->waitList[fibers[i]->priority].addToEnd(&fibers[i]->lnode);
        gDispatcher->jobLock.unlock();

        // post to semaphore so worker threads can continue and fetch them
        gDispatcher->semaphore.post(count);
    } else {
        // No job created ...
        bx::LockScope lock(gDispatcher->counterLock);
        gDispatcher->counterPool.deleteInstance((CounterContainer*)counter);
        counter = nullptr;
    }

    return counter;
}

JobHandle tee::dispatchSmallJobs(const JobDesc* jobs, uint16_t numJobs) TEE_THREAD_SAFE
{
    return dispatch(jobs, numJobs, &gDispatcher->smallFibers);
}

JobHandle tee::dispatchBigJobs(const JobDesc* jobs, uint16_t numJobs) TEE_THREAD_SAFE
{
    return dispatch(jobs, numJobs, &gDispatcher->bigFibers);
}

void tee::waitAndDeleteJob(JobHandle handle) TEE_THREAD_SAFE
{
    ThreadData* data = (ThreadData*)gDispatcher->threadData.get();

    while (*handle > 0) {
        // Jump back to worker thread (current job is not finished)
        fcontext_stack_t* stack = pushWaitStack(data);
        if (!stack) {
            BX_WARN("Maximum wait stacks '%d' exceeded. Cannot wait", MAX_WAIT_STACKS);
            return;
        }

        fcontext_t jobPusherCtx = make_fcontext(stack->sptr, stack->ssize, jobPusherCallback);

        // This means that user has called 'waitJobs' inside another running task
        // So the task is WIP, we have to re-add it to the pending jobs so it can continue later on
        if (data->running) {
            Fiber* fiber = data->running;
            data->running = nullptr;
            fiber->ownerThread = data->threadId;

            gDispatcher->jobLock.lock();
            gDispatcher->waitList[fiber->priority].addToEnd(&fiber->lnode);
            gDispatcher->jobLock.unlock();

            gDispatcher->semaphore.post();
        }

        // Switch to job-pusher To see if we can process any remaining jobs
        jump_fcontext(jobPusherCtx, data);
        popWaitStack(data);

        bx::yield();
    }

    // Delete the counter
    gDispatcher->counterLock.lock();
    gDispatcher->counterPool.deleteInstance((CounterContainer*)handle);
    gDispatcher->counterLock.unlock();
}

bool tee::isJobDone(JobHandle handle) TEE_THREAD_SAFE
{
    return *handle == 0;
}

void tee::deleteJob(JobHandle handle) TEE_THREAD_SAFE
{
    gDispatcher->counterLock.lock();
    gDispatcher->counterPool.deleteInstance((CounterContainer*)handle);
    gDispatcher->counterLock.unlock();
}

static int32_t threadFunc(bx::Thread* self, void* userData)
{
    // Initialize thread data
    ThreadData* data = createThreadData(gDispatcher->alloc, bx::getTid(), false);
    if (!data)
        return -1;
    gDispatcher->threadData.set(data);     

    fcontext_stack_t* stack = pushWaitStack(data);
    fcontext_t threadCtx = make_fcontext(stack->sptr, stack->ssize, jobPusherCallback);
    jump_fcontext(threadCtx, data);

    destroyThreadData(data, gDispatcher->alloc);
    return 0;
}

bool tee::initJobDispatcher(bx::AllocatorI* alloc, 
                            uint16_t maxSmallFibers, uint32_t smallFiberStackSize, 
                            uint16_t maxBigFibers, uint32_t bigFiberStackSize, 
                            bool lockThreadsToCores, uint8_t numThreads)
{
    if (gDispatcher) {
        assert(false);
        return false;
    }
    gDispatcher = BX_NEW(alloc, JobDispatcher);
    if (!gDispatcher)
        return false;
    gDispatcher->alloc = alloc;

    // Main thread data and stack
    gDispatcher->mainStack = create_fcontext_stack(8 * 1024);
    if (!gDispatcher->mainStack.sptr) {
        return false;
    }

    ThreadData* mainData = createThreadData(alloc, bx::getTid(), true);
    if (!mainData)
        return false;
    gDispatcher->threadData.set(mainData);

    // Create fibers with stack memories
    maxSmallFibers = maxSmallFibers ? maxSmallFibers : DEFAULT_MAX_SMALL_FIBERS;
    maxBigFibers = maxBigFibers ? maxBigFibers : DEFAULT_MAX_BIG_FIBERS;
    smallFiberStackSize = smallFiberStackSize ? smallFiberStackSize : DEFAULT_SMALL_STACKSIZE;
    bigFiberStackSize = bigFiberStackSize ? bigFiberStackSize : DEFAULT_BIG_STACKSIZE;

    if (!gDispatcher->counterPool.create(maxSmallFibers + maxBigFibers, alloc) ||
        !gDispatcher->bigFibers.create(maxBigFibers, bigFiberStackSize, alloc) ||
        !gDispatcher->smallFibers.create(maxSmallFibers, smallFiberStackSize, alloc)) 
    {
        return false;
    }

    // Create threads
    uint16_t numCores = numThreads;
    if (numThreads == UINT8_MAX) {
        numCores = std::min<uint16_t>(getHardwareInfo().numCores, UINT8_MAX);
        numCores = numCores ? (numCores - 1) : 0;   // NumThreads is always one less than number of cores
    }
    numThreads = (uint8_t)std::min<uint16_t>(numCores, numThreads);

    if (numThreads > 0) {
        gDispatcher->threads = (bx::Thread**)BX_ALLOC(alloc, sizeof(bx::Thread*)*numThreads);
        assert(gDispatcher->threads);
        
        gDispatcher->numThreads = numThreads;
        for (uint8_t i = 0; i < numThreads; i++) {
            gDispatcher->threads[i] = BX_NEW(alloc, bx::Thread);
            char name[32];
            bx::snprintf(name, sizeof(name), "TJobThread #%d", i + 1);
            gDispatcher->threads[i]->init(threadFunc, nullptr, 8 * 1024, name);
        }
    }
    return true;
}

void tee::shutdownJobDispatcher()
{
    if (!gDispatcher)
        return;

    // Command all worker threads to stop
    gDispatcher->stop = 1;
    gDispatcher->semaphore.post(gDispatcher->numThreads + 1);
    for (uint8_t i = 0; i < gDispatcher->numThreads; i++) {
        gDispatcher->threads[i]->shutdown();
        BX_DELETE(gDispatcher->alloc, gDispatcher->threads[i]);
    }
    BX_FREE(gDispatcher->alloc, gDispatcher->threads);

    ThreadData* data = (ThreadData*)gDispatcher->threadData.get();
    destroyThreadData(data, gDispatcher->alloc);

    gDispatcher->bigFibers.destroy();
    gDispatcher->smallFibers.destroy();
    destroy_fcontext_stack(&gDispatcher->mainStack);

    gDispatcher->counterPool.destroy();

    BX_DELETE(gDispatcher->alloc, gDispatcher);
    gDispatcher = nullptr;
}

uint8_t tee::getNumWorkerThreads()
{
	return gDispatcher->numThreads;
}
