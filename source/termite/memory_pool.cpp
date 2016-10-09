#include "pch.h"

#include "memory_pool.h"

#include "bxx/lock.h"
#include "bxx/linked_list.h"
#include "bxx/pool.h"
#include "bxx/linear_allocator.h"

using namespace termite;

#define DEFAULT_MAX_PAGES_PER_POOL 32     // 32 pages per pool
#define DEFAULT_PAGE_SIZE 2*1024*1024     // 2MB

struct Bucket;

struct Page
{
    typedef bx::ListNode<Page*> LNode;

    uint64_t tag;
    Bucket* owner;
    LNode lnode;
    bx::LinearAllocator linAlloc;

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

static MemoryPool* g_mempool = nullptr;

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
    bx::addListNode<Bucket*>(&g_mempool->buckets, &bucket->lnode, bucket);

    return bucket;
}

static void destroyBucket(Bucket* bucket, bx::AllocatorI* alloc)
{
    // Remove from bucket list
    bx::removeListNode<Bucket*>(&g_mempool->buckets, &bucket->lnode);

    BX_FREE(alloc, bucket);
}

result_t termite::initMemoryPool(bx::AllocatorI* alloc, size_t pageSize /*= 0*/, int maxPagesPerPool /* =0*/)
{
    if (g_mempool) {
        assert(false);
        return T_ERR_FAILED;
    }

    g_mempool = BX_NEW(alloc, MemoryPool);
    if (!g_mempool)
        return T_ERR_OUTOFMEM;
    g_mempool->alloc = alloc;

    maxPagesPerPool = maxPagesPerPool > 0 ? maxPagesPerPool : DEFAULT_MAX_PAGES_PER_POOL;
    pageSize = pageSize > 0 ? pageSize : DEFAULT_PAGE_SIZE;
    g_mempool->maxPagesPerBucket = maxPagesPerPool;
    g_mempool->pageSize = pageSize;

    return 0;
}

void termite::shutdownMemoryPool()
{
    if (!g_mempool) {
        return;
    }

    // Destroy buckets
    Bucket::LNode* bucket = g_mempool->buckets;
    while (bucket) {
        Bucket::LNode* next = bucket->next;
        destroyBucket(bucket->data, g_mempool->alloc);
        bucket = next;
    }

    BX_DELETE(g_mempool->alloc, g_mempool);
    g_mempool = nullptr;
}

static bx::AllocatorI* newPage(Bucket* bucket, uint64_t tag)
{
    Page* page = bucket->pagePtrs[--bucket->index];
    page->tag = tag;
    bx::addListNode<Page*>(&g_mempool->pages, &page->lnode, page);
    bx::atomicFetchAndAdd(&g_mempool->numPages, 1);
    page->linAlloc.reset();
    
    return &page->linAlloc;
}

bx::AllocatorI* termite::allocPage(uint64_t tag) T_THREAD_SAFE
{
    assert(g_mempool);

    bx::LockScope lk(g_mempool->lock);

    Bucket::LNode* node = g_mempool->buckets;
    while (node) {
        Bucket* bucket = node->data;
        if (bucket->index > 0) {
            return newPage(bucket, tag);
        }
        node = node->next;
    }
    
    // Create a new bucket
    Bucket* bucket = createBucket(g_mempool->pageSize, g_mempool->maxPagesPerBucket, g_mempool->alloc);
    if (!bucket) {
        BX_WARN("Out of memory");
        return nullptr;
    }

    return newPage(bucket, tag);
}

void termite::freeTag(uint64_t tag) T_THREAD_SAFE
{
    assert(g_mempool);

    bx::LockScope lk(g_mempool->lock);

    // Search all pages for the tag
    Page::LNode* node = g_mempool->pages;
    while (node) {
        Page::LNode* next = node->next;
        Page* page = node->data;
        if (page->tag == tag) {
            // Remove the page from bucket pool
            Bucket* bucket = page->owner;
            assert(page >= &bucket->pages[0] && page <= &bucket->pages[g_mempool->maxPagesPerBucket - 1]);
            assert(bucket->index != g_mempool->maxPagesPerBucket);

            bucket->pagePtrs[bucket->index++] = page;
            
            bx::atomicFetchAndSub(&g_mempool->numPages, 1);
            bx::removeListNode<Page*>(&g_mempool->pages, &page->lnode);
        }

        node = next;
    }
}

size_t termite::getNumPages() T_THREAD_SAFE
{
    return g_mempool->numPages;
}

size_t termite::getAllocSize() T_THREAD_SAFE
{
    return g_mempool->numPages*g_mempool->pageSize;
}

size_t termite::getTagSize(uint64_t tag) T_THREAD_SAFE
{
    bx::LockScope lk(g_mempool->lock);
    
    size_t pageSize = g_mempool->pageSize;
    size_t sz = 0;
    Page::LNode* node = g_mempool->pages;
    while (node) {
        Page* page = node->data;
        if (page->tag == tag)
            sz += pageSize;
        node = node->next;
    }

    return sz;
}



