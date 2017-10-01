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

    void beginDraw(NVGcontext* nvg, const Camera2D& cam, const recti_t& viewport);
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
    //void SayGoodbye(b2ParticleGroup* group) override;
    void SayGoodbye(b2ParticleSystem* particleSystem, int32 index) override;
};

namespace termite
{
    struct PhysScene2D
    {
        b2World w;
        ContactListenerBox2d contactListener;
        ContactFilterBox2d contactFilter;
        DestructionListenerBox2d destructionListener;
        PhysDebugDraw debugDraw;
        float timestep;
        float accumulator;

        PhysScene2D(const PhysSceneDef2D& worldDef) :
            w(b2vec2(worldDef.gravity)),
            timestep(worldDef.timestep),
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
            beginContactFn(nullptr),
            endContactFn(nullptr),
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
            destroyFn(nullptr),
            shapeContactFilterFn(nullptr),
            particleContactFilterFn(nullptr),
            shapeBeginContactFn(nullptr),
            particleBeginContactFn(nullptr),
            shapeEndContactFn(nullptr),
            particleEndContactFn(nullptr)
        {}
    };

    /*
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
    */
} // namespace termite



struct Box2dDriver
{
    bx::AllocatorI* alloc;
    bx::Pool<PhysScene2D> scenePool;
    bx::Pool<PhysBody2D> bodyPool;
    bx::Pool<PhysShape2D> shapePool;
    bx::Pool<PhysJoint2D> jointPool;
    bx::Pool<PhysParticleEmitter2D> emitterPool;
    bx::HashTable<PhysParticleEmitter2D*, uintptr_t> emitterTable;   // key=pointer-to-b2ParticleSystem -> ParticleEmitter
    NVGcontext* nvg;
    uint8_t debugViewId;
    PhysFlags2D::Bits initFlags;

    Box2dDriver() :
        emitterTable(bx::HashTableType::Mutable),
        nvg(nullptr)
    {
        debugViewId = 0;
        initFlags = 0;
    }
};

static Box2dDriver g_box2d;
static CoreApi_v0* g_coreApi = nullptr;
static GfxApi_v0* g_gfxApi = nullptr;
static CameraApi_v0* g_camApi = nullptr;

static bool initBox2dGraphicsObjects()
{
    if ((g_box2d.initFlags & PhysFlags2D::EnableDebug) && g_coreApi->getGfxDriver()) {
        g_box2d.nvg = nvgCreate(0, g_box2d.debugViewId, g_coreApi->getGfxDriver(), g_gfxApi, g_box2d.alloc);
        if (!g_box2d.nvg)
            return false;
    }

    return true;
}

static void shutdownBox2dGraphicsObjects()
{
    if (g_box2d.nvg) {
        nvgDelete(g_box2d.nvg);
        g_box2d.nvg = nullptr;
    }
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


static result_t initBox2d(bx::AllocatorI* alloc, PhysFlags2D::Bits flags, uint8_t debugViewId)
{
    if (!g_box2d.scenePool.create(6, alloc) ||
        !g_box2d.bodyPool.create(200, alloc) ||
        !g_box2d.shapePool.create(200, alloc) ||
        !g_box2d.jointPool.create(100, alloc) ||
        !g_box2d.emitterPool.create(30, alloc))
    {
        return T_ERR_OUTOFMEM;
    }

    if (!g_box2d.emitterTable.create(30, alloc))
        return T_ERR_OUTOFMEM;

    if ((flags & PhysFlags2D::EnableDebug) && g_coreApi->getGfxDriver()) {
        g_box2d.nvg = nvgCreate(0, debugViewId, g_coreApi->getGfxDriver(), g_gfxApi, alloc);
        if (!g_box2d.nvg) {
            BX_WARN_API(g_coreApi, "Initializing NanoVg for Debugging Physics failed");
        }
        debugViewId = debugViewId;
    }

    g_box2d.alloc = alloc;
    g_box2d.initFlags = flags;

    b2SetAllocFreeCallbacks(b2AllocCallback, b2FreeCallback, nullptr);

    return 0;
}

static void shutdownBox2d()
{
    shutdownBox2dGraphicsObjects();

    g_box2d.emitterTable.destroy();
//    g_box2d.pgroupPool.destroy();
    g_box2d.emitterPool.destroy();
    g_box2d.jointPool.destroy();
    g_box2d.shapePool.destroy();
    g_box2d.bodyPool.destroy();
    g_box2d.scenePool.destroy();
    g_box2d.alloc = nullptr;
}

static PhysScene2D* createSceneBox2d(const PhysSceneDef2D& worldDef)
{
    PhysScene2D* scene = g_box2d.scenePool.newInstance<const PhysSceneDef2D&>(worldDef);
    if (!scene)
        return nullptr;
    scene->w.SetContactFilter(&scene->contactFilter);
    scene->w.SetContactListener(&scene->contactListener);
    scene->w.SetDestructionListener(&scene->destructionListener);
    return scene;
}

static void destroySceneBox2d(PhysScene2D* scene)
{
    g_box2d.scenePool.deleteInstance(scene);
}

static void stepSceneBox2d(PhysScene2D* scene, float dt)
{
    scene->w.Step(dt, 8, 3, 2);
}

static void debugSceneBox2d(PhysScene2D* scene, const recti_t viewport, const Camera2D& cam,
                            PhysDebugFlags2D::Bits flags)
 {
    assert(scene);

    if (g_box2d.nvg) {
        scene->debugDraw.SetFlags(flags);
        scene->w.SetDebugDraw(&scene->debugDraw);
        scene->debugDraw.beginDraw(g_box2d.nvg, cam, viewport);
        scene->w.DrawDebugData();
        scene->debugDraw.endDraw();
    }
}

static PhysBody2D* createBodyBox2d(PhysScene2D* scene, const PhysBodyDef2D& bodyDef)
{
    static_assert(PhysBodyType2D::Static == b2_staticBody, "BodyType mismatch");
    static_assert(PhysBodyType2D::Dynamic == b2_dynamicBody, "BodyType mismatch");
    static_assert(PhysBodyType2D::Kinematic == b2_kinematicBody, "BodyType mismatch");

    PhysBody2D* pbody = g_box2d.bodyPool.newInstance<>();
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
    bdef.angularVelocity = -bodyDef.angularVel;
    bdef.bullet = (bodyDef.flags & PhysBodyFlags2D::IsBullet) ? true : false;
    bdef.fixedRotation = (bodyDef.flags & PhysBodyFlags2D::FixedRotation) ? true : false;
    bdef.gravityScale = bodyDef.gravityScale;
    bdef.linearDamping = bodyDef.linearDamping;
    bdef.position = b2vec2(bodyDef.position);
    bdef.userData = pbody;

    b2Body* bbody = scene->w.CreateBody(&bdef);
    if (!bbody) {
        g_box2d.bodyPool.deleteInstance(pbody);
        return nullptr;
    }

    pbody->b = bbody;
    pbody->userData = bodyDef.userData;
    return pbody;
}

static void destroyBodyBox2d(PhysBody2D* body)
{
    assert(body);
    assert(body->b);
    
    body->ownerScene->w.DestroyBody(body->b);
    body->b = nullptr;
    g_box2d.bodyPool.deleteInstance(body);
}

static PhysShape2D* createBoxShapeBox2d(PhysBody2D* body, const vec2_t& halfSize, const PhysShapeDef2D& shapeDef)
{
    PhysShape2D* shape = g_box2d.shapePool.newInstance<>();
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
    fdef.isSensor = (shapeDef.flags & PhysShapeFlags2D::IsSensor) ? true : false;
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
    PhysShape2D* shape = g_box2d.shapePool.newInstance<>();
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
    fdef.isSensor = (shapeDef.flags & PhysShapeFlags2D::IsSensor) ? true : false;
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
    PhysShape2D* shape = g_box2d.shapePool.newInstance<>();
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
    fdef.isSensor = (shapeDef.flags & PhysShapeFlags2D::IsSensor) ? true : false;
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
    PhysShape2D* shape = g_box2d.shapePool.newInstance<>();
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
    fdef.isSensor = (shapeDef.flags & PhysShapeFlags2D::IsSensor) ? true : false;
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

static PhysDistanceJoint2D* createDistanceJointBox2d(PhysScene2D* scene, PhysBody2D* bodyA, PhysBody2D* bodyB,
                                                     const vec2_t& anchorA, const vec2_t& anchorB,
                                                     float length, float frequencyHz/* = 0*/, float dampingRatio/* = 0.7f*/,
                                                     bool collide/* = false*/, void* userData/* = nullptr*/)
{
    PhysJoint2D* joint = g_box2d.jointPool.newInstance<>();

    b2DistanceJointDef def;
    def.bodyA = bodyA->b;
    def.bodyB = bodyB->b;
    def.localAnchorA = b2vec2(anchorA);
    def.localAnchorB = b2vec2(anchorB);
    def.length = length;
    def.frequencyHz = frequencyHz;
    def.dampingRatio = dampingRatio;
    def.collideConnected = collide;
    def.userData = joint;

    joint->j = scene->w.CreateJoint(&def);
    if (!joint->j) {
        g_box2d.jointPool.deleteInstance(joint);
        return nullptr;
    }
    joint->userData = userData;

    return (PhysDistanceJoint2D*)joint;
}

static PhysWeldJoint2D* createWeldJointBox2d(PhysScene2D* scene, PhysBody2D* bodyA, PhysBody2D* bodyB, const vec2_t& worldPt,
                                        float dampingRatio, float frequencyHz, void* userData)
{
    PhysJoint2D* joint = g_box2d.jointPool.newInstance<>();
    b2WeldJointDef def;
    def.Initialize(bodyA->b, bodyB->b, b2vec2(worldPt));
    def.dampingRatio = dampingRatio;
    def.frequencyHz = frequencyHz;    
    def.collideConnected = false;
    def.userData = joint;
    
    joint->j = scene->w.CreateJoint(&def);
    if (!joint->j) {
        g_box2d.jointPool.deleteInstance(joint);
        return nullptr;
    }
    joint->userData = userData;

    return (PhysWeldJoint2D*)joint;
}


PhysWeldJoint2D* createWeldJoint2PtsBox2d(PhysScene2D* scene, PhysBody2D* bodyA, PhysBody2D* bodyB,
                                          const vec2_t& anchorA, const vec2_t& anchorB,
                                          float dampingRatio/* = 0*/, float frequencyHz/* = 0*/, void* userData/* = nullptr*/)
{
    PhysJoint2D* joint = g_box2d.jointPool.newInstance<>();
    b2WeldJointDef def;
    def.bodyA = bodyA->b;
    def.bodyB = bodyB->b;
    def.localAnchorA = b2vec2(anchorA);
    def.localAnchorB = b2vec2(anchorB);
    def.referenceAngle = bodyB->b->GetAngle() - bodyA->b->GetAngle();
    def.dampingRatio = dampingRatio;
    def.frequencyHz = frequencyHz;
    def.collideConnected = false;
    def.userData = joint;

    joint->j = scene->w.CreateJoint(&def);
    if (!joint->j) {
        g_box2d.jointPool.deleteInstance(joint);
        return nullptr;
    }
    joint->userData = userData;

    return (PhysWeldJoint2D*)joint;
}

static PhysMouseJoint2D* createMouseJointBox2d(PhysScene2D* scene, PhysBody2D* body, const vec2_t& target,
                                               float maxForce/* = 0*/, float frequencyHz/* = 5.0f*/, float dampingRatio/* = 0.7f*/,
                                               bool collide/* = false*/, void* userData/* = nullptr*/)
{
    PhysJoint2D* joint = g_box2d.jointPool.newInstance<>();
    b2MouseJointDef def;
    def.userData = joint;

    joint->j = scene->w.CreateJoint(&def);
    if (!joint->j) {
        g_box2d.jointPool.deleteInstance(joint);
        return nullptr;
    }
    joint->userData = userData;

    return (PhysMouseJoint2D*)joint;
}



void ContactListenerBox2d::BeginContact(b2Contact* contact)
{
    const b2Fixture* fixtureA = contact->GetFixtureA();
    const b2Fixture* fixtureB = contact->GetFixtureB();
    PhysShape2D* shapeA = (PhysShape2D*)fixtureA->GetUserData();
    PhysShape2D* shapeB = (PhysShape2D*)fixtureB->GetUserData();

    bool enabled = true;
    if (shapeA->beginContactFn) {
        if (!shapeA->beginContactReportInfo) {
            enabled = shapeA->beginContactFn(shapeA, shapeB, nullptr);
        } else {
            b2WorldManifold manifold;
            PhysContactInfo2D cinfo;
            contact->GetWorldManifold(&manifold);
            cinfo.normal = tvec2(manifold.normal);
            cinfo.points[0] = tvec2(manifold.points[0]);
            cinfo.points[1] = tvec2(manifold.points[1]);
            cinfo.separations[0] = manifold.separations[0];
            cinfo.separations[1] = manifold.separations[1];
            enabled = shapeA->beginContactFn(shapeA, shapeB, &cinfo);
        }
    }

    if (shapeB->beginContactFn) {
        if (!shapeB->beginContactReportInfo) {
            enabled = shapeB->beginContactFn(shapeB, shapeA, nullptr);
        } else {
            b2WorldManifold manifold;
            PhysContactInfo2D cinfo;
            contact->GetWorldManifold(&manifold);
            cinfo.normal = tvec2(manifold.normal);
            cinfo.points[0] = tvec2(manifold.points[0]);
            cinfo.points[1] = tvec2(manifold.points[1]);
            cinfo.separations[0] = manifold.separations[0];
            cinfo.separations[1] = manifold.separations[1];
            enabled = shapeB->beginContactFn(shapeB, shapeA, &cinfo);
        }
    }

    contact->SetEnabled(enabled);
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

    const b2Filter& filterA = fixtureA->GetFilterData();
    const b2Filter& filterB = fixtureB->GetFilterData();

    bool collide =
        (filterA.maskBits & filterB.categoryBits) != 0 &&
        (filterA.categoryBits & filterB.maskBits) != 0;
    return collide;
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

void DestructionListenerBox2d::SayGoodbye(b2ParticleSystem* particleSystem, int32 index)
{
    int r = g_box2d.emitterTable.find(uintptr_t(particleSystem));
    if (r != -1) {
        PhysParticleEmitter2D* emitter = g_box2d.emitterTable.getValue(r);
        if (emitter->destroyFn)
            emitter->destroyFn(emitter, index);
    }
}

static void box2dRayCast(PhysScene2D* scene, const vec2_t& p1, const vec2_t& p2, PhysRayCastCallback2D callback, void* userData)
{
    class RayCastCallback : public b2RayCastCallback
    {
    private:
        PhysRayCastCallback2D m_callback;
        void* m_userData;

    public:
        RayCastCallback(PhysRayCastCallback2D _callback, void* _userData) :
            m_callback(_callback),
            m_userData(_userData)
        {
        }

        float32 ReportFixture(b2Fixture* fixture, const b2Vec2& point, const b2Vec2& normal, float32 fraction) override
        {
            return m_callback((PhysShape2D*)fixture->GetUserData(), tvec2(point), tvec2(normal), fraction, m_userData);
        }
    };

    RayCastCallback b2callback(callback, userData);
    scene->w.RayCast(&b2callback, b2vec2(p1), b2vec2(p2));
}

static void box2dQueryShapeCircle(PhysScene2D* scene, float radius, const vec2_t pos, PhysQueryShapeCallback2D callback, void *userData)
{
    class QueryCircleCallback : public b2QueryCallback
    {
    private:
        PhysQueryShapeCallback2D m_callback;
        void* m_userData;

    public:
        QueryCircleCallback(PhysQueryShapeCallback2D _callback, void* _userData) :
            m_callback(_callback),
            m_userData(_userData)
        {
        }

        bool ReportFixture(b2Fixture* fixture) override
        {
            return m_callback((PhysShape2D*)fixture->GetUserData(), m_userData);
        }
    };

    QueryCircleCallback b2callback(callback, userData);
    b2AABB aabb;
    aabb.lowerBound = b2vec2(pos - vec2f(radius, radius));
    aabb.upperBound = b2vec2(pos + vec2f(radius, radius));

    scene->w.QueryAABB(&b2callback, aabb);
}

static void box2dQueryShapeBox(PhysScene2D* scene, const vec2_t pos, vec2_t halfSize, PhysQueryShapeCallback2D callback, void *userData)
{
    class QueryCircleCallback : public b2QueryCallback
    {
    private:
        PhysQueryShapeCallback2D m_callback;
        void* m_userData;

    public:
        QueryCircleCallback(PhysQueryShapeCallback2D _callback, void* _userData) :
            m_callback(_callback),
            m_userData(_userData)
        {
        }

        bool ReportFixture(b2Fixture* fixture) override
        {
            return m_callback((PhysShape2D*)fixture->GetUserData(), m_userData);
        }
    };

    QueryCircleCallback b2callback(callback, userData);
    b2AABB aabb;
    aabb.lowerBound = b2vec2(pos - halfSize);
    aabb.upperBound = b2vec2(pos + halfSize);

    scene->w.QueryAABB(&b2callback, aabb);
}

static PhysParticleEmitter2D* box2dCreateParticleEmitter(PhysScene2D* scene, const PhysParticleEmitterDef2D& def)
{
    b2ParticleSystemDef b2def;
    b2def.strictContactCheck = (def.flags & PhysEmitterFlags2D::StrictContactCheck) ? true : false;
    b2def.density = def.density;
    b2def.gravityScale = def.gravityScale;
    b2def.radius = def.radius;
    b2def.maxCount = def.maxCount;

    b2def.pressureStrength = 0.05f;
    b2def.dampingStrength = 1.0f;
    b2def.elasticStrength = 0.25f;
    b2def.springStrength = 0.25f;
    b2def.viscousStrength = 0.25f;
    b2def.surfaceTensionPressureStrength = 0.2f;
    b2def.surfaceTensionNormalStrength = 0.2f;
    b2def.repulsiveStrength = 1.0f;
    b2def.powderStrength = 0.5f;
    b2def.ejectionStrength = 0.5f;
    b2def.staticPressureStrength = 0.2f;
    b2def.staticPressureRelaxation = 0.2f;
    b2def.staticPressureIterations = 8;
    b2def.colorMixingStrength = 0.5f;
    b2def.destroyByAge = (def.flags & PhysEmitterFlags2D::DestroyByAge) ? true : false;
    b2def.lifetimeGranularity = 1.0f / 60.0f;

    PhysParticleEmitter2D* emitter = g_box2d.emitterPool.newInstance<>();
    emitter->p = scene->w.CreateParticleSystem(&b2def);
    if (!emitter->p) {
        g_box2d.emitterPool.deleteInstance(emitter);
        return nullptr;
    }

    emitter->userData = def.userData;
    g_box2d.emitterTable.add(uintptr_t(emitter->p), emitter);
    return emitter;    
}

static void box2dDestroyParticleEmitter(PhysScene2D* scene, PhysParticleEmitter2D* emitter)
{
    assert(emitter);

    scene->w.DestroyParticleSystem(emitter->p);
    
    int r = g_box2d.emitterTable.find(uintptr_t(emitter->p));
    if (r != -1)
        g_box2d.emitterTable.remove(r);
    g_box2d.emitterPool.deleteInstance(emitter);
}

static void* box2dGetParticleEmitterUserData(PhysParticleEmitter2D* emitter)
{
    assert(emitter);
    return emitter->userData;
}

static int box2dCreateParticle(PhysParticleEmitter2D* emitter, const PhysParticleDef2D& particleDef)
{
    assert(emitter);
    b2ParticleDef b2def;
    b2def.flags = particleDef.flags;
    b2def.position = b2vec2(particleDef.position);
    b2def.velocity = b2vec2(particleDef.velocity);
    b2def.group = (b2ParticleGroup*)particleDef.group;
    b2def.lifetime = particleDef.lifetime;
    b2def.userData = particleDef.userData;
    b2def.color = b2ParticleColor(particleDef.color.r, particleDef.color.g, particleDef.color.b, particleDef.color.a);
    return emitter->p->CreateParticle(b2def);
}

static void box2dDestroyParticle(PhysParticleEmitter2D* emitter, int index, bool callDestructionCallback)
{
    assert(emitter);
    emitter->p->DestroyParticle(index, callDestructionCallback);
}

static int box2dGetParticleCount(PhysParticleEmitter2D* emitter)
{
    assert(emitter);
    return emitter->p->GetParticleCount();
}

static void box2dSetMaxParticleCount(PhysParticleEmitter2D* emitter, int maxCount)
{
    assert(emitter);
    emitter->p->SetMaxParticleCount(maxCount);
}

static int box2dGetMaxParticleCount(PhysParticleEmitter2D* emitter)
{
    assert(emitter);
    return emitter->p->GetMaxParticleCount();
}

static void box2dApplyParticleForceBatch(PhysParticleEmitter2D* emitter, int firstIdx, int lastIdx, const vec2_t& force)
{
    assert(emitter);
    emitter->p->ApplyForce(firstIdx, lastIdx, b2vec2(force));
}

static void box2dApplyParticleImpulseBatch(PhysParticleEmitter2D* emitter, int firstIdx, int lastIdx, const vec2_t& impulse)
{
    assert(emitter);
    emitter->p->ApplyLinearImpulse(firstIdx, lastIdx, b2vec2(impulse));
}

static void box2dApplyParticleForce(PhysParticleEmitter2D* emitter, int index, const vec2_t& force)
{
    assert(emitter);
    emitter->p->ParticleApplyForce(index, b2vec2(force));
}

static void box2dApplyParticleImpulse(PhysParticleEmitter2D* emitter, int index, const vec2_t& impulse)
{
    assert(emitter);
    emitter->p->ParticleApplyLinearImpulse(index, b2vec2(impulse));
}

static int box2dGetEmitterPositionBuffer(PhysParticleEmitter2D* emitter, vec2_t* poss, int maxItems)
{
    assert(emitter);
    int count = std::min<int>(emitter->p->GetParticleCount(), maxItems);
    memcpy(poss, emitter->p->GetPositionBuffer(), count*sizeof(vec2_t));
    return count;
}

static int box2dGetEmitterVelocityBuffer(PhysParticleEmitter2D* emitter, vec2_t* vels, int maxItems)
{
    assert(emitter);
    int count = std::min<int>(emitter->p->GetParticleCount(), maxItems);
    memcpy(vels, emitter->p->GetVelocityBuffer(), count*sizeof(vec2_t));
    return count;
}

static int box2dGetEmitterColorBuffer(PhysParticleEmitter2D* emitter, color_t* colors, int maxItems)
{
    assert(emitter);
    int count = std::min<int>(emitter->p->GetParticleCount(), maxItems);
    memcpy(colors, emitter->p->GetColorBuffer(), count*sizeof(color_t));
    return count;
}

static PhysParticleGroup2D* box2dCreateParticleGroupCircleShape(PhysParticleEmitter2D* emitter, 
                                                                const PhysParticleGroupDef2D& groupDef, float radius)
{
    b2ParticleGroupDef b2def;
    b2def.groupFlags = groupDef.flags;
    b2def.flags = groupDef.particleFlags;
    b2def.angle = groupDef.angle;
    b2def.angularVelocity = groupDef.angularVelocity;
    b2def.linearVelocity = b2vec2(groupDef.linearVelocity);
    b2def.color = b2ParticleColor(groupDef.color.r, groupDef.color.g, groupDef.color.b, groupDef.color.a);
    b2def.position = b2vec2(groupDef.position);
    b2def.strength = groupDef.strength;
    b2def.lifetime = groupDef.lifetime;
    b2def.userData = groupDef.userData;

    b2CircleShape shape;
    shape.m_radius = radius;
    b2def.shapeCount = 1;
    b2def.shape = &shape;

    return (PhysParticleGroup2D*)emitter->p->CreateParticleGroup(b2def);
}

static void box2dApplyParticleGroupImpulse(PhysParticleGroup2D* group, const vec2_t& impulse)
{
    ((b2ParticleGroup*)group)->ApplyLinearImpulse(b2vec2(impulse));
}

static void box2dApplyParticleGroupForce(PhysParticleGroup2D* group, const vec2_t& force)
{
    ((b2ParticleGroup*)group)->ApplyForce(b2vec2(force));
}

static void box2dDestroyParticleGroupParticles(PhysParticleGroup2D* group, bool callDestructionCallback/* = false*/)
{
    ((b2ParticleGroup*)group)->DestroyParticles();
}

static void* box2dGetParticleGroupUserData(PhysParticleGroup2D* group)
{
    return ((b2ParticleGroup*)group)->GetUserData();
}

static void box2dSetParticleGroupFlags(PhysParticleGroup2D* group, uint32_t flags)
{
    ((b2ParticleGroup*)group)->SetGroupFlags(flags);
}

static uint32_t box2dGetParticleGroupFlags(PhysParticleGroup2D* group)
{
    return ((b2ParticleGroup*)group)->GetGroupFlags();
}

void PhysDebugDraw::beginDraw(NVGcontext* nvg, const Camera2D& cam, const recti_t& viewport)
{
    assert(nvg);
    m_nvg = nvg;
    int viewWidth = viewport.xmax - viewport.xmin;
    int viewHeight = viewport.ymax - viewport.ymin;
    nvgBeginFrame(nvg, viewport.xmin, viewport.ymin, viewWidth, viewHeight, 1.0f);

    // Adjust nvg to camera
    nvgTranslate(nvg, float(viewWidth)*0.5f, float(viewHeight)*0.5f);
    float scale = 1.0f;
    switch (cam.policy) {
    case DisplayPolicy::FitToHeight:
        scale = float(viewWidth) * cam.zoom;
        break;
    case DisplayPolicy::FitToWidth:
        scale = float(viewHeight) * cam.zoom;
        break;
    }
    
    nvgScale(nvg, scale, -scale);
    nvgTranslate(nvg, -cam.pos.x, -cam.pos.y);
    nvgGlobalAlpha(nvg, 0.8f);

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
    api.initGraphicsObjects = initBox2dGraphicsObjects;
    api.shutdownGraphicsObjects = shutdownBox2dGraphicsObjects;
    api.createScene = createSceneBox2d;
    api.destroyScene = destroySceneBox2d;
    api.getSceneTimeStep = [](PhysScene2D* scene)->float { return scene->timestep; };
    api.createBody = createBodyBox2d;
    api.destroyBody = destroyBodyBox2d;
    api.createBoxShape = createBoxShapeBox2d;
    api.createPolyShape = createPolyShapeBox2d;
    api.createArbitaryBoxShape = createArbitaryBoxShapeBox2d;
    api.createCircleShape = createCircleShapeBox2d;
    api.stepScene = stepSceneBox2d;
    api.debugScene = debugSceneBox2d;
    api.setTransform = [](PhysBody2D* body, const vec2_t& pos, float angle) {
        body->b->SetTransform(b2vec2(pos), -angle);
    };
    api.getTransform = [](PhysBody2D* body, vec2_t* pPos, float* pAngle) {
        *pPos = tvec2(body->b->GetPosition());
        *pAngle = -body->b->GetAngle();
    };
    api.getPosition = [](PhysBody2D* body)->vec2_t { return tvec2(body->b->GetPosition()); };
    api.getAngle = [](PhysBody2D* body)->float { return -body->b->GetAngle(); };
    api.getBodyUserData = [](PhysBody2D* body)->void* { return body->userData; };
    
    api.setLinearVelocity = [](PhysBody2D* body, const vec2_t& vel) { body->b->SetLinearVelocity(b2vec2(vel)); };
    api.setAngularVelocity = [](PhysBody2D* body, float omega) { body->b->SetAngularVelocity(-omega); };
    api.getLinearVelocity = [](PhysBody2D* body)->vec2_t { return tvec2(body->b->GetLinearVelocity()); };
    api.getAngularVelocity = [](PhysBody2D* body)->float { return -body->b->GetAngularVelocity(); };
    api.isAwake = [](PhysBody2D* body)->bool { return body->b->IsAwake(); };
    api.setAwake = [](PhysBody2D* body, bool awake) { body->b->SetAwake(awake); };
    api.isActive = [](PhysBody2D* body)->bool { return body->b->IsActive(); };
    api.setActive = [](PhysBody2D* body, bool active) { body->b->SetActive(active); };
    api.setGravityScale = [](PhysBody2D* body, float gravityScale) { body->b->SetGravityScale(gravityScale); };

    api.getWorldCenter = [](PhysBody2D* body)->vec2_t { return tvec2(body->b->GetWorldCenter()); };
    api.getWorldPoint = [](PhysBody2D* body, const vec2_t& localPt) { return tvec2(body->b->GetWorldPoint(b2vec2(localPt))); };
    api.getLocalPoint = [](PhysBody2D* body, const vec2_t& worldPt) { return tvec2(body->b->GetLocalPoint(b2vec2(worldPt))); };
    api.applyLinearImpulse = [](PhysBody2D* body, const vec2_t& impulse, const vec2_t& worldPt, bool wake) {
        body->b->ApplyLinearImpulse(b2vec2(impulse), b2vec2(worldPt), wake);
    };
    api.applyAngularImpulse = [](PhysBody2D* body, float impulse, bool wake) {
        body->b->ApplyAngularImpulse(-impulse, true);
    };
    api.applyForce = [](PhysBody2D* body, const vec2_t& force, const vec2_t& worldPt, bool wake) {
        body->b->ApplyForce(b2vec2(force), b2vec2(worldPt), wake);
    };
    api.applyTorque = [](PhysBody2D* body, float torque, bool wake) {
        body->b->ApplyTorque(-torque, wake);
    };

    api.setBeginShapeContactCallback = [](PhysShape2D* shape, PhysShapeContactCallback2D callback, bool reportContactInfo) {
        shape->beginContactFn = callback;
        shape->beginContactReportInfo = reportContactInfo;
    };
    api.setEndShapeContactCallback = [](PhysShape2D* shape, PhysShapeContactCallback2D callback) {
        shape->endContactFn = callback;
    };

    api.setShapeContactFilterCallback = [](PhysShape2D* shape, PhysShapeContactFilterCallback2D callback) {
        shape->contactFilterFn = callback;
    };

    api.getShapeUserData = [](PhysShape2D* shape) { return shape->userData; };
    api.getShapeBody = [](PhysShape2D* shape)->PhysBody2D* { return (PhysBody2D*)shape->fixture->GetBody()->GetUserData(); };
    api.getShapeAABB = [](PhysShape2D* shape)->rect_t {
        b2AABB aabb = shape->fixture->GetAABB(0);
        rect_t r;
        r.vmin = tvec2(aabb.lowerBound);
        r.vmax = tvec2(aabb.upperBound);
        return r;
    };
    api.setShapeContactFilterData = [](PhysShape2D* shape, uint16_t catBits, uint16_t maskBits, int16_t groupIndex) {
        b2Filter filter;
        filter.categoryBits = catBits;
        filter.maskBits = maskBits;
        filter.groupIndex = groupIndex;
        shape->fixture->SetFilterData(filter);
    };
    api.getShapeContactFilterData = [](PhysShape2D* shape, uint16_t* catBits, uint16_t* maskBits, int16_t* groupIndex) {
        const b2Filter& filter = shape->fixture->GetFilterData();
        *catBits = filter.categoryBits;
        *maskBits = filter.maskBits;
        *groupIndex = filter.groupIndex;
    };

    api.createDistanceJoint = createDistanceJointBox2d;
    api.createWeldJoint = createWeldJointBox2d;
    api.createWeldJoint2Pts = createWeldJoint2PtsBox2d;
    api.destroyWeldJoint = [](PhysScene2D* scene, PhysWeldJoint2D* _joint) {
        PhysJoint2D* joint = (PhysJoint2D*)_joint;
        assert(joint->j);
        scene->w.DestroyJoint(joint->j);
    };

    api.rayCast = box2dRayCast;
    api.queryShapeCircle = box2dQueryShapeCircle;
    api.queryShapeBox = box2dQueryShapeBox;
    api.getMassCenter = [](PhysBody2D* body)->vec2_t {
        b2MassData massData;
        body->b->GetMassData(&massData);
        return tvec2(massData.center);
    };
    api.setMassCenter = [](PhysBody2D* body, const vec2_t& center) {
        b2MassData massData;
        body->b->GetMassData(&massData);
        massData.center = b2vec2(center);
        body->b->SetMassData(&massData);
    };
    api.getMass = [](PhysBody2D* body)->float {
        b2MassData massData;
        body->b->GetMassData(&massData);
        return massData.mass;
    };
    api.getInertia = [](PhysBody2D* body)->float {
        return body->b->GetInertia();
    };

    api.createParticleEmitter = box2dCreateParticleEmitter;
    api.destroyParticleEmitter = box2dDestroyParticleEmitter;
    api.createParticle = box2dCreateParticle;
    api.destroyParticle = box2dDestroyParticle;
    api.getParticleEmitterUserData = box2dGetParticleEmitterUserData;
    api.getParticleCount = box2dGetParticleCount;
    api.setMaxParticleCount = box2dSetMaxParticleCount;
    api.getMaxParticleCount = box2dGetMaxParticleCount;
    api.applyParticleForceBatch = box2dApplyParticleForceBatch;
    api.applyParticleImpulseBatch = box2dApplyParticleImpulseBatch;
    api.applyParticleForce = box2dApplyParticleForce;
    api.applyParticleImpulse = box2dApplyParticleImpulse;
    api.getEmitterColorBuffer = box2dGetEmitterColorBuffer;
    api.getEmitterPositionBuffer = box2dGetEmitterPositionBuffer;
    api.getEmitterVelocityBuffer = box2dGetEmitterVelocityBuffer;
    api.setParticleShapeContactFilterCallback = [](PhysParticleEmitter2D* emitter, PhysParticleShapeContactFilterCallback2D callback) {
        emitter->shapeContactFilterFn = callback;
    };
    api.createParticleGroupCircleShape = box2dCreateParticleGroupCircleShape;
    api.destroyParticleGroupParticles = box2dDestroyParticleGroupParticles;
    api.getParticleGroupFlags = box2dGetParticleGroupFlags;
    api.getParticleGroupUserData = box2dGetParticleGroupUserData;
    api.applyParticleGroupForce = box2dApplyParticleGroupForce;
    api.applyParticleGroupImpulse = box2dApplyParticleGroupImpulse;

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
