#include "pch.h"
#include "component_system.h"

#include "bx/uint32_t.h"
#include "bxx/array.h"
#include "bxx/pool.h"
#include "bxx/queue.h"
#include "bxx/indexed_pool.h"
#include "bxx/hash_table.h"
#include "bxx/logger.h"

#define MIN_FREE_INDICES 1024

static const uint32_t kComponentIndexBits = 16;
static const uint32_t kComponentIndexMask = (1 << kComponentIndexBits) - 1;
static const uint32_t kComponentTypeHandleBits = 16;
static const uint32_t kComponentTypeHandleMask = (1 << kComponentTypeHandleBits) - 1;

#define COMPONENT_INSTANCE_INDEX(_Handle) uint16_t(_Handle.value & kComponentIndexMask)
#define COMPONENT_TYPE_INDEX(_Handle) uint16_t((_Handle.value >> kComponentIndexBits) & kComponentTypeHandleMask)
#define COMPONENT_MAKE_HANDLE(_CTypeIdx, _CIdx) ComponentHandle((uint32_t(_CTypeIdx) << kComponentTypeHandleBits) | uint32_t(_CIdx))

using namespace termite;

namespace termite
{
    struct EntityManager
    {
        struct FreeIndex
        {
            typedef bx::QueueNode<FreeIndex*> QNode;

            uint32_t index;
            QNode qnode;
        };

        bx::Pool<FreeIndex> freeIndexPool;
        FreeIndex::QNode* freeIndexQueue;
        uint32_t freeIndexSize;
        bx::Array<uint8_t> generations;
        bx::AllocatorI* alloc;
        bx::MultiHashTableInt destroyTable; // keep a multi-hash for all components that entity has to destroy
		bx::Pool<bx::MultiHashTableInt::Node> nodePool;
        
        EntityManager(bx::AllocatorI* _alloc) : 
            alloc(_alloc),
            destroyTable(bx::HashTableType::Mutable)
        {
            freeIndexSize = 0;
            freeIndexQueue = nullptr;
        }
    };
}

struct ComponentType
{
    char name[32];
    uint32_t id;
    ComponentCallbacks callbacks;
    ComponentFlag flags;
    uint32_t dataSize;
    bx::IndexedPool dataPool;
    bx::HashTableInt entTable;  // Entity -> ComponentHandle

    ComponentType() : 
        entTable(bx::HashTableType::Mutable)
    {
        strcpy(name, "");
        id = 0;
        memset(&callbacks, 0x00, sizeof(callbacks));
        flags = ComponentFlag::None;
        dataSize = 0;
    }
};

struct ComponentSystem
{
    bx::Array<ComponentType> components;
    bx::HashTableInt nameTable;
    bx::HashTableInt idTable;
    bx::AllocatorI* alloc;

    ComponentSystem(bx::AllocatorI* _alloc) : 
        alloc(_alloc),
        nameTable(bx::HashTableType::Mutable),
        idTable(bx::HashTableType::Mutable)
    {
    }
};

static ComponentSystem* g_csys = nullptr;

EntityManager* termite::createEntityManager(bx::AllocatorI* alloc, int bufferSize)
{
    EntityManager* emgr = BX_NEW(alloc, EntityManager)(alloc);
    if (!emgr)
        return nullptr;
    if (bufferSize <= 0)
        bufferSize = MIN_FREE_INDICES;
    if (!emgr->generations.create(bufferSize, bufferSize, alloc) ||
        !emgr->freeIndexPool.create(bufferSize, alloc) ||
		!emgr->nodePool.create(bufferSize, alloc) ||
        !emgr->destroyTable.create(bufferSize, alloc, &emgr->nodePool))
    {
        destroyEntityManager(emgr);
        return nullptr;
    }

    return emgr;
}

void termite::destroyEntityManager(EntityManager* emgr)
{
    assert(emgr);

    emgr->freeIndexPool.destroy();
	emgr->nodePool.destroy();
    emgr->generations.destroy();
    emgr->destroyTable.destroy();

    BX_DELETE(emgr->alloc, emgr);
}

Entity termite::createEntity(EntityManager* emgr)
{
    uint32_t idx;
    if (emgr->freeIndexSize > MIN_FREE_INDICES) {
        idx = bx::popQueue<EntityManager::FreeIndex*>(&emgr->freeIndexQueue)->index;
        emgr->freeIndexSize--;
    } else {
        idx = emgr->generations.getCount();
        uint8_t* gen = emgr->generations.push();
        *gen = 1;
        assert(idx < (1 << kEntityIndexBits));
    }
    return Entity(idx, emgr->generations[idx]);
}

static void destroyComponentNoImmDestroy(EntityManager* emgr, Entity ent, ComponentHandle handle)
{
    assert(handle.isValid());

    ComponentType& ctype = g_csys->components[COMPONENT_TYPE_INDEX(handle)];

    // Call destroy callback
    if (ctype.callbacks.destroyInstance)
        ctype.callbacks.destroyInstance(ent, handle);

    ctype.dataPool.freeHandle(COMPONENT_INSTANCE_INDEX(handle));

    int r = ctype.entTable.find(ent.id);
    if (r != -1)
        ctype.entTable.remove(r);
}

void termite::destroyEntity(EntityManager* emgr, Entity ent)
{
    assert(isEntityAlive(emgr, ent));

    // Check if the entity has immediate destroy components
    // And destroy all components registered to entity
    int entIdx = emgr->destroyTable.find(ent.id);
    if (entIdx != -1) {
        bx::MultiHashTableInt::Node* node = emgr->destroyTable.getNode(entIdx);
        while (node) {
            bx::MultiHashTableInt::Node* next = node->next;
            ComponentHandle component(uint32_t(node->value));
            destroyComponentNoImmDestroy(emgr, ent, component);

            emgr->destroyTable.remove(entIdx, node);
            node = next;
        }
    }

    uint32_t idx = ent.getIndex();
    ++emgr->generations[idx];
    
    EntityManager::FreeIndex* fi = emgr->freeIndexPool.newInstance();
    if (fi) {
        bx::pushQueueNode<EntityManager::FreeIndex*>(&emgr->freeIndexQueue, &fi->qnode, fi);
        emgr->freeIndexSize++;
    }
}

bool termite::isEntityAlive(EntityManager* emgr, Entity ent)
{
    return emgr->generations[ent.getIndex()] == ent.getGeneration();
}

result_t termite::initComponentSystem(bx::AllocatorI* alloc)
{
    if (g_csys) {
        assert(false);
        return T_ERR_ALREADY_INITIALIZED;
    }

    g_csys = BX_NEW(alloc, ComponentSystem)(alloc);
    if (!g_csys) {
        return T_ERR_OUTOFMEM;
    }

    if (!g_csys->components.create(32, 128, alloc)) {
        return T_ERR_OUTOFMEM;
    }

    if (!g_csys->nameTable.create(128, alloc) ||
        !g_csys->idTable.create(128, alloc))
    {
        return T_ERR_OUTOFMEM;
    }

    return 0;
}

void termite::shutdownComponentSystem()
{
    if (!g_csys)
        return;

    BX_BEGINP("Shutting down Component System");
    for (int i = 0; i < g_csys->components.getCount(); i++) {
        ComponentType& ctype = g_csys->components[i];
        ctype.dataPool.destroy();
        ctype.entTable.destroy();
    }
    g_csys->components.destroy();
    g_csys->nameTable.destroy();
    g_csys->idTable.destroy();
}

ComponentTypeHandle termite::registerComponentType(const char* name, uint32_t id, const ComponentCallbacks* callbacks, 
                                                   ComponentFlag flags, uint32_t dataSize, uint16_t poolSize, 
                                                   uint16_t growSize)
{
    assert(g_csys);
    assert(g_csys->components.getCount() < UINT16_MAX);

    ComponentType* buff = g_csys->components.push();
    if (!buff)
        return ComponentTypeHandle();

    ComponentType* ctype = new(buff) ComponentType();

    bx::strlcpy(ctype->name, name, sizeof(ctype->name));
    ctype->id = id;
    if (callbacks)
        memcpy(&ctype->callbacks, callbacks, sizeof(ComponentCallbacks));
    ctype->flags = flags;
    ctype->dataSize = dataSize;
    const uint32_t itemSizes[2] = {sizeof(Entity), dataSize};
    if (!ctype->dataPool.create(itemSizes, 2, poolSize, growSize, g_csys->alloc) ||
        !ctype->entTable.create(poolSize, g_csys->alloc)) 
    {
        return ComponentTypeHandle();
    }

    // Add to ComponentType database
    int index = g_csys->components.getCount() - 1;
    g_csys->idTable.add(id, index);
    g_csys->nameTable.add(bx::hashMurmur2A(name, (uint32_t)strlen(name)), index);

    return ComponentTypeHandle(uint16_t(index));
}

void termite::garbageCollectComponents(EntityManager* emgr)
{
    // For each component perform garbage collection
    for (int i = 0, c = g_csys->components.getCount(); i < c; i++) {
        ComponentType& ctype = g_csys->components[i];

        if ((ctype.flags & ComponentFlag::ImmediateDestroy) == ComponentFlag::None) {
            int aliveInRow = 0;
            while (ctype.dataPool.getCount() && aliveInRow < 4) {
                uint16_t r = ctype.dataPool.indexAt((uint16_t)getRandomIntUniform(0, (int)ctype.dataPool.getCount() - 1));
                Entity ent = *ctype.dataPool.getHandleData<Entity>(0, r);
                if (isEntityAlive(emgr, ent)) {
                    aliveInRow++;
                    continue;
                }

                aliveInRow = 0;
                destroyComponent(emgr, ent, COMPONENT_MAKE_HANDLE(i, r));
            }
        }
    }
}

ComponentHandle termite::createComponent(EntityManager* emgr, Entity ent, ComponentTypeHandle handle)
{
    ComponentType& ctype = g_csys->components[handle.value];

    if (ctype.entTable.find(ent.id) != -1)
        return ComponentHandle();

    uint16_t cIdx = ctype.dataPool.newHandle();
    if (cIdx == UINT16_MAX)
        return ComponentHandle();
    Entity* pEnt = ctype.dataPool.getHandleData<Entity>(0, cIdx);
    *pEnt = ent;

    ComponentHandle chandle = COMPONENT_MAKE_HANDLE(handle.value, cIdx);
    ctype.entTable.add(ent.id, int(chandle.value));

    if ((ctype.flags & ComponentFlag::ImmediateDestroy) != ComponentFlag::None) {
        emgr->destroyTable.add(ent.id, int(chandle.value));
    }

    // Call create callback
    if (ctype.callbacks.createInstance) {
        ctype.callbacks.createInstance(ent, chandle);
    }

    return chandle;
}

void termite::destroyComponent(EntityManager* emgr, Entity ent, ComponentHandle handle)
{
    destroyComponentNoImmDestroy(emgr, ent, handle);
    
    ComponentType& ctype = g_csys->components[COMPONENT_TYPE_INDEX(handle)];

    if ((ctype.flags & ComponentFlag::ImmediateDestroy) != ComponentFlag::None) {
        int r = emgr->destroyTable.find(ent.id);
        if (r != -1) {
            bx::MultiHashTableInt::Node* node = emgr->destroyTable.getNode(r);
            while (node) {
                if (node->value == int(handle.value)) {
                    emgr->destroyTable.remove(r, node);
                    break;
                }
                node = node->next;
            }
        }
    }
}

ComponentTypeHandle termite::findComponentTypeByName(const char* name)
{
    int index = g_csys->nameTable.find(bx::hashMurmur2A(name, (uint32_t)strlen(name)));
    if (index != -1)
        return ComponentTypeHandle(uint16_t(g_csys->nameTable.getValue(index)));
    else
        return ComponentTypeHandle();
}

ComponentTypeHandle termite::findComponentTypeById(uint32_t id)
{
    int index = g_csys->idTable.find(id);
    if (index != -1)
        return ComponentTypeHandle(uint16_t(g_csys->idTable.getValue(index)));
    else
        return ComponentTypeHandle();
}

ComponentHandle termite::getComponent(ComponentTypeHandle handle, Entity ent)
{
    assert(handle.isValid());

    const ComponentType& ctype = g_csys->components[handle.value];
    int r = ctype.entTable.find(ent.id);
    if (r != -1)
        return ComponentHandle(uint32_t(ctype.entTable.getValue(r)));
    else
        return ComponentHandle();
}

void* termite::getComponentData(ComponentHandle handle)
{
    assert(handle.isValid());

    ComponentType& ctype = g_csys->components[COMPONENT_TYPE_INDEX(handle)];
    return ctype.dataPool.getHandleData(1, COMPONENT_INSTANCE_INDEX(handle));    
}

Entity termite::getComponentEntity(ComponentHandle handle)
{
    assert(handle.isValid());

    ComponentType& ctype = g_csys->components[COMPONENT_TYPE_INDEX(handle)];
    return *ctype.dataPool.getHandleData<Entity>(0, COMPONENT_INSTANCE_INDEX(handle));
}

uint16_t termite::getAllComponents(ComponentTypeHandle typeHandle, ComponentHandle* handles, uint16_t maxComponents)
{
	assert(typeHandle.isValid());

	const ComponentType& ctype = g_csys->components[typeHandle.value];
	uint16_t count = ctype.dataPool.getCount();

	count = bx::uint32_min(count, maxComponents);

	for (uint16_t i = 0; i < count; i++) {
		handles[i] = COMPONENT_MAKE_HANDLE(typeHandle.value, ctype.dataPool.indexAt(i));
	}

	return count;
}

