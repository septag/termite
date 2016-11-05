#pragma once

#include "bx/bx.h"
#include "vec_math.h"

namespace termite
{   
    struct p2Scene;
    struct p2Body;
    struct p2Shape;
    struct p2Joint;
    struct p2FrictionJoint;
    struct p2RevoluteJoint;
    struct p2DistanceJoint;
    struct p2PrismaticJoint;
    struct p2PulleyJoint;
    struct p2WeldJoint;
    struct p2GearJoint;
    struct p2RopeJoint;
    struct p2MouseJoint;
    struct p2WheelJoint;
    struct p2ParticleEmitter;
    struct p2ParticleGroup;

    struct ContactInfo
    {
        vec2_t normal;          // in world-coords
        vec2_t points[2];       // world-coords
        float separations[2];   // A negative value indicates overlap, in meters
    };

    typedef void(*p2JointDestroyCallback)(p2Joint* joint);
    typedef void(*p2ShapeDestroyCallback)(p2Shape* shape);
    typedef void(*p2ParticleGroupDestroyCallback)(p2ParticleGroup* pgroup);
    typedef void(*p2ParticleDestroyCallback)(p2ParticleEmitter* emitter, int index);

    typedef bool(*p2ShapeContactFilterCallback)(p2Shape* shapeA, p2Shape* shapeB);
    typedef bool(*p2ShapeParticleContactFilterCallback)(p2Shape* shape, p2ParticleEmitter* emitter, int index);
    typedef bool(*p2ParticleContactFilterCallback)(p2ParticleEmitter* emitter, int indexA, int indexB);

    typedef void(*p2ShapeContactCallback)(p2Shape* shapeA, p2Shape* shapeB, const ContactInfo& contactInfo);
    typedef void(*p2ParticleShapeContactCallback)(p2ParticleEmitter* emitter, int index, p2Shape* shape, 
                                                  const vec2_t& normal, float weight);
    typedef void(*p2ParticleContactCallback)(p2ParticleEmitter* emitter, int indexA, int indexB, 
                                             const vec2_t& normal, float weight);

    TERMITE_API void p2SetJointDestroyCallback(p2Joint* joint, p2JointDestroyCallback callback);
    TERMITE_API void p2SetShapeDestroyCallback(p2Shape* shape, p2ShapeDestroyCallback callback);
    TERMITE_API void p2SetParticleGroupDestroyCallback(p2ParticleGroup* pgroup, p2ParticleGroupDestroyCallback callback);
    TERMITE_API void p2SetParticleDestroyCallback(p2ParticleEmitter* emitter, p2ParticleDestroyCallback callback);
    TERMITE_API void p2SetShapeContactFilterCallback(p2Shape* shape, p2ShapeContactFilterCallback callback);
    TERMITE_API void p2SetParticleContactFilterCallback(p2ParticleEmitter* emitter, p2ParticleContactFilterCallback callback);
    TERMITE_API void p2SetShapeContactCallback(p2Shape* shape, p2ShapeContactCallback callback);
    TERMITE_API void p2SetParticleShapeContactCallback(p2ParticleEmitter* emitter, p2ParticleShapeContactCallback callback);
    TERMITE_API void p2SetParticleContactCallback(p2ParticleEmitter* emitter, p2ParticleContactCallback callback);

    struct p2SceneDef
    {
        vec2_t gravity;
        float timestep;

        p2SceneDef(const vec2_t _gravity = vec2f(0, -9.8f), float _timestep = 1.0f/60.0f) :
            gravity(_gravity),
            timestep(_timestep)
        {
        }
    };

    TERMITE_API p2Scene* p2CreateScene(const p2SceneDef& worldDef);
    TERMITE_API void p2DestroyScene(p2Scene* scene);
    TERMITE_API void p2StepScene(float dt);

    struct p2BodyDef
    {
        enum Type
        {
            Static,
            Dynamic,
            Kinematic
        };

        enum Flags
        {
            AllowSleep = 0x1, /// unset this flag and the body should never fall asleep
            IsAwake = 0x2, /// Is this body initially awake or sleeping?
            FixedRotation = 0x4, /// Should this body be prevented from rotating? Useful for characters.
            IsBullet = 0x8, /// Is this a fast moving body that should be prevented from tunneling
            IsActive = 0x10 /// Does this body start out active?
        };

        Type type;
        vec2_t position;
        float angle;
        vec2_t linearVel; /// The linear velocity of the body's origin in world co-ordinates.
        float angularVel; /// The angular velocity of the body.
        float linearDamping; /// Linear damping is use to reduce the linear velocity. The damping parameter
                             /// can be larger than 1.0f
        float angularDamping;   /// Angular damping is use to reduce the angular velocity. The damping parameter
                                /// can be larger than 1.0
        uint32_t flags;     /// BodyFlags
        void* userData;
        float gravityScale;

        p2BodyDef(Type _type = Static, const vec2_t& _pos = vec2f(0, 0), float _angle = 0,
                  const vec2_t& _linearVel = vec2f(0, 0), float _angularVel = 0, 
                  float _linearDamping = 0, float _angularDamping = 0,
                  uint32_t _flags = AllowSleep | IsAwake | IsActive, void* _userData = nullptr, 
                  float _gravityScale = 1.0f) :
            type(_type),
            position(_pos),
            angle(_angle),
            linearVel(_linearVel),
            angularVel(_angularVel),
            linearDamping(_linearDamping),
            angularDamping(_angularDamping),
            flags(_flags),
            userData(_userData),
            gravityScale(_gravityScale)
        {
        }
    };

    struct p2ShapeDef
    {
        enum Flags
        {
            IsSensor = 0x1 /// A sensor shape collects contact information but never generates a collision response.
        };

        float friction; /// The friction coefficient, usually in the range [0,1].
        float restitution;  /// The restitution (elasticity) usually in the range [0,1].
        float density; /// The density, usually in kg/m^2.
        uint32_t flags;
        void* userData;
        uint16_t categoryBits;  /// The collision category bits. Normally you would just set one bit.
        uint16_t maskBits;  /// The collision mask bits. This states the categories that this
                            /// shape would accept for collision.
        int16_t groupIndex; /// Collision groups allow a certain group of objects to never collide (negative)
                            /// or always collide (positive). Zero means no collision group. Non-zero group
                            /// filtering always wins against the mask bits.

        p2ShapeDef(float _friction = 0.2f, float _restitution = 0, float _density = 0, uint32_t _flags = 0,
                   void* _userData = nullptr, uint16_t _catBits = 0x0001, uint16_t _maskBits = 0xffff, 
                   int16_t _groupIdx = 0) :
            friction(_friction),
            restitution(_restitution),
            density(_density),
            flags(_flags),
            userData(_userData),
            categoryBits(_catBits),
            maskBits(_maskBits),
            groupIndex(_groupIdx)
        {
        }    
    };

    TERMITE_API p2Body* p2CreateBody(p2Scene* scene, const p2BodyDef& bodyDef);
    TERMITE_API void p2DestroyBody(p2Body* body);

    TERMITE_API p2Shape* p2CreateBoxShape(p2Body* body, const vec2_t& halfSize, const p2ShapeDef& shapeDef);
    TERMITE_API p2Shape* p2CreatePolyShape(p2Body* body, const vec2_t* verts, int numVerts, const p2ShapeDef& shapeDef);
    TERMITE_API p2Shape* p2CreateCircleShape(p2Body* body, float radius, const p2ShapeDef& shapeDef);
    TERMITE_API void p2RegisterCallback();

    // Body
    TERMITE_API void p2SetTransform(p2Body* body, const vec2_t& pos, float angle);
    TERMITE_API vec2_t p2GetPosition(p2Body* body);
    TERMITE_API float p2GetAngle(p2Body* body);
    TERMITE_API vec2_t p2GetWorldCenter(p2Body* body);
    TERMITE_API vec2_t p2GetLocalCenter(p2Body* body);
    TERMITE_API vec2_t p2GetLocalPoint(p2Body* body, const vec2_t& worldPt);
    TERMITE_API vec2_t p2GetLocalVector(p2Body* body, const vec2_t& worldVec);
    TERMITE_API vec2_t p2GetWorldPoint(p2Body* body, const vec2_t& localPt);
    TERMITE_API void p2SetLinearVelocity(p2Body* body, const vec2_t& vel);
    TERMITE_API void p2SetAngularVelocity(p2Body* body, float vel);
    TERMITE_API vec2_t p2GetLinearVelocity(p2Body* body);
    TERMITE_API float p2GetAngularVelocity(p2Body* body);
    TERMITE_API void p2ApplyForce(p2Body* body, const vec2_t& force, const vec2_t& worldPt, bool wake = true);
    TERMITE_API void p2ApplyForceToCenter(p2Body* body, const vec2_t& force, bool wake = true);
    TERMITE_API void p2ApplyTorque(p2Body* body, float torque, bool wake = true);
    TERMITE_API void p2ApplyLinearImpulse(p2Body* body, const vec2_t& worldPt, bool wake = true);
    TERMITE_API void p2ApplyAngularImpulse(p2Body* body, float impulse, bool wake = true);
    TERMITE_API void p2SetActive(p2Body* body, bool active);
    TERMITE_API bool p2IsActive(p2Body* body);
    TERMITE_API void p2AddShape(p2Body* body, p2Shape* shape);
    TERMITE_API void* p2GetBodyUserData(p2Body* body);

    // Shape
    TERMITE_API void p2ComputeDistance(p2Shape* shape, const vec2_t& p, float* distance, vec2_t* normal);
    TERMITE_API void p2RayCast(p2Shape* shape, const vec2_t& startPt, const vec2_t& endPt, float k, vec2_t* pNorm, float* pK);
    TERMITE_API void* p2GetShapeUserData(p2Shape* shape);
    TERMITE_API void p2SetShapeContactFilterData(p2Shape* shape, uint16_t maskBits, int16_t groupIndex);
    TERMITE_API void p2GetShapeContactFilterData(p2Shape* shape, uint16_t* catBits, uint16_t* maskBits, int16_t* groupIndex);

    // Joints

    // Note: all initialization coordinates are in local coords of each body
    // Friction Joint: provides friction between two bodies
    TERMITE_API p2FrictionJoint* p2CreateFrictionJoint(p2Scene* scene, p2Body* bodyA, p2Body* bodyB, 
                                                       const vec2_t& anchorA, const vec2_t& anchorB, 
                                                       float maxForce = 0, float maxTorque = 0,
                                                       bool collide = false, void* userData = nullptr);
    TERMITE_API void p2DestroyFrictionJoint(p2FrictionJoint* joint);

    // Revolute Joint: Two bodies share a point that they rotate around
    TERMITE_API p2RevoluteJoint* p2CreateRevoluteJoint(p2Scene* scene, p2Body* bodyA, p2Body* bodyB, 
                                                       const vec2_t& anchorA, const vec2_t& anchorB,
                                                       float refAngle = 0, 
                                                       bool enableLimit = false, float lowerAngle = 0, float upperAngle = 0,
                                                       bool enableMotor = false, float motorSpeed = 0, float maxMotorTorque = 0,
                                                       bool collide = false, void* userData = nullptr);
    TERMITE_API void p2DestroyRevoluteJoint(p2RevoluteJoint* joint);

    // Distance Joint: Two bodies keeps a distance (like a rigid rod between bodies), can use to simulate springs too
    TERMITE_API p2DistanceJoint* p2CreateDistanceJoint(p2Scene* scene, p2Body* bodyA, p2Body* bodyB,
                                                       const vec2_t& anchorA, const vec2_t& anchorB,
                                                       float length, float frequencyHz = 0, float dampingRatio = 0.7f,
                                                       bool collide = false, void* userData = nullptr);
    TERMITE_API void p2SetDistanceJointLength(p2DistanceJoint* joint, float length);
    TERMITE_API void p2DestroyDistanceJoint(p2DistanceJoint* joint);

    // Prismatic Joint: Defines a line of motion using an axis and an anchor point (one degree of freedom)
    TERMITE_API p2PrismaticJoint* p2CreatePrismaticJoint(p2Scene* scene, p2Body* bodyA, p2Body* bodyB, 
                                                         const vec2_t& anchorA, const vec2_t& anchorB, const vec2_t& axisA,
                                                         bool enableLimit = false, float lowerTranslation = 0, float upperTranslation = 0, 
                                                         bool enableMotor = false, float maxMotorForce = 0, float motorSpeed = 0,
                                                         bool collide = false, void* userData = nullptr);
    TERMITE_API void p2DestroyPrismaticJoint(p2PrismaticJoint* joint);

    // Pully Joint: joint between two bodies and two ground points (world-static), length1 + ratio*length2 <= constant
    TERMITE_API p2PulleyJoint* p2CreatePulleyJoint(p2Scene* scene, p2Body* bodyA, p2Body* bodyB,
                                                   const vec2_t& groundWorldAnchorA, const vec2_t& groundWorldAnchorB,
                                                   const vec2_t& anchorA, const vec2_t& anchorB, float ratio = 1.0f,
                                                   bool collide = false, void* userData = nullptr);
    TERMITE_API void p2DestroyPulleyJoint(p2PulleyJoint* joint);

    // Weld Joint: Attaches/welds two bodies
    TERMITE_API p2WeldJoint* p2CreateWeldJoint(p2Scene* scene, p2Body* bodyA, p2Body* bodyB,
                                               const vec2_t& anchorA, const vec2_t& anchorB);
    TERMITE_API void p2DestroyWeldJoint(p2WeldJoint* joint);

    // Gear Joint: Connects two prismatic or revolute joints together. coordsA + ratio*coordsB = constant
    TERMITE_API p2GearJoint* p2CreateGearJoint(p2Scene* scene, p2Joint* jointA, p2Joint* jointB, float ratio = 1.0f,
                                               bool collide = false, void* userData = nullptr);
    TERMITE_API void p2DestroyGearJoint(p2GearJoint* joint);

    // Mouse Joint: Makes a body track a target in world coords
    TERMITE_API p2MouseJoint* p2CreateMouseJoint(p2Scene* scene, p2Body* body, const vec2_t& target,
                                                 float maxForce = 0, float frequencyHz = 5.0f, float dampingRatio = 0.7f,
                                                 bool collide = false, void* userData = nullptr);
    TERMITE_API void p2SetMouseTarget(p2MouseJoint* joint, const vec2_t& target);
    TERMITE_API vec2_t p2GetMouseTarget(p2MouseJoint* joint);
    TERMITE_API void p2DestroyMouseJoint(p2MouseJoint* joint);

    // Motor Joint: Controls relative motion between two bodies
    struct p2MotorJoint;
    TERMITE_API p2MotorJoint* p2CreateMotorJoint(p2Scene* scene, p2Body* bodyA, p2Body* bodyB,
                                   const vec2_t& linearOffset,  /// bodyB_position - bodyA_position, relative to bodyA
                                   float angularOffset, /// bodyB_Angle - bodyA_angle
                                   float maxForce = 1.0f, float maxTorque = 1.0f, float correctionFactor = 0.3f,
                                   bool collide = false, void* userData = nullptr);
    TERMITE_API void p2SetMotorLinearOffset(p2MotorJoint* joint, const vec2_t& linearOffset);
    TERMITE_API void p2SetMotorAngularOffset(p2MotorJoint* joint, const vec2_t& angularOffset);
    TERMITE_API void p2DestroyMotorJoint(p2MotorJoint* joint);

    // Rope Joint: Enforces maximum distance between two points in bodies
    TERMITE_API p2RopeJoint* p2CreateRopeJoint(p2Scene* scene, p2Body* bodyA, p2Body* bodyB,
                                               const vec2_t& anchorA, const vec2_t& anchorB,
                                               float maxLength,
                                               bool collide = false, void* userData = nullptr);
    TERMITE_API void p2DestroyRopeJoint(p2RopeJoint* joint);

    // Wheel Joint: provides two-degrees of freedom between two bodies. Translation along an axis fixed in bodyA
    //              and rotation in the plane, used for vehicle suspension. this is actually a combination of prismatic and revolute
    TERMITE_API p2WheelJoint* p2CreateWheelJoint(p2Scene* scene, p2Body* bodyA, p2Body* bodyB,
                                                 const vec2_t& anchorA, const vec2_t& anchorB, const vec2_t& axisA,
                                                 bool enableMotor = false, float maxMotorTorque = 0, float motorSpeed = 0,
                                                 float frequencyHz = 2.0f, float dampingRatio = 0.7f,
                                                 bool collide = false, void* userData = nullptr);
    TERMITE_API void p2DestroyWheelJoint(p2WheelJoint* joint);
    TERMITE_API void* p2GetJointUserData(p2Joint* joint);

    // Particle System
    struct p2ParticleEmmiterDef
    {
        enum Flags
        {
            StrictContactCheck = 0x1,   /// Enable strict Particle/Body contact check.
            DestroyByAge = 0x2 /// Whether to destroy particles by age when no more particles can be created
        };

        uint32_t flags;
        float density;
        float gravityScale; /// Change the particle gravity scale. Adjusts the effect of the global
        float radius;       /// Particles behave as circles with this radius. In Box2D units.
        int maxCount;       /// Set the maximum number of particles By default, there is no maximum. 
        float pressureStrength;  /// Increases pressure in response to compression Smaller values allow more compression
        float dampingStrength;  /// Reduces velocity along the collision normal,  Smaller value reduces less
        float elasticStrength;  /// Restores shape of elastic particle groups. Larger values increase elastic particle velocity
        float springStrength; /// Restores length of spring particle groups, Larger values increase spring particle velocity
        float viscousStrength;  /// Reduces relative velocity of viscous particles, Larger values slow down viscous particles more
        float repulsiveStrength; 	/// Produces additional pressure on repulsive particles, Larger values repulse more
                                   /// Negative values mean attraction. The range where particles behave, stably is about -0.2 to 2.0.
        float powderStrength; /// Produces repulsion between powder particles, Larger values repulse more
        float ejectionStrength; /// Pushes particles out of solid particle group, Larger values repulse more
        float surfaceTensionPressureStrength; /// Produces pressure on tensile particles, 0~0.2. Larger values increase the amount of surface tension.
        float surfaceTensionNormalStrength;  	/// Smoothes outline of tensile particles, 0~0.2. Larger values result in rounder, smoother, water-drop-like
                                                /// clusters of particles.

        float staticPressureStrength; /// Produces static pressure, Larger values increase the pressure on neighboring partilces
        float staticPressureRelaxation;  /// Reduces instability in static pressure calculation, Larger values make stabilize static pressure with fewer iterations
        int32_t staticPressureIterations; 	/// Computes static pressure more precisely
        float colorMixingStrength;  /// Determines how fast colors are mixed, 1.0f ==> mixed immediately, 0.5f ==> mixed half way each simulation step (see b2World::Step())
        float lifetimeGranularity; /// Granularity of particle lifetimes in seconds

        p2ParticleEmmiterDef(float _density = 1.0f, float _radius = 1.0f, 
                             float _gravityScale = 1.0f, int _maxCount = 0, uint32_t _flags = DestroyByAge) :
            density(_density),
            radius(_radius),
            gravityScale(_gravityScale),
            maxCount(_maxCount),
            flags(_flags)
        {
            pressureStrength = 0.05f;
            dampingStrength = 1.0f;
            elasticStrength = 0.25f;
            springStrength = 0.25f;
            viscousStrength = 0.25f;
            surfaceTensionPressureStrength = 0.2f;
            surfaceTensionNormalStrength = 0.2f;
            repulsiveStrength = 1.0f;
            powderStrength = 0.5f;
            ejectionStrength = 0.5f;
            staticPressureStrength = 0.2f;
            staticPressureRelaxation = 0.2f;
            staticPressureIterations = 8;
            colorMixingStrength = 0.5f;
            lifetimeGranularity = 1.0f / 60.0f;
        }
    };

    p2ParticleEmitter* p2CreateParticleEmitter(p2Scene* scene);
    void p2DestroyParticleEmitter(p2Scene* scene);
    void* p2GetParticleEmitterUserData(p2ParticleEmitter* emitter);

    struct p2ParticleDef
    {
        enum Flags {
            WaterParticle = 0,   /// Water particle.
            ZombieParticle = 1 << 1, /// Removed after next simulation step.
            WallParticle = 1 << 2,   /// Zero velocity.
            SpringParticle = 1 << 3, /// With restitution from stretching.
            ElasticParticle = 1 << 4, /// With restitution from deformation.
            ViscousParticle = 1 << 5, /// With viscosity.
            PowderParticle = 1 << 6, /// Without isotropic pressure.
            TensileParticle = 1 << 7,    /// With surface tension.
            ColorMixingParticle = 1 << 8,    /// Mix color between contacting particles.
            DestructionListenerParticle = 1 << 9,    /// Call b2DestructionListener on destruction.
            BarrierParticle = 1 << 10, /// Prevents other particles from leaking.
            StaticPressureParticle = 1 << 11,    /// Less compressibility.
            ReactiveParticle = 1 << 12,  /// Makes pairs or triads with other particles.
            RepulsiveParticle = 1 << 13, /// With high repulsive force.
            /// Call b2ContactListener when this particle is about to interact with
            /// a rigid body or stops interacting with a rigid body.
            /// This results in an expensive operation compared to using
            /// b2_fixtureContactFilterParticle to detect collisions between
            /// particles.
            FixtureContactListenerParticle = 1 << 14,
            /// Call b2ContactListener when this particle is about to interact with
            /// another particle or stops interacting with another particle.
            /// This results in an expensive operation compared to using
            /// b2_particleContactFilterParticle to detect collisions between
            /// particles.
            ParticleContactListenerParticle = 1 << 15,
            /// Call b2ContactFilter when this particle interacts with rigid bodies.
            FixtureContactFilterParticle = 1 << 16,
            /// Call b2ContactFilter when this particle interacts with other particles.
            ParticleContactFilterParticle = 1 << 17,
        };

        uint32_t flags;
        vec2_t position;
        vec2_t velocity;
        color_t color;
        float lifetime;
        void* userData;
        p2ParticleGroup* group;

        p2ParticleDef(uint32_t _flags = 0, const vec2_t& _pos = vec2f(0, 0), 
                      const vec2_t& _velocity = vec2f(0, 0), color_t _color = 0, float _lifetime = 0,
                      void* _userData = nullptr, p2ParticleGroup* _group = nullptr) :
            flags(_flags),
            position(_pos),
            velocity(_velocity),
            color(_color),
            lifetime(_lifetime),
            userData(_userData),
            group(_group)
        {
        }
    };

    TERMITE_API int p2CreateParticle(p2ParticleEmitter* emitter, const p2ParticleDef& particleDef);
    TERMITE_API void p2DestroyParticle(p2ParticleEmitter* emitter, int index, bool callDestructionCallback = false);
    TERMITE_API void p2JoinParticleGroups(p2ParticleGroup* groupA, p2ParticleGroup* groupB);
    TERMITE_API int p2GetParticleCount(p2ParticleEmitter* emitter);
    TERMITE_API void p2SetMaxParticleCount(p2ParticleEmitter* emitter, int maxCount);
    TERMITE_API int p2GetMaxParticleCount(p2ParticleEmitter* emitter);
    TERMITE_API void p2ApplyParticleForceBatch(p2ParticleEmitter* emitter, int firstIdx, int lastIdx, const vec2_t& force);
    TERMITE_API void p2ApplyParticleImpulseBatch(p2ParticleEmitter* emitter, int firstIdx, int lastIdx, const vec2_t& impulse);
    TERMITE_API void p2ApplyParticleForce(p2ParticleEmitter* emitter, int index, const vec2_t& force);
    TERMITE_API void p2ApplyParticleImpulse(p2ParticleEmitter* emitter, int index, const vec2_t& impulse);
    TERMITE_API void* p2GetParticleUserData(p2ParticleEmitter* emitter, int index);

    struct p2ParticleGroupDef
    {
        enum Flags
        {
            SolidParticleGroup = 1 << 0, /// Prevents overlapping or leaking.
            RigidParticleGroup = 1 << 1, /// Keeps its shape.
            ParticleGroupCanBeEmpty = 1 << 2,   /// Won't be destroyed if it gets empty.
            ParticleGroupWillBeDestroyed = 1 << 3,  /// Will be destroyed on next simulation step.
            ParticleGroupNeedsUpdateDepth = 1 << 4, /// Updates depth data on next simulation step.
            ParticleGroupInternalMask = ParticleGroupWillBeDestroyed | ParticleGroupNeedsUpdateDepth,
        };
        

        uint32_t particleFlags;
        uint32_t flags;
        vec2_t position;
        float angle;
        vec2_t linearVelocity;
        float angularVelocity;
        color_t color;
        float strength; /// The strength of cohesion among the particles in a group with flag, b2_elasticParticle or b2_springParticle.
        float lifetime;
        void* userData;
    };

    TERMITE_API p2ParticleGroup* p2CreateParticleGroup(p2ParticleEmitter* emitter, const p2ParticleGroupDef& groupDef,
                                                       p2Shape* shape);
    TERMITE_API void p2DestroyParticleGroup(p2ParticleGroup* pgroup);
    TERMITE_API void p2ApplyParticleGroupImpulse(p2ParticleGroup* group, const vec2_t& impulse);
    TERMITE_API void p2ApplyParticleGroupForce(p2ParticleGroup* group, const vec2_t& force);
    TERMITE_API void p2DestroyParticleGroupParticles(p2ParticleGroup* group, bool callDestructionCallback = false);
    TERMITE_API void* p2GetParticleGroupUserData(p2ParticleGroup* group);


} // namespace termite