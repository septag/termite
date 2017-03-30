#include "pch.h"
#include "scene_manager.h"

#include "bxx/handle_pool.h"
#include "bxx/array.h"
#include "bxx/pool.h"
#include "bxx/logger.h"
#include "bx/string.h"
#include "bx/fpumath.h"
#include "bx/cpu.h"
#include "tinystl/hash.h"
#include "bxx/linked_list.h"

#include "io_driver.h"
#include "event_dispatcher.h"
#include "gfx_driver.h"
#include "gfx_utils.h"

#define T_IMGUI_API
#include "plugin_api.h"

#include T_MAKE_SHADER_PATH(shaders_h, effect_fade_in_color.fso)
#include T_MAKE_SHADER_PATH(shaders_h, effect_fade_in_color.vso)
#include T_MAKE_SHADER_PATH(shaders_h, effect_fade_out_color.fso)
#include T_MAKE_SHADER_PATH(shaders_h, effect_fade_out_color.vso)
#include T_MAKE_SHADER_PATH(shaders_h, effect_fade_in_alpha.fso)
#include T_MAKE_SHADER_PATH(shaders_h, effect_fade_in_alpha.vso)
#include T_MAKE_SHADER_PATH(shaders_h, effect_fade_out_alpha.fso)
#include T_MAKE_SHADER_PATH(shaders_h, effect_fade_out_alpha.vso)

using namespace termite;

#define MAX_ACTIVE_SCENES 4
#define MAX_ACTIVE_LINKS 4

namespace termite
{
    struct SceneTransitionEffect
    {
        size_t nameHash;
        SceneTransitionEffectCallbacksI* callbacks;
        uint32_t paramSize;
    };

    struct Scene
    {
        enum State
        {
            Dead,
            LoadResource,
            Create,
            Ready,
            Destroy,
            UnloadResource
        };
    
        State state;
        char name[32];
        uint8_t order;
        SceneCallbacksI* callbacks;
        uint32_t tag;
        SceneFlag::Bits flags;
        LoadingScheme loadScheme;
        LoaderGroupHandle loaderGroup;
        void* userData;
        bx::List<Scene*>::Node lnode;

        bool drawOnEffectFb;

        Scene() :
            state(Dead),
            order(0),
            callbacks(nullptr),
            tag(0),
            flags(0),
            userData(nullptr),
            lnode(this),
            drawOnEffectFb(false)
        {
            name[0] = 0;
        }
    };

    struct SceneLink
    {
        enum State
        {
            InA,
            InB,
            InLoad
        };

        State state;
        Scene* sceneA;
        Scene* sceneB;
        Scene* loadScene;
        SceneTransitionEffect* effectA;
        SceneTransitionEffect* effectB;
        uint8_t effectParamsA[128];
        uint8_t effectParamsB[128];
        bool effectBeginA;
        bool effectBeginB;

        SceneLink() :
            state(InA),
            sceneA(nullptr),
            sceneB(nullptr),
            loadScene(nullptr),
            effectA(nullptr),
            effectB(nullptr),
            effectBeginA(false),
            effectBeginB(false)
        {
        }
    };

    struct SceneManager
    {
        bx::AllocatorI* alloc;
        bx::Pool<Scene> scenePool;
        bx::Array<SceneTransitionEffect> effects;
        bx::HandlePool linkPool;
        CProgressiveLoader loader;
        uint8_t viewId;
        uint8_t viewIdOffset;

        Scene* activeScenes[MAX_ACTIVE_SCENES];
        int numActiveScenes;
        SceneLinkHandle activeLinks[MAX_ACTIVE_LINKS];    // Active links (it's actually a queue)
        int numActiveLinks;

        FrameBufferHandle mainFb;
        TextureHandle mainTex;
        FrameBufferHandle effectFb;
        TextureHandle effectTex;
        FrameBufferHandle finalFb;
        TextureHandle finalTex;

        bx::List<Scene*> sceneList;

        SceneManager(bx::AllocatorI* _alloc) :
            alloc(_alloc),
            viewId(0),
            viewIdOffset(0),
            numActiveScenes(0),
            numActiveLinks(0)
        {
        }
    };

}   // namespace termite

class FadeEffect : public SceneTransitionEffectCallbacksI
{
public:
    enum Mode
    {
        FadeOutColor = 0,
        FadeInColor,
        FadeOutAlpha,
        FadeInAlpha,
        Count
    };

private:
    Mode m_mode;
    FadeEffectParams m_params;
    GfxDriverApi* m_driver;
    ProgramHandle m_prog;
    UniformHandle m_ufadeColor;
    UniformHandle m_umixValue;
    UniformHandle m_utexture;
    float m_elapsedTm;
    bool m_finished;

public:
    explicit FadeEffect(Mode mode) :
        m_mode(mode),
        m_driver(nullptr),
        m_elapsedTm(0),
        m_finished(false)
    {
    }

    bool create() override
    {
        m_driver = getGfxDriver();
        m_ufadeColor = m_driver->createUniform("u_fadeColor", UniformType::Vec4, 1);
        m_umixValue = m_driver->createUniform("u_mixValue", UniformType::Vec4, 1);
        m_utexture = m_driver->createUniform("u_texture", UniformType::Int1, 1);

        ShaderHandle vshader;
        ShaderHandle fshader;
        switch (m_mode) {
        case FadeOutColor:
            vshader = m_driver->createShader(m_driver->makeRef(effect_fade_out_color_vso, sizeof(effect_fade_out_color_vso), nullptr, nullptr));
            fshader = m_driver->createShader(m_driver->makeRef(effect_fade_out_color_fso, sizeof(effect_fade_out_color_fso), nullptr, nullptr));
            break;
        case FadeInColor:
            vshader = m_driver->createShader(m_driver->makeRef(effect_fade_in_color_vso, sizeof(effect_fade_in_color_vso), nullptr, nullptr));
            fshader = m_driver->createShader(m_driver->makeRef(effect_fade_in_color_fso, sizeof(effect_fade_in_color_fso), nullptr, nullptr));
            break;
        case FadeOutAlpha:
            vshader = m_driver->createShader(m_driver->makeRef(effect_fade_out_alpha_vso, sizeof(effect_fade_out_alpha_vso), nullptr, nullptr));
            fshader = m_driver->createShader(m_driver->makeRef(effect_fade_out_alpha_fso, sizeof(effect_fade_out_alpha_fso), nullptr, nullptr));
            break;
        case FadeInAlpha:
            vshader = m_driver->createShader(m_driver->makeRef(effect_fade_in_alpha_vso, sizeof(effect_fade_in_alpha_vso), nullptr, nullptr));
            fshader = m_driver->createShader(m_driver->makeRef(effect_fade_in_alpha_fso, sizeof(effect_fade_in_alpha_fso), nullptr, nullptr));
            break;
        }

        if (!vshader.isValid() || !fshader.isValid())
            return false;
        m_prog = m_driver->createProgram(vshader, fshader, true);
        if (!m_prog.isValid())
            return false;

        return true;
    }

    void destroy() override
    {
        if (m_driver) {
            if (m_prog.isValid())
                m_driver->destroyProgram(m_prog);
            if (m_ufadeColor.isValid())
                m_driver->destroyUniform(m_ufadeColor);
            if (m_umixValue)
                m_driver->destroyUniform(m_umixValue);
            if (m_utexture)
                m_driver->destroyUniform(m_utexture);
        }
    }

    void begin(const void* params, uint8_t viewId) override
    {
        assert(params);

        memcpy(&m_params, params, sizeof(m_params));
        m_elapsedTm = 0;
        m_finished = false;
    }

    void render(float dt, uint8_t viewId, FrameBufferHandle renderFb, TextureHandle sourceTex) override
    {
        m_elapsedTm += dt;
        float normTm = bx::fmin(1.0f, m_elapsedTm / m_params.duration);
        vec4_t mixValue = vec4f(bx::fbias(normTm, m_params.biasFactor), 0, 0, 0);
        uint64_t extraState = (m_mode != FadeEffect::FadeOutAlpha && m_mode != FadeEffect::FadeInAlpha) ? 0 : 
            gfxStateBlendAlpha();
        m_driver->setViewFrameBuffer(viewId, renderFb);
        m_driver->setViewRectRatio(viewId, 0, 0, BackbufferRatio::Equal);
        m_driver->setState(GfxState::RGBWrite | GfxState::AlphaWrite | extraState, 0);
        m_driver->setTexture(0, m_utexture, sourceTex, TextureFlag::FromTexture);
        m_driver->setUniform(m_ufadeColor, m_params.fadeColor.f, 1);
        m_driver->setUniform(m_umixValue, mixValue.f, 1);
        drawFullscreenQuad(viewId, m_prog);

        m_finished = bx::fequal(normTm, 1.0f, 0.00001f);
    }

    void end() override
    {
        
    }

    bool isDone() const override
    {
        return m_finished;
    }
};

static FadeEffect g_fadeInColor(FadeEffect::FadeInColor);
static FadeEffect g_fadeOutColor(FadeEffect::FadeOutColor);
static FadeEffect g_fadeInAlpha(FadeEffect::FadeInAlpha);
static FadeEffect g_fadeOutAlpha(FadeEffect::FadeOutAlpha);

SceneManager* termite::createSceneManager(bx::AllocatorI* alloc)
{
    SceneManager* mgr = BX_NEW(alloc, SceneManager)(alloc);
    if (!mgr)
        return nullptr;

    uint32_t linkSize = (uint32_t)sizeof(SceneLink);
    if (!mgr->scenePool.create(32, alloc) ||
        !mgr->effects.create(8, 8, alloc) ||
        !mgr->linkPool.create(&linkSize, 1, 32, 64, alloc) ||
        !mgr->loader.create(alloc)) 
    {
        destroySceneManager(mgr);
        return nullptr;
    }

    // Register default effects
    registerSceneTransitionEffect(mgr, "FadeIn", &g_fadeInColor, sizeof(FadeEffectParams));
    registerSceneTransitionEffect(mgr, "FadeOut", &g_fadeOutColor, sizeof(FadeEffectParams));
    registerSceneTransitionEffect(mgr, "FadeInAlpha", &g_fadeInAlpha, sizeof(FadeEffectParams));
    registerSceneTransitionEffect(mgr, "FadeOutAlpha", &g_fadeOutAlpha, sizeof(FadeEffectParams));

    return mgr;
}

void termite::destroySceneManager(SceneManager* smgr)
{
    assert(smgr);

    // Release active Scenes
    for (int i = 0; i < smgr->numActiveScenes; i++)
        destroyScene(smgr, smgr->activeScenes[i]);

    // destroy all effects
    for (int i = 0, c = smgr->effects.getCount(); i < c; i++)
        smgr->effects[i].callbacks->destroy();

    smgr->loader.destroy();
    smgr->linkPool.destroy();
    smgr->effects.destroy();
    smgr->scenePool.destroy();
    BX_DELETE(smgr->alloc, smgr);
}

static void updateScene(SceneManager* mgr, Scene* scene, float dt, bool loadIfDead = false)
{
    if (loadIfDead) {
        if (scene->state == Scene::Dead)
            scene->state = Scene::LoadResource;
    }

    switch (scene->state) {
    case Scene::Ready:
        if (!scene->drawOnEffectFb) {
            scene->callbacks->update(scene, dt, mgr->viewId, mgr->mainFb, false);
        } else {
            scene->callbacks->update(scene, dt, mgr->viewId, mgr->effectFb, true);
        }
        mgr->viewId++;
        break;

        // LoadResource proceeds to Create
    case Scene::LoadResource:
        if (!scene->loaderGroup.isValid()) {
            mgr->loader.beginGroup(scene->loadScheme);
            scene->callbacks->loadResources(scene, mgr->loader);
            scene->loaderGroup = mgr->loader.endGroup();
        }

        // Proceed to next state if loaded
        if (mgr->loader.checkGroupDone(scene->loaderGroup)) {
            scene->state = Scene::Create;
            scene->loaderGroup.reset();
            updateScene(mgr, scene, dt);
        }
        break;

        // Create proceeds to Ready
    case Scene::Create:
    {
        SceneCallbackResult::Enum res = scene->callbacks->createObjects(scene);
        if (res == SceneCallbackResult::Finished) {
            scene->state = Scene::Ready;
        } else if (res == SceneCallbackResult::Failed) {
            scene->state = Scene::Ready;
            BX_WARN("Creating scene: %s failed", scene->name);
        }
        break;
    }

    // Destroy proceeds to unloadResource
    case Scene::Destroy:
    {
        SceneCallbackResult::Enum res = scene->callbacks->destroyObjects(scene);
        if (res == SceneCallbackResult::Finished) {
            scene->state = Scene::UnloadResource;
            updateScene(mgr, scene, dt);
        } else if (res == SceneCallbackResult::Failed) {
            scene->state = Scene::UnloadResource;
            BX_WARN("Destroying scene: %s failed", scene->name);
        }
        break;
    }

    // UnloadResource proceeds to Dead
    case Scene::UnloadResource:
        if (!scene->loaderGroup.isValid()) {
            mgr->loader.beginGroup(scene->loadScheme);
            scene->callbacks->unloadResources(scene, mgr->loader);
            scene->loaderGroup = mgr->loader.endGroup();
        }

        if (mgr->loader.checkGroupDone(scene->loaderGroup)) {
            scene->loaderGroup.reset();
            scene->state = Scene::Dead;
        }
        break;

    default:
        break;
    }
}

static void preloadScene(SceneManager* mgr, Scene* scene)
{
    while (scene->state != Scene::Ready) {
        if (getAsyncIoDriver())
            getAsyncIoDriver()->runAsyncLoop();
        mgr->loader.step(1.0f);
        updateScene(mgr, scene, 1.0f, true);
        bx::yieldCpu();
    }
}

static SceneLinkHandle findLink(SceneManager* mgr, std::function<bool(const SceneLink& link)> matchFn)
{
    for (uint16_t i = 0, c = mgr->linkPool.getCount(); i < c; i++) {
        uint16_t handle = mgr->linkPool.handleAt(i);
        if (!matchFn(*mgr->linkPool.getHandleData<SceneLink>(0, handle)))
            continue;
        return SceneLinkHandle(handle);
    }

    return SceneLinkHandle();
}

Scene* termite::createScene(SceneManager* mgr, const char* name, SceneCallbacksI* callbacks, 
                            uint32_t tag /*= 0*/, SceneFlag::Bits flags /*= 0*/, const LoadingScheme& loadScheme /*= LoadingScheme()*/, 
                            void* userData /*= nullptr*/, uint8_t order /*= 0*/)
{
    assert(mgr);

    Scene* scene = mgr->scenePool.newInstance<>();
    if (!scene)
        return nullptr;
    bx::strlcpy(scene->name, name, sizeof(scene->name));
    scene->order = order;
    scene->callbacks = callbacks;
    scene->tag = tag;
    scene->flags = flags;
    memcpy(&scene->loadScheme, &loadScheme, sizeof(loadScheme));
    scene->userData = userData;

    if (flags & SceneFlag::Preload)
        preloadScene(mgr, scene);

    mgr->sceneList.addToEnd(&scene->lnode);

    return scene;
}

static void addActiveScene(SceneManager* mgr, Scene* scene)
{
    assert(scene->state == Scene::Ready);

    int index = -1;
    for (int i = 0; i < mgr->numActiveScenes; i++) {
        if (mgr->activeScenes[i] == scene) {
            index = i;
            break;
        }
    }

    if (index == -1 && mgr->numActiveScenes < MAX_ACTIVE_SCENES) {
        mgr->activeScenes[mgr->numActiveScenes++] = scene;
        std::sort(mgr->activeScenes, mgr->activeScenes + mgr->numActiveScenes,
                  [](const Scene* sA, const Scene* sB)->bool { return sA->order < sB->order; });
    }
}

static bool removeActiveScene(SceneManager* mgr, Scene* scene)
{
    int index = -1;
    for (int i = 0; i < mgr->numActiveScenes; i++) {
        if (mgr->activeScenes[i] == scene) {
            index = i;
            break;
        }
    }

    if (index != -1) {
        std::swap<Scene*>(mgr->activeScenes[index], mgr->activeScenes[mgr->numActiveScenes-1]);
        mgr->numActiveScenes--;
        std::sort(mgr->activeScenes, mgr->activeScenes + mgr->numActiveScenes,
                  [](const Scene* sA, const Scene* sB)->bool { return sA->order < sB->order; });
        return true;
    }

    return false;
}


void termite::destroyScene(SceneManager* mgr, Scene* scene)
{
    assert(mgr);
    assert(scene);

    // If Scene is active, destroy and release all it's resources/objects
    if (scene->state != Scene::Dead) {
        scene->state = Scene::Destroy;

        while (scene->state != Scene::Dead) {
            mgr->loader.step(1.0f);
            updateScene(mgr, scene, 1.0f);
            bx::yieldCpu();
        }
    }

    removeActiveScene(mgr, scene);
    mgr->sceneList.remove(&scene->lnode);
    mgr->scenePool.deleteInstance(scene);

    // Delete all links related to this scene
    SceneLinkHandle linkHandle;
    do {
        linkHandle = findLink(mgr, [scene](const SceneLink& link)->bool {
            if (link.sceneA == scene || link.sceneB == scene)
                return true;
            else
                return false;
        });
        if (linkHandle.isValid())
            removeSceneLink(mgr, linkHandle);
    } while (linkHandle.isValid());
}

void* termite::getSceneUserData(Scene* scene)
{
    return scene->userData;
}

const char* termite::getSceneName(Scene* scene)
{
    return scene->name;
}

static void pushActiveLink(SceneManager* mgr, SceneLinkHandle handle)
{
    for (int i = 0, c = mgr->numActiveLinks; i < c; i++) {
        if (handle != mgr->activeLinks[i])
            continue;
        return;
    }

    if (mgr->numActiveLinks < MAX_ACTIVE_LINKS)
        mgr->activeLinks[mgr->numActiveLinks++] = handle;
}

static SceneLinkHandle popActiveLink(SceneManager* mgr)
{
    if (mgr->numActiveLinks > 0) {
        SceneLinkHandle handle = mgr->activeLinks[0];
        // we popped one from the start of the array, shift all links to left
        for (int i = 1; i < mgr->numActiveLinks; i++)
            mgr->activeLinks[i-1] = mgr->activeLinks[i];
        mgr->numActiveLinks--;
        return handle;
    } else {
        return SceneLinkHandle();
    }
}

static SceneTransitionEffect* findEffect(SceneManager* mgr, const char* name)
{
    size_t nameHash = tinystl::hash_string(name, strlen(name));
    for (int i = 0, c = mgr->effects.getCount(); i < c; i++) {
        SceneTransitionEffect* effect = mgr->effects.itemPtr(i);
        if (effect->nameHash != nameHash)
            continue;
        return effect;
    }
    return nullptr;
}

bool termite::registerSceneTransitionEffect(SceneManager* mgr, const char* name, SceneTransitionEffectCallbacksI* callbacks,
                                            uint32_t paramSize)
{
    size_t nameHash = tinystl::hash_string(name, strlen(name));
    for (int i = 0; i < mgr->effects.getCount(); i++) {
        if (mgr->effects[i].nameHash == nameHash)
            return false;
    }

    SceneTransitionEffect* fx = mgr->effects.push();
    if (!fx)
        return false;
    fx->nameHash = nameHash;
    fx->callbacks = callbacks;
    fx->paramSize = paramSize;

    return callbacks->create();
}

SceneLinkHandle termite::linkScene(SceneManager* mgr, Scene* sceneA, Scene* sceneB, const SceneLinkDef& def /*= SceneLinkDef()*/)
{
    assert(sceneA);
    assert(sceneB);

    SceneLinkHandle handle = SceneLinkHandle(mgr->linkPool.newHandle());
    if (!handle.isValid())
        return handle;
    SceneLink* link = new(mgr->linkPool.getHandleData(0, handle)) SceneLink();
    link->sceneA = sceneA;
    link->sceneB = sceneB;
    link->effectA = def.effectNameA ? findEffect(mgr, def.effectNameA) : nullptr;
    link->effectB = def.effectNameB ? findEffect(mgr, def.effectNameB) : nullptr;
    link->loadScene = def.loadScene;
    if (link->effectA && link->effectA->paramSize > 0) {
        assert(def.effectParamsA);
        memcpy(link->effectParamsA, def.effectParamsA, std::min<uint32_t>(link->effectA->paramSize, sizeof(link->effectParamsA)));
    }

    if (link->effectB && link->effectB->paramSize > 0) {
        assert(def.effectParamsB);
        memcpy(link->effectParamsB, def.effectParamsB, std::min<uint32_t>(link->effectB->paramSize, sizeof(link->effectParamsB)));
    }

    return handle;
}

void termite::removeSceneLink(SceneManager* mgr, SceneLinkHandle handle)
{
    mgr->linkPool.freeHandle(handle);
}

void termite::triggerSceneLink(SceneManager* mgr, SceneLinkHandle handle)
{
    SceneLink* link = mgr->linkPool.getHandleData<SceneLink>(0, handle);

    // link sceneA should belong to one of the active scenes
    Scene* sceneA = link->sceneA;
    for (int i = 0, c = mgr->numActiveScenes; i < c; i++) {
        if (sceneA == mgr->activeScenes[i]) {
            pushActiveLink(mgr, handle);
            return;
        }
    }
}

Scene* termite::findScene(SceneManager* mgr, const char* name, FindSceneMode::Enum mode /*= 0*/)
{
    switch (mode) {
    case FindSceneMode::Active:
        for (int i = 0; i < mgr->numActiveScenes; i++) {
            if (bx::stricmp(mgr->activeScenes[i]->name, name) == 0)
                return mgr->activeScenes[i];
        }
        break;
    case FindSceneMode::Linked:
        assert(0);
        break;
    default:
    {
        bx::List<Scene*>::Node* node = mgr->sceneList.getFirst();
        while (node) {
            if (bx::stricmp(node->data->name, name) == 0)
                return node->data;
            node = node->next;
        }
        break;
    }
    }

    return nullptr;
}

int termite::findSceneByTag(SceneManager* mgr, Scene** pScenes, int maxScenes, uint32_t tag, FindSceneMode::Enum mode /*= 0*/)
{
    int index = 0;
    switch (mode) {
    case FindSceneMode::Active:
        for (int i = 0; i < mgr->numActiveScenes && index < maxScenes; i++) {
            if (mgr->activeScenes[i]->tag == tag)
                pScenes[index++] = mgr->activeScenes[i];
        }
        break;
    case FindSceneMode::Linked:
        assert(0);
        break;
    default:
    {
        bx::List<Scene*>::Node* node = mgr->sceneList.getFirst();
        while (node && index < maxScenes) {
            if (node->data->tag == tag)
                pScenes[index++] = node->data;
            node = node->next;
        }
        break;
    }
    }

    return index;
}

static void updateLink(SceneManager* mgr, SceneLink* link, float dt)
{
    switch (link->state) {
    case SceneLink::InA:
        if (link->effectA) {
            if (!link->effectBeginA) {
                link->effectA->callbacks->begin(link->effectParamsA, mgr->viewId);
                link->effectBeginA = true;
                link->sceneA->drawOnEffectFb = (link->sceneA->flags & SceneFlag::Overlay) ? true : false;
            }

            if (!(link->sceneA->flags & SceneFlag::Overlay)) {
                link->effectA->callbacks->render(dt, mgr->viewId, mgr->effectFb, mgr->mainTex);
                mgr->finalFb = mgr->effectFb;
                mgr->finalTex = mgr->effectTex;
            } else {
                link->effectA->callbacks->render(dt, mgr->viewId, mgr->mainFb, mgr->effectTex);
            }

            if (link->effectA->callbacks->isDone()) {
                link->effectA->callbacks->end();
                link->effectBeginA = false;
                link->sceneA->drawOnEffectFb = false;

                link->state = SceneLink::InLoad;
                updateLink(mgr, link, dt);
            }
        } else {
            link->state = SceneLink::InLoad;
            updateLink(mgr, link, dt);
        }
        break;

    case SceneLink::InLoad:
        /*if (link->effectA) {
            mgr->finalFb = mgr->effectFb;
            mgr->finalTex = mgr->effectTex;
        }*/

        // add 'loading' scene to active scenes
        if (link->loadScene)
            addActiveScene(mgr, link->loadScene);

        if (link->sceneB->state == Scene::Ready) {
            // sceneB is ready, decide what should we do with sceneA
            if (!(link->sceneB->flags & SceneFlag::Overlay)) {
                // Issue destroy for non-cached scenes
                if (removeActiveScene(mgr, link->sceneA)) {
                    link->sceneA->callbacks->onExit(link->sceneA, link->sceneB);
                    if (!(link->sceneA->flags & SceneFlag::CacheAlways))
                        link->sceneA->state = Scene::Destroy;
                }

                if (link->sceneA->state != Scene::Ready)
                    updateScene(mgr, link->sceneA, dt); // update until Dead
            }

            // sceneB is ready, proceed to next step
            if ((link->sceneB->flags & SceneFlag::Overlay) || 
                (link->sceneA->flags & SceneFlag::CacheAlways) ||
                link->sceneA->state == Scene::Dead) 
            {
                link->state = SceneLink::InB;
                if (link->loadScene)
                    removeActiveScene(mgr, link->loadScene);
                addActiveScene(mgr, link->sceneB);
                link->sceneB->callbacks->onEnter(link->sceneB, link->sceneA);
                updateLink(mgr, link, dt);
            }                
        } else {
            updateScene(mgr, link->sceneB, dt, true);   // update sceneB until Ready
        }
        break;

    case SceneLink::InB:
        if (link->effectB) {
            if (!link->effectBeginB) {
                link->effectB->callbacks->begin(link->effectParamsB, mgr->viewId);
                link->effectBeginB = true;
                link->sceneB->drawOnEffectFb = (link->sceneB->flags & SceneFlag::Overlay) ? true : false;
            }

            if (!(link->sceneB->flags & SceneFlag::Overlay)) {
                link->effectB->callbacks->render(dt, mgr->viewId, mgr->effectFb, mgr->mainTex);
                mgr->finalFb = mgr->effectFb;
                mgr->finalTex = mgr->effectTex;
            } else {
                link->effectB->callbacks->render(dt, mgr->viewId, mgr->mainFb, mgr->effectTex);
            }

            if (link->effectB->callbacks->isDone()) {
                link->effectB->callbacks->end();
                link->effectBeginB = false;
                link->sceneB->drawOnEffectFb = false;
                // Finished
                link->state = SceneLink::InA;
                popActiveLink(mgr);
            }
        } else {
            // Finished
            link->state = SceneLink::InA;
            popActiveLink(mgr);
        }
        break;

    default:
        break;
    }
}

void termite::updateSceneManager(SceneManager* mgr, float dt, uint8_t* viewId,
                                 FrameBufferHandle* pRenderFb, TextureHandle* pRenderTex)
{
    assert(viewId);
    mgr->viewId = *viewId;
    mgr->finalFb = mgr->mainFb;
    mgr->finalTex = mgr->mainTex;

    mgr->loader.step(dt);

    for (int i = 0, c = mgr->numActiveScenes; i < c; i++) {
        updateScene(mgr, mgr->activeScenes[i], dt, true);
    }

    if (mgr->numActiveLinks) {
        SceneLinkHandle handle = mgr->activeLinks[0];
        updateLink(mgr, mgr->linkPool.getHandleData<SceneLink>(0,  handle), dt);
    }

    *viewId = mgr->viewId;
    if (pRenderFb)
        *pRenderFb = mgr->finalFb;
    if (pRenderTex)
        *pRenderTex = mgr->finalTex;
}

void termite::startSceneManager(SceneManager* mgr, Scene* entryScene,
                                FrameBufferHandle mainFb, TextureHandle mainTex,
                                FrameBufferHandle effectFb, TextureHandle effectTex)
{
    assert(entryScene);
    mgr->numActiveScenes = 0;
    mgr->numActiveLinks = 0;
    mgr->mainFb = mainFb;
    mgr->mainTex = mainTex;
    mgr->effectFb = effectFb;
    mgr->effectTex = effectTex;

    // PreLoad First scene
    if (entryScene->state != Scene::Ready) {
        preloadScene(mgr, entryScene);
    }

    addActiveScene(mgr, entryScene);
}

void termite::debugSceneManager(SceneManager* mgr)
{
    static bool opened = false;
    static int curActiveScene = -1;

    ImGuiApi_v0* imgui = (ImGuiApi_v0*)getEngineApi(ApiId::ImGui, 0);
    if (imgui->begin("SceneManager", &opened, 0)) {
        const char* scenes[MAX_ACTIVE_SCENES];
        for (int i = 0, c = mgr->numActiveScenes; i < c; i++)
            scenes[i] = mgr->activeScenes[i]->name;

        imgui->listBox("Active Scenes", &curActiveScene, scenes, mgr->numActiveScenes, -1);
        if (mgr->numActiveLinks) {
            SceneLink* link = mgr->linkPool.getHandleData<SceneLink>(0, mgr->activeLinks[0]);
            imgui->labelText("Active Link", "%s -> %s", link->sceneA->name, link->sceneB->name);
        }

        imgui->image((ImTextureID)&mgr->mainTex, ImVec2(128, 128), ImVec2(0, 0), ImVec2(1.0f, 1.0f),
                     ImVec4(1.0f, 1.0f, 1.0f, 1.0f), ImVec4(255, 255, 255, 255));
        imgui->image((ImTextureID)&mgr->effectTex, ImVec2(128, 128), ImVec2(0, 0), ImVec2(1.0f, 1.0f),
                     ImVec4(1.0f, 1.0f, 1.0f, 1.0f), ImVec4(255, 255, 255, 255));
    }
    imgui->end();
}
