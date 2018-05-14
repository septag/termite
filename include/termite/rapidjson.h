#pragma once

#include <algorithm>
#include "bx/allocator.h"
#include "bx/debug.h"

#include "rapidjson/allocators.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/prettywriter.h"

namespace tee
{
    namespace json {
        // NoFree Allocator. It's FAST. Can be used with Stack based allocators
        class TEE_API StackAllocator
        {
        private:
            bx::AllocatorI* m_alloc;

        public:
            static const bool kNeedFree = false;

            StackAllocator();
            explicit StackAllocator(bx::AllocatorI* alloc);
            void* Malloc(size_t size);
            void* Realloc(void* originalPtr, size_t originalSize, size_t newSize);
            static void Free(void *ptr) {}
        };

        // HeapAllocator. Allocates memory from the heap
        class TEE_API HeapAllocator
        {
        public:
            static const bool kNeedFree = true;

            HeapAllocator();
            void* Malloc(size_t size);
            void* Realloc(void* originalPtr, size_t originalSize, size_t newSize);
            static void Free(void *ptr);

            // This function must be called before any use
            static void SetAlloc(bx::AllocatorI* alloc);
        private:
            static bx::AllocatorI* Alloc;
        };

        // Stack based json types
        typedef rapidjson::MemoryPoolAllocator<StackAllocator> StackPoolAllocator;
        typedef rapidjson::GenericDocument<rapidjson::UTF8<>, rapidjson::MemoryPoolAllocator<StackAllocator>, StackAllocator> StackDocument;
        typedef rapidjson::GenericValue<rapidjson::UTF8<>, rapidjson::MemoryPoolAllocator<StackAllocator>> svalue_t;
        typedef rapidjson::GenericStringBuffer<rapidjson::UTF8<>, StackAllocator> StackStringBuffer;

        // Heap based json types
        typedef rapidjson::MemoryPoolAllocator<HeapAllocator> HeapPoolAllocator;
        typedef rapidjson::GenericDocument<rapidjson::UTF8<>, rapidjson::MemoryPoolAllocator<HeapAllocator>, HeapAllocator> HeapDocument;
        typedef rapidjson::GenericValue<rapidjson::UTF8<>, rapidjson::MemoryPoolAllocator<HeapAllocator>> hvalue_t;
        typedef rapidjson::GenericStringBuffer<rapidjson::UTF8<>, HeapAllocator> HeapStringBuffer;

        // Helpers for arrays
        template <typename _T> void getFloatArray(const _T& jvalue, float* f, int num);
        template <typename _T> void getIntArray(const _T& jvalue, int* n, int num);
        template <typename _T> void getUInt16Array(const _T& jvalue, uint16_t* n, int num);
        template <typename _T, typename _AllocT> _T createFloatArray(const float* f, int num, _AllocT& alloc);
        template <typename _T, typename _AllocT> _T createIntArray(const int* n, int num, _AllocT& alloc);
    } // namespace json

    TEE_API void testSax(const char* filepath);
} // namespace tee


#include "inline/rapidjson.inl"
