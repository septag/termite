#pragma once

// stb_leakcheck.h - v0.2 - quick & dirty malloc leak-checking - public domain
#ifdef STB_LEAKCHECK_IMPLEMENTATION
#undef STB_LEAKCHECK_IMPLEMENTATION // don't implenment more than once

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <assert.h>
typedef struct malloc_info stb_leakcheck_malloc_info;

struct malloc_info
{
   const char *file;
   int line;
   size_t size;
   stb_leakcheck_malloc_info *next,*prev;
};

static stb_leakcheck_malloc_info *mi_head;

void *stb_leakcheck_malloc(size_t sz, const char *file, int line)
{
   stb_leakcheck_malloc_info *mi = (stb_leakcheck_malloc_info *) malloc(sz + sizeof(*mi));
   if (mi == NULL) return mi;
   mi->file = file;
   mi->line = line;
   mi->next = mi_head;
   if (mi_head)
      mi->next->prev = mi;
   mi->prev = NULL;
   mi->size = (int) sz;
   mi_head = mi;
   return mi+1;
}

void stb_leakcheck_free(void *ptr)
{
   if (ptr != NULL) {
      stb_leakcheck_malloc_info *mi = (stb_leakcheck_malloc_info *) ptr - 1;
      mi->size = ~mi->size;
      #ifndef STB_LEAKCHECK_SHOWALL
      if (mi->prev == NULL) {
         assert(mi_head == mi);
         mi_head = mi->next;
      } else
         mi->prev->next = mi->next;
      if (mi->next)
         mi->next->prev = mi->prev;
      #endif
   }
}

void *stb_leakcheck_realloc(void *ptr, size_t sz, const char *file, int line)
{
   if (ptr == NULL) {
      return stb_leakcheck_malloc(sz, file, line);
   } else if (sz == 0) {
      stb_leakcheck_free(ptr);
      return NULL;
   } else {
      stb_leakcheck_malloc_info *mi = (stb_leakcheck_malloc_info *) ptr - 1;
      if (sz <= mi->size)
         return ptr;
      else {
         #ifdef STB_LEAKCHECK_REALLOC_PRESERVE_MALLOC_FILELINE
         void *q = stb_leakcheck_malloc(sz, mi->file, mi->line);
         #else
         void *q = stb_leakcheck_malloc(sz, file, line);
         #endif
         if (q) {
            memcpy(q, ptr, mi->size);
            stb_leakcheck_free(ptr);
         }
         return q;
      }
   }
}

void stb_leakcheck_dumpmem(void)
{
   stb_leakcheck_malloc_info *mi = mi_head;
   while (mi) {
      if ((ptrdiff_t) mi->size >= 0)
         printf("LEAKED: %s (%4d): %8d bytes at %p\n", mi->file, mi->line, mi->size, mi+1);
      mi = mi->next;
   }
   #ifdef STB_LEAKCHECK_SHOWALL
   mi = mi_head;
   while (mi) {
      if ((ptrdiff_t) mi->size < 0)
         printf("FREED : %s (%4d): %8d bytes at %p\n", mi->file, mi->line, ~mi->size, mi+1);
      mi = mi->next;
   }
   #endif
}
#endif // STB_LEAKCHECK_IMPLEMENTATION

extern void * stb_leakcheck_malloc(size_t sz, const char *file, int line);
extern void * stb_leakcheck_realloc(void *ptr, size_t sz, const char *file, int line);
extern void   stb_leakcheck_free(void *ptr);
extern void   stb_leakcheck_dumpmem(void);

#include "../bx/allocator.h"

namespace bx
{
    //
    class LeakCheckAllocator : public ReallocatorI
    {
    public:
        LeakCheckAllocator()
        {
        }

        virtual ~LeakCheckAllocator()
        {
        }

        virtual void* alloc(size_t _size, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
        {
            if (BX_CONFIG_ALLOCATOR_NATURAL_ALIGNMENT >= _align)
            {
                return stb_leakcheck_malloc(_size, _file, _line);
            }

            return bx::alignedAlloc(this, _size, _align, _file, _line);
        }

        virtual void free(void* _ptr, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
        {
            if (BX_CONFIG_ALLOCATOR_NATURAL_ALIGNMENT >= _align)
            {
                stb_leakcheck_free(_ptr);
                return;
            }

            bx::alignedFree(this, _ptr, _align, _file, _line);
        }

        virtual void* realloc(void* _ptr, size_t _size, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
        {
            if (BX_CONFIG_ALLOCATOR_NATURAL_ALIGNMENT >= _align)
            {
                return stb_leakcheck_realloc(_ptr, _size, _file, _line);
            }

            return bx::alignedRealloc(this, _ptr, _size, _align, _file, _line);
        }
    };
}