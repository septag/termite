#pragma once

#include "types.h"
#include "bx/bx.h"
#include "bx/allocator.h"

namespace tee
{
    static const uint32_t kEntityIndexBits = 16;
    static const uint32_t kEntityIndexMask = (1 << kEntityIndexBits) - 1;
    static const uint32_t kEntityGenerationBits = 14;
    static const uint32_t kEntityGenerationMask = (1 << kEntityGenerationBits) - 1;
    struct ImGuiApi;

    struct EntityManager;
    struct ComponentTypeT {};
    struct ComponentT {};
    struct ComponentGroupT {};
    typedef PhantomType<uint16_t, ComponentTypeT, UINT16_MAX> ComponentTypeHandle;
    typedef PhantomType<uint32_t, ComponentT, UINT32_MAX> ComponentHandle;
    typedef PhantomType<uint16_t, ComponentGroupT, UINT16_MAX> ComponentGroupHandle;

    struct Entity
    {
        uint32_t id;

        inline Entity() : id(0)        {}

        explicit Entity(uint32_t _id) : id(_id)      {}
        inline Entity(uint32_t index, uint32_t generation)  { 
            id = (index & kEntityIndexMask) | ((generation & kEntityGenerationMask) << kEntityIndexBits); 
        }
        inline uint32_t getIndex() { return (id & kEntityIndexMask);  }
        inline uint32_t getGeneration() {   return (id >> kEntityIndexBits) & kEntityGenerationMask;  }
        inline bool operator==(const Entity& ent) const { return this->id == ent.id; }
        inline bool operator!=(const Entity& ent) const { return this->id != ent.id; }
        inline bool isValid() const   {  return this->id != 0;  }
    };

    struct ComponentUpdateStage
    {
        enum Enum
        {
            InputUpdate = 0,
            PreUpdate,
            FixedUpdate,
            Update,
            PostUpdate,
            Count
        };
    };

    struct ComponentCallbacks
    {
        bool(*createInstance)(Entity ent, ComponentHandle handle, void* data);
        void(*destroyInstance)(Entity ent, ComponentHandle handle, void* data);
        void(*setActive)(ComponentHandle handle, void* data, bool active, uint32_t flags);

        typedef void(*UpdateStageFunc)(const ComponentHandle* handles, uint16_t count, float dt);
        UpdateStageFunc updateStage[ComponentUpdateStage::Count];

        void(*debug)(const ComponentHandle* handles, uint16_t count, ImGuiApi* imgui, void* userData);

        ComponentCallbacks() :
            createInstance(nullptr),
            destroyInstance(nullptr),
            setActive(nullptr),
            debug(nullptr)
        {
            bx::memSet(updateStage, 0x00, sizeof(UpdateStageFunc)*ComponentUpdateStage::Count);
        }
    };

    struct ComponentFlag
    {
        enum Enum
        {
            None = 0x0,
            ImmediateDestroy = 0x01,   // Destroys component immediately after owner entity is destroyed
            ImmediateDeactivate = 0x02   // Deactivates component immediately after owner entity is destroyed
        };

        typedef uint8_t Bits;
    };

    namespace ecs {
        // Entity Management
        TEE_API EntityManager* createEntityManager(bx::AllocatorI* alloc, int bufferSize = 0);
        TEE_API void destroyEntityManager(EntityManager* emgr);

        TEE_API Entity create(EntityManager* emgr);
        TEE_API void destroy(EntityManager* emgr, Entity ent);
        TEE_API bool isAlive(EntityManager* emgr, Entity ent);
        TEE_API void setActive(Entity ent, bool active, uint32_t flags = 0);
        TEE_API bool isActive(Entity ent);

        // ComponentGroup: Caches bunch of ComponentHandles for updates, render and other stuff 
        TEE_API ComponentGroupHandle createGroup(bx::AllocatorI* alloc, uint16_t poolSize = 0);
        TEE_API void destroyGroup(ComponentGroupHandle handle);

        TEE_API ComponentTypeHandle registerComponent(const char* name,
                                                      const ComponentCallbacks* callbacks, ComponentFlag::Bits flags,
                                                      uint32_t dataSize, uint16_t poolSize, uint16_t growSize,
                                                      bx::AllocatorI* alloc = nullptr);

        /// Garbage collect dead entities 4 ents per call randomly
        TEE_API void garbageCollect(EntityManager* emgr);

        /// Garbage collect dead entities aggressively by searching all dead components and destroy them at once
        TEE_API void garbageCollectAggressive(EntityManager* emgr);

        TEE_API ComponentHandle createComponent(EntityManager* emgr, Entity ent, ComponentTypeHandle handle, 
                                             ComponentGroupHandle group = ComponentGroupHandle());
        TEE_API void destroyComponent(EntityManager* emgr, Entity ent, ComponentHandle handle);

        TEE_API void updateGroup(ComponentUpdateStage::Enum stage, ComponentGroupHandle groupHandle, float dt);
        TEE_API void cleanupGroupUpdates();

        /// Calls 'debug' callbacks on all components
        TEE_API void debug(ImGuiApi* imgui, void* userData);
        TEE_API void debugType(ComponentTypeHandle typeHandle, ImGuiApi* imgui, void* userData);

        TEE_API ComponentTypeHandle findType(const char* name);
        TEE_API ComponentTypeHandle findType(size_t nameHash);
        TEE_API ComponentHandle get(ComponentTypeHandle handle, Entity ent);
        TEE_API const char* getTypeName(ComponentHandle handle);
        TEE_API void* getData(ComponentHandle handle);
        TEE_API Entity getEntity(ComponentHandle handle);
        TEE_API ComponentGroupHandle getGroup(ComponentHandle handle);

        // Pass handles=nullptr to just get the count of all components
        TEE_API uint16_t getAllComponents(ComponentTypeHandle typeHandle, ComponentHandle* handles, uint16_t maxComponents);
        TEE_API uint16_t getEntityComponents(Entity ent, ComponentHandle* handles, uint16_t maxComponents);
        TEE_API uint16_t getGroupComponents(ComponentGroupHandle groupHandle, ComponentHandle* handles, uint16_t maxComponents);
        TEE_API uint16_t getGroupComponents(ComponentGroupHandle groupHandle, ComponentHandle* handles, 
                                            uint16_t maxComponents, ComponentTypeHandle typeHandle);

        template <typename Ty>
        Ty* getData(ComponentHandle handle)
        {
            return (Ty*)getData(handle);
        }

    } // namespace ecs
} // namespace tee
