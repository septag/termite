#include "pch.h"
#include "ecs.h"
#include "internal.h"

#include "bx/uint32_t.h"
#include "bxx/array.h"
#include "bxx/pool.h"
#include "bxx/queue.h"
#include "bxx/handle_pool.h"
#include "bxx/hash_table.h"
#include "bxx/logger.h"

#include "remotery/Remotery.h"

#define MIN_FREE_INDICES 1024

#define COMPONENT_INSTANCE_HANDLE(_Handle) uint16_t(_Handle.value & kComponentHandleMask)
#define COMPONENT_TYPE_INDEX(_Handle) uint16_t((_Handle.value >> kComponentHandleBits) & kComponentTypeHandleMask)
#define COMPONENT_MAKE_HANDLE(_CTypeIdx, _CHdl) ComponentHandle((uint32_t(_CTypeIdx) << kComponentTypeHandleBits) | uint32_t(_CHdl))

namespace tee
{
    static const uint32_t kComponentHandleBits = 16;
    static const uint32_t kComponentHandleMask = (1 << kComponentHandleBits) - 1;
    static const uint32_t kComponentTypeHandleBits = 16;
    static const uint32_t kComponentTypeHandleMask = (1 << kComponentTypeHandleBits) - 1;

    typedef bx::MultiHashTable<ComponentHandle, uint32_t> EntityHashTable;  // EntityId -> Multiple Component Handles

    struct EntityManager
    {
        struct FreeIndex
        {
            typedef bx::Queue<FreeIndex*>::Node QNode;

            uint32_t index;
            QNode qnode;

            explicit FreeIndex(uint32_t _index) :
                index(_index),
                qnode(this)
            {
            }
        };

        bx::AllocatorI* alloc;
        bx::Pool<FreeIndex> freeIndexPool;
        bx::Queue<FreeIndex*> freeIndexQueue;
        uint32_t freeIndexSize;
        bx::Array<uint16_t> generations;
        EntityHashTable destroyTable; // keep a multi-hash for all components that entity has to destroy
        EntityHashTable deactiveTable;
        bx::Pool<EntityHashTable::Node> nodePool;
        uint16_t numEnts;

        EntityManager(bx::AllocatorI* _alloc) :
            alloc(_alloc),
            freeIndexSize(0),
            destroyTable(bx::HashTableType::Mutable),
            deactiveTable(bx::HashTableType::Mutable)
        {
            numEnts = 0;
        }
    };

    struct ComponentType
    {
        char name[32];
        ComponentCallbacks callbacks;
        ComponentFlag::Bits flags;
        uint32_t dataSize;
        bx::HandlePool dataPool;
        bx::HashTable<ComponentHandle, uint32_t> entTable;  // Entity -> ComponentHandle

        ComponentType() :
            entTable(bx::HashTableType::Mutable)
        {
            strcpy(name, "");
            bx::memSet(&callbacks, 0x00, sizeof(callbacks));
            flags = ComponentFlag::None;
            dataSize = 0;
        }
    };

    struct ComponentGroupPair
    {
        ComponentGroupHandle cgroup;
        ComponentHandle component;
    };

    struct ComponentGroup
    {
        struct Batch
        {
            int index;
            int count;
        };

        bx::Array<ComponentHandle> components;
        bx::Array<Batch> batches;
        bool sorted;

        ComponentGroup() :
            sorted(false)
        {
        }
    };

    struct ComponentSystem
    {
        bx::AllocatorI* alloc;
        bx::Array<ComponentType> components;
        bx::HashTableInt nameTable;
        bx::HandlePool componentGroups;
        bool lockComponentGroups;
        bx::Array<ComponentGroupPair> deferredGroupAddCmds;
        bx::Array<ComponentGroupPair> deferredGroupRemoveCmds;

        ComponentSystem(bx::AllocatorI* _alloc) :
            alloc(_alloc),
            nameTable(bx::HashTableType::Mutable)
        {
            lockComponentGroups = false;
        }
    };

    static ComponentSystem* gECS = nullptr;

    EntityManager* ecs::createEntityManager(bx::AllocatorI* alloc, int bufferSize)
    {
        EntityManager* emgr = BX_NEW(alloc, EntityManager)(alloc);
        if (!emgr)
            return nullptr;
        if (bufferSize <= 0)
            bufferSize = MIN_FREE_INDICES;
        if (!emgr->generations.create(bufferSize, bufferSize, alloc) ||
            !emgr->freeIndexPool.create(bufferSize, alloc) ||
            !emgr->nodePool.create(bufferSize, alloc) ||
            !emgr->destroyTable.create(bufferSize, alloc, &emgr->nodePool) ||
            !emgr->deactiveTable.create(bufferSize, alloc, &emgr->nodePool)) {
            ecs::destroyEntityManager(emgr);
            return nullptr;
        }

        return emgr;
    }

    void ecs::destroyEntityManager(EntityManager* emgr)
    {
        assert(emgr);

        emgr->freeIndexPool.destroy();
        emgr->nodePool.destroy();
        emgr->generations.destroy();
        emgr->deactiveTable.destroy();
        emgr->destroyTable.destroy();

        BX_DELETE(emgr->alloc, emgr);
    }

    Entity ecs::create(EntityManager* emgr)
    {
        uint32_t idx;
        if (emgr->freeIndexSize > MIN_FREE_INDICES) {
            EntityManager::FreeIndex* fidx;
            emgr->freeIndexQueue.pop(&fidx);
            idx = fidx->index;
            emgr->freeIndexSize--;
        } else {
            idx = emgr->generations.getCount();
            uint16_t* gen = emgr->generations.push();
            *gen = 1;
            assert(idx < (1 << kEntityIndexBits));
        }
        Entity ent = Entity(idx, emgr->generations[idx]);
        return ent;
    }

    static void addToComponentGroup(ComponentGroupHandle handle, ComponentHandle component)
    {
        if (!gECS->lockComponentGroups) {
            ComponentGroup* group = gECS->componentGroups.getHandleData<ComponentGroup>(0, handle);
            ComponentHandle* pchandle = group->components.push();
            if (pchandle) {
                *pchandle = component;
                group->sorted = false;
            }
        } else {
            ComponentGroupPair* p = gECS->deferredGroupAddCmds.push();
            assert(p);
            p->cgroup = handle;
            p->component = component;
        }
    }

    static void removeFromComponentGroup(ComponentGroupHandle handle, ComponentHandle component)
    {
        assert(component.isValid());
        assert(handle.isValid());

        if (!gECS->lockComponentGroups) {
            ComponentGroup* group = gECS->componentGroups.getHandleData<ComponentGroup>(0, handle);
            int count = group->components.getCount();
            // Find the component and Swap current with the last group
            int index = group->components.find(component);
            if (index != -1) {
                ComponentHandle* buff = group->components.getBuffer();
                std::swap<ComponentHandle>(buff[index], buff[count-1]);
                group->sorted = false;
                group->components.pop();
            }
        } else {
            ComponentGroupPair* p = gECS->deferredGroupRemoveCmds.push();
            assert(p);
            p->cgroup = handle;
            p->component = component;
        }
    }

    static void destroyComponentNoImmAction(Entity ent, ComponentHandle handle)
    {
        assert(handle.isValid());

        ComponentType& ctype = gECS->components[COMPONENT_TYPE_INDEX(handle)];
        uint16_t instHandle = COMPONENT_INSTANCE_HANDLE(handle);

        // Remove from component group
        ComponentGroupHandle groupHandle = *ctype.dataPool.getHandleData<ComponentGroupHandle>(2, instHandle);
        if (groupHandle.isValid())
            removeFromComponentGroup(groupHandle, handle);

        // Call destroy callback
        if (ctype.callbacks.destroyInstance)
            ctype.callbacks.destroyInstance(ent, handle, ctype.dataPool.getHandleData(1, instHandle));

        ctype.dataPool.freeHandle(instHandle);

        int r = ctype.entTable.find(ent.id);
        if (r != -1)
            ctype.entTable.remove(r);
    }

    void ecs::destroy(EntityManager* emgr, Entity ent)
    {
        BX_ASSERT(ecs::isAlive(emgr, ent), "Entity should already be alive");

        // Check if the entity has immediate deactivate components
        // And deactivate all components registered to entity
        int entIdx = emgr->deactiveTable.find(ent.id);
        if (entIdx != -1) {
            EntityHashTable::Node* node = emgr->deactiveTable[entIdx];
            while (node) {
                EntityHashTable::Node* next = node->next;

                ComponentHandle handle = node->value;
                ComponentType& ctype = gECS->components[COMPONENT_TYPE_INDEX(handle)];
                bool prevActive = *ctype.dataPool.getHandleData<bool>(3, handle);
                if (prevActive) {
                    uint16_t cHandle = COMPONENT_INSTANCE_HANDLE(handle);
                    *(ctype.dataPool.getHandleData<bool>(3, cHandle)) = false;
                    if (ctype.callbacks.setActive)
                        ctype.callbacks.setActive(handle, ctype.dataPool.getHandleData(1, cHandle), false, 0);
                }

                emgr->deactiveTable.remove(entIdx, node);
                node = next;
            }
        }

        // Check if the entity has immediate destroy components
        // And destroy all components registered to entity
        entIdx = emgr->destroyTable.find(ent.id);
        if (entIdx != -1) {
            EntityHashTable::Node* node = emgr->destroyTable[entIdx];
            while (node) {
                EntityHashTable::Node* next = node->next;
                destroyComponentNoImmAction(ent, node->value);

                emgr->destroyTable.remove(entIdx, node);
                node = next;
            }
        }

        uint32_t idx = ent.getIndex();
        ++emgr->generations[idx];

        EntityManager::FreeIndex* fi = emgr->freeIndexPool.newInstance<int>(idx);
        if (fi) {
            emgr->freeIndexQueue.push(&fi->qnode);
            emgr->freeIndexSize++;
        }
    }

    bool ecs::isAlive(EntityManager* emgr, Entity ent)
    {
        return emgr->generations[ent.getIndex()] == ent.getGeneration();
    }

    void ecs::setActive(Entity ent, bool active, uint32_t flags)
    {
        const int maxHandles = 200;
        ComponentHandle handles[maxHandles];
        int numHandles = ecs::getEntityComponents(ent, handles, maxHandles);
        for (int i = 0; i < numHandles; i++) {
            ComponentType& ctype = gECS->components[COMPONENT_TYPE_INDEX(handles[i])];

            uint16_t cHandle = COMPONENT_INSTANCE_HANDLE(handles[i]);
            bool prevActive = *ctype.dataPool.getHandleData<bool>(3, cHandle);
            if (prevActive != active) {
                *ctype.dataPool.getHandleData<bool>(3, cHandle) = active;
                if (ctype.callbacks.setActive)
                    ctype.callbacks.setActive(handles[i], ctype.dataPool.getHandleData(1, handles[i]), active, flags);

                ComponentGroupHandle groupHandle = *ctype.dataPool.getHandleData<ComponentGroupHandle>(2, cHandle);
                if (groupHandle.isValid()) {
                    if (active)
                        addToComponentGroup(groupHandle, handles[i]);
                    else
                        removeFromComponentGroup(groupHandle, handles[i]);
                }
            }
        }
    }

    bool ecs::isActive(Entity ent)
    {
        const int maxHandles = 200;
        ComponentHandle handles[maxHandles];
        int numHandles = ecs::getEntityComponents(ent, handles, maxHandles);
        bool active = false;
        for (int i = 0; i < numHandles; i++) {
            ComponentType& ctype = gECS->components[COMPONENT_TYPE_INDEX(handles[i])];
            uint16_t cHandle = COMPONENT_INSTANCE_HANDLE(handles[i]);
            active |= *ctype.dataPool.getHandleData<bool>(3, cHandle);
        }
        return active;
    }

    bool ecs::init(bx::AllocatorI* alloc)
    {
        if (gECS) {
            assert(false);
            return false;
        }

        gECS = BX_NEW(alloc, ComponentSystem)(alloc);
        if (!gECS)
            return false;

        uint32_t cgSz = sizeof(ComponentGroup);
        if (!gECS->components.create(32, 128, alloc) ||
            !gECS->nameTable.create(128, alloc) ||
            !gECS->componentGroups.create(&cgSz, 1, 32, 32, alloc) ||
            !gECS->deferredGroupAddCmds.create(100, 200, alloc) ||
            !gECS->deferredGroupRemoveCmds.create(100, 200, alloc))
        {
            return false;
        }

        return true;
    }

    void ecs::shutdown()
    {
        if (!gECS)
            return;

        for (int i = 0; i < gECS->components.getCount(); i++) {
            ComponentType& ctype = gECS->components[i];
            // Destroy remaining components
            for (int k = 0; k < ctype.dataPool.getCount(); k++) {
                ComponentHandle handle = COMPONENT_MAKE_HANDLE(i, ctype.dataPool.handleAt(k));
                if (ctype.callbacks.destroyInstance)
                    ctype.callbacks.destroyInstance(ecs::getEntity(handle), handle, ctype.dataPool.getHandleData(1, handle));
            }

            ctype.dataPool.destroy();
            ctype.entTable.destroy();
        }
        gECS->componentGroups.destroy();
        gECS->components.destroy();
        gECS->nameTable.destroy();
        gECS->deferredGroupAddCmds.destroy();
        gECS->deferredGroupRemoveCmds.destroy();

        BX_DELETE(gECS->alloc, gECS);
    }

    ComponentGroupHandle ecs::createGroup(bx::AllocatorI* alloc, uint16_t poolSize /*= 0*/)
    {
        if (poolSize == 0)
            poolSize = 200;
        ComponentGroupHandle handle = ComponentGroupHandle(gECS->componentGroups.newHandle());
        if (handle.isValid()) {
            ComponentGroup* group = new(gECS->componentGroups.getHandleData(0, handle)) ComponentGroup();
            if (!group->components.create(poolSize, poolSize, alloc) ||
                !group->batches.create(32, 64, alloc)) {
                ecs::destroyGroup(handle);
                return ComponentGroupHandle();
            }
        }
        return handle;
    }

    void ecs::destroyGroup(ComponentGroupHandle handle)
    {
        assert(handle.isValid());
        ComponentGroup* group = gECS->componentGroups.getHandleData<ComponentGroup>(0, handle);

        // Unlink all component references
        // It is recommended that you Call this function before destroying components/entities 
        for (int i = 0; i < group->components.getCount(); i++) {
            ComponentHandle chandle = group->components[i];
            ComponentType& ctype = gECS->components[COMPONENT_TYPE_INDEX(chandle)];
            *ctype.dataPool.getHandleData<ComponentGroupHandle>(2, COMPONENT_INSTANCE_HANDLE(chandle)) = ComponentGroupHandle();
        }

        // Delete all deferred component group pairs with this handle
        for (int i = 0; i < gECS->deferredGroupAddCmds.getCount(); i++) {
            if (gECS->deferredGroupAddCmds[i].cgroup != handle)
                continue;
            std::swap<ComponentGroupPair>(gECS->deferredGroupAddCmds[i],
                                          gECS->deferredGroupAddCmds[gECS->deferredGroupAddCmds.getCount()-1]);
            gECS->deferredGroupAddCmds.pop();
        }
        for (int i = 0; i < gECS->deferredGroupRemoveCmds.getCount(); i++) {
            if (gECS->deferredGroupRemoveCmds[i].cgroup != handle)
                continue;
            std::swap<ComponentGroupPair>(gECS->deferredGroupRemoveCmds[i],
                                          gECS->deferredGroupRemoveCmds[gECS->deferredGroupRemoveCmds.getCount()-1]);
            gECS->deferredGroupRemoveCmds.pop();
        }

        group->batches.destroy();
        group->components.destroy();
        gECS->componentGroups.freeHandle(handle);
    }

    ComponentTypeHandle ecs::registerComponent(const char* name, const ComponentCallbacks* callbacks,
                                               ComponentFlag::Bits flags, uint32_t dataSize, uint16_t poolSize,
                                               uint16_t growSize, bx::AllocatorI* alloc)
    {
        assert(gECS);
        assert(gECS->components.getCount() < UINT16_MAX);

        ComponentType* buff = gECS->components.push();
        if (!buff)
            return ComponentTypeHandle();

        ComponentType* ctype = new(buff) ComponentType();

        bx::strCopy(ctype->name, sizeof(ctype->name), name);
        if (callbacks)
            memcpy(&ctype->callbacks, callbacks, sizeof(ComponentCallbacks));
        ctype->flags = flags;
        ctype->dataSize = dataSize;
        const uint32_t itemSizes[4] = {sizeof(Entity), dataSize, sizeof(ComponentGroupHandle), sizeof(bool)};
        if (!ctype->dataPool.create(itemSizes, BX_COUNTOF(itemSizes), poolSize, growSize, alloc ? alloc : gECS->alloc) ||
            !ctype->entTable.create(poolSize, alloc ? alloc : gECS->alloc)) {
            return ComponentTypeHandle();
        }

        // Add to ComponentType database
        int index = gECS->components.getCount() - 1;
        gECS->nameTable.add(tinystl::hash_string(name, strlen(name)), index);

        return  ComponentTypeHandle(uint16_t(index));
    }

    void ecs::garbageCollect(EntityManager* emgr)
    {
        // For each component perform garbage collection
        for (int i = 0, c = gECS->components.getCount(); i < c; i++) {
            ComponentType& ctype = gECS->components[i];

            if ((ctype.flags & ComponentFlag::ImmediateDestroy) == 0) {
                int aliveInRow = 0;
                while (ctype.dataPool.getCount() && aliveInRow < 4) {
                    uint16_t r = ctype.dataPool.handleAt(getRandomIntUniform(0, (int)ctype.dataPool.getCount() - 1));
                    Entity ent = *ctype.dataPool.getHandleData<Entity>(0, r);
                    if (ecs::isAlive(emgr, ent)) {
                        aliveInRow++;
                        continue;
                    }

                    aliveInRow = 0;
                    ecs::destroyComponent(emgr, ent, COMPONENT_MAKE_HANDLE(i, r));
                }
            }
        }
    }

    void ecs::garbageCollectAggressive(EntityManager* emgr)
    {
        // For each component type, Go through all instances and destroy them
        struct DestroyItem
        {
            Entity ent;
            ComponentHandle handle;
        };

        bx::Array<DestroyItem> destroyArr;
        destroyArr.create(100, 100, getTempAlloc());

        for (int i = 0, c = gECS->components.getCount(); i < c; i++) {
            ComponentType& ctype = gECS->components[i];

            if ((ctype.flags & ComponentFlag::ImmediateDestroy) == 0) {
                for (int k = 0, kc = ctype.dataPool.getCount(); k < kc; k++) {
                    uint16_t r = ctype.dataPool.handleAt(k);
                    Entity ent = *ctype.dataPool.getHandleData<Entity>(0, r);
                    if (ecs::isAlive(emgr, ent))
                        continue;
                    DestroyItem* item = destroyArr.push();
                    item->ent = ent;
                    item->handle = COMPONENT_MAKE_HANDLE(i, r);
                }

                for (int k = 0, kc = destroyArr.getCount(); k < kc; k++) {
                    DestroyItem item = destroyArr[k];
                    ecs::destroyComponent(emgr, item.ent, item.handle);
                }

                destroyArr.clear();
            }
        }


        destroyArr.destroy();
    }

    ComponentHandle ecs::createComponent(EntityManager* emgr, Entity ent, ComponentTypeHandle handle,
                                      ComponentGroupHandle group)
    {
        ComponentType& ctype = gECS->components[handle.value];

        if (ctype.entTable.find(ent.id) != -1) {
            assert(false);  // Component instance Already exists for the entity
            return ComponentHandle();
        }

        uint16_t cIdx = ctype.dataPool.newHandle();
        if (cIdx == UINT16_MAX)
            return ComponentHandle();
        *ctype.dataPool.getHandleData<Entity>(0, cIdx) = ent;
        void* data = ctype.dataPool.getHandleData(1, cIdx);
        *ctype.dataPool.getHandleData<ComponentGroupHandle>(2, cIdx) = group;
        *ctype.dataPool.getHandleData<bool>(3, cIdx) = true;

        ComponentHandle chandle = COMPONENT_MAKE_HANDLE(handle.value, cIdx);

        if (group.isValid())
            addToComponentGroup(group, chandle);

        ctype.entTable.add(ent.id, chandle);

        if (ctype.flags & ComponentFlag::ImmediateDestroy) {
            emgr->destroyTable.add(ent.id, chandle);
        }

        if (ctype.flags & ComponentFlag::ImmediateDeactivate) {
            emgr->deactiveTable.add(ent.id, chandle);
        }

        // Call create callback
        if (ctype.callbacks.createInstance) {
            ctype.callbacks.createInstance(ent, chandle, data);
        }

        return chandle;
    }

    void ecs::destroyComponent(EntityManager* emgr, Entity ent, ComponentHandle handle)
    {
        destroyComponentNoImmAction(ent, handle);

        ComponentFlag::Bits flags = gECS->components[COMPONENT_TYPE_INDEX(handle)].flags;

        if (flags & ComponentFlag::ImmediateDestroy) {
            int r = emgr->destroyTable.find(ent.id);
            if (r != -1) {
                EntityHashTable::Node* node = emgr->destroyTable[r];
                while (node) {
                    if (node->value == handle) {
                        emgr->destroyTable.remove(r, node);
                        break;
                    }
                    node = node->next;
                }
            }
        }

        if (flags & ComponentFlag::ImmediateDeactivate) {
            int r = emgr->deactiveTable.find(ent.id);
            if (r != -1) {
                EntityHashTable::Node* node = emgr->deactiveTable[r];
                while (node) {
                    if (node->value == handle) {
                        emgr->deactiveTable.remove(r, node);
                        break;
                    }
                    node = node->next;
                }
            }
        }

    }

    static void sortAndBatchComponents(ComponentGroup* group)
    {
        // Sort components if it's invalidated
        if (!group->sorted) {
            group->batches.clear();
            int count = group->components.getCount();
            if (count > 0) {
                std::sort(group->components.itemPtr(0), group->components.itemPtr(0) + count,
                          [](const ComponentHandle& a, const ComponentHandle& b) { return a.value < b.value; });

                // Batch by component-type
                ComponentTypeHandle prevHandle;
                ComponentGroup::Batch* curBatch = nullptr;

                for (int i = 0; i < count; i++) {
                    ComponentTypeHandle curHandle = ComponentTypeHandle(COMPONENT_TYPE_INDEX(group->components[i]));
                    if (curHandle != prevHandle) {
                        curBatch = group->batches.push();
                        curBatch->index = i;
                        curBatch->count = 0;
                        prevHandle = curHandle;
                    }
                    curBatch->count++;
                }

            }
            group->sorted = true;
        }
    }

    void ecs::updateGroup(ComponentUpdateStage::Enum stage, ComponentGroupHandle groupHandle, float dt)
    {
        assert(groupHandle.isValid());

#if RMT_ENABLED
        const char* stageName = "";
        switch (stage) {
        case ComponentUpdateStage::InputUpdate:
            stageName = "Input";
            break;

        case ComponentUpdateStage::PreUpdate:
            stageName = "PreUpdate";
            break;

        case ComponentUpdateStage::Update:
            stageName = "Update";
            break;

        case ComponentUpdateStage::PostUpdate:
            stageName = "PostUpdate";
            break;

        case ComponentUpdateStage::FixedUpdate:
            stageName = "FixedUpdate";
            break;
        }
        char name[64];
#endif

        ComponentGroup* group = gECS->componentGroups.getHandleData<ComponentGroup>(0, groupHandle);
        sortAndBatchComponents(group);

        // Call their callbacks
        gECS->lockComponentGroups = true;
        for (int i = 0, c = group->batches.getCount(); i < c; i++) {
            ComponentGroup::Batch batch = group->batches[i];
            const ComponentType& ctype = gECS->components[COMPONENT_TYPE_INDEX(group->components[batch.index])];
            if (ctype.callbacks.updateStage[stage]) {
#if RMT_ENABLED
                bx::snprintf(name, sizeof(name), "%s: %s (%d)", stageName, ctype.name, batch.count);
                rmt_BeginCPUSampleDynamic(name, 0);
#endif
                ctype.callbacks.updateStage[stage](group->components.itemPtr(batch.index), batch.count, dt);
#if RMT_ENABLED
                rmt_EndCPUSample();
#endif
            }
        }
        gECS->lockComponentGroups = false;
    }

    void ecs::cleanupGroupUpdates()
    {
        // Check for deferred add/removes and apply them

        if (gECS->deferredGroupAddCmds.getCount() > 0) {
            for (int i = 0, c = gECS->deferredGroupAddCmds.getCount(); i < c; i++) {
                const ComponentGroupPair p = gECS->deferredGroupAddCmds[i];
                addToComponentGroup(p.cgroup, p.component);
            }
            gECS->deferredGroupAddCmds.clear();
        }

        if (gECS->deferredGroupRemoveCmds.getCount() > 0) {
            for (int i = 0, c = gECS->deferredGroupRemoveCmds.getCount(); i < c; i++) {
                const ComponentGroupPair p = gECS->deferredGroupRemoveCmds[i];
                removeFromComponentGroup(p.cgroup, p.component);
            }
            gECS->deferredGroupRemoveCmds.clear();
        }
    }

    void ecs::debug(ImGuiApi* imgui, void* userData)
    {
        for (int i = 0, c = gECS->components.getCount(); i < c; i++) {
            const ComponentType& ctype = gECS->components[i];
            if (ctype.callbacks.debug) {
                uint16_t count = ctype.dataPool.getCount();
                if (count > 0) {
                    ComponentHandle* handles = (ComponentHandle*)alloca(count*sizeof(ComponentHandle));
                    if (!handles) {
                        handles = (ComponentHandle*)BX_ALLOC(getTempAlloc(), sizeof(ComponentHandle)*count);
                        if (!handles)
                            return;
                    }

                    for (int k = 0; k < count; k++)
                        handles[k] = COMPONENT_MAKE_HANDLE(i, ctype.dataPool.handleAt(k));
                    ctype.callbacks.debug(handles, count, imgui, userData);
                }
            }
        }
    }

    void ecs::debugType(ComponentTypeHandle typeHandle, ImGuiApi* imgui, void* userData)
    {
        const ComponentType& ctype = gECS->components[typeHandle.value];
        if (ctype.callbacks.debug) {
            uint16_t count = ctype.dataPool.getCount();
            if (count > 0) {
                ComponentHandle* handles = (ComponentHandle*)alloca(count*sizeof(ComponentHandle));
                if (!handles) {
                    handles = (ComponentHandle*)BX_ALLOC(getTempAlloc(), sizeof(ComponentHandle)*count);
                    if (!handles)
                        return;
                }

                for (int k = 0; k < count; k++)
                    handles[k] = COMPONENT_MAKE_HANDLE(typeHandle.value, ctype.dataPool.handleAt(k));
                ctype.callbacks.debug(handles, count, imgui, userData);
            }
        }
    }

    ComponentTypeHandle ecs::findType(const char* name)
    {
        int index = gECS->nameTable.find(tinystl::hash_string(name, strlen(name)));
        if (index != -1)
            return ComponentTypeHandle(uint16_t(gECS->nameTable[index]));
        else
            return ComponentTypeHandle();
    }

    ComponentTypeHandle ecs::findType(size_t nameHash)
    {
        int index = gECS->nameTable.find(nameHash);
        if (index != -1)
            return ComponentTypeHandle(uint16_t(gECS->nameTable[index]));
        else
            return ComponentTypeHandle();
    }

    ComponentHandle ecs::get(ComponentTypeHandle handle, Entity ent)
    {
        assert(handle.isValid());
        assert(ent.isValid());

        const ComponentType& ctype = gECS->components[handle.value];
        int r = ctype.entTable.find(ent.id);
        if (r != -1)
            return ComponentHandle(ctype.entTable[r]);
        else
            return ComponentHandle();
    }

    const char* ecs::getTypeName(ComponentHandle handle)
    {
        assert(handle.isValid());

        ComponentType& ctype = gECS->components[COMPONENT_TYPE_INDEX(handle)];
        return ctype.name;
    }

    void* ecs::getData(ComponentHandle handle)
    {
        assert(handle.isValid());

        ComponentType& ctype = gECS->components[COMPONENT_TYPE_INDEX(handle)];
        return ctype.dataPool.getHandleData(1, COMPONENT_INSTANCE_HANDLE(handle));
    }

    Entity ecs::getEntity(ComponentHandle handle)
    {
        assert(handle.isValid());

        ComponentType& ctype = gECS->components[COMPONENT_TYPE_INDEX(handle)];
        return *ctype.dataPool.getHandleData<Entity>(0, COMPONENT_INSTANCE_HANDLE(handle));
    }

    ComponentGroupHandle ecs::getGroup(ComponentHandle handle)
    {
        assert(handle.isValid());

        ComponentType& ctype = gECS->components[COMPONENT_TYPE_INDEX(handle)];
        return *ctype.dataPool.getHandleData<ComponentGroupHandle>(2, COMPONENT_INSTANCE_HANDLE(handle));
    }

    uint16_t ecs::getAllComponents(ComponentTypeHandle typeHandle, ComponentHandle* handles, uint16_t maxComponents)
    {
        assert(typeHandle.isValid());

        const ComponentType& ctype = gECS->components[typeHandle.value];
        uint16_t count = ctype.dataPool.getCount();
        if (handles == nullptr)
            return count;

        count = bx::uint32_min(count, maxComponents);
        for (uint16_t i = 0; i < count; i++) {
            handles[i] = COMPONENT_MAKE_HANDLE(typeHandle.value, ctype.dataPool.handleAt(i));
        }

        return count;
    }

    uint16_t ecs::getEntityComponents(Entity ent, ComponentHandle* handles, uint16_t maxComponents)
    {
        int index = 0;
        for (int i = 0, c = gECS->components.getCount(); i < c; i++) {
            const ComponentType& ctype = gECS->components[i];
            int r = ctype.entTable.find(ent.id);
            if (r == -1)
                continue;

            if (index == maxComponents)
                return maxComponents;

            if (handles)
                handles[index] = ctype.entTable[r];
            index ++;
        }

        return index;
    }

    uint16_t ecs::getGroupComponents(ComponentGroupHandle groupHandle, ComponentHandle* handles, uint16_t maxComponents)
    {
        assert(groupHandle.isValid());
        ComponentGroup* group = gECS->componentGroups.getHandleData<ComponentGroup>(0, groupHandle);
        uint16_t count = std::min<uint16_t>(maxComponents, (uint16_t)group->components.getCount());

        if (handles)
            memcpy(handles, group->components.getBuffer(), count*sizeof(ComponentHandle));
        return count;
    }

    uint16_t ecs::getGroupComponents(ComponentGroupHandle groupHandle, ComponentHandle* handles, uint16_t maxComponents,
                                     ComponentTypeHandle typeHandle)
    {
        assert(groupHandle.isValid());
        ComponentGroup* group = gECS->componentGroups.getHandleData<ComponentGroup>(0, groupHandle);

        sortAndBatchComponents(group);

        for (int i = 0, c = group->batches.getCount(); i < c; i++) {
            const ComponentGroup::Batch& batch = group->batches[i];
            if (typeHandle == ComponentTypeHandle(COMPONENT_TYPE_INDEX(group->components[batch.index]))) {
                uint16_t count = std::min<uint16_t>(maxComponents, (uint16_t)batch.count);
                if (handles) {
                    memcpy(handles, group->components.itemPtr(batch.index), count*sizeof(ComponentHandle));
                }
                return count;
            }
        }

        return 0;
    }

}   // namespace tee