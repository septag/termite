#pragma once

#include "bx/bx.h"
#include "math.h"

namespace tee
{   
    struct Camera2D;
    struct GfxDriver;
    struct PhysScene2D;
    struct PhysShape2D;
    struct PhysBody2D;
    struct PhysJoint2D;
    struct PhysFrictionJoint2D;
    struct PhysRevoluteJoint2D;
    struct PhysDistanceJoint2D;
    struct PhysPrismaticJoint2D;
    struct PhysPulleyJoint2D;
    struct PhysWeldJoint2D;
    struct PhysGearJoint2D;
    struct PhysRopeJoint2D;
    struct PhysMouseJoint2D;
    struct PhysWheelJoint2D;
    struct PhysMotorJoint2D;
    struct PhysParticleEmitter2D;
    struct PhysParticleGroup2D;

    struct PhysContactInfo2D
    {
        vec2_t normal;          // in world-coords
        vec2_t points[2];       // world-coords
        float separations[2];   // A negative value indicates overlap, in meters
    };

    struct PhysSceneDef2D
    {
        vec2_t gravity;
        float timestep;

        PhysSceneDef2D(const vec2_t _gravity = vec2(0, -9.8f), float _timestep = 1.0f/60.0f) :
            gravity(_gravity),
            timestep(_timestep)
        {
        }
    };

    struct PhysFlags2D
    {
        enum Enum {
            EnableDebug = 0x1
        };

        typedef uint16_t Bits;
    };

    struct PhysDebugFlags2D
    {
        enum Enum {
            Shape = 0x0001,	
            Joint = 0x0002,	
            AABB = 0x0004,	
            Pairs = 0x0008,	
            CenterOfMass = 0x0010,	
            Particle = 0x0020  
        };

        typedef uint16_t Bits;
    };

    struct PhysBodyType2D
    {
        enum Enum
        {
            Static = 0,
            Kinematic,
            Dynamic
        };
    };

    struct PhysBodyFlags2D
    {
        enum Enum
        {
            AllowSleep = 0x1, /// unset this flag and the body should never fall asleep
            IsAwake = 0x2, /// Is this body initially awake or sleeping?
            FixedRotation = 0x4, /// Should this body be prevented from rotating? Useful for characters.
            IsBullet = 0x8, /// Is this a fast moving body that should be prevented from tunneling
            IsActive = 0x10 /// Does this body start out active?
        };

        typedef uint8_t Bits;
    };

    struct PhysShapeFlags2D
    {
        enum Enum
        {
            IsSensor = 0x1 /// A sensor shape collects contact information but never generates a collision response.
        };

        typedef uint8_t Bits;
    };

    struct PhysEmitterFlags2D
    {
        enum Enum
        {
            StrictContactCheck = 0x1,   /// Enable strict Particle/Body contact check.
            DestroyByAge = 0x2 /// Whether to destroy particles by age when no more particles can be created
        };

        typedef uint8_t Bits;
    };

    struct PhysParticleFlags2D
    {
        enum Enum
        {
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

        typedef uint32_t Bits;
    };

    struct PhysParticleGroupFlags2D
    {
        enum Enum
        {
            SolidParticleGroup = 1 << 0, /// Prevents overlapping or leaking.
            RigidParticleGroup = 1 << 1, /// Keeps its shape.
            ParticleGroupCanBeEmpty = 1 << 2,   /// Won't be destroyed if it gets empty.
            ParticleGroupWillBeDestroyed = 1 << 3,  /// Will be destroyed on next simulation step.
            ParticleGroupNeedsUpdateDepth = 1 << 4, /// Updates depth data on next simulation step.
            ParticleGroupInternalMask = ParticleGroupWillBeDestroyed | ParticleGroupNeedsUpdateDepth,
        };

        typedef uint16_t Bits;
    };

    struct PhysBodyDef2D
    {
        PhysBodyType2D::Enum type;
        vec2_t position;
        float angle;
        vec2_t linearVel; /// The linear velocity of the body's origin in world co-ordinates.
        float angularVel; /// The angular velocity of the body.
        float linearDamping; /// Linear damping is use to reduce the linear velocity. The damping parameter
                             /// can be larger than 1.0f
        float angularDamping;   /// Angular damping is use to reduce the angular velocity. The damping parameter
                                /// can be larger than 1.0
        PhysBodyFlags2D::Bits flags;     /// BodyFlags
        void* userData;
        float gravityScale;

        PhysBodyDef2D(PhysBodyType2D::Enum _type = PhysBodyType2D::Static, 
                      const vec2_t& _pos = vec2(0, 0), float _angle = 0,
                      const vec2_t& _linearVel = vec2(0, 0), float _angularVel = 0,
                      float _linearDamping = 0, float _angularDamping = 0,
                      uint32_t _flags = PhysBodyFlags2D::AllowSleep | PhysBodyFlags2D::IsAwake | PhysBodyFlags2D::IsActive, 
                      void* _userData = nullptr,
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

    struct PhysShapeDef2D
    {
        float friction; /// The friction coefficient, usually in the range [0,1].
        float restitution;  /// The restitution (elasticity) usually in the range [0,1].
        float density; /// The density, usually in kg/m^2.
        PhysShapeFlags2D::Bits flags;
        void* userData;
        uint16_t categoryBits;  /// The collision category bits. Normally you would just set one bit.
        uint16_t maskBits;  /// The collision mask bits. This states the categories that this
                            /// shape would accept for collision.
        int16_t groupIndex; /// Collision groups allow a certain group of objects to never collide (negative)
                            /// or always collide (positive). Zero means no collision group. Non-zero group
                            /// filtering always wins against the mask bits.

        PhysShapeDef2D(float _friction = 0.2f, float _restitution = 0, float _density = 0, PhysShapeFlags2D::Bits _flags = 0,
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

    // Particle System
    struct PhysParticleEmitterDef2D
    {
        PhysEmitterFlags2D::Bits flags;
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

        void* userData;

        PhysParticleEmitterDef2D(float _density = 1.0f, float _radius = 1.0f,
                                 float _gravityScale = 1.0f, int _maxCount = 0, 
                                 PhysEmitterFlags2D::Bits _flags = PhysEmitterFlags2D::DestroyByAge,
                                 void* _userData = nullptr) :
            flags(_flags),
            density(_density),
            gravityScale(_gravityScale),
            radius(_radius),
            maxCount(_maxCount),
            userData(_userData)
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

    struct PhysParticleDef2D
    {
        PhysParticleFlags2D::Bits flags;
        vec2_t position;
        vec2_t velocity;
        ucolor_t color;
        float lifetime;
        void* userData;
        PhysParticleGroup2D* group;

        PhysParticleDef2D(PhysParticleFlags2D::Bits _flags = 0, const vec2_t& _pos = vec2(0, 0),
                          const vec2_t& _velocity = vec2(0, 0), ucolor_t _color = ucolor(0), float _lifetime = 0,
                          void* _userData = nullptr, PhysParticleGroup2D* _group = nullptr) :
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

    struct PhysParticleGroupDef2D
    {
        PhysParticleFlags2D::Bits particleFlags; 
        PhysParticleGroupFlags2D::Bits flags;     
        vec2_t position;    /// world pos
        float angle;
        vec2_t linearVelocity;
        float angularVelocity;
        ucolor_t color;
        float strength; /// The strength of cohesion among the particles in a group with flag, b2_elasticParticle or b2_springParticle.
        float lifetime; /// Lifetime of the particle group in seconds.  A value <= 0.0f indicates a particle group with infinite lifetime.
        void* userData;

        PhysParticleGroupDef2D(PhysParticleFlags2D::Bits _particleFlags = 0, PhysParticleGroupFlags2D::Bits _flags = 0, 
                               const vec2_t& _pos = vec2(0, 0), float _angle = 0,
                               const vec2_t& _linearVel = vec2(0, 0), float _angularVel = 0,
                               void* _userData = nullptr) :
            particleFlags(_particleFlags),
            flags(_flags),
            position(_pos),
            angle(_angle),
            linearVelocity(_linearVel),
            angularVelocity(_angularVel),
            userData(_userData)
        {
            strength = 1.0f;
            lifetime = 0;
        }
    };

    typedef void(*PhysJointDestroyCallback2D)(PhysJoint2D* joint);
    typedef void(*PhysShapeDestroyCallback2D)(PhysShape2D* shape);
    typedef void(*PhysParticleGroupDestroyCallback2D)(PhysParticleGroup2D* pgroup);
    typedef void(*PhysParticleDestroyCallback2D)(PhysParticleEmitter2D* emitter, int index);

    typedef bool(*PhysShapeContactFilterCallback2D)(PhysShape2D* shapeA, PhysShape2D* shapeB);
    typedef bool(*PhysParticleShapeContactFilterCallback2D)(PhysParticleEmitter2D* emitter, int index, PhysShape2D* shape);
    typedef bool(*PhysParticleContactFilterCallback2D)(PhysParticleEmitter2D* emitter, int indexA, int indexB);

    typedef bool(*PhysShapeContactCallback2D)(PhysShape2D* shapeA, PhysShape2D* shapeB, const PhysContactInfo2D* contactInfo);
    typedef void(*PhysParticleShapeContactCallback2D)(PhysParticleEmitter2D* emitter, int index, PhysShape2D* shape,
                                                      const vec2_t& normal, float weight);
    typedef void(*PhysParticleContactCallback2D)(PhysParticleEmitter2D* emitter, int indexA, int indexB,
                                                 const vec2_t& normal, float weight);

    /// @param fraction interpolation value between p1 and p2
    /// @return shrinks or grows the ray by the fraction value returned
    ///         return -1 to ignore current
    ///         return 0 abort the next checks because we have no rays
    ///         return 1 , testing ray does not change
    ///         return fraction shrinks the ray after this callback, do this and keep recent result for closest hit
    typedef float(*PhysRayCastCallback2D)(PhysShape2D* shape, const vec2_t& point, const vec2_t& normal, float fraction, void* userData);

    /// @param shape shape that collides the shape queried
    /// @return Return false if you want to stop the query
    typedef bool(*PhysQueryShapeCallback2D)(PhysShape2D* shape, void* userData);

    struct PhysDriver2D
    {
        bool (*init)(bx::AllocatorI* alloc, PhysFlags2D::Bits flags/*=0*/, uint8_t debugViewId/*=255*/);
        void (*shutdown)();

        bool (*initGraphicsObjects)();
        void (*shutdownGraphicsObjects)();

        PhysScene2D* (*createScene)(const PhysSceneDef2D& worldDef);
        void (*destroyScene)(PhysScene2D* scene);
        float (*getSceneTimeStep)(PhysScene2D* scene);

        // Returns blending coeff for interpolating between frames
        // State = currentState * alpha + prevState * (1 - alpha)
        void (*stepScene)(PhysScene2D* scene, float dt);   
        void (*debugScene)(PhysScene2D* scene, const irect_t viewport, const Camera2D& cam, 
                           PhysDebugFlags2D::Bits flags/* = PhysDebugFlags2D::All*/);

        PhysBody2D* (*createBody)(PhysScene2D* scene, const PhysBodyDef2D& bodyDef);
        void (*destroyBody)(PhysBody2D* body);

        PhysShape2D* (*createBoxShape)(PhysBody2D* body, const vec2_t& halfSize, const PhysShapeDef2D& shapeDef);
        PhysShape2D* (*createArbitaryBoxShape)(PhysBody2D* body, const vec2_t& halfSize, const vec2_t& pos, float angle,
                                               const PhysShapeDef2D& shapeDef);
        PhysShape2D* (*createPolyShape)(PhysBody2D* body, const vec2_t* verts, int numVerts, const PhysShapeDef2D& shapeDef);
        PhysShape2D* (*createCircleShape)(PhysBody2D* body, const vec2_t& pos, float radius, const PhysShapeDef2D& shapeDef);

        // Body
        void (*setTransform)(PhysBody2D* body, const vec2_t& pos, float angle);
        void (*getTransform)(PhysBody2D* body, vec2_t* pPos, float* pAngle);
        vec2_t (*getPosition)(PhysBody2D* body);
        float (*getAngle)(PhysBody2D* body);
        vec2_t (*getWorldCenter)(PhysBody2D* body);
        vec2_t (*getLocalCenter)(PhysBody2D* body);
        vec2_t (*getLocalPoint)(PhysBody2D* body, const vec2_t& worldPt);
        vec2_t (*getLocalVector)(PhysBody2D* body, const vec2_t& worldVec);
        vec2_t (*getWorldPoint)(PhysBody2D* body, const vec2_t& localPt);
        void (*setLinearVelocity)(PhysBody2D* body, const vec2_t& vel);
        void (*setAngularVelocity)(PhysBody2D* body, float vel);
        vec2_t (*getLinearVelocity)(PhysBody2D* body);
        float (*getAngularVelocity)(PhysBody2D* body);
        void (*setLinearDamping)(PhysBody2D* body, float damping);
        float (*getLinearDamping)(PhysBody2D* body);
        void (*setAngularDamping)(PhysBody2D* body, float damping);
        float (*getAngularDamping)(PhysBody2D* body);
        void (*applyForce)(PhysBody2D* body, const vec2_t& force, const vec2_t& worldPt, bool wake/* = true*/);
        void (*applyForceToCenter)(PhysBody2D* body, const vec2_t& force, bool wake/* = true*/);
        void (*applyTorque)(PhysBody2D* body, float torque, bool wake/* = true*/);
        void (*applyLinearImpulse)(PhysBody2D* body, const vec2_t& impulse, const vec2_t& worldPt, bool wake/* = true*/);
        void (*applyAngularImpulse)(PhysBody2D* body, float impulse, bool wake/* = true*/);
        void (*setActive)(PhysBody2D* body, bool active);
        bool (*isActive)(PhysBody2D* body);
        bool (*isAwake)(PhysBody2D* body);
        void (*setAwake)(PhysBody2D* body, bool awake);
        void (*addShapeToBody)(PhysBody2D* body, PhysShape2D* shape);
        void* (*getBodyUserData)(PhysBody2D* body);
        void (*setGravityScale)(PhysBody2D* body, float gravityScale);
        vec2_t (*getMassCenter)(PhysBody2D* body);
        void (*setMassCenter)(PhysBody2D* body, const vec2_t& center);
        float (*getMass)(PhysBody2D* body);
        float(*getInertia)(PhysBody2D* body);

        // Shape
        void* (*getShapeUserData)(PhysShape2D* shape);
        void (*setShapeContactFilterData)(PhysShape2D* shape, uint16_t catBits, uint16_t maskBits, int16_t groupIndex);
        void (*getShapeContactFilterData)(PhysShape2D* shape, uint16_t* catBits, uint16_t* maskBits, int16_t* groupIndex);
        PhysBody2D* (*getShapeBody)(PhysShape2D* shape);
        rect_t (*getShapeAABB)(PhysShape2D* shape);

        // Ray Cast/Query
        void (*rayCast)(PhysScene2D* scene, const vec2_t& p1, const vec2_t& p2, PhysRayCastCallback2D callback, void* userData);
        void(*queryShapeCircle)(PhysScene2D* scene, float radius, const vec2_t pos, PhysQueryShapeCallback2D callback, void *userData);
        void(*queryShapeBox)(PhysScene2D* scene, vec2_t pos, vec2_t halfSize, PhysQueryShapeCallback2D callback, void *userData);

        // Joints

        // Note: all initialization coordinates are in local coords of each body
        // Friction Joint: provides friction between two bodies
        PhysFrictionJoint2D* (*createFrictionJoint)(PhysScene2D* scene, PhysBody2D* bodyA, PhysBody2D* bodyB,
                                                           const vec2_t& anchorA, const vec2_t& anchorB,
                                                           float maxForce/* = 0*/, float maxTorque/* = 0*/,
                                                           bool collide/* = false*/, void* userData/* = nullptr*/);
        void (*destroyFrictionJoint)(PhysScene2D* scene, PhysFrictionJoint2D* joint);

        // Revolute Joint: Two bodies share a point that they rotate around
        PhysRevoluteJoint2D* (*createRevoluteJoint)(PhysScene2D* scene, PhysBody2D* bodyA, PhysBody2D* bodyB,
                                                           const vec2_t& anchorA, const vec2_t& anchorB,
                                                           float refAngle/* = 0*/,
                                                           bool enableLimit/* = false*/, float lowerAngle/* = 0*/, float upperAngle/* = 0*/,
                                                           bool enableMotor/* = false*/, float motorSpeed/* = 0*/, float maxMotorTorque/* = 0*/,
                                                           bool collide/* = false*/, void* userData/* = nullptr*/);
        void (*destroyRevoluteJoint)(PhysScene2D* scene, PhysRevoluteJoint2D* joint);

        // Distance Joint: Two bodies keeps a distance (like a rigid rod between bodies), can use to simulate springs too
        PhysDistanceJoint2D* (*createDistanceJoint)(PhysScene2D* scene, PhysBody2D* bodyA, PhysBody2D* bodyB,
                                                           const vec2_t& anchorA, const vec2_t& anchorB,
                                                           float length, float frequencyHz/* = 0*/, float dampingRatio/* = 0.7f*/,
                                                           bool collide/* = false*/, void* userData/* = nullptr*/);
        void (*setDistanceJointLength)(PhysDistanceJoint2D* joint, float length);
        void (*destroyDistanceJoint)(PhysScene2D* scene, PhysDistanceJoint2D* joint);

        // Prismatic Joint: Defines a line of motion using an axis and an anchor point (one degree of freedom)
        PhysPrismaticJoint2D* (*createPrismaticJoint)(PhysScene2D* scene, PhysBody2D* bodyA, PhysBody2D* bodyB,
                                                             const vec2_t& anchorA, const vec2_t& anchorB, const vec2_t& axisA,
                                                             bool enableLimit/* = false*/, float lowerTranslation/* = 0*/, float upperTranslation/* = 0*/,
                                                             bool enableMotor/* = false*/, float maxMotorForce/* = 0*/, float motorSpeed/* = 0*/,
                                                             bool collide/* = false*/, void* userData/* = nullptr*/);
        void (*destroyPrismaticJoint)(PhysScene2D* scene, PhysPrismaticJoint2D* joint);

        // Pully Joint: joint between two bodies and two ground points (world-static), length1 + ratio*length2 <= constant
        PhysPulleyJoint2D* (*createPulleyJoint)(PhysScene2D* scene, PhysBody2D* bodyA, PhysBody2D* bodyB,
                                                       const vec2_t& groundWorldAnchorA, const vec2_t& groundWorldAnchorB,
                                                       const vec2_t& anchorA, const vec2_t& anchorB, float ratio/* = 1.0f*/,
                                                       bool collide/* = false*/, void* userData/* = nullptr*/);
        void (*destroyPulleyJoint)(PhysScene2D* scene, PhysPulleyJoint2D* joint);

        // Weld Joint: Attaches/welds two bodies
        PhysWeldJoint2D* (*createWeldJoint)(PhysScene2D* scene, PhysBody2D* bodyA, PhysBody2D* bodyB, const vec2_t& worldPt,
                                            float dampingRatio/* = 0*/, float frequencyHz/* = 0*/, void* userData/* = nullptr*/);
        PhysWeldJoint2D* (*createWeldJoint2Pts)(PhysScene2D* scene, PhysBody2D* bodyA, PhysBody2D* bodyB, 
                                                const vec2_t& anchorA, const vec2_t& anchorB,
                                                float dampingRatio/* = 0*/, float frequencyHz/* = 0*/, void* userData/* = nullptr*/);
        void (*destroyWeldJoint)(PhysScene2D* scene, PhysWeldJoint2D* joint);

        // Gear Joint: Connects two prismatic or revolute joints together. coordsA + ratio*coordsB = constant
        PhysGearJoint2D* (*createGearJoint)(PhysScene2D* scene, PhysJoint2D* jointA, PhysJoint2D* jointB, float ratio/* = 1.0f*/,
                                                   bool collide/* = false*/, void* userData/* = nullptr*/);
        void (*destroyGearJoint)(PhysScene2D* scene, PhysGearJoint2D* joint);

        // Mouse Joint: Makes a body track a target in world coords
        PhysMouseJoint2D* (*createMouseJoint)(PhysScene2D* scene, PhysBody2D* body, const vec2_t& target,
                                              float maxForce/* = 0*/, float frequencyHz/* = 5.0f*/, float dampingRatio/* = 0.7f*/,
                                              bool collide/* = false*/, void* userData/* = nullptr*/);
        void (*setMouseTarget)(PhysMouseJoint2D* joint, const vec2_t& target);
        vec2_t (*getMouseTarget)(PhysMouseJoint2D* joint);
        void (*destroyMouseJoint)(PhysScene2D* scene, PhysMouseJoint2D* joint);

        // Motor Joint: Controls relative motion between two bodies
        PhysMotorJoint2D* (*createMotorJoint)(PhysScene2D* scene, PhysBody2D* bodyA, PhysBody2D* bodyB,
                                          const vec2_t& linearOffset,  /// bodyB_position - bodyA_position, relative to bodyA
                                          float angularOffset, /// bodyB_Angle - bodyA_angle
                                          float maxForce/* = 1.0f*/, float maxTorque/* = 1.0f*/, float correctionFactor/* = 0.3f*/,
                                          bool collide/* = false*/, void* userData/* = nullptr*/);
        void (*setMotorLinearOffset)(PhysMotorJoint2D* joint, const vec2_t& linearOffset);
        void (*setMotorAngularOffset)(PhysMotorJoint2D* joint, const vec2_t& angularOffset);
        void (*destroyMotorJoint)(PhysScene2D* scene, PhysMotorJoint2D* joint);

        // Rope Joint: Enforces maximum distance between two points in bodies
        PhysRopeJoint2D* (*createRopeJoint)(PhysScene2D* scene, PhysBody2D* bodyA, PhysBody2D* bodyB,
                                                   const vec2_t& anchorA, const vec2_t& anchorB,
                                                   float maxLength,
                                                   bool collide/* = false*/, void* userData/* = nullptr*/);
        void (*destroyRopeJoint)(PhysScene2D* scene, PhysRopeJoint2D* joint);

        // Wheel Joint: provides two-degrees of freedom between two bodies. Translation along an axis fixed in bodyA
        //              and rotation in the plane, used for vehicle suspension. this is actually a combination of prismatic and revolute
        PhysWheelJoint2D* (*createWheelJoint)(PhysScene2D* scene, PhysBody2D* bodyA, PhysBody2D* bodyB,
                                                     const vec2_t& anchorA, const vec2_t& anchorB, const vec2_t& axisA,
                                                     bool enableMotor/* = false*/, float maxMotorTorque/* = 0*/, float motorSpeed/* = 0*/,
                                                     float frequencyHz/* = 2.0f*/, float dampingRatio/* = 0.7f*/,
                                                     bool collide/* = false*/, void* userData/* = nullptr*/);
        void (*destroyWheelJoint)(PhysScene2D* scene, PhysWheelJoint2D* joint);
        void* (*getJointUserData)(PhysJoint2D* joint);

        PhysParticleEmitter2D* (*createParticleEmitter)(PhysScene2D* scene, const PhysParticleEmitterDef2D& def);
        void (*destroyParticleEmitter)(PhysScene2D* scene, PhysParticleEmitter2D* emitter);
        void* (*getParticleEmitterUserData)(PhysParticleEmitter2D* emitter);

        int (*createParticle)(PhysParticleEmitter2D* emitter, const PhysParticleDef2D& particleDef);
        void (*destroyParticle)(PhysParticleEmitter2D* emitter, int index, bool callDestructionCallback/* = false*/);
        void (*joinParticleGroups)(PhysParticleGroup2D* groupA, PhysParticleGroup2D* groupB);
        int (*getParticleCount)(PhysParticleEmitter2D* emitter);
        void (*setMaxParticleCount)(PhysParticleEmitter2D* emitter, int maxCount);
        int (*getMaxParticleCount)(PhysParticleEmitter2D* emitter);
        void (*applyParticleForceBatch)(PhysParticleEmitter2D* emitter, int firstIdx, int lastIdx, const vec2_t& force);
        void (*applyParticleImpulseBatch)(PhysParticleEmitter2D* emitter, int firstIdx, int lastIdx, const vec2_t& impulse);
        void (*applyParticleForce)(PhysParticleEmitter2D* emitter, int index, const vec2_t& force);
        void (*applyParticleImpulse)(PhysParticleEmitter2D* emitter, int index, const vec2_t& impulse);

        PhysParticleGroup2D* (*createParticleGroupCircleShape)
            (PhysParticleEmitter2D* emitter, const PhysParticleGroupDef2D& groupDef, float radius);
        void (*applyParticleGroupImpulse)(PhysParticleGroup2D* group, const vec2_t& impulse);
        void (*applyParticleGroupForce)(PhysParticleGroup2D* group, const vec2_t& force);
        void (*destroyParticleGroupParticles)(PhysParticleGroup2D* group, bool callDestructionCallback/* = false*/);
        void* (*getParticleGroupUserData)(PhysParticleGroup2D* group);
        void (*setParticleGroupFlags)(PhysParticleGroup2D* group, uint32_t flags);
        uint32_t (*getParticleGroupFlags)(PhysParticleGroup2D* group);

        int (*getEmitterPositionBuffer)(PhysParticleEmitter2D* emitter, vec2_t* poss, int maxItems);
        int (*getEmitterVelocityBuffer)(PhysParticleEmitter2D* emitter, vec2_t* vels, int maxItems);
        int (*getEmitterColorBuffer)(PhysParticleEmitter2D* emitter, ucolor_t* colors, int maxItems);
                
        // Callbacks
        void (*setJointDestroyCallback)(PhysJoint2D* joint, PhysJointDestroyCallback2D callback);
        void (*setShapeDestroyCallback)(PhysShape2D* shape, PhysShapeDestroyCallback2D callback);
        void (*setParticleGroupDestroyCallback)(PhysParticleGroup2D* pgroup, PhysParticleGroupDestroyCallback2D callback);
        void (*setParticleDestroyCallback)(PhysParticleEmitter2D* emitter, PhysParticleDestroyCallback2D callback);
        void (*setShapeContactFilterCallback)(PhysShape2D* shape, PhysShapeContactFilterCallback2D callback);
        void (*setParticleShapeContactFilterCallback)(PhysParticleEmitter2D* emitter, PhysParticleShapeContactFilterCallback2D callback);
        void (*setParticleContactFilterCallback)(PhysParticleEmitter2D* emitter, PhysParticleContactFilterCallback2D callback);

        void (*setBeginShapeContactCallback)(PhysShape2D* shape, PhysShapeContactCallback2D callback, bool reportContactInfo/* = false*/);
        void (*setEndShapeContactCallback)(PhysShape2D* shape, PhysShapeContactCallback2D callback);
        void (*setBeginParticleShapeContactCallback)(PhysParticleEmitter2D* emitter, PhysParticleShapeContactCallback2D callback);
        void (*setEndParticleShapeContactCallback)(PhysParticleEmitter2D* emitter, PhysParticleShapeContactCallback2D callback);
        void (*setBeginParticleContactCallback)(PhysParticleEmitter2D* emitter, PhysParticleContactCallback2D callback);
        void (*setEndParticleContactCallback)(PhysParticleEmitter2D* emitter, PhysParticleContactCallback2D callback);
    };
} // namespace tee
