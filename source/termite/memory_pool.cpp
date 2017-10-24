#include "pch.h"

#include "memory_pool.h"
#include <inttypes.h>

#include "bx/cpu.h"
#include "bxx/lock.h"
#include "bxx/linked_list.h"
#include "bxx/pool.h"
#include "bxx/logger.h"
#include "bxx/linear_allocator.h"
#include "bx/string.h"

#define T_IMGUI_API
#include "plugin_api.h"

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
    bx::RwLock lock;

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

bx::AllocatorI* termite::allocMemPage(uint64_t tag) T_THREAD_SAFE
{
    assert(g_mempool);

    bx::WriteLockScope lock(g_mempool->lock);

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

void termite::freeMemTag(uint64_t tag) T_THREAD_SAFE
{
    assert(g_mempool);

    bx::ReadLockScope lock(g_mempool->lock);

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

size_t termite::getNumMemPages() T_THREAD_SAFE
{
    return g_mempool->numPages;
}

size_t termite::getMemPoolAllocSize() T_THREAD_SAFE
{
    bx::ReadLockScope lock(g_mempool->lock);

    size_t pageSize = g_mempool->pageSize;
    size_t sz = 0;
    MemoryPage::LNode* node = g_mempool->pageList.getFirst();
    while (node) {
        MemoryPage* page = node->data;
        sz += page->linAlloc.getOffset();
        node = node->next;
    }

    return sz;
}


size_t termite::getMemTagAllocSize(uint64_t tag) T_THREAD_SAFE
{
    bx::ReadLockScope lock(g_mempool->lock);
    
    size_t pageSize = g_mempool->pageSize;
    size_t sz = 0;
    MemoryPage::LNode* node = g_mempool->pageList.getFirst();
    while (node) {
        MemoryPage* page = node->data;
        if (page->tag == tag)
            sz += page->linAlloc.getOffset();
        node = node->next;
    }

    return sz;
}

int termite::getMemTags(uint64_t* tags, int maxTags, size_t* pageSizes) T_THREAD_SAFE
{
    bx::ReadLockScope lock(g_mempool->lock);
    
    int count = 0;
    MemoryPage::LNode* node = g_mempool->pageList.getFirst();
    while (node && count < maxTags) {
        MemoryPage* page = node->data;
        if (pageSizes)
            pageSizes[count] = page->linAlloc.getOffset();
        tags[count++] = page->tag;
        node = node->next;
    }

    return count;
}

void termite::debugMemoryPool(ImGuiApi_v0* imgui)
{
    imgui->setNextWindowSize(ImVec2(350.0f, 200.0f), ImGuiSetCond_FirstUseEver);
    if (imgui->begin("Memory Pool", nullptr, 0)) {
        uint64_t tags[512];
        size_t pageSizes[512];
        int numTags = getMemTags(tags, BX_COUNTOF(tags), pageSizes);

        static ImGuiGraphData<128> memGraph;

        char name[64];
        static char selName[64];
        static int selected = -1;
        imgui->columns(2, "MemoryPageList", false);
        for (int i = 0; i < numTags; i++) {
            bx::snprintf(name, sizeof(name), "0x%" PRIx64, tags[i]);

            if (imgui->selectable(name, i == selected, ImGuiSelectableFlags_DontClosePopups, ImVec2(0, 0))) {
                memGraph.reset();
                selected = i;
                bx::strCopy(selName, sizeof(selName), name);
            }

            imgui->nextColumn();
            imgui->setColumnOffset(1, 200.0f);

            imgui->text("%" PRIu32 "KB", pageSizes[i]/1024);

            if (i == selected)
                memGraph.add(float(pageSizes[i]/1024));
            imgui->nextColumn();
        }

        imgui->columns(1, nullptr, false);
        if (selected != -1) {
            imgui->plotHistogram(selName, memGraph.getValues(), memGraph.getCount(), 0, nullptr, 0,
                                 float(g_mempool->pageSize/1024), ImVec2(0, 100.0f), 4);
        }
    }
    imgui->end();
}

void* termite::PageAllocator::realloc(void* _ptr, size_t _size, size_t _align, const char* _file, uint32_t _line)
{
    if (_size <= g_mempool->pageSize) {
        if (_size > 0) {
            if (!m_linAlloc) {
                m_linAlloc = allocMemPage(m_tag);
                if (!m_linAlloc)
                    return nullptr;
            }

            void* p = m_linAlloc->realloc(_ptr, _size, _align, _file, _line);
            if (!p) {
                m_linAlloc = allocMemPage(m_tag);
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
