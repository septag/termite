#include "pch.h"
#include "core.h"
#include "lang.h"

#include "bxx/hash_table.h"
#include "tinystl/hash.h"

#include "rapidjson/error/en.h"
#include "rapidjson/document.h"
#include "bxx/rapidjson_allocator.h"

using namespace rapidjson;

namespace termite
{
    class LangLoader : public ResourceCallbacksI
    {
    public:
        LangLoader() {}

        bool loadObj(const MemoryBlock* mem, const ResourceTypeParams& params, uintptr_t* obj, bx::AllocatorI* alloc) override;
        void unloadObj(uintptr_t obj, bx::AllocatorI* alloc) override;
        void onReload(ResourceHandle handle, bx::AllocatorI* alloc) override {}
    };

    //
    struct LangEntry
    {
        size_t idHash;
        char text[128];
    };

    struct Lang
    {
        bx::HashTableInt idTable;   // IdHash -> index to entries
        LangEntry* entries;
        int numEntries;

        Lang() :
            idTable(bx::HashTableType::Immutable),
            entries(nullptr),
            numEntries(0)
        {
        }
    };

    static LangLoader g_langLoader;

    const char* getText(Lang* lang, const char* strId)
    {
        if (lang) {
            int index = lang->idTable.find(tinystl::hash_string(strId, strlen(strId)));
            if (index != -1)
                return lang->entries[lang->idTable[index]].text;
        } 
        return "";
    }

    void registerLangToResourceLib()
    {
        ResourceTypeHandle handle;
        handle = registerResourceType("lang", &g_langLoader, 0, 0, 0);
        assert(handle.isValid());
    }

    bool LangLoader::loadObj(const MemoryBlock* mem, const ResourceTypeParams& params, uintptr_t* obj, bx::AllocatorI* alloc)
    {
        bx::AllocatorI* heapAlloc = getHeapAlloc();
        char* jsonStr = (char*)BX_ALLOC(heapAlloc, mem->size + 1);
        if (!jsonStr) {
            T_ERROR("Out of Memory");
            return false;
        }
        memcpy(jsonStr, mem->data, mem->size);
        jsonStr[mem->size] = 0;

        BxAllocatorStatic::Alloc = heapAlloc;
        BxAllocatorStatic jalloc;
        BxsAllocator jpool(4096, &jalloc);
        BxsDocument jdoc(&jpool, 1024, &jalloc);
        if (jdoc.ParseInsitu(jsonStr).HasParseError()) {
            T_ERROR("Parse Json Error: %s (Pos: %d)", GetParseError_En(jdoc.GetParseError()), jdoc.GetErrorOffset());
            BX_FREE(heapAlloc, jsonStr);
            return false;
        }

        const BxsValue& jEntries = jdoc.GetArray();
        if (jEntries.Size() == 0) {
            T_ERROR("Language File is empty");
            BX_FREE(heapAlloc, jsonStr);
            return false;
        }

        //
        size_t hashTableSize = bx::HashTableInt::GetImmutableSizeBytes(jEntries.Size());
        size_t totalSz = sizeof(Lang) + hashTableSize + sizeof(LangEntry)*jEntries.Size();

        uint8_t* buff = (uint8_t*)BX_ALLOC(alloc ? alloc : heapAlloc, totalSz);
        Lang* lang = new(buff) Lang;                                buff += sizeof(Lang);
        lang->idTable.createWithBuffer(jEntries.Size(), buff);      buff += hashTableSize;
        lang->entries = (LangEntry*)buff; 
        lang->numEntries = (int)jEntries.Size();

        for (int i = 0; i < lang->numEntries; i++) {
            LangEntry& entry = lang->entries[i];
            const BxsValue& jentry = jEntries[i];
            if (jentry.HasMember("Id")) {
                entry.idHash = tinystl::hash_string(jentry["Id"].GetString(), jentry["Id"].GetStringLength());
            } 
            if (jentry.HasMember("Value")) {
                memcpy(entry.text, jentry["Value"].GetString(), 
                       std::min<size_t>(sizeof(entry.text), jentry["Value"].GetStringLength()+1));
            }
            lang->idTable.add(entry.idHash, i);
        }

        BX_FREE(heapAlloc, jsonStr);

        *obj = uintptr_t(lang);
        return true;
    }

    void LangLoader::unloadObj(uintptr_t obj, bx::AllocatorI* alloc)
    {
        if (obj) {
            Lang* lang = (Lang*)obj;
            lang->idTable.destroy();
            BX_FREE(alloc ? alloc : getHeapAlloc(), lang);
        }
    }
}