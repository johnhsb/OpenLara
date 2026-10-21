#ifndef H_OBJECTS_TR1
#define H_OBJECTS_TR1

// the objects of Tomb Raider 1

#include "objects.h"

struct Gear : Controller {
    enum {
        STATE_STATIC,
        STATE_ROTATE,
    };

    Gear(IGame *game, int entity) : Controller(game, entity) {}

    virtual void update() {
        updateAnimation(true);
        animation.setState(isActive() ? STATE_ROTATE : STATE_STATIC);
    }
};


struct MovingBlock : Controller {
    enum {
        STATE_BEGIN,
        STATE_END,
        STATE_MOVE,
    };

    MovingBlock(IGame *game, int entity) : Controller(game, entity) {
        if (!flags.invisible)
            updateFloor(true);
    }

    virtual void setSaveData(const SaveEntity &data) {
        updateFloor(false);
        Controller::setSaveData(data);
        if (state != STATE_MOVE)
            updateFloor(true);
    }

    void updateFloor(bool rise) {
        TR::Level::FloorInfo info;
        getFloorInfo(getRoomIndex(), pos, info);
        if (info.roomNext != 0xFF)
            roomIndex = info.roomNext;
        int dx, dz;
        TR::Room::Sector &s = level->getSector(getRoomIndex(), int(pos.x), int(pos.z), dx, dz);
        s.floor += rise ? -8 : 8;
    }

    virtual void update() {
        updateAnimation(true);

        if (isActive()) {
            if (state == STATE_BEGIN) {
                updateFloor(false);
                animation.setState(STATE_END);
            }
        } else {
            if (state == STATE_END) {
                updateFloor(false);
                animation.setState(STATE_BEGIN);
            }
        }

        if (flags.state == TR::Entity::asInactive) {
            if (flags.active == TR::ACTIVE)
                flags.state = TR::Entity::asActive; // stay in active items list
            pos.x = int(pos.x / 1024.0f) * 1024.0f + 512.0f;
            pos.z = int(pos.z / 1024.0f) * 1024.0f + 512.0f;
            updateFloor(true);
            game->checkTrigger(this, true);
            return;
        }

        pos += getDir() * (animation.getSpeed() * Core::deltaTime * 30.0f);
    }
};

#define BLADE_DAMAGE    100
#define BLADE_RANGE     1024

struct TrapSwingBlade : Controller {
    enum {
        STATE_STATIC = 0,
        STATE_SWING  = 2,
    };

    TrapSwingBlade(IGame *game, int entity) : Controller(game, entity) {}

    virtual void update() {
        updateAnimation(true);

        if (isActive()) {
            if (state == STATE_STATIC)
                animation.setState(STATE_SWING);
        } else {
            if (state == STATE_SWING)
                animation.setState(STATE_STATIC);
        }

        if (state != STATE_SWING)
            return;

        int f = animation.frameIndex;
        if ((f <= 8 || f >= 20) && (f <= 42 || f >= 57))
            return;

        Character* lara = (Character*)game->getLara(pos);
        if (!checkRange(lara, BLADE_RANGE) || !touchLara(this, lara))
            return;

        lara->hit(BLADE_DAMAGE * 30.0f * Core::deltaTime, this, TR::HIT_BLADE);
    }
};

#define SWORD_DAMAGE 100.0f
#define SWORD_RANGE  1536.0f

struct TrapSword : Controller {
    vec3  dir;
    float rot;

    TrapSword(IGame *game, int entity) : Controller(game, entity), dir(0) {
        rot = (randf() * 2.0f - 1.0f) * PI;
    }

    virtual void update() {
        TR::Level::FloorInfo info;
        getFloorInfo(getRoomIndex(), pos, info);

        Controller *lara = game->getLara(pos);

        if (dir.y == 0.0f) {
            dir = lara->pos - pos;
            if (dir.y > 0 && dir.y < 3072.0f && fabsf(dir.x) < SWORD_RANGE && fabsf(dir.z) < SWORD_RANGE) {
                dir /= 32.0f;
                dir.y = 50.0f;
            } else
                dir.y = 0.0f;
        } else {
            angle.y += rot * Core::deltaTime;
            applyGravity(dir.y);
            pos += dir * (30.0f * Core::deltaTime);
            if (pos.y > info.floor) {
                pos.y = info.floor;
                game->playSound(TR::SND_SWORD, pos, Sound::PAN);
                deactivate(true);
            }

            if (touchLara(this, lara, false)) // the original tests the bounds only
                lara->hit(SWORD_DAMAGE * 30.0f * Core::deltaTime, this, TR::HIT_SWORD); // TODO: push lara
        }
    }
};

struct ThorHammerBlock : Controller {
    Controller *handle;

    ThorHammerBlock(IGame *game, int entity) : Controller(game, entity), handle(NULL) {}

    virtual bool isCollider() {
        return handle ? handle->isCollider() : false;
    }

    virtual bool getSaveData(SaveEntity &data) {
        return false;
    }
};

struct ThorHammer : Controller {
    enum {
        STATE_IDLE,
        STATE_START,
        STATE_FALL,
        STATE_DOWN,
    };

    ThorHammerBlock *block;

    ThorHammer(IGame *game, int entity) : Controller(game, entity) {
        initBlock();
    }

    void initBlock() {
        block = (ThorHammerBlock*)game->addEntity(TR::Entity::HAMMER_BLOCK, getRoomIndex(), pos, angle.y);
        ASSERT(block);
        if (block)
            block->handle = this;
    }

    void updateBlock() {
        block->animation.frameA = animation.frameA;
        block->animation.frameB = animation.frameB;
        block->animation.delta  = animation.delta;
    }

    virtual void setSaveData(const SaveEntity &data) {
        Controller::setSaveData(data);
        initBlock();
        updateBlock();
    }

    virtual bool isCollider() {
        return state == STATE_DOWN;
    }

    virtual void update() {
        switch (state) {
            case STATE_IDLE  : if (isActive()) animation.setState(STATE_START); break;
            case STATE_START : animation.setState(isActive() ? STATE_FALL : STATE_IDLE); break;
            case STATE_FALL  : {
                Character *lara = (Character*)game->getLara(pos);
                if (animation.frameIndex > 30 && lara->health > 0.0f) {
                    vec3 d = pos + getDir() * (3.0f * 1024.0f) - lara->pos;
                    if (fabsf(d.x) < 520.0f && fabsf(d.z) < 520.0f)
                        lara->hit(1001, this, TR::HIT_BOULDER);
                }
                break;
            }
            case STATE_DOWN : {
                game->checkTrigger(this, 1); 
                deactivate(true);
                break;
            }
        }

        updateAnimation(true);
        if (block)
            updateBlock();
    }
};

#define LIGHTNING_DAMAGE 400

struct Lightning : Controller {
    vec3  target;
    float timer;
    bool  flash;
    bool  armed;

    Lightning(IGame *game, int entity) : Controller(game, entity), timer(0), flash(false) {}

    virtual void update() {
        if (isActive()) {
            timer -= Core::deltaTime;

            Character *lara = (Character*)game->getLara(pos);

            if (timer <= 0.0f) {
                if (flash) {
                    if (level->state.flags.flipped)
                        game->flipMap();
                    flash = false;
                    armed = true;
                    timer = (35.0f + randf() * 45.0f) / 30.0f;                    
                } else {
                    if (!level->state.flags.flipped)
                        game->flipMap();
                    flash = true;
                    timer = 20.0f / 30.0f;

                    bool hasTargets = getModel()->mCount > 1; // LEVEL4 has, LEVEL10C not

                    if ((lara->pos - pos).length() < (hasTargets ? 2560.0f : 1024.0f)) {
                        lara->hit(LIGHTNING_DAMAGE, this, TR::HIT_LIGHTNING);
                        armed = false;
                    } else if (!hasTargets) {
                        target = pos + vec3(0.0f, 1024.0f, 0.0f);
                    } else
                        target = getJoint(1 + rand() % 5).pos;
                }
                game->playSound(TR::SND_LIGHTNING, pos, Sound::PAN);
            }
        } else {
            timer = 0.0f;
            flash = false;
            if (level->state.flags.flipped)
                game->flipMap();
            deactivate(true);
        }
    }

    void divide(vec3 *points, int L, int R, float spread) {
        int M = (L + R) / 2;
        if (M == L || M == R) return;
        points[M] = (points[L] + points[R]) * 0.5f + (vec3(randf(), randf(), randf()) - 0.5f) * spread;
        spread *= 0.5f;
        divide(points, L, M, spread);
        divide(points, M, R, spread);
    }

    short4 toCoord(const vec3 &v, int16 joint) {
        return short4(int16(v.x), int16(v.y), int16(v.z), joint);
    }

    void setVertex(Vertex &v, const vec3 &coord, int16 joint, int idx) {
        TR::TextureInfo &tex = CommonTex[CTEX_FLASH];
        v.coord     = toCoord(coord, joint);
        v.normal    = short4( 0, -1, 0, 0 );
        v.texCoord  = short4( tex.texCoordAtlas[idx].x, tex.texCoordAtlas[idx].y, 32767, 32767 );
        v.color     = ubyte4( 255, 255, 255, 255 );
    }

    void renderPolyline(const vec3 &start, const vec3 &end, float width, float spread, int depth) {
        vec3 points[9];
        points[0] = start;
        points[8] = end;
        divide(points, 0, 8, spread);

        Index  indices[(COUNT(points) - 1) * 6];
        Vertex vertices[COUNT(points) * 2];
        
        int iCount = 0;
        int vCount = 0;
        int count = COUNT(points);
    // build indices
        for (int i = 0; i < count - 1; i++) {
            indices[iCount++] = vCount;
            indices[iCount++] = vCount + 1;
            indices[iCount++] = vCount + 2;
            indices[iCount++] = vCount + 1;
            indices[iCount++] = vCount + 3;
            indices[iCount++] = vCount + 2;
            vCount += 2;
        }
        vCount += 2;
        ASSERT(iCount == (count - 1) * 6);
        ASSERT(vCount == count * 2);

    // build vertices
        vec3 dir = Core::mViewInv.dir().xyz();

        vCount = 0;
        vec3 n;
        for (int i = 0; i < count; i++) {
            if (i < count - 1)
                n = dir.cross(points[i + 1] - points[i]).normal() * width;
            setVertex(vertices[vCount++], points[i] - n, 0, 0);
            setVertex(vertices[vCount++], points[i] + n, 0, 3);
        }
        ASSERT(vCount == count * 2);

        game->getMesh()->renderBuffer(indices, iCount, vertices, vCount);

        if (depth > 0) {
            for (int i = 0; i < 2; i++) {
                vec3 a = points[rand() % (count - 1)];
                vec3 b = a;
                b.x += (randf() - 0.5f) * spread;
                b.y  = points[count - 1].y;
                b.z += (randf() - 0.5f) * spread;

                renderPolyline(a, b, width * 0.75f, spread * 0.5f, depth - 1);
            }
        }
    }


    virtual void render(Frustum *frustum, MeshBuilder *mesh, Shader::Type type, bool caustics) {
        Controller::render(frustum, mesh, type, caustics);
        if (!flash) return;

        if (!armed)
            target = game->getLara(pos)->pos;

        Basis b = getJoint(0);
        b.rot = quat(0, 0, 0, 1);

        game->setShader(Core::pass, Shader::FLASH, false, false);
        Core::setMaterial(0.0f, 0.0f, 0.0f, 1.0f);
        Core::setBasis(&b, 1);

        Core::setCullMode(cmNone);
        Core::setBlendMode(bmAdd);
        Core::setDepthWrite(false);

        renderPolyline(vec3(0.0f), target - b.pos, 64.0f, 512.0f, 1);

        Core::setDepthWrite(true);
        Core::setBlendMode(bmNone);
        Core::setCullMode(cmFront);
    }
};

struct MidasHand : Controller {
    TR::Entity::Type invItem;
    bool interaction;

    MidasHand(IGame *game, int entity) : Controller(game, entity), invItem(TR::Entity::NONE), interaction(false) {
        activate();
    }

    virtual void update() {
        Character *lara = (Character*)game->getLara(pos);
        
        if (lara->health <= 0.0f || lara->stand != Character::STAND_GROUND || lara->getRoomIndex() != getRoomIndex())
            return;

        vec3 d = (pos - lara->pos).abs();

        if (d.x < 512.0f && d.z < 512.0f) { // check for same sector
            lara->hit(1001.0f, this, TR::HIT_MIDAS);
            return;
        }

        interaction = (d.x < 700.0f && d.z < 700.0f) && lara->state == 2; // 2 = Lara::STATE_STOP

        if (interaction) {
            if (invItem != TR::Entity::NONE) {
                if (invItem == TR::Entity::INV_LEADBAR) {
                    lara->angle.y = PI * 0.5f;
                    lara->pos.x   = pos.x - 612.0f;
                    lara->animation.setAnim(level->models[TR::MODEL_LARA_SPEC].animation);
                    game->invAdd(TR::Entity::PUZZLE_1);
                } else
                    game->playSound(TR::SND_NO, pos, Sound::PAN); // uncompatible item
                invItem = TR::Entity::NONE;
            } else if (Input::state[0][cAction] && !game->invChooseKey(0, getEntity().type)) // TODO: add callback for useItem // TODO: player[1]
                game->playSound(TR::SND_NO, pos, Sound::PAN); // no compatible items in inventory
        }
    }
};

struct TrapLava : Controller {
    TrapLava(IGame *game, int entity) : Controller(game, entity) {}

    virtual void update() {
        Character *lara = (Character*)game->getLara(pos);
        if (lara->health > 0.0f && touchLara(this, lara))
            lara->hit(1001.0f, this, TR::HIT_LAVA);

        vec3 dir = getDir();

        roomIndex = getRoomIndex();
        TR::Room::Sector *s = level->getSector(roomIndex, pos + dir * 2048.0f);
        if (!s || s->floor * 256 != int(pos.y))
            return;

        pos += dir * (25.0f * 30.0f * Core::deltaTime);
    }
};


struct MovingObject : Controller {
    enum {
        STATE_CLOSE,
        STATE_OPEN,
    };

    MovingObject(IGame *game, int entity) : Controller(game, entity) {}

    virtual bool isCollider() {
        return true;
    }

    virtual void update() {
        updateAnimation(true);
        animation.setState(isActive() ? STATE_OPEN : STATE_CLOSE);
        pos += getDir() * (animation.getSpeed() * Core::deltaTime * 30.0f);

        TR::Level::FloorInfo info;
        getFloorInfo(getRoomIndex(), pos, info);
        if (info.roomNext != TR::NO_ROOM)
            roomIndex = info.roomNext;
    }
};


#define CENTAUR_STATUE_RANGE (3072 + 512)

struct CentaurStatue : Controller {

    CentaurStatue(IGame *game, int entity) : Controller(game, entity) {
        flags.unused = false;
    }

    virtual bool activate() {
        return (flags.unused ? false : Controller::activate());
    }

    virtual void update() {
        if (explodeMask) {
            Controller::update();
            if (!explodeMask)
                deactivate(true);
            return;
        }

        if ((pos - game->getLara(pos)->pos).length() < CENTAUR_STATUE_RANGE) {
            flags.unused = true;
            explode(0xFFFFFFFF, 0.0f);
            game->playSound(TR::SND_EXPLOSION, pos, Sound::PAN);
            Controller *enemy = game->addEntity(TR::Entity::ENEMY_CENTAUR, getRoomIndex(), pos, angle.y);
            if (enemy)
                enemy->animation.setAnim(7, -36);
        }
    }
};


struct Cabin : Controller {
    enum {
        STATE_UP,
        STATE_DOWN_1,
        STATE_DOWN_2,
        STATE_DOWN_3,
        STATE_GROUND,
    };

    Cabin(IGame *game, int entity) : Controller(game, entity) {}

    virtual void update() {
        if (flags.active == TR::ACTIVE) {
            if (state >= STATE_UP && state <= STATE_DOWN_2)
                animation.setState(state + 1);
            flags.active = 0;
        }

        if (state == STATE_GROUND) {
            flags.invisible        = true;
            level->state.flipmaps[3].active = TR::ACTIVE;
            game->flipMap();
            deactivate(true);
        }

        updateAnimation(true);
    }
};


struct Boat : Controller {
    enum {
        STATE_IDLE = 1,
        STATE_MOVE,
        STATE_STOP,
    };

    Boat(IGame *game, int entity) : Controller(game, entity) {}

    virtual void update() {
        switch (state) {
            case STATE_IDLE : animation.setState(STATE_MOVE); break;
            case STATE_MOVE : animation.setState(STATE_STOP); break;
            case STATE_STOP : deactivate(true); flags.invisible = true; break;
        }
        updateAnimation(true);
        pos = pos + getDir() * (animation.getSpeed() * Core::deltaTime * 30.0f);
    }
};


#define MUTANT_EGG_RANGE 4096

struct MutantEgg : Controller {
    enum {
        STATE_IDLE,
        STATE_EXPLOSION,
    };

    TR::Entity::Type enemy;
    
    MutantEgg(IGame *game, int entity) : Controller(game, entity) {
        initMeshOverrides();
        layers[0].mask = 0xff0001ff; // hide dynamic meshes

        switch (flags.active) {
            case 1  : enemy = TR::Entity::ENEMY_MUTANT_2;     break;
            case 2  : enemy = TR::Entity::ENEMY_CENTAUR;      break;
            case 4  : enemy = TR::Entity::ENEMY_GIANT_MUTANT; break;
            case 8  : enemy = TR::Entity::ENEMY_MUTANT_3;     break;
            default : enemy = TR::Entity::ENEMY_MUTANT_1;
        }
    }

    virtual void update() {
        if (state != STATE_EXPLOSION) {
            Box box = Box(pos + vec3(-MUTANT_EGG_RANGE), pos + vec3(MUTANT_EGG_RANGE));
            if ( flags.once || getEntity().type == TR::Entity::MUTANT_EGG_BIG || box.contains(((game->getLara(pos))->pos)) ) {
                animation.setState(STATE_EXPLOSION);
                layers[0].mask = 0xffffffff & ~(1 << 24);
                explode(0x00fffe00, 0.0f);
                // the egg hatches empty when the level has no model for its enemy (e.g. giant mutant eggs in The Hive)
                if (level->getModelIndex(enemy))
                    game->addEntity(enemy, getRoomIndex(), pos, angle.y);
            }
        }
        Controller::update();
    }

    virtual void setSaveData(const SaveEntity &data) {
        Controller::setSaveData(data);
        visibleMask = (state == STATE_IDLE) ? 0xff0001ff : (0xffffffff & ~(1 << 24));
    }
};

#endif
