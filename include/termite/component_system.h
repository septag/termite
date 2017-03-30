#pragma once

#include "types.h"
#include "bx/bx.h"
#include "bx/allocator.h"

#define T_COMPONENT_DATA(_C, _Ent) getComponentData<_C>(getComponent(_C::Handle, _Ent));
#define T_COMPONENT_DATA_H(_C, _Handle) getComponentData<_C>(getComponent(_C::Handle, getComponentEntity(_Handle)));

namespace termite
{
    static const uint32_t kEntityIndexBits = 16;
    static const uint32_t kEntityIndexMask = (1 << kEntityIndexBits) - 1;
    static const uint32_t kEntityGenerationBits = 14;
    static const uint32_t kEntityGenerationMask = (1 << kEntityGenerationBits) - 1;
    
    struct GfxDriverApi;

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

    struct EntityManager;
    struct ComponentTypeT {};
    struct ComponentT {};
    struct ComponentGroupT {};
    typedef PhantomType<uint16_t, ComponentTypeT, UINT16_MAX> ComponentTypeHandle;
    typedef PhantomType<uint32_t, ComponentT, UINT32_MAX> ComponentHandle;
    typedef PhantomType<uint16_t, ComponentGroupT, UINT16_MAX> ComponentGroupHandle;

    // Entity Management
    TERMITE_API EntityManager* createEntityManager(bx::AllocatorI* alloc, int bufferSize = 0);
    TERMITE_API void destroyEntityManager(EntityManager* emgr);

	TERMITE_API Entity createEntity(EntityManager* emgr);
	TERMITE_API void destroyEntity(EntityManager* emgr, Entity ent);
	TERMITE_API bool isEntityAlive(EntityManager* emgr, Entity ent);
    TERMITE_API void setEntityActive(Entity ent, bool active);

    // Component System
    result_t initComponentSystem(bx::AllocatorI* alloc);
    void shutdownComponentSystem();

    struct ComponentUpdateStage
    {
        enum Enum
        {
            PreUpdate = 0,
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
        void(*setActive)(ComponentHandle handle, void* data, bool active);

        typedef void (*UpdateStageFunc)(const ComponentHandle* handles, uint16_t count, float dt);
        UpdateStageFunc updateStageFn[ComponentUpdateStage::Count];

        ComponentCallbacks() :
            createInstance(nullptr),
            destroyInstance(nullptr),
            setActive(nullptr)
        {
            memset(updateStageFn, 0x00, sizeof(UpdateStageFunc)*ComponentUpdateStage::Count);
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

    // ComponentGroup: Caches bunch of ComponentHandles for updates, render and other stuff 
    TERMITE_API ComponentGroupHandle createComponentGroup(bx::AllocatorI* alloc, uint16_t poolSize = 0);
    TERMITE_API void destroyComponentGroup(ComponentGroupHandle handle);

	TERMITE_API ComponentTypeHandle registerComponentType(const char* name, 
							                              const ComponentCallbacks* callbacks, ComponentFlag::Bits flags,
										                  uint32_t dataSize, uint16_t poolSize, uint16_t growSize,
                                                          bx::AllocatorI* alloc = nullptr);
	TERMITE_API void garbageCollectComponents(EntityManager* emgr);

	TERMITE_API ComponentHandle createComponent(EntityManager* emgr, Entity ent, ComponentTypeHandle handle, 
                                                ComponentGroupHandle group = ComponentGroupHandle());
	TERMITE_API void destroyComponent(EntityManager* emgr, Entity ent, ComponentHandle handle);

    TERMITE_API void runComponentGroup(ComponentUpdateStage::Enum stage, ComponentGroupHandle groupHandle, float dt);

	TERMITE_API ComponentTypeHandle findComponentTypeByName(const char* name);
	TERMITE_API ComponentTypeHandle findComponentTypeByNameHash(size_t nameHash);
	TERMITE_API ComponentHandle getComponent(ComponentTypeHandle handle, Entity ent);
    TERMITE_API const char* getComponentName(ComponentHandle handle);
	TERMITE_API void* getComponentData(ComponentHandle handle);
    TERMITE_API Entity getComponentEntity(ComponentHandle handle);
    TERMITE_API ComponentGroupHandle getComponentGroup(ComponentHandle handle);

    // Pass handles=nullptr to just get the count of all components
    TERMITE_API uint16_t getAllComponents(ComponentTypeHandle typeHandle, ComponentHandle* handles, uint16_t maxComponents);
    TERMITE_API uint16_t getEntityComponents(Entity ent, ComponentHandle* handles, uint16_t maxComponents);
    TERMITE_API uint16_t getGroupComponents(ComponentGroupHandle groupHandle, ComponentHandle* handles, uint16_t maxComponents);
    TERMITE_API uint16_t getGroupComponentsByType(ComponentGroupHandle groupHandle, ComponentHandle* handles, 
                                                  uint16_t maxComponents, ComponentTypeHandle typeHandle);

    template <typename Ty> 
    Ty* getComponentData(ComponentHandle handle)
    {
        return (Ty*)getComponentData(handle);
    }
    
} // namespace termite
