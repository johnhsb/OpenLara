#ifndef H_TRIGGER
#define H_TRIGGER

#include "core.h"
#include "controller.h"
#include "character.h"
#include "sprite.h"

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

struct KeyItemInv : Controller {
    KeyItemInv(IGame *game, int entity) : Controller(game, entity) {
        angle.x = PI * 0.5f;
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
        if (armed && collide(lara)) {
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

    Flame(IGame *game, int entity) : Sprite(game, entity, false, Sprite::FRAME_ANIMATED), owner(NULL), jointIndex(0), sleepTime(0.0f) {
        time = randf() * 3.0f;
        activate();
    }

    virtual bool getSaveData(SaveEntity &data) {
        return false;
    }

    virtual void update() {
        Sprite::update();

        if (jointIndex <= 0) {
            game->playSound(TR::SND_FLAME, pos, Sound::PAN);
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

    MuzzleFlash(IGame *game, int entity) : Controller(game, entity), owner(NULL), joint(0), lightIndex(-1) {
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

struct TrapLavaEmitter : Controller {

    struct Particle {
        vec3  pos;
        vec3  velocity;
        int16 roomIndex;
        int8  frame;

        void update(TR::Level *level, Controller *lara) {
            Controller::applyGravity(velocity.y);
            vec3 opos = pos;
            pos += velocity * (30.0f * Core::deltaTime);

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

    TrapLavaEmitter(IGame *game, int entity) : Controller(game, entity) {
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
                particles.push(part);

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
    }

    virtual void render(Frustum *frustum, MeshBuilder *mesh, Shader::Type type, bool caustics) {
        for (int i = 0; i < particles.length; i++) {
            Particle &part = particles[i];

            uint8 intensity = clamp(int(part.frame * 0.18f * 255.0f), 0, 255);
            Color32 color(intensity, intensity, intensity, 255);

            vec3 p = part.pos - Core::viewPos.xyz();

            mesh->addDynSprite(level->spriteSequences[-(spriteIndex + 1)].sStart + part.frame, short3(int16(p.x), int16(p.y), int16(p.z)), false, false, color, color);
        }
    }
};



#define BOULDER_DAMAGE_GROUND 1000
#define BOULDER_DAMAGE_AIR    100

struct TrapBoulder : Controller {
    enum {
        STATE_FALL,
        STATE_ROLL,
    };

    vec3 velocity;

    TrapBoulder(IGame *game, int entity) : Controller(game, entity), velocity(0) {
        flags.unused = false;
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
            }
            return;
        }

        if (flags.unused) {
            return;
        }

        vec3 p = pos;

        if (velocity.y != 0.0f) {
            applyGravity(velocity.y);
        }
        pos += velocity * (30.0f * Core::deltaTime);

        TR::Room::Sector *sector = level->getSector(roomIndex, pos);
        p.y = level->getFloor(sector, pos);

        if (pos.y < p.y) {
            if (velocity.y == 0.0f) {
                velocity.y = -10.0f;
            }
        } else {
            if (state != STATE_ROLL) {
                animation.setState(STATE_ROLL);
                velocity = getDir() * animation.getSpeed();
            }
        }

        if (pos.y >= p.y - 256.0f) {
            velocity.y = 0.0f;
            pos.y      = p.y;
        }

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

        game->checkTrigger(this, true);
        updateAnimation(true);

        Character *lara = (Character*)game->getLara(pos);
        if (lara->health > 0.0f && collide(lara)) {
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

    TrapFloor(IGame *game, int entity) : Controller(game, entity), velocity(0) {}

    virtual bool isCollider() {
        return state == STATE_STATIC || state == STATE_SHAKE;
    }

    virtual bool activate() {
        if (state != STATE_STATIC) return false;
        vec3 &p = game->getLara(pos)->pos;
        if (fabsf(p.y - (pos.y - 512.0f)) <= 8 && Controller::activate()) {
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

        if (!checkRange(lara, SPIKES_RANGE) || !collide(lara))
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
            if (collide(lara))
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
            if (animation.state == STATE_SLAM && !bite && collide(lara)) {
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
        if (level->extra.waterSplash > -1)
            game->addEntity(TR::Entity::WATER_SPLASH, getRoomIndex(), dropPos);
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

    Explosion(IGame *game, int entity) : Sprite(game, entity, true, Sprite::FRAME_ANIMATED) {
        game->playSound(TR::SND_EXPLOSION, pos, Sound::PAN);
        level->spriteSequences[-(getEntity().modelIndex + 1)].transp = 2; // fix blending mode to additive
    }

    virtual bool getSaveData(SaveEntity &data) {
        return false;
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
            default : speed = 200.0f;
        }
        velocity = getDir() * (speed * 30.0f);
    }

    virtual void update() {
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
            default : ;
        }

        if (directHit)
            game->playSound(lara->stand == Character::STAND_UNDERWATER ? TR::SND_HIT_UNDERWATER : TR::SND_HIT, lara->pos, Sound::PAN);

        game->removeEntity(this);
    }
};

#include "objects_tr1.h"
#include "objects_tr2.h"

#endif
