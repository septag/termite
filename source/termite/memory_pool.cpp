#include "pch.h"

#include "memory_pool.h"

#include "bxx/lock.h"
#include "bxx/linked_list.h"
#include "bxx/pool.h"
#include "bxx/stack_allocator.h"

using namespace termite;

#define DEFAULT_MAX_PAGES_PER_POOL 32   // 32 pages per pool
#define DEFAULT_PAGE_SIZE 1024*2048     // 2MB

struct Bucket;

struct Page
{
    typedef bx::ListNode<Page*> LNode;

    uint64_t tag;
    Bucket* owner;
    LNode lnode;
    bx::StackAllocator linAlloc;

    Page(void* buff, size_t size) :
        linAlloc(buff, size)
    {
        tag = 0;
        owner = nullptr;
        lnode.next = lnode.prev = nullptr;
    }
};

struct Bucket
{
    typedef bx::ListNode<Bucket*> LNode;

    Page* pages;
    Page** pagePtrs;
    Bucket* next;
    int index;
    uint8_t* buffer;
    LNode lnode;
};

struct MemoryPool
{
    bx::AllocatorI* alloc;
    int maxPagesPerBucket;
    volatile int32_t numPages;
    size_t pageSize;
    Bucket::LNode* buckets;
    Page::LNode* pages;
    bx::Lock lock;

    MemoryPool()
    {
        alloc = nullptr;
        maxPagesPerBucket = 0;
        numPages = 0;
        pageSize = 0;
        buckets = nullptr;
        pages = nullptr;
    }
};

static MemoryPool* gMemPool = nullptr;

static Bucket* createBucket(size_t pageSize, int maxPages, bx::AllocatorI* alloc)
{
    size_t totalSize =
        sizeof(Bucket) +
        sizeof(Page)*maxPages +
        sizeof(Page*)*maxPages +
        pageSize*maxPages;

    uint8_t* buff = (uint8_t*)BX_ALLOC(alloc, totalSize);
    if (!buff)
        return nullptr;
    
    Bucket* bucket = (Bucket*)buff;     buff += sizeof(Bucket);
    bucket->pages = (Page*)buff;        buff += sizeof(Page)*maxPages;
    bucket->pagePtrs = (Page**)buff;    buff += sizeof(Page**)*maxPages;
    bucket->buffer = buff;

    for (int i = 0; i < maxPages; i++) {
        // Init page
        uint8_t* pageBuff = buff + i*pageSize;
        bucket->pagePtrs[maxPages - i - 1] = new(bucket->pages + i) Page(pageBuff, pageSize);
        bucket->pages[i].owner = bucket;
    }

    bucket->index = maxPages;

    // Add to bucket list
    bx::addListNode<Bucket*>(&gMemPool->buckets, &bucket->lnode, bucket);

    return bucket;
}

static void destroyBucket(Bucket* bucket, bx::AllocatorI* alloc)
{
    // Remove from bucket list
    bx::removeListNode<Bucket*>(&gMemPool->buckets, &bucket->lnode);

    BX_FREE(alloc, bucket);
}

result_t termite::memInit(bx::AllocatorI* alloc, size_t pageSize /*= 0*/, int maxPagesPerPool /* =0*/)
{
    if (gMemPool) {
        assert(false);
        return T_ERR_FAILED;
    }

    gMemPool = BX_NEW(alloc, MemoryPool);
    if (!gMemPool)
        return T_ERR_OUTOFMEM;
    gMemPool->alloc = alloc;

    maxPagesPerPool = maxPagesPerPool > 0 ? maxPagesPerPool : DEFAULT_MAX_PAGES_PER_POOL;
    pageSize = pageSize > 0 ? pageSize : DEFAULT_PAGE_SIZE;
    gMemPool->maxPagesPerBucket = maxPagesPerPool;
    gMemPool->pageSize = pageSize;

    return T_OK;
}

void termite::memShutdown()
{
    if (!gMemPool) {
        return;
    }

    // Destroy buckets
    Bucket::LNode* bucket = gMemPool->buckets;
    while (bucket) {
        Bucket::LNode* next = bucket->next;
        destroyBucket(bucket->data, gMemPool->alloc);
        bucket = next;
    }

    BX_DELETE(gMemPool->alloc, gMemPool);
    gMemPool = nullptr;
}

static bx::AllocatorI* allocPage(Bucket* bucket, uint64_t tag)
{
    Page* page = bucket->pagePtrs[--bucket->index];
    page->tag = tag;
    bx::addListNode<Page*>(&gMemPool->pages, &page->lnode, page);
    bx::atomicFetchAndAdd(&gMemPool->numPages, 1);
    return &page->linAlloc;
}

bx::AllocatorI* termite::memAllocPage(uint64_t tag) T_THREAD_SAFE
{
    assert(gMemPool);

    bx::LockScope lk(gMemPool->lock);

    Bucket::LNode* node = gMemPool->buckets;
    while (node) {
        Bucket* bucket = node->data;
        if (bucket->index > 0) {
            return allocPage(bucket, tag);
        }
        node = node->next;
    }
    
    // Create a new bucket
    Bucket* bucket = createBucket(gMemPool->pageSize, gMemPool->maxPagesPerBucket, gMemPool->alloc);
    if (!bucket) {
        BX_WARN("Out of memory");
        return nullptr;
    }
    return allocPage(bucket, tag);
}

void termite::memFreeTag(uint64_t tag) T_THREAD_SAFE
{
    assert(gMemPool);

    bx::LockScope lk(gMemPool->lock);

    // Search all pages for the tag
    Page::LNode* node = gMemPool->pages;
    while (node) {
        Page::LNode* next = node->next;
        Page* page = node->data;
        if (page->tag == tag) {
            // Remove the page from bucket pool
            Bucket* bucket = page->owner;
            assert(page >= &bucket->pages[0] && page <= &bucket->pages[gMemPool->maxPagesPerBucket - 1]);
            assert(bucket->index != gMemPool->maxPagesPerBucket);

            bucket->pagePtrs[bucket->index++] = page;
            
            bx::atomicFetchAndSub(&gMemPool->numPages, 1);
            bx::removeListNode<Page*>(&gMemPool->pages, &page->lnode);
        }

        node = next;
    }
}

size_t termite::memGetNumPages() T_THREAD_SAFE
{
    return gMemPool->numPages;
}

size_t termite::memGetAllocSize() T_THREAD_SAFE
{
    return gMemPool->numPages*gMemPool->pageSize;
}

size_t termite::memGetTagSize(uint64_t tag) T_THREAD_SAFE
{
    bx::LockScope lk(gMemPool->lock);
    
    size_t pageSize = gMemPool->pageSize;
    size_t sz = 0;
    Page::LNode* node = gMemPool->pages;
    while (node) {
        Page* page = node->data;
        if (page->tag == tag)
            sz += pageSize;
        node = node->next;
    }

    return sz;
}



