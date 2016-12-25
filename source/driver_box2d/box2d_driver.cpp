#include "termite/core.h"

#include "termite/physics_2d.h"
#include "termite/camera.h"

#include "Box2D/Box2D.h"
#include "nanovg.h"

#include "bxx/pool.h"
#include "bxx/proxy_allocator.h"
#include "bxx/hash_table.h"

#include <assert.h>

#define T_CORE_API
#define T_GFX_API
#define T_CAMERA_API
#include "termite/plugin_api.h"

using namespace termite;

// conversion functions
inline b2Vec2 b2vec2(const vec2_t& v)
{
    return b2Vec2(v.x, v.y);
}

inline vec2_t tvec2(const b2Vec2& v)
{
    return vec2f(v.x, v.y);
}

class PhysDebugDraw : public b2Draw
{
private:
    NVGcontext* m_nvg;
    rect_t m_viewRect;
    float m_strokeScale;

public:
    PhysDebugDraw() :
        m_nvg(nullptr),
        m_strokeScale(0)
    {
    }

    void beginDraw(NVGcontext* nvg, const Camera2D& cam, int viewWidth, int viewHeight);
    void endDraw();
    bool intersectVerts(const b2Vec2* verts, int vertexCount) const;
    
    void DrawPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color) override;
    void DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color) override;
    void DrawCircle(const b2Vec2& center, float32 radius, const b2Color& color) override;
    void DrawSolidCircle(const b2Vec2& center, float32 radius, const b2Vec2& axis, const b2Color& color) override;
    void DrawParticles(const b2Vec2 *centers, float32 radius, const b2ParticleColor *colors, int32 count) override;
    void DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color) override;
    void DrawTransform(const b2Transform& xf) override;
};

class ContactListenerBox2d : public b2ContactListener
{
public:
    ContactListenerBox2d() {}

    void BeginContact(b2Contact* contact) override;
    void EndContact(b2Contact* contact) override;
    void BeginContact(b2ParticleSystem* particleSystem, b2ParticleBodyContact* particleBodyContact) override;
    void EndContact(b2Fixture* fixture, b2ParticleSystem* particleSystem, int32 index) override;
    void BeginContact(b2ParticleSystem* particleSystem, b2ParticleContact* particleContact) override;
    void EndContact(b2ParticleSystem* particleSystem, int32 indexA, int32 indexB) override;
};

class ContactFilterBox2d : public b2ContactFilter
{
public:
    ContactFilterBox2d() {}

    bool ShouldCollide(b2Fixture* fixtureA, b2Fixture* fixtureB) override;
    bool ShouldCollide(b2Fixture* fixture, b2ParticleSystem* particleSystem, int32 particleIndex) override;
    bool ShouldCollide(b2ParticleSystem* particleSystem, int32 particleIndexA, int32 particleIndexB) override;
};

class DestructionListenerBox2d : public b2DestructionListener
{
public:
    DestructionListenerBox2d() {}

    void SayGoodbye(b2Joint* joint) override;
    void SayGoodbye(b2Fixture* fixture) override;
    void SayGoodbye(b2ParticleGroup* group) override;
    void SayGoodbye(b2ParticleSystem* particleSystem, int32 index) override;
};

namespace termite
{
    struct PhysScene2D
    {
        b2World* w;
        ContactListenerBox2d contactListener;
        ContactFilterBox2d contactFilter;
        DestructionListenerBox2d destructionListener;
        PhysDebugDraw debugDraw;
        float timestep;
        float accumulator;

        PhysScene2D() :
            w(nullptr),
            timestep(0),
            accumulator(0)
        {
        }
    };

    struct PhysBody2D
    {
        PhysScene2D* ownerScene;
        b2Body* b;
        void* userData;

        PhysBody2D() :
            b(nullptr),
            userData(nullptr)
        {}
    };

    struct PhysShape2D
    {
        PhysBody2D* ownerBody;
        b2Fixture* fixture;
        void* userData;
        PhysShapeDestroyCallback2D destroyFn;
        PhysShapeContactFilterCallback2D contactFilterFn;
        PhysShapeContactCallback2D beginContactFn;
        PhysShapeContactCallback2D endContactFn;

        bool beginContactReportInfo;

        PhysShape2D() :
            ownerBody(nullptr),
            fixture(nullptr),
            userData(nullptr),
            destroyFn(nullptr),
            contactFilterFn(nullptr),
            beginContactReportInfo(false)
        {}
    };

    struct PhysJoint2D
    {
        b2Joint* j;
        void* userData;
        PhysJointDestroyCallback2D destroyFn;

        PhysJoint2D() :
            j(nullptr),
            userData(nullptr),
            destroyFn(nullptr)
        {}
    };

    struct PhysParticleEmitter2D
    {
        b2ParticleSystem* p;
        void* userData;
        PhysParticleDestroyCallback2D destroyFn;
        PhysParticleShapeContactFilterCallback2D shapeContactFilterFn;
        PhysParticleContactFilterCallback2D particleContactFilterFn;
        PhysParticleShapeContactCallback2D shapeBeginContactFn;
        PhysParticleContactCallback2D particleBeginContactFn;
        PhysParticleShapeContactCallback2D shapeEndContactFn;
        PhysParticleContactCallback2D particleEndContactFn;

        PhysParticleEmitter2D() :
            p(nullptr),
            userData(nullptr),
            destroyFn(nullptr)
        {}
    };

    struct PhysParticleGroup2D
    {
        b2ParticleGroup* g;
        void* userData;
        PhysParticleGroupDestroyCallback2D destroyFn;

        PhysParticleGroup2D() :
            g(nullptr),
            userData(nullptr),
            destroyFn(nullptr)
        {}
    };
} // namespace termite



struct Box2dDriver
{
    bx::AllocatorI* alloc;
    bx::Pool<PhysScene2D> scenePool;
    bx::Pool<PhysBody2D> bodyPool;
    bx::Pool<PhysShape2D> shapePool;
    bx::Pool<PhysJoint2D> jointPool;
    bx::Pool<PhysParticleEmitter2D> emitterPool;
    bx::Pool<PhysParticleGroup2D> pgroupPool;
    bx::HashTable<PhysParticleEmitter2D*, uintptr_t> emitterTable;   // key=pointer-to-b2ParticleSystem -> ParticleEmitter
    NVGcontext* nvg;

    Box2dDriver() :
        emitterTable(bx::HashTableType::Mutable),
        nvg(nullptr)
    {}
};

static Box2dDriver g_box2d;
static CoreApi_v0* g_coreApi = nullptr;
static GfxApi_v0* g_gfxApi = nullptr;
static CameraApi_v0* g_camApi = nullptr;

static result_t initBox2d(bx::AllocatorI* alloc, PhysFlags2D::Bits flags, uint8_t debugViewId)
{
    if (!g_box2d.scenePool.create(6, alloc) ||
        !g_box2d.bodyPool.create(200, alloc) ||
        !g_box2d.shapePool.create(200, alloc) ||
        !g_box2d.jointPool.create(100, alloc) ||
        !g_box2d.emitterPool.create(30, alloc) ||
        !g_box2d.pgroupPool.create(30, alloc))
    {
        return T_ERR_OUTOFMEM;
    }

    if (!g_box2d.emitterTable.create(30, alloc))
        return T_ERR_OUTOFMEM;

    if ((flags & PhysFlags2D::EnableDebug)	 && g_coreApi->getGfxDriver()) {
        g_box2d.nvg = nvgCreate(0, debugViewId, g_coreApi->getGfxDriver(), g_gfxApi, alloc);
        if (!g_box2d.nvg) {
            BX_WARN_API(g_coreApi, "Initializing NanoVg for Debugging Physics failed");
        }
    }

    g_box2d.alloc = alloc;
    return 0;
}

static void shutdownBox2d()
{
    if (g_box2d.nvg)
        nvgDelete(g_box2d.nvg);

    g_box2d.emitterTable.destroy();
    g_box2d.pgroupPool.destroy();
    g_box2d.emitterPool.destroy();
    g_box2d.jointPool.destroy();
    g_box2d.shapePool.destroy();
    g_box2d.bodyPool.destroy();
    g_box2d.scenePool.destroy();
    g_box2d.alloc = nullptr;
}

static void* b2AllocCallback(int32 size, void* callbackData)
{
    assert(g_box2d.alloc);
    return BX_ALLOC(g_box2d.alloc, size);
}

static void b2FreeCallback(void* mem, void* callbackData)
{
    assert(g_box2d.alloc);
    BX_FREE(g_box2d.alloc, mem);
}

static PhysScene2D* createSceneBox2d(const PhysSceneDef2D& worldDef)
{
    PhysScene2D* scene = g_box2d.scenePool.newInstance();
    if (!scene)
        return nullptr;
    b2SetAllocFreeCallbacks(b2AllocCallback, b2FreeCallback, nullptr);
    scene->w = BX_NEW(g_box2d.alloc, b2World)(b2vec2(worldDef.gravity));
    if (!scene->w)
        return nullptr;
    scene->timestep = worldDef.timestep;
    scene->w->SetContactFilter(&scene->contactFilter);
    scene->w->SetContactListener(&scene->contactListener);
    scene->w->SetDestructionListener(&scene->destructionListener);
    return scene;
}

static void destroySceneBox2d(PhysScene2D* scene)
{
    bx::AllocatorI* alloc = g_box2d.alloc;
    if (scene->w)
        BX_DELETE(alloc, scene->w);
    g_box2d.scenePool.deleteInstance(scene);
}

static void stepSceneBox2d(PhysScene2D* scene, float dt)
{
    const float timestep = scene->timestep;
    float accum = scene->accumulator + dt;
    while (accum >= timestep) {
        scene->w->Step(timestep, 8, 3, 2);
        accum -= timestep;
    }
    scene->accumulator = accum;

    // TODO: Interpolate transform data

}

static void debugSceneBox2d(PhysScene2D* scene, int viewWidth, int viewHeight, const Camera2D& cam,
                            PhysDebugFlags2D::Bits flags)
{
    assert(scene);
    assert(scene->w);

    if (g_box2d.nvg) {
        scene->debugDraw.SetFlags(flags);
        scene->w->SetDebugDraw(&scene->debugDraw);
        scene->debugDraw.beginDraw(g_box2d.nvg, cam, viewWidth, viewHeight);
        scene->w->DrawDebugData();
        scene->debugDraw.endDraw();
    }
}

static PhysBody2D* createBodyBox2d(PhysScene2D* scene, const PhysBodyDef2D& bodyDef)
{
    assert(scene->w);
    static_assert(PhysBodyType2D::Static == b2_staticBody, "BodyType mismatch");
    static_assert(PhysBodyType2D::Dynamic == b2_dynamicBody, "BodyType mismatch");
    static_assert(PhysBodyType2D::Kinematic == b2_kinematicBody, "BodyType mismatch");

    PhysBody2D* pbody = g_box2d.bodyPool.newInstance();
    if (!pbody)
        return nullptr;
    pbody->ownerScene = scene;

    b2BodyDef bdef;
    bdef.type = (b2BodyType)bodyDef.type;
    bdef.allowSleep = (bodyDef.flags & PhysBodyFlags2D::AllowSleep) ? true : false;
    bdef.active = (bodyDef.flags & PhysBodyFlags2D::IsActive) ? true : false;
    bdef.angle = bodyDef.angle;
    bdef.angularDamping = bodyDef.angularDamping;
    bdef.linearVelocity = b2vec2(bodyDef.linearVel);
    bdef.bullet = (bodyDef.flags & PhysBodyFlags2D::IsBullet) ? true : false;
    bdef.fixedRotation = (bodyDef.flags & PhysBodyFlags2D::FixedRotation) ? true : false;
    bdef.gravityScale = bodyDef.gravityScale;
    bdef.linearDamping = bodyDef.linearDamping;
    bdef.position = b2vec2(bodyDef.position);
    bdef.userData = pbody;

    b2Body* bbody = scene->w->CreateBody(&bdef);
    if (!bbody) {
        g_box2d.bodyPool.deleteInstance(pbody);
        return nullptr;
    }

    pbody->b = bbody;
    pbody->userData = bdef.userData;
    return pbody;
}

static void destroyBodyBox2d(PhysBody2D* body)
{
    assert(body);
    assert(body->b);
    
    body->ownerScene->w->DestroyBody(body->b);
    g_box2d.bodyPool.deleteInstance(body);
}

static PhysShape2D* createBoxShapeBox2d(PhysBody2D* body, const vec2_t& halfSize, const PhysShapeDef2D& shapeDef)
{
    PhysShape2D* shape = g_box2d.shapePool.newInstance();
    if (!shape)
        return nullptr;
    b2PolygonShape box;
    box.SetAsBox(halfSize.x, halfSize.y);

    b2FixtureDef fdef;
    fdef.friction = shapeDef.friction;
    fdef.userData = shape;
    fdef.density = shapeDef.density;
    fdef.filter.categoryBits = shapeDef.categoryBits;
    fdef.filter.groupIndex = shapeDef.groupIndex;
    fdef.filter.maskBits = shapeDef.maskBits;
    fdef.restitution = shapeDef.restitution;
    fdef.shape = &box;

    shape->ownerBody = body;
    shape->fixture = body->b->CreateFixture(&fdef);
    if (!shape->fixture) {
        g_box2d.shapePool.deleteInstance(shape);
        return nullptr;
    }
    shape->userData = shapeDef.userData;
    return shape;
}

static PhysShape2D* createArbitaryBoxShapeBox2d(PhysBody2D* body, const vec2_t& halfSize, const vec2_t& pos, float angle,
                                                const PhysShapeDef2D& shapeDef)
{
    PhysShape2D* shape = g_box2d.shapePool.newInstance();
    if (!shape)
        return nullptr;
    b2PolygonShape box;
    box.SetAsBox(halfSize.x, halfSize.y, b2vec2(pos), angle);

    b2FixtureDef fdef;
    fdef.friction = shapeDef.friction;
    fdef.userData = shape;
    fdef.density = shapeDef.density;
    fdef.filter.categoryBits = shapeDef.categoryBits;
    fdef.filter.groupIndex = shapeDef.groupIndex;
    fdef.filter.maskBits = shapeDef.maskBits;
    fdef.restitution = shapeDef.restitution;
    fdef.shape = &box;

    shape->ownerBody = body;
    shape->fixture = body->b->CreateFixture(&fdef);
    if (!shape->fixture) {
        g_box2d.shapePool.deleteInstance(shape);
        return nullptr;
    }
    shape->userData = shapeDef.userData;
    return shape;
}

static PhysShape2D* createPolyShapeBox2d(PhysBody2D* body, const vec2_t* verts, int numVerts, const PhysShapeDef2D& shapeDef)
{
    PhysShape2D* shape = g_box2d.shapePool.newInstance();
    if (!shape)
        return nullptr;
    b2PolygonShape poly;
    b2Vec2* bverts = (b2Vec2*)alloca(sizeof(b2Vec2)*numVerts);
    if (!bverts)
        return nullptr;
    for (int i = 0; i < numVerts; i++)
        bverts[i] = b2vec2(verts[i]);
    poly.Set(bverts, numVerts);

    b2FixtureDef fdef;
    fdef.friction = shapeDef.friction;
    fdef.userData = shape;
    fdef.density = shapeDef.density;
    fdef.filter.categoryBits = shapeDef.categoryBits;
    fdef.filter.groupIndex = shapeDef.groupIndex;
    fdef.filter.maskBits = shapeDef.maskBits;
    fdef.restitution = shapeDef.restitution;
    fdef.shape = &poly;

    shape->ownerBody = body;
    shape->fixture = body->b->CreateFixture(&fdef);
    if (!shape->fixture) {
        g_box2d.shapePool.deleteInstance(shape);
        return nullptr;
    }
    shape->userData = shapeDef.userData;
    return shape;
}

static PhysShape2D* createCircleShapeBox2d(PhysBody2D* body, const vec2_t& pos, float radius, const PhysShapeDef2D& shapeDef)
{
    PhysShape2D* shape = g_box2d.shapePool.newInstance();
    if (!shape)
        return nullptr;
    b2CircleShape circle;
    circle.m_p = b2vec2(pos);
    circle.m_radius = radius;
    
    b2FixtureDef fdef;
    fdef.friction = shapeDef.friction;
    fdef.userData = shape;
    fdef.density = shapeDef.density;
    fdef.filter.categoryBits = shapeDef.categoryBits;
    fdef.filter.groupIndex = shapeDef.groupIndex;
    fdef.filter.maskBits = shapeDef.maskBits;
    fdef.restitution = shapeDef.restitution;
    fdef.shape = &circle;

    shape->ownerBody = body;
    shape->fixture = body->b->CreateFixture(&fdef);
    if (!shape->fixture) {
        g_box2d.shapePool.deleteInstance(shape);
        return nullptr;
    }
    shape->userData = shapeDef.userData;
    return shape;
}

void ContactListenerBox2d::BeginContact(b2Contact* contact)
{
    const b2Fixture* fixtureA = contact->GetFixtureA();
    const b2Fixture* fixtureB = contact->GetFixtureB();
    PhysShape2D* shapeA = (PhysShape2D*)fixtureA->GetUserData();
    PhysShape2D* shapeB = (PhysShape2D*)fixtureB->GetUserData();

    if (shapeA->beginContactFn) {
        if (!shapeA->beginContactReportInfo) {
            shapeA->beginContactFn(shapeA, shapeB, nullptr);
        } else {
            b2WorldManifold manifold;
            PhysContactInfo2D cinfo;
            contact->GetWorldManifold(&manifold);
            cinfo.normal = tvec2(manifold.normal);
            cinfo.points[0] = tvec2(manifold.points[0]);
            cinfo.points[1] = tvec2(manifold.points[1]);
            cinfo.separations[0] = manifold.separations[0];
            cinfo.separations[1] = manifold.separations[1];
            shapeA->beginContactFn(shapeA, shapeB, &cinfo);
        }
    }

    if (shapeB->beginContactFn) {
        if (!shapeB->beginContactReportInfo) {
            shapeB->beginContactFn(shapeB, shapeA, nullptr);
        } else {
            b2WorldManifold manifold;
            PhysContactInfo2D cinfo;
            contact->GetWorldManifold(&manifold);
            cinfo.normal = tvec2(manifold.normal);
            cinfo.points[0] = tvec2(manifold.points[0]);
            cinfo.points[1] = tvec2(manifold.points[1]);
            cinfo.separations[0] = manifold.separations[0];
            cinfo.separations[1] = manifold.separations[1];
            shapeB->beginContactFn(shapeB, shapeA, &cinfo);
        }
    }
}

void ContactListenerBox2d::BeginContact(b2ParticleSystem* particleSystem, b2ParticleBodyContact* particleBodyContact)
{
    int index = g_box2d.emitterTable.find(uintptr_t(particleSystem));
    if (index != -1) {
        PhysParticleEmitter2D* emitter = g_box2d.emitterTable.getValue(index);
        if (emitter->shapeBeginContactFn) {
            emitter->shapeBeginContactFn(emitter, particleBodyContact->index,
                (PhysShape2D*)particleBodyContact->fixture->GetUserData(),
                                         tvec2(particleBodyContact->normal), particleBodyContact->weight);
        }
    }
}

void ContactListenerBox2d::BeginContact(b2ParticleSystem* particleSystem, b2ParticleContact* particleContact)
{
    int index = g_box2d.emitterTable.find(uintptr_t(particleSystem));
    if (index != -1) {
        PhysParticleEmitter2D* emitter = g_box2d.emitterTable.getValue(index);
        if (emitter->particleBeginContactFn) {
            emitter->particleBeginContactFn(emitter, particleContact->GetIndexA(), particleContact->GetIndexB(),
                                            tvec2(particleContact->GetNormal()), particleContact->GetWeight());
        }
    }
}

void ContactListenerBox2d::EndContact(b2Contact* contact)
{
    const b2Fixture* fixtureA = contact->GetFixtureA();
    const b2Fixture* fixtureB = contact->GetFixtureB();
    PhysShape2D* shapeA = (PhysShape2D*)fixtureA->GetUserData();
    PhysShape2D* shapeB = (PhysShape2D*)fixtureB->GetUserData();

    if (shapeA->endContactFn)
        shapeA->endContactFn(shapeA, shapeB, nullptr);

    if (shapeB->endContactFn)
        shapeB->endContactFn(shapeB, shapeA, nullptr);
}

void ContactListenerBox2d::EndContact(b2Fixture* fixture, b2ParticleSystem* particleSystem, int32 index)
{
    int r = g_box2d.emitterTable.find(uintptr_t(particleSystem));
    if (r != -1) {
        PhysParticleEmitter2D* emitter = g_box2d.emitterTable.getValue(r);
        if (emitter->shapeEndContactFn) {
            emitter->shapeEndContactFn(emitter, index, (PhysShape2D*)fixture->GetUserData(), vec2f(0, 0), 0);
        }
    }
}

void ContactListenerBox2d::EndContact(b2ParticleSystem* particleSystem, int32 indexA, int32 indexB)
{
    int r = g_box2d.emitterTable.find(uintptr_t(particleSystem));
    if (r != -1) {
        PhysParticleEmitter2D* emitter = g_box2d.emitterTable.getValue(r);
        if (emitter->particleEndContactFn) {
            emitter->particleEndContactFn(emitter, indexA, indexB, vec2f(0, 0), 0);
        }
    }
}

bool ContactFilterBox2d::ShouldCollide(b2Fixture* fixtureA, b2Fixture* fixtureB)
{
    PhysShape2D* shapeA = (PhysShape2D*)fixtureA->GetUserData();
    PhysShape2D* shapeB = (PhysShape2D*)fixtureB->GetUserData();
    if (shapeA->contactFilterFn)
        return shapeA->contactFilterFn(shapeA, shapeB);
    if (shapeB->contactFilterFn)
        return shapeB->contactFilterFn(shapeB, shapeA);
    return true;
}

bool ContactFilterBox2d::ShouldCollide(b2Fixture* fixture, b2ParticleSystem* particleSystem, int32 particleIndex)
{
    int index = g_box2d.emitterTable.find(uintptr_t(particleSystem));
    if (index != -1) {
        PhysParticleEmitter2D* emitter = g_box2d.emitterTable.getValue(index);
        if (emitter->shapeContactFilterFn) {
            return emitter->shapeContactFilterFn(emitter, particleIndex, (PhysShape2D*)fixture->GetUserData());
        }
    }
    return true;
}

bool ContactFilterBox2d::ShouldCollide(b2ParticleSystem* particleSystem, int32 particleIndexA, int32 particleIndexB)
{
    int index = g_box2d.emitterTable.find(uintptr_t(particleSystem));
    if (index != -1) {
        PhysParticleEmitter2D* emitter = g_box2d.emitterTable.getValue(index);
        if (emitter->particleContactFilterFn) {
            emitter->particleContactFilterFn(emitter, particleIndexA, particleIndexB);
        }
    }
    return true;
}


void DestructionListenerBox2d::SayGoodbye(b2Joint* joint)
{
    PhysJoint2D* pj = (PhysJoint2D*)joint->GetUserData();
    if (pj->destroyFn)
        pj->destroyFn(pj);
    g_box2d.jointPool.deleteInstance(pj);
}

void DestructionListenerBox2d::SayGoodbye(b2Fixture* fixture)
{
    PhysShape2D* shape = (PhysShape2D*)fixture->GetUserData();
    if (shape->destroyFn)
        shape->destroyFn(shape);
    g_box2d.shapePool.deleteInstance(shape);
}

void DestructionListenerBox2d::SayGoodbye(b2ParticleGroup* group)
{
    PhysParticleGroup2D* pgroup = (PhysParticleGroup2D*)group->GetUserData();
    if (pgroup->destroyFn)
        pgroup->destroyFn(pgroup);
    g_box2d.pgroupPool.deleteInstance(pgroup);
}

void DestructionListenerBox2d::SayGoodbye(b2ParticleSystem* particleSystem, int32 index)
{
    int r = g_box2d.emitterTable.find(uintptr_t(particleSystem));
    if (r != -1) {
        PhysParticleEmitter2D* emitter = g_box2d.emitterTable.getValue(r);
        if (emitter->destroyFn)
            emitter->destroyFn(emitter, index);
    }
}

void PhysDebugDraw::beginDraw(NVGcontext* nvg, const Camera2D& cam, int viewWidth, int viewHeight)
{
    assert(nvg);
    m_nvg = nvg;
    nvgBeginFrame(nvg, viewWidth, viewHeight, 1.0f);

    // Adjust nvg to camera
    nvgTranslate(nvg, float(viewWidth)*0.5f, float(viewHeight)*0.5f);
    float scale = 1.0f;
    switch (cam.policy) {
    case DisplayPolicy::FitToHeight:
        scale = 0.5f*float(viewWidth) * cam.zoom;
        break;
    case DisplayPolicy::FitToWidth:
        scale = 0.5f*float(viewHeight) * cam.zoom;
        break;
    }
    
    nvgScale(nvg, scale, -scale);
    nvgTranslate(nvg, -cam.pos.x, -cam.pos.y);

    m_strokeScale = 2.0f / scale;
    m_viewRect = g_camApi->cam2dGetRect(cam);
}

void PhysDebugDraw::endDraw()
{
    nvgEndFrame(m_nvg);
    m_nvg = nullptr;
}

bool PhysDebugDraw::intersectVerts(const b2Vec2* verts, int vertexCount) const
{
    rect_t rc = m_viewRect;
    for (int i = 0; i < vertexCount; i++) {
        if (rectTestPoint(rc, tvec2(verts[i])))
            return true;
    }

    return false;
}

void PhysDebugDraw::DrawPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color)
{
    if (!intersectVerts(vertices, vertexCount))
        return;

    NVGcontext* vg = m_nvg;
    nvgBeginPath(vg);
    nvgStrokeWidth(vg, m_strokeScale);

    nvgMoveTo(vg, vertices[0].x, vertices[0].y);
    for (int i = 1; i < vertexCount; i++)
        nvgLineTo(vg, vertices[i].x, vertices[i].y);
    nvgClosePath(vg);

    nvgStrokeColor(vg, nvgRGBf(color.r, color.g, color.b));
    nvgStroke(vg);
}

void PhysDebugDraw::DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color)
{
    if (!intersectVerts(vertices, vertexCount))
        return;

    NVGcontext* vg = m_nvg;
    nvgBeginPath(vg);
    nvgStrokeWidth(vg, m_strokeScale);

    nvgMoveTo(vg, vertices[0].x, vertices[0].y);
    for (int i = 1; i < vertexCount; i++)
        nvgLineTo(vg, vertices[i].x, vertices[i].y);
	nvgClosePath(vg);

    nvgFillColor(vg, nvgRGBf(color.r, color.g, color.b));
    nvgFill(vg);
}

void PhysDebugDraw::DrawCircle(const b2Vec2& center, float32 radius, const b2Color& color)
{
    if (!rectTestCircle(m_viewRect, tvec2(center), radius))
        return;

    NVGcontext* vg = m_nvg;
    nvgBeginPath(vg);
    nvgStrokeWidth(vg, m_strokeScale);
    nvgCircle(vg, center.x, center.y, radius);
    nvgStrokeColor(vg, nvgRGBf(color.r, color.g, color.b));
    nvgStroke(vg);
}

void PhysDebugDraw::DrawSolidCircle(const b2Vec2& center, float32 radius, const b2Vec2& axis, const b2Color& color)
{
    if (!rectTestCircle(m_viewRect, tvec2(center), radius))
        return;

    NVGcontext* vg = m_nvg;
    nvgBeginPath(vg);
    nvgStrokeWidth(vg, m_strokeScale);
    nvgCircle(vg, center.x, center.y, radius);
    nvgFillColor(vg, nvgRGBf(color.r, color.g, color.b));
    nvgFill(vg);
}

void PhysDebugDraw::DrawParticles(const b2Vec2 *centers, float32 radius, const b2ParticleColor *colors, int32 count)
{
    // Make a rect from particles
    rect_t particlesRect;
    for (int i = 0; i < count; i++) {
        rectPushPoint(&particlesRect, tvec2(centers[i]));
    }
    if (!rectTestRect(m_viewRect, particlesRect))
        return;

    NVGcontext* vg = m_nvg;
    nvgBeginPath(vg);
    nvgStrokeWidth(vg, m_strokeScale);
    nvgStrokeColor(vg, nvgRGB(255, 255, 255));
    for (int i = 0; i < count; i++)
        nvgCircle(vg, centers[i].x, centers[i].y, radius);
    nvgStroke(vg);
}

void PhysDebugDraw::DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color)
{
    if (!rectTestPoint(m_viewRect, tvec2(p1)) && !rectTestPoint(m_viewRect, tvec2(p2)))
        return;

    NVGcontext* vg = m_nvg;
    nvgBeginPath(vg);
    nvgStrokeWidth(vg, m_strokeScale);
    nvgMoveTo(vg, p1.x, p1.y);
    nvgLineTo(vg, p2.x, p2.y);
    nvgStrokeColor(vg, nvgRGBf(color.r, color.g, color.b));
    nvgStroke(vg);
}

void PhysDebugDraw::DrawTransform(const b2Transform& xf)
{
    if (!rectTestPoint(m_viewRect, tvec2(xf.p)))
        return;

    NVGcontext* vg = m_nvg;
    nvgBeginPath(vg);
    nvgStrokeWidth(vg, m_strokeScale);

    b2Vec2 p = b2Mul(xf.q, xf.p + b2Vec2(1.0f, 0));

    nvgCircle(vg, xf.p.x, xf.p.y, 0.5f);
    nvgMoveTo(vg, xf.p.x, xf.p.y);
    nvgLineTo(vg, p.x, p.y);

    nvgStrokeColor(vg, nvgRGB(255, 0, 0));
    nvgStroke(vg);
}

// Plugin functions
PluginDesc* getBox2dDriverDesc()
{
    static PluginDesc desc;
    strcpy(desc.name, "Box2D");
    strcpy(desc.description, "Box2D Physics Driver");
    desc.type = PluginType::Physics2dDriver;
    desc.version = T_MAKE_VERSION(1, 0);
    return &desc;
}

void* initBox2dDriver(bx::AllocatorI* alloc, GetApiFunc getApi)
{
    static PhysDriver2DApi api;
    memset(&api, 0x00, sizeof(api));

    g_coreApi = (CoreApi_v0*)getApi(ApiId::Core, 0);
    g_gfxApi = (GfxApi_v0*)getApi(ApiId::Gfx, 0);
    g_camApi = (CameraApi_v0*)getApi(ApiId::Camera, 0);

    api.init = initBox2d;
    api.shutdown = shutdownBox2d;
    api.createScene = createSceneBox2d;
    api.destroyScene = destroySceneBox2d;
    api.createBody = createBodyBox2d;
    api.destroyBody = destroyBodyBox2d;
    api.createBoxShape = createBoxShapeBox2d;
    api.createPolyShape = createPolyShapeBox2d;
    api.createArbitaryBoxShape = createArbitaryBoxShapeBox2d;
    api.createCircleShape = createCircleShapeBox2d;
    api.stepScene = stepSceneBox2d;
    api.debugScene = debugSceneBox2d;

    static_assert(b2_maxManifoldPoints >= 2, "Manifold points mistmatch");

    return &api;
}

void shutdownBox2dDriver()
{
}

#ifdef termite_SHARED_LIB
T_PLUGIN_EXPORT void* termiteGetPluginApi(uint16_t apiId, uint32_t version)
{
    static PluginApi_v0 v0;

    if (version == 0) {
        v0.init = initBox2dDriver;
        v0.shutdown = shutdownBox2dDriver;
        v0.getDesc = getBox2dDriverDesc;
        return &v0;
    } else {
        return nullptr;
    }
}
#endif
