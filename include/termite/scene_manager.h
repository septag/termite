#pragma once

#include "bx/bx.h"
#include "gfx_defines.h"
#include "progressive_loader.h"
#include "vec_math.h"

namespace termite
{
    struct SceneManager;
    struct Scene;
    struct SceneLinkT {};
    typedef PhantomType<uint16_t, SceneLinkT, UINT16_MAX> SceneLinkHandle;

    TERMITE_API SceneManager* createSceneManager(bx::AllocatorI* alloc);
    TERMITE_API void destroySceneManager(SceneManager* smgr);

    struct SceneFlag
    {
        enum Enum
        {
            Preload = 0x0001,       // Loads level immediately after create
            CacheLevel1 = 0x0002,   // TODO: Loads and caches if neighbor scene is active (currently not working)
            CacheLevel2 = 0x0004,   // TODO: Loads and caches if neighbors of neighbors of the scene is active (currently not working)
            CacheAlways = 0x0008,   // Always stays if loaded once (can be used with Preload to always stay in memory)
            Overlay = 0x0016
        };

        typedef uint16_t Bits;
    };

    struct SceneLinkDef
    {
        Scene* loadScene;           // Loading scene in the middle of the transition
        const char* effectNameA;    // SceneA transition effect name
        const char* effectNameB;    // SceneB transition effect name
        const void* effectParamsA;  
        const void* effectParamsB;  

        SceneLinkDef(Scene* _loadScene = nullptr, 
                     const char* _effectA = nullptr, const void* _effectParamsA = nullptr,
                     const char* _effectB = nullptr, const void* _effectParamsB = nullptr) :
            loadScene(_loadScene),
            effectNameA(_effectA),
            effectNameB(_effectB),
            effectParamsA(_effectParamsA),
            effectParamsB(_effectParamsB)
        {
        }
    };

    struct FindSceneFlag
    {
        enum Enum
        {
            Linked = 0x1,
            LinkedChildren = 0x2,
            Active = 0x4
        };

        typedef uint8_t Bits;
    };

    struct SceneCallbackResult
    {
        enum Enum
        {
            Repeat,
            Failed,
            Finished
        };
    };

    // Scene Callbacks
    // SceneManager manages and calls these callbacks like the order described below:
    //  - loadResources: right after data file is loadaed and is ready 
    //  - createObjects: Assume that resources are ready, now start creating entities, components and other objects
    //  - onEnter: Scene is entered and is active
    //  - update: Scene is being updated every frame, this is called every frame when scene is active
    //  - onExit: Scene is about to be exited, this doesn't mean that data should be unloaded too
    //  - destroyObjects: Scene is not in cache, it should destroy entities and other object stuff
    //  - unloadResources: Happens after destroyObjects to release resources
    class BX_NO_VTABLE SceneCallbacksI
    {
    public:
        virtual void loadResources(Scene* scene, const CProgressiveLoader& loader) = 0;
        virtual SceneCallbackResult::Enum createObjects(Scene* scene) = 0;
        virtual void onEnter(Scene* scene, Scene* prevScene) = 0;
        virtual bool onExit(Scene* scene, Scene* nextScene) = 0;        // Return TRUE to permit the scene to unload
        virtual SceneCallbackResult::Enum destroyObjects(Scene* scene) = 0;
        virtual void unloadResources(Scene* scene, const CProgressiveLoader& loader) = 0;
        virtual void update(Scene* scene, float dt, uint8_t& viewId, FrameBufferHandle renderFb, bool mustClearFb) = 0;
    };

    class BX_NO_VTABLE SceneTransitionEffectCallbacksI
    {
    public:
        virtual bool create() = 0;
        virtual void destroy() = 0;
        virtual void begin(const void* params, uint8_t viewId) = 0;
        virtual void render(float dt, uint8_t viewId, FrameBufferHandle renderFb, TextureHandle srcTex) = 0;
        virtual void end() = 0;
        virtual bool isDone() const = 0;
    };

    TERMITE_API Scene* createScene(SceneManager* mgr, 
                                   const char* name, 
                                   SceneCallbacksI* callbacks,
                                   uint32_t tag = 0,
                                   SceneFlag::Bits flags = 0, 
                                   const LoadingScheme& loadScheme = LoadingScheme(),
                                   void* userData = nullptr,
                                   uint8_t order = 0);
    TERMITE_API void destroyScene(SceneManager* mgr, Scene* scene);

    TERMITE_API void* getSceneUserData(Scene* scene);
    TERMITE_API const char* getSceneName(Scene* scene);

    //
    TERMITE_API bool registerSceneTransitionEffect(SceneManager* mgr, const char* name, 
                                                   SceneTransitionEffectCallbacksI* callbacks, uint32_t paramSize);

    // Links
    TERMITE_API SceneLinkHandle linkScene(SceneManager* mgr, Scene* sceneA, Scene* sceneB, 
                                          const SceneLinkDef& def = SceneLinkDef());
    TERMITE_API void removeSceneLink(SceneManager* mgr, SceneLinkHandle handle);
    TERMITE_API void triggerSceneLink(SceneManager* mgr, SceneLinkHandle handle);

    // 
    TERMITE_API Scene* findScene(SceneManager* mgr, const char* name, FindSceneFlag::Bits flags = 0);
    TERMITE_API int findSceneByTag(SceneManager* mgr, Scene** pScenes, uint32_t tag, FindSceneFlag::Bits flags = 0);

    // 
    TERMITE_API void updateSceneManager(SceneManager* mgr, float dt, uint8_t* viewId,
                                        FrameBufferHandle* pRenderFb = nullptr, TextureHandle* pRenderTex = nullptr);
    TERMITE_API void startSceneManager(SceneManager* mgr, Scene* entryScene, 
                                       FrameBufferHandle mainFb, TextureHandle mainTex,
                                       FrameBufferHandle effectFb, TextureHandle effectTex);
    TERMITE_API void debugSceneManager(SceneManager* mgr);

    // "FadeIn"/"FadeOut" Effect Params
    struct FadeEffectParams
    {
        vec4_t fadeColor;
        float duration;
        float biasFactor;   // between (0-1): values 0~0.5 slower slope, values 0.5~1.0 are faster slope

        FadeEffectParams(color_t _fadeColor = color1n(0xff000000), float _duration = 0.5f, float _biasFactor = 0.2f)
        {
            fadeColor = colorToVec4(_fadeColor);
            duration = _duration;
            biasFactor = _biasFactor;
        }
    };

} // namespace termite
