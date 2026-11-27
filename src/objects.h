#ifndef H_TRIGGER
#define H_TRIGGER

#include "core.h"
#include "controller.h"
#include "character.h"
#include "sprite.h"

#define TRAP_TOUCH_RADIUS       100.0f // Lara's collision radius

// fire and explosions light their surroundings (a modern effect, the original has it for the flares and the shots only)
#define FIRE_LIGHT_COLOR        vec3(1.4f, 0.8f, 0.35f)
#define FIRE_LIGHT_RADIUS       4096.0f
#define EXPLOSION_LIGHT_RADIUS  6144.0f

struct EffectLight {
    int room;

    EffectLight() : room(-1) {}

    void set(TR::Level *level, int id, int roomIndex, const vec3 &pos, const vec3 &color, float radius) {
        if (Core::settings.detail.lighting < Core::Settings::MEDIUM) { // from medium lighting
            off(level, id);
            return;
        }
        if (room != roomIndex)
            off(level, id);
        level->rooms[roomIndex].addDynLight(id, vec4(pos, 0.0f), vec4(color, 1.0f / radius));
        room = roomIndex;
    }

    void off(TR::Level *level, int id) {
        if (room >= 0)
            level->rooms[room].removeDynLight(id);
        room = -1;
    }
};

// the meshes of the trap touching Lara, as in the original (Object_Collision): only when her origin is
// within the bounds of the trap widened by her radius, then the spheres of their meshes are tested
// (spheres = false: the bounds only, returns 1 then)
inline int touchLara(Controller *trap, Controller *lara, bool spheres = true) {
    // only the traps of her room and the rooms through its portals (M_ObjectCollision): the spinning blade of the
    // room behind the wall of the long ladder in The Great Wall reached her on the ladder
    if (!lara->isNearRoom(lara->getRoomIndex(), trap->getRoomIndex()))
        return 0;

    Box box = trap->getBoundingBoxLocal();
    Box laraBox = lara->getBoundingBoxLocal();

    if (trap->pos.y + box.min.y >= lara->pos.y + laraBox.max.y || trap->pos.y + box.max.y <= lara->pos.y + laraBox.min.y)
        return 0;

    vec3 d = (lara->pos - trap->pos).rotateY(trap->angle.y);
    if (d.x < box.min.x - TRAP_TOUCH_RADIUS || d.x > box.max.x + TRAP_TOUCH_RADIUS ||
        d.z < box.min.z - TRAP_TOUCH_RADIUS || d.z > box.max.z + TRAP_TOUCH_RADIUS)
        return 0;

    return spheres ? trap->collide(lara, false) : 1;
}


struct Switch : Controller {
    enum {
        STATE_DOWN,
        STATE_UP,
    };

    Switch(IGame *game, int entity) : Controller(game, entity) {}
    
    bool setTimer(float t) {
        if (flags.state == TR::Entity::asInactive) {
            if (state == STATE_DOWN && t > 0.0f) {
                timer = t;
                Controller::activate();
            } else
                Controller::deactivate(true);
            return true;
        }
        return false;
    }

    virtual void deactivate(bool removeFromList) {
        Controller::deactivate(true);
        flags.state = TR::Entity::asInactive;
    }
    
    virtual bool activate() {
        if (Controller::activate()) {
            animation.setState(state == STATE_UP ? STATE_DOWN : STATE_UP);
            return true;
        }
        return false;
    }

    virtual void update() {
        updateAnimation(true);
        flags.active = TR::ACTIVE;
        if (!isActive()) {
            animation.setState(STATE_UP);
            timer = 0.0f;
        }
    }
};

// A key left in its hole (an addition of OpenLara, the originals draw nothing there).
// Its pos is the point Lara puts it at: the hole + vec3(0, -590, 484) turned by the hole.
// TR1 stands its inventory model up with angle.x. The models of TR2 and TR3 lie in all
// axes (a key flat in XY or along Z, tilted ones, a card), so the pose comes from the mesh:
// its longest axis goes into the wall with the narrow end (the blade) first, its broad
// side stands upright, and the handle stays out of the plate of the hole.
struct KeyItemInv : Controller {
    bool  posed;
    vec3  axis[3]; // model axes in the space of the hole (x - right, y - down, z - into the wall)
    vec3  shift;   // model origin in that space, from pos

    KeyItemInv(IGame *game, int entity) : Controller(game, entity), posed(false) {
        if (level->version & TR::VER_TR1) {
            angle.x = PI * 0.5f;
            return;
        }
        posed = initPose();
    }

    bool initPose() {
        const TR::Model *model = getModel();
        if (!model || !model->mCount) return false;
        int index = level->meshOffsets[model->mStart];
        if (!index && model->mStart) return false;
        const TR::Mesh &mesh = level->meshes[index];
        if (mesh.vCount < 3 || !mesh.vertices) return false;

        vec3 c(0.0f);
        for (int i = 0; i < mesh.vCount; i++)
            c += vertex(mesh, i);
        c *= 1.0f / mesh.vCount;

        float m[3][3] = { {0} };
        for (int i = 0; i < mesh.vCount; i++) {
            vec3 d = vertex(mesh, i) - c;
            for (int a = 0; a < 3; a++)
                for (int b = 0; b < 3; b++)
                    m[a][b] += d[a] * d[b];
        }

        vec3 e[3];
        float ev[3];
        eigen(m, ev, e);

        int order[3] = { 0, 1, 2 };
        for (int i = 0; i < 2; i++)
            for (int j = i + 1; j < 3; j++)
                if (ev[order[j]] > ev[order[i]]) swap(order[i], order[j]);

        vec3 L = e[order[0]].normal(); // length
        vec3 W = e[order[1]].normal(); // breadth

        float lo = INF, hi = -INF;
        for (int i = 0; i < mesh.vCount; i++) {
            float t = (vertex(mesh, i) - c).dot(L);
            lo = min(lo, t);
            hi = max(hi, t);
        }
        float length = hi - lo;
        if (length < 1.0f) return false;

        // the blade is the end that is narrower across the breadth
        float wMin[2] = { INF, INF }, wMax[2] = { -INF, -INF };
        for (int i = 0; i < mesh.vCount; i++) {
            vec3  d = vertex(mesh, i) - c;
            float t = d.dot(L);
            float w = d.dot(W);
            for (int k = 0; k < 2; k++) {
                if (k == 0 ? (t < lo + length * 0.25f) : (t > hi - length * 0.25f)) {
                    wMin[k] = min(wMin[k], w);
                    wMax[k] = max(wMax[k], w);
                }
            }
        }
        if (wMax[0] - wMin[0] < wMax[1] - wMin[1]) {
            L  = -L;
            hi = -lo;
        }
        vec3 T = W.cross(L);

        // model axes in the hole space: x = T, y = W, z = L (rotation, no mirror)
        axis[0] = vec3(T.x, W.x, L.x);
        axis[1] = vec3(T.y, W.y, L.y);
        axis[2] = vec3(T.z, W.z, L.z);

        // the plate of the hole is 68 units thick with its face at z = 443, the slot at y = -571
        // (pos is at 0, -590, 484): the blade end goes 40% of the length into it
        vec3 tip = vec3(0.0f, 19.0f, -41.0f + length * 0.4f);
        shift = tip - vec3(c.dot(T), c.dot(W), c.dot(L) + hi);
        return true;
    }

    static vec3 vertex(const TR::Mesh &mesh, int i) {
        const short4 &v = mesh.vertices[i].coord;
        return vec3(float(v.x), float(v.y), float(v.z));
    }

    // eigenvectors of a symmetric 3x3 (Jacobi rotations)
    static void eigen(float a[3][3], float val[3], vec3 vec[3]) {
        float v[3][3] = { {1, 0, 0}, {0, 1, 0}, {0, 0, 1} };
        for (int iter = 0; iter < 50; iter++) {
            int p = 0, q = 1;
            if (fabsf(a[0][2]) > fabsf(a[p][q])) { p = 0; q = 2; }
            if (fabsf(a[1][2]) > fabsf(a[p][q])) { p = 1; q = 2; }
            if (fabsf(a[p][q]) < 1e-6f) break;
            float th = 0.5f * atan2f(2.0f * a[p][q], a[q][q] - a[p][p]);
            float c = cosf(th), s = sinf(th);
            for (int k = 0; k < 3; k++) {
                float kp = a[k][p], kq = a[k][q];
                a[k][p] = c * kp - s * kq;
                a[k][q] = s * kp + c * kq;
            }
            for (int k = 0; k < 3; k++) {
                float pk = a[p][k], qk = a[q][k];
                a[p][k] = c * pk - s * qk;
                a[q][k] = s * pk + c * qk;
            }
            for (int k = 0; k < 3; k++) {
                float kp = v[k][p], kq = v[k][q];
                v[k][p] = c * kp - s * kq;
                v[k][q] = s * kp + c * kq;
            }
        }
        for (int i = 0; i < 3; i++) {
            val[i] = a[i][i];
            vec[i] = vec3(v[0][i], v[1][i], v[2][i]);
        }
    }

    void updatePose() {
        float ay = -angle.y;
        vec3 r = axis[0].rotateY(ay);
        vec3 u = axis[1].rotateY(ay);
        vec3 d = axis[2].rotateY(ay);
        vec3 o = pos + shift.rotateY(ay);

        lockMatrix = true;
        matrix.identity();
        matrix.right()  = vec4(r, 0.0f);
        matrix.up()     = vec4(u, 0.0f);
        matrix.dir()    = vec4(d, 0.0f);
        matrix.offset() = vec4(o, 1.0f);

        joints[0]   = Basis(matrix);
        jointsFrame = Core::stats.frame;
    }

    virtual void render(Frustum *frustum, MeshBuilder *mesh, Shader::Type type, bool caustics) {
        if (posed) updatePose();
        Controller::render(frustum, mesh, type, caustics);
    }
};

#define DART_DAMAGE 50

struct Dart : Controller {
    vec3 velocity;
    vec3 dir;
    bool armed;

    Dart(IGame *game, int entity) : Controller(game, entity), armed(true) {
        dir = vec3(sinf(angle.y), 0, cosf(angle.y));
        activate();
    }

    virtual void update() {
        velocity = dir * animation.getSpeed();
        pos = pos + velocity * (Core::deltaTime * 30.0f);

        Controller *lara = game->getLara(pos);
        if (armed && touchLara(this, lara)) {
            lara->hit(DART_DAMAGE, this, TR::HIT_DART);
            armed = false;
        }

        TR::Level::FloorInfo info;
        getFloorInfo(getRoomIndex(), pos, info);
        if (pos.y > info.floor || pos.y < info.ceiling || !insideRoom(pos, getRoomIndex())) {
            game->addEntity(TR::Entity::RICOCHET, getRoomIndex(), pos - dir * 64.0f); // with wall offset
            game->removeEntity(this);
        }
    }
};

struct TrapDartEmitter : Controller {
    enum {
        STATE_IDLE,
        STATE_FIRE
    };

    TrapDartEmitter(IGame *game, int entity) : Controller(game, entity) {}
    
    void virtual update() {
        if (state == STATE_IDLE || state == STATE_FIRE)
            animation.setState(isActive() ? STATE_FIRE : STATE_IDLE);
        else
            ASSERT(false);

        if (state == STATE_FIRE && animation.framePrev == -1) {
            vec3 p = pos + vec3(0.0f, -512.0f, 256.0f).rotateY(PI - angle.y);

            game->addEntity(TR::Entity::DART, getRoomIndex(), p, angle.y);
            if (level->extra.smoke != -1)
                game->addEntity(TR::Entity::SMOKE, getRoomIndex(), p);
            game->playSound(TR::SND_DART, p, Sound::PAN);
        }

        updateAnimation(true);
    }
};

#define FLAME_HEAT_DAMAGE 90
#define FLAME_BURN_DAMAGE 150
#define FLAME_EMBERS      12    // the embers in the air from a flame on a burning body

struct Flame : Sprite {

    static Flame* add(IGame *game, Controller *owner, int jointIndex) {
        ASSERT(owner);

        if (owner && owner->getEntity().isLara()) {
            ((Character*)owner)->burn = true;
        }

        Flame *flame = (Flame*)game->addEntity(TR::Entity::FLAME, owner->getRoomIndex(), owner->pos);

        int jCount = owner->getModel()->mCount;

        if (flame) {
            flame->owner = owner;
            flame->jointIndex = jCount > 1 ? jointIndex : -1;
            ASSERT(flame->jointIndex < jCount);
        }
        return flame;
    }

    Controller *owner;
    int32 jointIndex;
    float sleepTime;
    EffectLight light;

    // the embers that fly up from a burning body and the smoke it gives off (they only show)
    struct Ember {
        vec3  pos, vel;
        float age, life;
    } embers[FLAME_EMBERS];
    int   emberCount;
    float emberTimer, smokeTimer;

    Flame(IGame *game, int entity) : Sprite(game, entity, false, Sprite::FRAME_ANIMATED), owner(NULL), jointIndex(0), sleepTime(0.0f), emberCount(0) {
        time = randf() * 3.0f;
        emberTimer = randf() * 0.3f;
        smokeTimer = randf() * 0.6f;
        activate();
    }

    void updateEmbers() {
        float dt = Core::deltaTime;
        for (int i = 0; i < emberCount; i++) {
            Ember &e = embers[i];
            e.age += dt;
            if (e.age >= e.life) {
                e = embers[--emberCount];
                i--;
                continue;
            }
            e.vel *= 1.0f - dt * 0.9f;
            e.vel.x += sinf(e.age * 9.0f + e.life * 20.0f) * 300.0f * dt; // they flutter
            e.pos += e.vel * dt;
        }

        if (jointIndex < 0) return; // a flame of a body only

        emberTimer -= dt;
        if (emberTimer <= 0.0f) {
            emberTimer = 0.12f + randf() * 0.3f;
            if (emberCount < FLAME_EMBERS) {
                Ember &e = embers[emberCount++];
                e.pos  = pos + vec3((randf() - 0.5f) * 80.0f, -150.0f - randf() * 100.0f, (randf() - 0.5f) * 80.0f); // at the tips of the flame
                e.vel  = vec3((randf() - 0.5f) * 120.0f, -(250.0f + randf() * 350.0f), (randf() - 0.5f) * 120.0f);
                e.age  = 0.0f;
                e.life = 0.6f + randf() * 0.6f;
            }
        }

    // the smoke from a few of the flames only, a column over the body
        if (jointIndex % 4 == 0 && level->extra.smoke != -1) {
            smokeTimer -= dt;
            if (smokeTimer <= 0.0f) {
                smokeTimer = 0.45f + randf() * 0.45f;
                Sprite *smoke = (Sprite*)game->addEntity(TR::Entity::SMOKE, getRoomIndex(), pos - vec3(0.0f, 200.0f, 0.0f));
                if (smoke)
                    smoke->velocity = vec3(randf() - 0.5f, -(4.0f + randf() * 3.0f), randf() - 0.5f);
            }
        }
    }

    // the flame and its embers are drawn by the flame shader where there is one (Level::renderFlames), sprites otherwise
    virtual void render(Frustum *frustum, MeshBuilder *mesh, Shader::Type type, bool caustics) {
        if (Core::lavaShader())
            return;

        Sprite::render(frustum, mesh, type, caustics);

        for (int i = 0; i < emberCount; i++) {
            const Ember &e = embers[i];
            float k = e.age / e.life;
            vec3  c = k < 0.4f ? vec3(1.0f, 0.85f, 0.45f).lerp(vec3(1.0f, 0.45f, 0.08f), k / 0.4f)
                               : vec3(1.0f, 0.45f, 0.08f).lerp(vec3(0.5f, 0.06f, 0.0f), (k - 0.4f) / 0.6f);
            c = c * ((1.0f - k * k) * 255.0f);
            vec3 p = e.pos - Core::viewPos.xyz();
            int  s = getSequence().sStart + (i % max(int(getSequence().sCount), 1)); // a speck of the flame sprite
            mesh->addDynSpriteSized(s, short3(int16(p.x), int16(p.y), int16(p.z)), int16(8.0f + 8.0f * (1.0f - k)), Color32(uint8(c.x), uint8(c.y), uint8(c.z), 255));
        }
    }

    virtual ~Flame() {
        light.off(level, entity);
    }

    virtual bool getSaveData(SaveEntity &data) {
        return false;
    }

    virtual void update() {
        Sprite::update();

        if (jointIndex <= 0) {
            game->playSound(TR::SND_FLAME, pos, Sound::PAN);
            light.set(level, entity, getRoomIndex(), pos, FIRE_LIGHT_COLOR * (0.8f + randf() * 0.2f), FIRE_LIGHT_RADIUS); // flickering
        }

        Character *lara = (Character*)((owner && owner->getEntity().isLara()) ? owner : game->getLara(pos));

        if (jointIndex > -1) {
            if (!lara->burn) {
                game->removeEntity(this);
                return;
            }

            pos = lara->getJoint(jointIndex).pos;
            if (jointIndex == 0)
                pos.y += 100.0f;

            updateEmbers();

            lara->hit(FLAME_BURN_DAMAGE * Core::deltaTime, this);
        } else 
            if (lara->health > 0.0f) {
                if (sleepTime > 0.0f)
                    sleepTime = max(0.0f, sleepTime - Core::deltaTime);

                if (sleepTime == 0.0f && !lara->burn && lara->collide(Sphere(pos, 600.0f))) {
                    lara->hit(FLAME_HEAT_DAMAGE * Core::deltaTime, this);

                    if (lara->collide(Sphere(pos, 300.0f))) {
                        Flame::add(game, lara, 0);
                        sleepTime = 3.0f; // stay inactive for 3 seconds
                    }
                }
            }
    }
};

struct TrapFlameEmitter : Controller {
    Flame *flame;

    TrapFlameEmitter(IGame *game, int entity) : Controller(game, entity), flame(NULL) {}

    void virtual update() {
        if (!isActive()) {
            if (flame) {
                Sound::stop(TR::SND_FLAME);
                game->removeEntity(flame);
                flame = NULL;
            }
            return;
        }

        if (flame) return;

        flame = Flame::add(game, this, -1);
    }
};


#define MUZZLE_FLASH_TIME   0.1f
#define FLASH_LIGHT_COLOR   vec4(0.6f, 0.5f, 0.1f, 1.0f / 3072.0f)

struct MuzzleFlash : Controller {
    Controller *owner;
    int        joint;
    int        lightIndex;
    bool       forward;     // along the joint's z (the guns of a vehicle), not its y (a gun in a hand)

    MuzzleFlash(IGame *game, int entity) : Controller(game, entity), owner(NULL), joint(0), lightIndex(-1), forward(false) {
        pos.z += (level->version & (TR::VER_TR2 | TR::VER_TR3)) ? 180.0f : 150.0f;
        activate();
        timer = 0.0f;
    }

    virtual bool getSaveData(SaveEntity &data) {
        return false;
    }

    virtual void update() {
        if (!owner) {
            game->removeEntity(this);
            return;
        }

        timer += Core::deltaTime;
        if (timer < MUZZLE_FLASH_TIME) {
            float intensity = clamp((MUZZLE_FLASH_TIME - timer) * 20.0f, EPS, 1.0f);

            vec4 lightPos   = vec4(owner->getJoint(joint).pos, 0);
            vec4 lightColor = FLASH_LIGHT_COLOR * vec4(intensity, intensity, intensity, 1.0f / sqrtf(intensity));

            if (lightIndex > -1) {
                ASSERT(lightIndex + 1 < MAX_LIGHTS);
                Core::lightPos[lightIndex]   = lightPos;
                Core::lightColor[lightIndex] = lightColor;
            } else {
                getRoom().addDynLight(owner->entity, lightPos, lightColor, true);
            }

        } else {
            
            if (lightIndex > -1) {
                ASSERT(lightIndex < MAX_LIGHTS);
                Core::lightPos[lightIndex]   = vec4(0);
                Core::lightColor[lightIndex] = vec4(0, 0, 0, 1);
            } else {
                getRoom().removeDynLight(owner->entity);
            }

            game->removeEntity(this);
        }
    }

    virtual void render(Frustum *frustum, MeshBuilder *mesh, Shader::Type type, bool caustics) {
        ASSERT(level->extra.muzzleFlash);
        ASSERT(owner);

        float alpha = min(1.0f, (0.1f - timer) * 20.0f);
        float lum   = 3.0f;
        Basis b = owner->getJoint(joint);
        b.w = 1.0f;
        if (!forward)
            b.rotate(quat(vec3(1, 0, 0), -PI * 0.5f));
        b.translate(pos);
        if (level->version & (TR::VER_TR2 | TR::VER_TR3))
            lum = alpha;

        game->setShader(Core::pass, Shader::FLASH, false, false);
        Core::setMaterial(lum * alpha, 0.0f, 0.0f, alpha);
        Core::setBasis(&b, 1);
        Core::setDepthWrite(false);
        mesh->renderModel(level->extra.muzzleFlash);
        Core::setDepthWrite(true);
    }
};

#define LAVA_PARTICLE_DAMAGE  10
#define LAVA_V_SPEED          -165
#define LAVA_H_SPEED          32
#define LAVA_EMITTER_RANGE    (1024 * 10)
#define LAVA_BLOB_COOL        1.6f    // seconds a blob takes to cool from yellow to dark red
#define LAVA_SPARK_LIFE       0.6f
#define LAVA_LIGHT_RADIUS     3072.0f

struct TrapLavaEmitter : Controller {

    struct Particle {
        vec3  pos;
        vec3  velocity;
        int16 roomIndex;
        int8  frame;
        bool  spark;    // a spark that only shows, it neither hurts nor bounces
        float age;

        void update(TR::Level *level, Controller *lara) {
            Controller::applyGravity(velocity.y);
            vec3 opos = pos;
            pos += velocity * (30.0f * Core::deltaTime);
            age += Core::deltaTime;

            if (spark) {
                TR::Room::Sector *sector = level->getSector(roomIndex, pos);
                if (age > LAVA_SPARK_LIFE || pos.y > level->getFloor(sector, pos))
                    frame = -1;
                return;
            }

            if (lara->collide(Sphere(pos, 0.0f))) {
                lara->hit(LAVA_PARTICLE_DAMAGE);
                frame = -1;
                return;
            }

            if (level->rooms[roomIndex].flags.water) {
                frame = -1;
                return;
            }

            TR::Room::Sector *sector = level->getSector(roomIndex, pos);
            float floor = level->getFloor(sector, pos);
            float ceiling = level->getCeiling(sector, pos);

            if (pos.y > floor || pos.y < ceiling) {
                vec3 n;

                if (pos.y - floor > 128) {
                    int ix = int(pos.x);
                    int iz = int(pos.z);
                    ix -= ix / 1024 * 1024 + 512;
                    iz -= iz / 1024 * 1024 + 512;

                    if (abs(ix) > abs(iz)) {
                        n = vec3(ix < 0 ? -1.0f : 1.0f, 0, 0);
                    } else {
                        n = vec3(0, 0, iz < 0 ? -1.0f : 1.0f);
                    }

                } else if (pos.y > floor) {
                    n = vec3(0, -1, 0);
                } else if (pos.y < ceiling) {
                    n = vec3(0,  1, 0);
                }

                velocity = velocity.reflect(n) * 0.5f;
                pos      = opos;
                frame--;
            }
        }
    };

    Array<Particle> particles;
    int             spriteIndex;
    EffectLight     light;
    float           flicker;

    TrapLavaEmitter(IGame *game, int entity) : Controller(game, entity), flicker(0.0f) {
        spriteIndex = level->getModelIndex(TR::Entity::LAVA_PARTICLE);
        if (spriteIndex) {
            level->spriteSequences[-(spriteIndex + 1)].transp = 2; // fix blending mode to additive
        }
        particles.capacity = 128;
    }

    void virtual update() {
        if (!spriteIndex)
            return;

        Controller *lara = game->getLara(pos);

        vec3 d = (lara->pos - pos).abs();

        if (isActive() && max(d.x, d.z) < LAVA_EMITTER_RANGE) {
            if (timer <= 0.0f) {
                vec2 d;
                sincos(PI * 2.0f * randf(), &d.x, &d.y);
                d *= randf() * LAVA_H_SPEED;

                Particle part;
                part.pos       = pos;
                part.velocity  = vec3(d.x, randf() * LAVA_V_SPEED, d.y);
                part.roomIndex = getRoomIndex();
                part.frame     = rand() % level->spriteSequences[-(spriteIndex + 1)].sCount;
                part.spark     = false;
                part.age       = 0.0f;
                particles.push(part);

            // sparks fly off with the blob, faster and lighter (they only show)
                for (int i = rand() % 3; i > 0; i--) {
                    vec2 s;
                    sincos(PI * 2.0f * randf(), &s.x, &s.y);
                    part.velocity = vec3(s.x * LAVA_H_SPEED * 2.0f * randf(), (0.5f + randf()) * LAVA_V_SPEED * 0.8f, s.y * LAVA_H_SPEED * 2.0f * randf());
                    part.spark    = true;
                    part.age      = randf() * LAVA_SPARK_LIFE * 0.3f;
                    particles.push(part);
                }

                game->playSound(TR::SND_LAVA, pos, Sound::PAN);
                timer += 1.0f / 30.0f;

            } else {
                timer -= Core::deltaTime;
            }
        }

        for (int i = 0; i < particles.length; i++) {
            particles[i].update(level, lara);
            if (particles[i].frame < 0) {
                particles.removeFast(i);
                i--;
            }
        }

    // the glow of the burst lights the pit, it flickers with the blobs in the air
        if (particles.length) {
            flicker += Core::deltaTime;
            float f = 0.75f + 0.15f * sinf(flicker * 13.0f) + 0.1f * sinf(flicker * 31.0f + 1.3f);
            float s = min(1.0f, particles.length / 24.0f) * f;
            light.set(level, entity, getRoomIndex(), pos - vec3(0.0f, 384.0f, 0.0f), vec3(1.0f, 0.42f, 0.1f) * s, LAVA_LIGHT_RADIUS);
        } else
            light.off(level, entity);
    }

    // the colour of a blob as it cools: white hot yellow, orange, dark red
    static Color32 blobColor(float age, float alpha) {
        float t = clamp(age / LAVA_BLOB_COOL, 0.0f, 1.0f);
        vec3 c = t < 0.35f ? vec3(1.0f, 0.9f, 0.55f).lerp(vec3(1.0f, 0.5f, 0.12f), t / 0.35f)
                           : vec3(1.0f, 0.5f, 0.12f).lerp(vec3(0.55f, 0.08f, 0.0f), (t - 0.35f) / 0.65f);
        c = c * (alpha * 255.0f);
        return Color32(uint8(clamp(int(c.x), 0, 255)), uint8(clamp(int(c.y), 0, 255)), uint8(clamp(int(c.z), 0, 255)), 255);
    }

    virtual void render(Frustum *frustum, MeshBuilder *mesh, Shader::Type type, bool caustics) {
    // (additive) a blob is a hot core in a soft glow with a trail along its way that fades, a spark a small streak
        int sStart = level->spriteSequences[-(spriteIndex + 1)].sStart;
        for (int i = 0; i < particles.length; i++) {
            Particle &part = particles[i];

            int   sprite = sStart + part.frame;
            const TR::TextureInfo &tex = level->spriteTextures[sprite];
            float size   = float(max(tex.r - tex.l, tex.b - tex.t)) * 0.5f;
            if (size < 8.0f) size = 8.0f;

            vec3  p      = part.pos - Core::viewPos.xyz();
            vec3  step   = part.velocity * -1.2f; // back along the way, a frame and a bit

            if (part.spark) {
                float a = 1.0f - part.age / LAVA_SPARK_LIFE;
                for (int j = 0; j < 3; j++) {
                    vec3 q = p + step * float(j) * 0.7f;
                    mesh->addDynSpriteSized(sprite, short3(int16(q.x), int16(q.y), int16(q.z)), int16(size * 0.35f * (1.0f - j * 0.25f)), blobColor(part.age * 0.5f, a * (1.0f - j * 0.3f)));
                }
                continue;
            }

            for (int j = 3; j > 0; j--) { // the trail
                vec3 q = p + step * float(j);
                mesh->addDynSpriteSized(sprite, short3(int16(q.x), int16(q.y), int16(q.z)), int16(size * (1.0f - j * 0.18f)), blobColor(part.age + j * 0.15f, 0.45f - j * 0.1f));
            }
            mesh->addDynSpriteSized(sprite, short3(int16(p.x), int16(p.y), int16(p.z)), int16(size * 3.0f), blobColor(part.age, 0.3f));  // the glow
            mesh->addDynSpriteSized(sprite, short3(int16(p.x), int16(p.y), int16(p.z)), int16(size), blobColor(part.age, 1.0f));         // the core
        }
    }
};



#define BOULDER_DAMAGE_GROUND 1000
#define SND_TR2_ROLLING_BALL  147
#define SND_TR2_ROLLING_BALL_2_ROLL 222
#define SND_TR2_ROLLING_BALL_2_STOP 223
#define SND_TR2_ROLLING_DRUMS_ROLL  227
#define SND_TR2_ROLLING_DRUMS_STOP  228
#define BOULDER_DAMAGE_AIR    100

struct TrapBoulder : Controller {
    enum {
        STATE_FALL,
        STATE_ROLL,
        STATE_STOP_TR2, // the second TR2 ball settling
    };

    vec3 velocity;

    TrapBoulder(IGame *game, int entity) : Controller(game, entity), velocity(0) {
        flags.unused = false;
    }

    // the second TR2 ball and the rolling drums settle where they stop, the first ball just stays
    bool settles() {
        return getEntity().type != TR::Entity::TRAP_BOULDER;
    }

    int soundRoll() {
        switch (getEntity().type) {
            case TR::Entity::TRAP_BOULDERS : return SND_TR2_ROLLING_BALL_2_ROLL;
            case TR::Entity::ROLLING_DRUMS : return SND_TR2_ROLLING_DRUMS_ROLL;
            default                        : return SND_TR2_ROLLING_BALL; // the Opera House has no such sample
        }
    }

    int soundStop() {
        switch (getEntity().type) {
            case TR::Entity::TRAP_BOULDERS : return SND_TR2_ROLLING_BALL_2_STOP;
            case TR::Entity::ROLLING_DRUMS : return SND_TR2_ROLLING_DRUMS_STOP;
            default                        : return -1;
        }
    }

    // a TR2 ball stops at a wall, a raised floor or a gap too low for it ahead (as the original tests it)
    bool testStopTR2() {
        float dist = settles() ? 1024.0f : 384.0f;
        vec3 v = pos + getDir() * dist;

        TR::Level::FloorInfo info;
        getFloorInfo(getRoomIndex(), v, info);
        if (info.floor == -32512.0f || info.floor < pos.y)
            return true;

        Box box = getBoundingBoxLocal();
        int h = (int(fabsf(box.max.y - box.min.y)) + 255) / 256 * 256;
        if (h > 768)
            h = h / 768 * 768;

        TR::Level::FloorInfo up;
        getFloorInfo(getRoomIndex(), v - vec3(0.0f, float(h), 0.0f), up);
        if (up.ceiling == -32512.0f || (up.ceiling > pos.y && velocity.y == 0.0f))
            return true;

        return fabsf(up.ceiling - info.floor) < h;
    }

    // a ball that has stopped drops straight down when its floor goes away under it (an addition of TRX, #4808, the
    // originals leave it in the air): the ice of Tibetan Foothills breaks under the snowballs (a flip of the rooms).
    // The stop animation of a TR2 ball ends with the kill command, so the flip wakes it up (Level::updateBodies)
    bool isFloorGone() {
        if (!flags.unused) return false;
        TR::Level::FloorInfo info;
        getFloorInfo(getRoomIndex(), pos, info);
        return info.floor != TR::NO_FLOOR * 256.0f && pos.y < info.floor;
    }

    // true while it falls
    bool fallStopped() {
        TR::Level::FloorInfo info;
        getFloorInfo(getRoomIndex(), pos, info);
        if (info.floor == TR::NO_FLOOR * 256.0f || pos.y >= info.floor) {
            velocity.y = 0.0f;
            return false;
        }

        if (velocity.y == 0.0f)
            velocity.y = -10.0f; // the hop the original gives a ball that sets off falling
        applyGravity(velocity.y);
        pos.y += velocity.y * (30.0f * Core::deltaTime);

        bool falls = true;
        if (pos.y >= info.floor) {
            pos.y      = info.floor;
            velocity.y = 0.0f;
            falls      = false;
        }
        level->getSector(roomIndex, pos); // the room under it
        return falls;
    }

    virtual void update() {
        if (flags.active == 0) {
            if (state != STATE_FALL) {
                const TR::Entity &e = getEntity();
                roomIndex = e.room;
                pos       = vec3(float(e.x), float(e.y), float(e.z));
                velocity  = vec3(0.0f);
                animation.setAnim(0);
                flags.unused = false;
                Sound::stop(soundRoll(), &pos);
            }
            return;
        }

        bool tr2 = (level->version & TR::VER_TR2) != 0;

        if (flags.unused) {
            if (fallStopped())
                return; // the stop animation (with its kill command) goes on once it lands
            if (tr2 && settles()) { // it plays its stop animation and stays
                animation.setState(STATE_STOP_TR2);
                updateAnimation(true);
            }
            return;
        }

        vec3 p = pos;
        vec3 old = pos;

        if (tr2) { // the TR2 balls roll with the speed of their animation whatever their state
            vec3 d = getDir() * animation.getSpeed();
            velocity.x = d.x;
            velocity.z = d.z;
        }

        if (velocity.y != 0.0f) {
            applyGravity(velocity.y);
        }
        pos += velocity * (30.0f * Core::deltaTime);

        TR::Room::Sector *sector = level->getSector(roomIndex, pos);

        // the floor with the bridges, a ball rolls down a tilting bridge instead of dropping through it (as the
        // original takes its height)
        TR::Level::FloorInfo info;
        getFloorInfo(getRoomIndex(), pos, info);
        p.y = info.floor;

        if (pos.y < p.y) {
            if (velocity.y == 0.0f) {
                velocity.y = -10.0f;
            }
        } else if (!tr2) {
            if (state != STATE_ROLL) {
                animation.setState(STATE_ROLL);
                velocity = getDir() * animation.getSpeed();
            }
        } else if (state == STATE_FALL) { // set off on the floor it starts to roll (as the original does)
            animation.setState(STATE_ROLL);
        }

        if (pos.y >= p.y - 256.0f) {
            velocity.y = 0.0f;
            pos.y      = p.y;
            if (tr2)
                game->playSound(soundRoll(), pos, Sound::PAN | Sound::UNIQUE);
        }

        if (tr2) {
            if (testStopTR2()) {
                flags.unused = true;
                pos.x    = old.x;
                pos.z    = old.z;
                velocity = vec3(0.0f);
                Sound::stop(soundRoll(), &pos); // the roll sample loops, so it plays on until stopped
                if (soundStop() != -1)
                    game->playSound(soundStop(), pos, Sound::PAN);
                if (settles())
                    animation.setState(STATE_STOP_TR2);
            }
        } else {
            int16 roomIdx = this->roomIndex;
            vec3 v = pos + getDir() * 512.0f;
            sector = level->getSector(roomIdx, v);
            if (pos.y > level->getFloor(sector, v)) {
                flags.unused = true;

                pos.x = int(pos.x / 1024.0f) * 1024.0f + 512.0f;
                pos.z = int(pos.z / 1024.0f) * 1024.0f + 512.0f;
                sector = level->getSector(roomIndex, pos);
                pos.y = level->getFloor(sector, pos);
            }
        }

        game->checkTrigger(this, true);
        updateAnimation(true);

        Character *lara = (Character*)game->getLara(pos);
        if (lara->health > 0.0f && touchLara(this, lara)) {
            if (lara->stand == Character::STAND_GROUND)
                lara->hit(BOULDER_DAMAGE_GROUND, this, TR::HIT_BOULDER);
            if (lara->stand == Character::STAND_AIR)
                lara->hit(BOULDER_DAMAGE_AIR * 30.0f * Core::deltaTime, this);
        }
    }
};

// not a trigger
struct Block : Controller {
    enum {
        STATE_STAND = 1,
        STATE_PUSH,
        STATE_PULL,
    };

    float velocity;

    Block(IGame *game, int entity) : Controller(game, entity), velocity(0.0f) {
        updateFloor(true);
    }

    virtual void setSaveData(const SaveEntity &data) {
        updateFloor(false);
        Controller::setSaveData(data);
        if (state == STATE_STAND)
            updateFloor(true);
    }

    void updateFloor(bool rise) {
        TR::Level::FloorInfo info;
        getFloorInfo(getRoomIndex(), pos, info);
        if (info.roomNext != 0xFF)
            roomIndex = info.roomNext;
        int dx, dz;
        TR::Room::Sector &s = level->getSector(getRoomIndex(), int(pos.x), int(pos.z), dx, dz);
        s.floor += rise ? -4 : 4;
    }

    bool doMove(bool push) {
    // check floor height of next floor
        vec3 dir = getDir() * (push ? 1024.0f : -1024.0f);
        TR::Level::FloorInfo info;

        vec3 p = pos + dir;
        getFloorInfo(getRoomIndex(), p, info);

        if ((info.slantX | info.slantZ) || info.floor != pos.y || info.floor - info.ceiling < 1024)
            return false;

        // check for trapdoor
        int px = int(p.x) / 1024;
        int pz = int(p.z) / 1024;
        for (int i = 0; i < info.trigCmdCount; i++) {
            if (info.trigCmd[i].action == TR::Action::ACTIVATE) {
                TR::Entity &e = level->entities[info.trigCmd[i].args];
                vec3 objPos = ((Controller*)e.controller)->pos;
                if ((e.type == TR::Entity::TRAP_DOOR_1 || e.type == TR::Entity::TRAP_DOOR_2) && px == int(objPos.x) / 1024 && pz == int(objPos.z) / 1024)
                    return false;
            } else if (info.trigCmd[i].action == TR::Action::CAMERA_SWITCH)
                i++; // skip camera switch delay info
        }

        // check Laras destination position
        if (!push) {
            dir = getDir() * (-2048.0f);
            getFloorInfo(getRoomIndex(), pos + dir, info);
            if ((info.slantX | info.slantZ) || info.floor != pos.y || info.floor - info.ceiling < 1024)
                return false;
        }

        if (!animation.setState(push ? STATE_PUSH : STATE_PULL))
            return false;
        updateFloor(false);
        activate();
        return true;
    }

    virtual void update() {
        TR::Level::FloorInfo info;        
        getFloorInfo(getRoomIndex(), pos, info);

        if (pos.y < info.floor) {
            if (info.roomBelow != TR::NO_ROOM)
                roomIndex = info.roomBelow;

            applyGravity(velocity);
            pos.y += velocity * (30.0f * Core::deltaTime);

            if (pos.y >= info.floor) {
                velocity = 0.0f;
                pos.y    = info.floor;
                game->setEffect(this, TR::Effect::FLOOR_SHAKE);
                game->playSound(TR::SND_BOULDER, pos, Sound::PAN);
                deactivate(true);
                updateFloor(true);
            }
        } else {
            if (state == STATE_STAND) return;
            updateAnimation(true);
            if (state == STATE_STAND) {
                updateFloor(true);
                deactivate();
                game->checkTrigger(this, true);
            }
        }
        updateLights();
    }
};


struct Door : Controller {
    enum {
        STATE_CLOSE,
        STATE_OPEN,
    };

    struct BlockInfo {
        int              roomIndex[2];
        int              sectorIndex[2];
        TR::Room::Sector sectors[2];

        BlockInfo() {}
        BlockInfo(TR::Level *level, int room, int nx, int nz, int x, int z, bool flip) {
        // front
            roomIndex[0] = room;
            roomIndex[1] = TR::NO_ROOM;

            if (roomIndex[0] == TR::NO_ROOM)
                return;
            if (flip && level->rooms[roomIndex[0]].alternateRoom != -1)
                roomIndex[0] = level->rooms[roomIndex[0]].alternateRoom;

            sectors[0] = level->getSector(roomIndex[0], x, z, sectorIndex[0]);

        // behind
            roomIndex[1] = level->getNextRoom(&sectors[0]);

            if (roomIndex[1] == TR::NO_ROOM)
                return;
            if (flip && level->rooms[roomIndex[1]].alternateRoom != -1)
                roomIndex[1] = level->rooms[roomIndex[1]].alternateRoom;

            sectors[1] = level->getSector(roomIndex[1], nx, nz, sectorIndex[1]);
        }

        void set(TR::Level *level) {
            for (int i = 0; i < 2; i++)
                if (roomIndex[i] != TR::NO_ROOM) {
                    TR::Room::Sector &s = level->rooms[roomIndex[i]].sectors[sectorIndex[i]];
                    s.floorIndex = 0;
                    s.boxIndex   = TR::NO_BOX;
                    s.roomBelow  = TR::NO_ROOM;
                    s.floor      = TR::NO_FLOOR;
                    s.roomAbove  = TR::NO_ROOM;
                    s.ceiling    = TR::NO_FLOOR;

                    if (sectors[i].boxIndex != TR::NO_BOX) {
                        ASSERT(sectors[i].boxIndex < level->boxesCount);
                        TR::Box &box = level->boxes[sectors[i].boxIndex];
                        if (box.overlap.blockable)
                            box.overlap.block = true;
                    }
                }
        }

        void reset(TR::Level *level) {
            for (int i = 0; i < 2; i++)
                if (roomIndex[i] != TR::NO_ROOM) {
                    level->rooms[roomIndex[i]].sectors[sectorIndex[i]] = sectors[i];
                    if (sectors[i].boxIndex != TR::NO_BOX) {
                        TR::Box &box = level->boxes[sectors[i].boxIndex];
                        if (box.overlap.blockable)
                            box.overlap.block = false;
                    }
                }
        }

    } block[2];

    Door(IGame *game, int entity) : Controller(game, entity) {
        vec3 p = pos - getDir() * 1024.0f;
        block[0] = BlockInfo(level, getRoomIndex(), int(pos.x), int(pos.z), int(p.x), int(p.z), false);
        block[1] = BlockInfo(level, getRoomIndex(), int(pos.x), int(pos.z), int(p.x), int(p.z), true);
        updateBlock(false);
    }

    void updateBlock(bool open) {
        if (open) {
            block[0].reset(level);
            block[1].reset(level);
        } else {
            block[0].set(level);
            block[1].set(level);
        }
    }
    
    virtual void update() {
        updateAnimation(true);
        int targetState = isActive() ? STATE_OPEN : STATE_CLOSE;

        if (state == targetState)
            updateBlock(targetState == STATE_OPEN);
        else
            animation.setState(targetState);
    }
};

struct TrapDoor : Controller {
    enum {
        STATE_CLOSE,
        STATE_OPEN,
    };

    TrapDoor(IGame *game, int entity) : Controller(game, entity) {}
    
    virtual bool isCollider() {
        int targetState = isActive(false) ? STATE_OPEN : STATE_CLOSE;
        return state == targetState && state == STATE_CLOSE;
    }

    virtual void update() {
        updateAnimation(true);
        int targetState = isActive() ? STATE_OPEN : STATE_CLOSE;

        if (state != targetState)
            animation.setState(targetState);
    }
};


struct TrapFloor : Controller {
    enum {
        STATE_STATIC,
        STATE_SHAKE,
        STATE_FALL,
        STATE_DOWN,
    };

    float velocity;
    int   origin; // the top of it, from the first frame (the TR2 boards differ from the TR1 floors)

    TrapFloor(IGame *game, int entity) : Controller(game, entity), velocity(0) {
        TR::AnimFrame *frame = animation.getFrame(&level->anims[getModel()->animation], 0);
        origin = (frame->pos.y < 0 ? -(-frame->pos.y + 128) : (frame->pos.y + 128)) / 256 * 256;
    }

    virtual int floorOrigin() {
        return origin;
    }

    virtual bool isCollider() {
        return state == STATE_STATIC || state == STATE_SHAKE;
    }

    virtual bool activate() {
        if (state != STATE_STATIC) return false;
        vec3 &p = game->getLara(pos)->pos;
        if (fabsf(p.y - (pos.y + origin)) <= 8 && Controller::activate()) {
            animation.setState(STATE_SHAKE);
            return true;
        }
        return false;
    }

    virtual void update() {
        updateAnimation(true);
        if (state == STATE_FALL) {
            applyGravity(velocity);
            pos.y += velocity * (30.0f * Core::deltaTime);

            TR::Level::FloorInfo info;
            getFloorInfo(getRoomIndex(), pos, info);

            if (pos.y > info.roomFloor && info.roomBelow != 0xFF)
                roomIndex = info.roomBelow;

            if (pos.y > info.floor) {
                pos.y = (float)info.floor;
                animation.setState(STATE_DOWN);
            }
        }
    }
};

struct Bridge : Controller {
    Bridge(IGame *game, int entity) : Controller(game, entity) {}
};

struct Drawbridge : Controller {
    enum {
        STATE_UP,
        STATE_DOWN,
    };

    Drawbridge(IGame *game, int entity) : Controller(game, entity) {}

    virtual bool isCollider() {
        return flags.active != TR::ACTIVE;
    }

    virtual void update() {
        updateAnimation(true);
        animation.setState(isActive() ? STATE_DOWN : STATE_UP);
    }
};


#define CRYSTAL_LIGHT_RADIUS 1024.0f
#define CRYSTAL_LIGHT_COLOR  vec4(0.1f, 0.1f, 3.0f, 1.0f / CRYSTAL_LIGHT_RADIUS)
#define CRYSTAL_CUBEMAP_SIZE 64

struct Crystal : Controller {
    Texture *environment;

    Crystal(IGame *game, int entity) : Controller(game, entity), environment(NULL) {
        activate();
    }

    virtual ~Crystal() {
        delete environment;
    }

    virtual bool isCollider() {
        return !flags.invisible;
    }

    virtual void deactivate(bool removeFromList = false) {
        flags.invisible = true;
        Controller::deactivate(removeFromList);
        getRoom().removeDynLight(entity);
    }

    virtual void update() {
        updateAnimation(false);
        vec3 lightPos = getJoint(0).pos - vec3(0, 256, 0);
        getRoom().addDynLight(entity, vec4(lightPos, 0.0f), CRYSTAL_LIGHT_COLOR);
    }

    virtual void render(Frustum *frustum, MeshBuilder *mesh, Shader::Type type, bool caustics) {
        Core::setMaterial(0.5, 0.5, 3.0, 0.0f); // 0.0f - use vertex normal as texcoord
        GAPI::Texture *dtex = Core::active.textures[sDiffuse];
        if (environment) {
            environment->bind(sDiffuse);
        } else {
            Core::whiteCube->bind(sDiffuse);
        }
        Controller::render(frustum, mesh, type, caustics);
        if (dtex) dtex->bind(sDiffuse);
    }

    void bake() {
        ASSERT(!environment);

        uint32 opt = OPT_CUBEMAP | OPT_TARGET;
        #ifdef USE_CUBEMAP_MIPS
            opt |= OPT_MIPMAPS;
        #endif

        environment = new Texture(CRYSTAL_CUBEMAP_SIZE, CRYSTAL_CUBEMAP_SIZE, 1, FMT_RGB16, opt);
        game->renderEnvironment(getRoomIndex(), pos - vec3(0, 512, 0), &environment);
        if (opt & OPT_MIPMAPS) {
            environment->generateMipMap();
        }
    }
};


#define CRYSTAL_PICKUP_LIGHT_COLOR  vec4(0.1f, 0.5f, 0.1f, 1.0f / CRYSTAL_LIGHT_RADIUS)

#define CRYSTAL_PICKUP_RANGE   256.0f
#define CRYSTAL_PICKUP_HEALTH  500.0f
#define CRYSTAL_PICKUP_MAX     1000.0f

struct CrystalPickup : Controller {

    CrystalPickup(IGame *game, int entity) : Controller(game, entity) {
        activate();
    }

    virtual void deactivate(bool removeFromList = false) {
        Controller::deactivate(removeFromList);
        getRoom().removeDynLight(entity);
    }

    virtual void update() {
        updateAnimation(false);
        vec3 lightPos = getJoint(0).pos;
        getRoom().addDynLight(entity, vec4(lightPos, 0.0f), CRYSTAL_PICKUP_LIGHT_COLOR);

        // the PC version has no saves at crystals: touched it gives health and is gone
        if (level->version == TR::VER_TR3_PC) {
            Character *lara = (Character*)game->getLara(pos);
            vec3 d = lara->pos - pos;
            if (lara->health > 0.0f && fabsf(d.x) < CRYSTAL_PICKUP_RANGE && fabsf(d.z) < CRYSTAL_PICKUP_RANGE && d.y > -CRYSTAL_PICKUP_RANGE && d.y < 768.0f + CRYSTAL_PICKUP_RANGE) {
                lara->health = min(CRYSTAL_PICKUP_MAX, lara->health + CRYSTAL_PICKUP_HEALTH);
                flags.invisible = true;
                deactivate(true);
            }
        }
    }
};

#define SPIKES_DAMAGE_FALL      1000
#define SPIKES_DAMAGE_RUN       15
#define SPIKES_RANGE            1024

struct TrapSpikes : Controller {
    TrapSpikes(IGame *game, int entity) : Controller(game, entity) {
        activate();
    }

    virtual void update() {
        if (getRoom().flags.water)
            return;

        Character *lara = (Character*)game->getLara(pos);
        if (lara->health <= 0.0f) return;

        if (!checkRange(lara, SPIKES_RANGE) || !touchLara(this, lara))
            return;

        if (lara->stand != Character::STAND_AIR || lara->velocity.y <= 0.0f || (pos.y - lara->pos.y) > 256.0f) {
            if (lara->speed < 30.0f)
                return;
            lara->hit(SPIKES_DAMAGE_RUN * 30.0f * Core::deltaTime, this, TR::HIT_SPIKES);
        } else
            lara->hit(SPIKES_DAMAGE_FALL, this, TR::HIT_SPIKES);
    }
};

struct TrapCeiling : Controller {
    enum {
        STATE_STATIC,
        STATE_FALL,
        STATE_DOWN,
    };

    float velocity;

    TrapCeiling(IGame *game, int entity) : Controller(game, entity), velocity(0) {}

    virtual void update() {
        updateAnimation(true);

        if (state == STATE_STATIC)
            animation.setState(STATE_FALL);
       
        if (state == STATE_FALL) {
            applyGravity(velocity);
            pos.y += velocity * (30.0f * Core::deltaTime);

            TR::Level::FloorInfo info;
            getFloorInfo(getRoomIndex(), pos, info);

            if (pos.y > info.roomFloor && info.roomBelow != 0xFF)
                roomIndex = info.roomBelow;

            if (pos.y > info.floor) {
                pos.y = (float)info.floor;
                animation.setState(STATE_DOWN);
            }

            Controller *lara = game->getLara(pos);
            if (touchLara(this, lara))
                lara->hit(1000);
        }
    }
};

#define SLAM_DAMAGE 400

struct TrapSlam : Controller {
    enum {
        STATE_OPEN,
        STATE_SLAM,
    };
    
    bool bite;

    TrapSlam(IGame *game, int entity) : Controller(game, entity), bite(false) {}

    virtual void update() {
        if (isActive()) {
            animation.setState(STATE_SLAM);

            if (animation.frameIndex >= 20)
                bite = false;

            Character *lara = (Character*)game->getLara(pos);
            if (animation.state == STATE_SLAM && !bite && touchLara(this, lara)) {
                lara->hit(SLAM_DAMAGE, this, TR::HIT_SLAM);
                bite = true;
            }

        } else
            animation.setState(STATE_OPEN);

        updateAnimation(true);
    }
};


struct KeyHole : Controller {
    KeyHole(IGame *game, int entity) : Controller(game, entity) {}

    virtual bool activate() {
        if (!Controller::activate()) return false;
        flags.active = TR::ACTIVE;
        deactivate();
        return true;
    }

    virtual const TR::Model* getModel() {
        if (getEntity().isPuzzleHole() && flags.active == TR::ACTIVE) {
            int doneIdx = TR::Level::convToInv(TR::Entity::getItemForHole(getEntity().type)) - TR::Entity::INV_PUZZLE_1;
            return &level->models[level->extra.puzzleDone[doneIdx]];
        }
        return Controller::getModel();
    }

    virtual void update() {}
};


struct Earthquake : Controller {
    float timer;

    Earthquake(IGame *game, int entity) : Controller(game, entity), timer(0.0f) {}

    virtual void update() {
        if (!isActive()) return;

        if (timer < (1.0f / 30.0f)){
            timer += Core::deltaTime;
            return;
        }

        float p = randf();
        if (p < 0.001f) {
            game->playSound(TR::SND_STOMP);
            game->shakeCamera(1.0f);
        } else if (p < 0.04f) {
            game->playSound(TR::SND_BOULDER);
            game->shakeCamera(0.3f);
        }

        timer = 0.0f;
    }
};


struct Waterfall : Controller {
    #define SPLASH_TIMESTEP (1.0f / 30.0f)

    float timer;

    Waterfall(IGame *game, int entity) : Controller(game, entity), timer(0.0f) {}

    virtual void update() {
        if (getRoom().alternateRoom != -1 && level->state.flags.flipped) // room is flipped
            return;

        vec3 delta = (game->getLara(pos)->pos - pos) * (1.0f / 1024.0f);
        if (delta.length2() > 100.0f)
            return;

        timer -= Core::deltaTime;
        if (timer > 0.0f) return;
        timer += SPLASH_TIMESTEP * (1.0f + randf() * 0.25f);

        float dropRadius   = randf() * 128.0f + 128.0f;
        float dropStrength = randf() * 0.1f + 0.05f;

        vec2 p = (vec2(randf(), randf()) * 2.0f - 1.0f) * (512.0f - dropRadius);
        vec3 dropPos = pos + vec3(p.x, 0.0f, p.y);
        game->waterDrop(dropPos, dropRadius, dropStrength);
        if (level->extra.waterSplash > -1) {
        // the spray thrown up and out from where the water falls
            Sprite *splash = (Sprite*)game->addEntity(TR::Entity::WATER_SPLASH, getRoomIndex(), dropPos);
            if (splash) {
                vec2 out = p.length2() > 1.0f ? p.normal() : vec2(0.0f, 1.0f);
                float side = randf() * 4.0f + 2.0f;
                splash->velocity = vec3(out.x * side, -(randf() * 6.0f + 4.0f), out.y * side);
            }
        }
    } 

    #undef SPLASH_TIMESTEP
};

struct Bubble : Sprite {
    float speed;

    Bubble(IGame *game, int entity) : Sprite(game, entity, true, Sprite::FRAME_RANDOM) {
        speed = (10.0f + randf() * 6.0f) * 30.0f;
    // get water height => bubble life time
        int dx, dz;
        int room = getRoomIndex();
        int h = int(pos.y);
        while (room != TR::NO_ROOM && level->rooms[room].flags.water) {
            TR::Room::Sector &s = level->getSector(room, int(pos.x), int(pos.z), dx, dz);
            h = s.ceiling * 256;
            room = s.roomAbove;
        }
        if (pos.y < h)
            time = 1.0f / SPRITE_FPS;
        else
            time -= (pos.y - h) / speed - (1.0f / SPRITE_FPS);
        activate();
    }

    virtual ~Bubble() {
        game->waterDrop(pos, 64.0f, 0.01f);
    }

    virtual bool getSaveData(SaveEntity &data) {
        return false;
    }

    virtual void update() {
        pos.y -= speed * Core::deltaTime;
        angle.x += 30.0f * 13.0f * DEG2RAD * Core::deltaTime;
        angle.y += 30.0f *  9.0f * DEG2RAD * Core::deltaTime;
        pos.x += sinf(angle.y) * (11.0f * 30.0f * Core::deltaTime);
        pos.z += cosf(angle.x) * (8.0f  * 30.0f * Core::deltaTime);
        Sprite::update();
    }
};


struct Explosion : Sprite {
    float volume; // of the explosion sound
    bool  sounded;

    EffectLight light;

    Explosion(IGame *game, int entity) : Sprite(game, entity, true, Sprite::FRAME_ANIMATED), volume(1.0f), sounded(false) {
        level->spriteSequences[-(getEntity().modelIndex + 1)].transp = 2; // fix blending mode to additive
    }

    virtual ~Explosion() {
        light.off(level, entity);
    }

    virtual bool getSaveData(SaveEntity &data) {
        return false;
    }

    virtual void update() {
        if (!sounded) {
            sounded = true;
            Sound::Sample *sample = game->playSound((level->version & TR::VER_TR1) ? TR::SND_EXPLOSION : 105, pos, Sound::PAN); // TR2+ explosion sound id
            if (sample)
                sample->volume *= volume;
        }
        float fade = 1.0f - clamp(time * SPRITE_FPS / max(1, int(getSequence().sCount)), 0.0f, 1.0f); // it flashes and fades out
        light.set(level, entity, getRoomIndex(), pos, FIRE_LIGHT_COLOR * 1.5f * fade, EXPLOSION_LIGHT_RADIUS);
        Sprite::update();
    }
};


#define STONE_ITEM_LIGHT_RADIUS 2048.0f

struct StoneItem : Controller {
    float phase;

    StoneItem(IGame *game, int entity) : Controller(game, entity), phase(0) {
        activate();
    }

    virtual void deactivate(bool removeFromList = false) {
        Controller::deactivate(removeFromList);
        getRoom().removeDynLight(entity);
    }

    virtual void update() {
        updateAnimation(false);
        
        angle.y += Core::deltaTime * 2.0f;
        phase += Core::deltaTime;
        float s = 0.3f + (sinf(phase * PI2) * 0.5f + 0.5f) * 0.7f;

        vec4 lightColor(0.1f * s, 1.0f * s, 1.0f * s, 1.0f / STONE_ITEM_LIGHT_RADIUS);
        vec3 lightPos = getJoint(0).pos;

        getRoom().addDynLight(entity, vec4(lightPos, 0.0f), lightColor);
    }
};

#define CENTAUR_BULLET_DIST   SQR(1024.0f)
#define CENTAUR_BULLET_DAMAGE 100.0f
#define MUTANT_BULLET_DAMAGE  30.0f
#define HARPOON_WTF_DAMAGE    50.0f
#define HARPOON_WTF_PITCH     (-67.5f * DEG2RAD) // the lowest it dips to out of the water
#define SND_TR2_PROJECTILE_HIT 258
#define SND_TR2_CRUNCH_1      317

struct Bullet : Controller {
    vec3 velocity;

    Bullet(IGame *game, int entity) : Controller(game, entity) {
        velocity  = vec3(200.0f) * 30.0f;
        intensity = intensityf(4096);
        activate();
    }

    virtual bool getSaveData(SaveEntity &data) {
        return false;
    }

    void setAngle(const vec3 &ang) {
        angle = ang;
        float speed;
        switch (getEntity().type) {
            case TR::Entity::NATLA_BULLET   : speed = 220.0f; break;
            case TR::Entity::MUTANT_BULLET  : speed = 250.0f; break;
            case TR::Entity::CENTAUR_BULLET : speed = 220.0f; break;
            case TR::Entity::HARPOON_WTF    :
            case TR::Entity::MISSILE_KNIFE  : speed = 150.0f; break;
            default : speed = 200.0f;
        }
        velocity = getDir() * (speed * 30.0f);
    }

    virtual void update() {
        bool harpoon = getEntity().type == TR::Entity::HARPOON_WTF;
        if (harpoon) { // the harpoon of a diver dips out of the water, it leaves bubbles in it
            if (!getRoom().flags.water) {
                if (angle.x > HARPOON_WTF_PITCH) {
                    angle.x = max(HARPOON_WTF_PITCH, angle.x - DEG2RAD * 30.0f * Core::deltaTime);
                    velocity = getDir() * velocity.length();
                }
            } else if (randf() < 0.2f)
                game->addEntity(TR::Entity::BUBBLE, getRoomIndex(), pos);
        }

        if (getEntity().type == TR::Entity::MISSILE_KNIFE) // a thrown knife spins
            angle.z += DEG2RAD * 30.0f * 30.0f * Core::deltaTime;

        //getRoom().removeDynLight(entity);
        pos = pos + velocity * Core::deltaTime;

        level->getSector(roomIndex, pos);

        Controller::update();
        //getRoom().addDynLight(entity, pos, vec4(1, 1, 0, 1.0f / 1024.0f));

        bool directHit = false;

        Character *lara = (Character*)game->getLara(pos);
        if (!collide(lara)) {
            TR::Level::FloorInfo info;
            getFloorInfo(getRoomIndex(), pos, info);
            if (!(pos.y > info.floor || pos.y < info.ceiling || !insideRoom(pos, getRoomIndex())))
                return;
        } else
            directHit = true;

        //getRoom().removeDynLight(entity);
        switch (getEntity().type) {
            case TR::Entity::CENTAUR_BULLET :
                if (directHit) {
                    lara->hit(CENTAUR_BULLET_DAMAGE);
                } else {
                    for (int i = 0; i < 2; i++) {
                        Controller *lara = game->getLara(i);
                        if (!lara) continue;

                        float dist = (pos - lara->pos).length2();
                        if (dist < CENTAUR_BULLET_DIST)
                            lara->hit(CENTAUR_BULLET_DAMAGE * (CENTAUR_BULLET_DIST - dist) / CENTAUR_BULLET_DIST);
                    }
                }

                game->addEntity(TR::Entity::EXPLOSION, getRoomIndex(), pos);
                break;
            case TR::Entity::MUTANT_BULLET  :
                if (directHit)
                    lara->hit(MUTANT_BULLET_DAMAGE);
                else
                    game->getLara()->addRicochet(pos - getDir() * 64.0f, true);
                break;
            case TR::Entity::HARPOON_WTF    :
            case TR::Entity::MISSILE_KNIFE  :
                if (directHit) {
                    lara->hit(HARPOON_WTF_DAMAGE);
                    game->addEntity(TR::Entity::BLOOD, getRoomIndex(), pos);
                    game->playSound(SND_TR2_CRUNCH_1, pos, Sound::PAN);
                } else {
                    game->getLara()->addRicochet(pos - getDir() * 64.0f, false);
                    game->playSound(SND_TR2_PROJECTILE_HIT, pos, Sound::PAN);
                }
                game->removeEntity(this);
                return;
            default : ;
        }

        if (directHit)
            game->playSound(lara->stand == Character::STAND_UNDERWATER ? TR::SND_HIT_UNDERWATER : TR::SND_HIT, lara->pos, Sound::PAN);

        game->removeEntity(this);
    }
};

#include "objects_tr1.h"
#include "objects_tr2.h"
#include "objects_tr3.h"

#endif
