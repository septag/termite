#include "pch.h"

#include "memory_pool.h"

#include "bx/mutex.h"
#include "bxx/linked_list.h"
#include "bxx/pool.h"
#include "bxx/linear_allocator.h"
#include "bxx/logger.h"

using namespace termite;

#define DEFAULT_MAX_PAGES_PER_POOL 32     // 32 pages per pool
#define DEFAULT_PAGE_SIZE 2*1024*1024     // 2MB

struct PageBucket;

struct MemoryPage
{
    typedef bx::List<MemoryPage*>::Node LNode;

    uint64_t tag;
    PageBucket* owner;
    bx::LinearAllocator linAlloc;
    LNode lnode;

    MemoryPage(void* buff, size_t size) :
        tag(0),
        owner(nullptr),
        linAlloc(buff, size),
        lnode(this)
    {
    }
};

struct PageBucket
{
    typedef bx::List<PageBucket*>::Node LNode;

    MemoryPage* pages;
    MemoryPage** pagePtrs;
    int index;
    LNode lnode;

    PageBucket() :
        pages(nullptr),
        pagePtrs(nullptr),
        index(-1),
        lnode(this)
    {
    }
};

struct MemoryPool
{
    bx::AllocatorI* alloc;
    int maxPagesPerBucket;
    volatile int32_t numPages;
    size_t pageSize;
    bx::List<PageBucket*> bucketList;
    bx::List<MemoryPage*> pageList;
    bx::Mutex mutex;

    MemoryPool()
    {
        alloc = nullptr;
        maxPagesPerBucket = 0;
        numPages = 0;
        pageSize = 0;
    }
};

static MemoryPool* g_mempool = nullptr;

static PageBucket* createBucket(size_t pageSize, int maxPages, bx::AllocatorI* alloc)
{
    size_t totalSize =
        sizeof(PageBucket) +
        sizeof(MemoryPage)*maxPages +
        sizeof(MemoryPage*)*maxPages +
        pageSize*maxPages;

    uint8_t* buff = (uint8_t*)BX_ALLOC(alloc, totalSize);
    if (!buff)
        return nullptr;
    
    PageBucket* bucket = new(buff) PageBucket();     buff += sizeof(PageBucket);
    bucket->pages = (MemoryPage*)buff;               buff += sizeof(MemoryPage)*maxPages;
    bucket->pagePtrs = (MemoryPage**)buff;           buff += sizeof(MemoryPage*)*maxPages;

    for (int i = 0; i < maxPages; i++) {
        // Init page
        bucket->pagePtrs[maxPages - i - 1] = new(bucket->pages + i) MemoryPage(buff, pageSize);
        bucket->pages[i].owner = bucket;
        buff += pageSize;
    }

    bucket->index = maxPages;

    // Add to bucket list
    g_mempool->bucketList.add(&bucket->lnode);

    return bucket;
}

static void destroyBucket(PageBucket* bucket, bx::AllocatorI* alloc)
{
    // Remove from bucket list
    g_mempool->bucketList.remove(&bucket->lnode);
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
    PageBucket::LNode* bucket = g_mempool->bucketList.getFirst();
    while (bucket) {
        PageBucket::LNode* next = bucket->next;
        destroyBucket(bucket->data, g_mempool->alloc);
        bucket = next;
    }

    BX_DELETE(g_mempool->alloc, g_mempool);
    g_mempool = nullptr;
}

static bx::AllocatorI* newPage(PageBucket* bucket, uint64_t tag)
{
    MemoryPage* page = bucket->pagePtrs[--bucket->index];
    page->tag = tag;
    g_mempool->pageList.add(&page->lnode);
    bx::atomicFetchAndAdd(&g_mempool->numPages, 1);
    page->linAlloc.reset();

    return &page->linAlloc;
}

bx::AllocatorI* termite::allocPage(uint64_t tag) T_THREAD_SAFE
{
    assert(g_mempool);

    bx::MutexScope mutex(g_mempool->mutex);

    PageBucket::LNode* node = g_mempool->bucketList.getFirst();
    while (node) {
        PageBucket* bucket = node->data;
        if (bucket->index > 0)
            return ::newPage(bucket, tag);
        node = node->next;
    }
    
    // Create a new bucket
    PageBucket* bucket = createBucket(g_mempool->pageSize, g_mempool->maxPagesPerBucket, g_mempool->alloc);
    if (!bucket) {
        BX_WARN("Out of memory for Tag '%d'", tag);
        return nullptr;
    }

    return ::newPage(bucket, tag);
}

void termite::freeTag(uint64_t tag) T_THREAD_SAFE
{
    assert(g_mempool);

    bx::MutexScope mutex(g_mempool->mutex);

    // Search all pages for the tag
    MemoryPage::LNode* node = g_mempool->pageList.getFirst();
    while (node) {
        MemoryPage::LNode* next = node->next;
        MemoryPage* page = node->data;
        if (page->tag == tag) {
            // Remove the page from bucket pool
            PageBucket* bucket = page->owner;
            assert(page >= &bucket->pages[0] && page <= &bucket->pages[g_mempool->maxPagesPerBucket - 1]);
            assert(bucket->index != g_mempool->maxPagesPerBucket);

            bucket->pagePtrs[bucket->index++] = page;
            
            bx::atomicFetchAndSub(&g_mempool->numPages, 1);
            g_mempool->pageList.remove(node);
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
    bx::MutexScope mutex(g_mempool->mutex);
    
    size_t pageSize = g_mempool->pageSize;
    size_t sz = 0;
    MemoryPage::LNode* node = g_mempool->pageList.getFirst();
    while (node) {
        MemoryPage* page = node->data;
        if (page->tag == tag)
            sz += pageSize;
        node = node->next;
    }

    return sz;
}

void* termite::PageAllocator::realloc(void* _ptr, size_t _size, size_t _align, const char* _file, uint32_t _line)
{
    if (_size <= g_mempool->pageSize) {
        if (_size > 0) {
            if (!m_linAlloc) {
                m_linAlloc = allocPage(m_tag);
                if (!m_linAlloc)
                    return nullptr;
            }

            void* p = m_linAlloc->realloc(_ptr, _size, _align, _file, _line);
            if (!p) {
                m_linAlloc = allocPage(m_tag);
                if (m_linAlloc)
                    p = m_linAlloc->realloc(_ptr, _size, _align, _file, _line);
            }
            return p;
        }
    } else {
        BX_WARN("Invalid memory requested from memory pool (requested: %d, max: %d)", _size, g_mempool->pageSize);
    }
    return nullptr;
}
