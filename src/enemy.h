#ifndef H_ENEMY
#define H_ENEMY

#include "character.h"
#include "objects.h"

#define STALK_BOX       (1024 * 3)
#define ESCAPE_BOX      (1024 * 5)
#define ATTACK_BOX      STALK_BOX

#define MAX_SHOT_DIST   (64 * 1024)
#define MAX_CARRIED     8       // the pickups an enemy carries

// random number of the original games, rand() goes up to 0x7FFFFFFF on some platforms
inline int randControl() {
    return rand() & 0x7FFF;
}

struct Enemy : Character {

    struct Path {
        int16       index;
        int16       count;
        uint16      *boxes;
        TR::Level   *level;

        Path(TR::Level *level, uint16 *boxes, int count) : index(0), count(count), boxes(new uint16[count]), level(level) {
            memcpy(this->boxes, boxes, count * sizeof(boxes[0]));
        }

        ~Path() {
            delete[] boxes;
        }

        bool getNextPoint(TR::Level *level, vec3 &point) {
            if (index >= count - 1)
                return false;

            TR::Box &a = level->boxes[boxes[index++]];
            TR::Box &b = level->boxes[boxes[index]];

            int minX = max(a.minX, b.minX);
            int minZ = max(a.minZ, b.minZ);
            int maxX = min(a.maxX, b.maxX);
            int maxZ = min(a.maxZ, b.maxZ);

        // the point of the overlap nearest to the center of the next box, the overlap of box rectangles may be wide
        // and a random point there could lie behind the enemy
            if (maxX - minX > 1024) { minX += 512; maxX -= 512; } else minX = maxX = (minX + maxX) / 2;
            if (maxZ - minZ > 1024) { minZ += 512; maxZ -= 512; } else minZ = maxZ = (minZ + maxZ) / 2;
            point.x = float(clamp((b.minX + b.maxX) / 2, minX, maxX));
            point.y = float((a.floor + b.floor) / 2);
            point.z = float(clamp((b.minZ + b.maxZ) / 2, minZ, maxZ));

            return true;
        }
    };

    enum AI {
        AI_FIXED, AI_RANDOM
    } ai;

    enum Mood {
        MOOD_SLEEP, MOOD_STALK, MOOD_ATTACK, MOOD_ESCAPE 
    } mood;

    bool  wound;
    int   nextState;

    uint16 targetBox;
    vec3   waypoint;

    float thinkTime;
    int   touchBits;    // the meshes Lara touched since the last think
    int   touchNow;
    float length;       // dist from center to head (jaws)
    float aggression;
    int   radius;
    int   hitSound;

    Character *target;
    Path      *path;

    int carried[MAX_CARRIED];   // the pickups it drops when it dies (TR2+, hidden until then)
    int carriedCount;

    float targetDist;
    float targetAngle;
    bool  targetDead;
    bool  targetInView;     // target in enemy view zone
    bool  targetFromView;   // enemy in target view zone
    bool  targetCanAttack;

    Enemy(IGame *game, int entity, float health, int radius, float length, float aggression) : Character(game, entity, health), ai(AI_RANDOM), mood(MOOD_SLEEP), wound(false), nextState(0), targetBox(TR::NO_BOX), thinkTime(1.0f / 30.0f), touchBits(0), touchNow(0), length(length), aggression(aggression), radius(radius), hitSound(-1), target(NULL), path(NULL), carriedCount(0) {
        targetDist   = +INF;
        targetInView = targetFromView = targetCanAttack = false;
        waypoint     = pos;
    }

    virtual ~Enemy() {
        delete path;
    }

    virtual bool getSaveData(SaveEntity &data) {
        Character::getSaveData(data);
        data.extraSize = sizeof(data.extra.enemy);
        data.extra.enemy.health    = health;
        data.extra.enemy.spec.mood = mood;
        data.extra.enemy.targetBox = targetBox;
        return true;
    }

    virtual void setSaveData(const SaveEntity &data) {
        Character::setSaveData(data);
        health    = data.extra.enemy.health;
        mood      = Mood(data.extra.enemy.spec.mood);
        targetBox = data.extra.enemy.targetBox;
        updateZone();
    }

    virtual bool activate() {
        return health > 0.0f && Character::activate();
    }

    virtual void updateVelocity() {
        if (stand == STAND_AIR && (!flying || health <= 0.0f))
            applyGravity(velocity.y);
        else
            velocity = getDir() * animation.getSpeed();

        if (health <= 0.0f) 
            velocity.x = velocity.z = 0.0f;
    }

    bool checkPoint(int x, int z) {
        TR::Box &a = level->boxes[box];
        if (a.contains(x, z))
            return true;

        bool big = getEntity().isBigEnemy();
        TR::Overlap *o = &level->overlaps[a.overlap.index];
        do {
            TR::Box &b = level->boxes[o->boxIndex];
            if (!b.contains(x, z))
                continue;
            if (big && b.overlap.blockable)
                continue;
            if (getZones()[o->boxIndex] == zone) {
                int d = a.floor - b.floor;
                if (d <= stepHeight && d >= dropHeight)
                    return true;
            }
        } while (!(o++)->end);

        return false;
    }

    void clipByBox(vec3 &pos) {
        int px = int(pos.x);
        int pz = int(pos.z);
        int nx = px;
        int nz = pz;

        TR::Box &a = level->boxes[box];
            
        if (!checkPoint(px - radius, pz)) nx = a.minX + radius;
        if (!checkPoint(px + radius, pz)) nx = a.maxX - radius;
        if (!checkPoint(px, pz - radius)) nz = a.minZ + radius;
        if (!checkPoint(px, pz + radius)) nz = a.maxZ - radius;

        if (px != nx) pos.x = float(nx);
        if (pz != nz) pos.z = float(nz);
    }

    void collideEnemies() {
        if (getEntity().isBigEnemy())
            return;

        Controller *c = Controller::first;
        while (c) {
            if (c != this && c->getEntity().isEnemy()) {
                Enemy *enemy = (Enemy*)c;
                if (enemy->health > 0.0f) {
                    vec3 dir = vec3(enemy->pos.x - pos.x, 0.0f, enemy->pos.z - pos.z);
                    float D = dir.length2();
                    float R = float((enemy->radius + radius) / 2);
                    if (D < R * R) {
                        D = sqrtf(D);
                        pos -= dir.normal() * (R - D);
                    }
                }
            }
            c = c->next;
        }
    }

    virtual void updatePosition() {
        if (!flags.active) return;

        vec3 p = pos;
        pos += velocity * (30.0f * Core::deltaTime);

        collideEnemies();

        clipByBox(pos);

        TR::Level::FloorInfo info;
        getFloorInfo(getRoomIndex(), pos, info);

    // blocked, slide along the wall instead of stopping dead (the corners of narrow passages)
        if (!canEnter(info) && stand == STAND_GROUND) {
            vec3 slide[2] = { vec3(pos.x, pos.y, p.z), vec3(p.x, pos.y, pos.z) };
            for (int i = 0; i < 2; i++) {
                TR::Level::FloorInfo slideInfo;
                getFloorInfo(getRoomIndex(), slide[i], slideInfo);
                if (canEnter(slideInfo)) {
                    pos  = slide[i];
                    info = slideInfo;
                    break;
                }
            }
        }

        if (stand == STAND_AIR && !flying && info.floor < pos.y) {
            stand = STAND_GROUND;
            pos.y = info.floor;
        }

        if (canEnter(info)) {
            switch (stand) {
                case STAND_GROUND : {
                    float fallSpeed = 2048.0f * Core::deltaTime;
                    decrease(info.floor - pos.y, pos.y, fallSpeed);
                    break;
                }
                case STAND_AIR    : 
                    pos.y = clamp(pos.y, info.ceiling, info.floor);
                    break;
                default : ;
            }
        } else
            pos = p;

        updateRoom();
    }

    bool canEnter(const TR::Level::FloorInfo &info) {
        return info.boxIndex != 0xFFFF && zone == getZones()[info.boxIndex] && !level->boxes[info.boxIndex].overlap.block;
    }

    void setOverrides(bool active, int chest, int head) {
        if (active && head  > -1) {
            animation.overrides[head] = animation.getJointRot(head);
            animation.overrideMask |=  (1 << head);
        } else
            animation.overrideMask &= ~(1 << head);

        if (active && chest  > -1) {
            animation.overrides[chest] = animation.getJointRot(chest);
            animation.overrideMask |=  (1 << chest);
        } else
            animation.overrideMask &= ~(1 << chest);
    }

    bool targetIsVisible(float maxDist) {
        if (targetInView && targetDist < maxDist && target->health > 0.0f) {
            TR::Location from, to;
            from.room = getRoomIndex();
            from.pos  = pos;
            to.pos    = target->pos;

        // vertical offset to ~gun/head height
            from.pos.y -= 768.0f;
            if (target->stand != STAND_UNDERWATER && target->stand != STAND_ONWATER)
                to.pos.y -= 768.0f;

            return trace(from, to);
        }
        return false;
    }

    virtual void lookAt(Controller *target) {
        Character::lookAt(targetInView ? target : NULL);
    }

    void turn(bool tilt, float w) {
        float speed = animation.getSpeed();

        if (!target || speed == 0.0f || w == 0.0f) {
            angle.z = lerp(angle.z, 0.0f, 4.0f * Core::deltaTime);
            return;
        }

        vec3  d = waypoint - pos;
        float a = clampAngle(normalizeAngle(PIH - d.angleY() - angle.y));

        w /= 30.0f;

        float minDist = speed * PIH / w;

        if ( (a > PIH || a < -PIH) && (SQR(d.x) + SQR(d.z) < SQR(minDist)) )
            w *= 0.5f;

        a = clamp(a, -w, w);

        angle.y += a * 30.0f * Core::deltaTime;
        angle.z = lerp(angle.z, tilt ? a * 2.0f : 0.0f, 4.0f * Core::deltaTime);
    }

    int lift(float delta, float speed) {
        speed *= Core::deltaTime;
        decrease(delta, pos.y, speed);
        if (speed != 0.0f) {
            return speed < 0 ? FORTH : BACK;
        }
        return 0;
    }

    virtual void hit(float damage, Controller *enemy = NULL, TR::HitType hitType = TR::HIT_DEFAULT) {
        bool alive = health > 0.0f;
        if (hitSound > -1 && alive)
            game->playSound(hitSound, pos, Sound::PAN);
        Character::hit(damage, enemy, hitType);
        wound = true;
        if (alive && health <= 0.0f)
            dropCarried();
    };

    // a TR2 level has the pickups an enemy carries where the enemy is, they show up on the floor where it dies
    virtual void carry(Controller *pickup) {
        if (carriedCount == MAX_CARRIED)
            return;
        carried[carriedCount++] = pickup->entity;
        pickup->flags.invisible = true;
    }

    void dropCarried() {
        for (int i = 0; i < carriedCount; i++) {
            Controller *pickup = (Controller*)level->entities[carried[i]].controller;
            if (!pickup || !pickup->flags.invisible)
                continue;

            TR::Level::FloorInfo info;
            getFloorInfo(getRoomIndex(), pos, info);

            pickup->pos       = vec3(pos.x, info.floor, pos.z);
            pickup->roomIndex = getRoomIndex();
            pickup->updateRoom();
            pickup->flags.invisible = false;
        }
        carriedCount = 0;
    }

    virtual void touchedBy(int mask) {
        touchBits |= mask;
    }

    // the meshes touching Lara, also when she was pushed off them since (an attack tests these, as in the original)
    int touching() {
        return touchNow | collide(target);
    }

    void bite(int joint, const vec3 &offset, float damage) {
        ASSERT(target);
        target->hit(damage, this);
        if (joint >= 0)
            game->addEntity(TR::Entity::BLOOD, target->getRoomIndex(), getJoint(joint) * offset);
    }

    // Lara's zone in the zone table of this enemy (it depends on how the enemy can move)
    int targetZone() {
        if (!target || target->box == TR::NO_BOX)
            return -1;
        return getZones()[target->box];
    }

    Mood getMoodFixed() {
        bool inZone = zone == targetZone();

        if (mood == MOOD_SLEEP || mood == MOOD_STALK)
            return inZone ? MOOD_ATTACK : (wound ? MOOD_ESCAPE : mood);
        
        if (mood == MOOD_ATTACK)
            return inZone ? mood : MOOD_SLEEP;

        return inZone ? MOOD_ATTACK : mood;
    }

    Mood getMoodRandom() {
        bool inZone = zone == targetZone();
        bool brave  = randControl() < (mood != MOOD_ESCAPE ? 0x7800 : 0x0100) && inZone;
            
        if (mood == MOOD_SLEEP || mood == MOOD_STALK) {
            if (wound && !brave)
                return MOOD_ESCAPE;
            if (inZone) {
                int dx = abs(int(pos.x - target->pos.x));
                int dz = abs(int(pos.z - target->pos.z));
                return ((dx <= ATTACK_BOX && dz <= ATTACK_BOX) || (mood == MOOD_STALK && targetBox == TR::NO_BOX)) ? MOOD_ATTACK : MOOD_STALK;
            }
            return mood;
        }

        if (mood == MOOD_ATTACK)
            return (wound && !brave) ? MOOD_ESCAPE : (!inZone ? MOOD_SLEEP : mood);

        return brave ? MOOD_STALK : mood;
    }
    
    bool think(bool fixedLogic) {
        thinkTime += Core::deltaTime;
        if (thinkTime < 1.0f / 30.0f)
            return false;
        thinkTime -= 1.0f / 30.0f;

        touchNow  = touchBits;
        touchBits = 0;

        int zoneOld = zone;
        updateZone();

        target = (Character*)game->getLara(pos);

        bool tr2 = (level->version & TR::VER_TR2) != 0;

        vec3 targetVec  = target->pos - pos - getDir() * length;
        targetDist      = tr2 ? sqrtf(SQR(targetVec.x) + SQR(targetVec.z)) : targetVec.length(); // TR2 ranges are horizontal
        targetAngle     = clampAngle(atan2f(targetVec.x, targetVec.z) - angle.y);
        targetDead      = target->health <= 0;
        targetInView    = targetVec.dot(getDir()) > 0;
        targetFromView  = targetVec.dot(target->getDir()) < 0;
        targetCanAttack = targetInView && fabsf(targetVec.y) <= 256.0f;

        int targetBoxOld = targetBox;

        bool inZone = zone == targetZone();

        if (target->health <= 0.0f || !inZone)
            targetBox = TR::NO_BOX;

    // update mood
        if (mood != MOOD_ATTACK && targetBox != TR::NO_BOX && !checkBox(targetBox)) {
            if (!inZone)
                mood = MOOD_SLEEP;
            targetBox = TR::NO_BOX;
        }

        mood = target->health <= 0 ? MOOD_SLEEP : (ai == AI_FIXED ? getMoodFixed() : getMoodRandom());

    // set behavior and target
        int box;

        switch (mood) {
            case MOOD_SLEEP :
                if (targetBox == TR::NO_BOX && checkBox(box = getRandomZoneBox()) && isStalkBox(box)) {
                    mood = MOOD_STALK;
                    gotoBox(box);
                }
                break;
            case MOOD_STALK :
                if ((targetBox == TR::NO_BOX || !isStalkBox(targetBox)) && checkBox(box = getRandomZoneBox())) {
                    if (isStalkBox(box))
                        gotoBox(box);
                    else
                        if (targetBox == TR::NO_BOX) {
                            if (!inZone)
                                mood = MOOD_SLEEP;
                            gotoBox(box);
                        }
                }
                break;
            case MOOD_ATTACK :
                if (randf() > aggression)
                    break;
                targetBox = TR::NO_BOX;
                break;
            case MOOD_ESCAPE :
                if (targetBox == TR::NO_BOX && checkBox(box = getRandomZoneBox())) {
                    if (isEscapeBox(box))
                        gotoBox(box);
                    else
                        if (inZone && isStalkBox(box)) {
                            mood = MOOD_STALK;
                            gotoBox(box);
                        }                    
                }
                break;
        }

        if (targetBox == TR::NO_BOX)
            gotoBox(target->box);

        if (path && (path->index == 0 || this->box != path->boxes[path->index - 1]) && this->box != path->boxes[path->index])
            targetBoxOld = TR::NO_BOX;

        if (zoneOld != zone)
            targetBoxOld = TR::NO_BOX;

        if (targetBoxOld != targetBox) {
            if (findPath(stepHeight, dropHeight, getEntity().isBigEnemy()))
                nextWaypoint();
            else
                targetBox = TR::NO_BOX;
        }

        if (targetBox != TR::NO_BOX && path) {
            vec3 d = pos - waypoint;

        // the waypoint is reached when close to it or inside the box it leads to (a fast runner may circle around the point)
            if ((fabsf(d.x) < 512 && fabsf(d.y) < 512 && fabsf(d.z) < 512) || (path->index > 0 && path->index < path->count && box == path->boxes[path->index]))
                nextWaypoint();
        }

        if (tr2) {
        // attacking, it heads for Lara herself every frame once on her box or the last leg of the path to it
            if (mood == MOOD_ATTACK && targetBox != TR::NO_BOX && targetBox == target->box && (box == targetBox || !path || path->index >= path->count - 1))
                waypoint = target->pos;
        // only the moment of a hit may scare it away
            wound = false;
        }

        return true;
    }

    void nextWaypoint() {
        if (!path->getNextPoint(level, waypoint))
            waypoint = target->pos;
    }

    uint16 getRandomZoneBox() {
        return game->getRandomBox(zone, getZones());
    }

    void gotoBox(int box) {
        targetBox = box;
    }

    bool checkBox(int box) {
        if (zone != getZones()[box])
            return false;

        TR::Box    &b = game->getLevel()->boxes[box];
        uint16   type = getEntity().type;

        if (b.overlap.block)
            return false;

        if (type == TR::Entity::ENEMY_REX || type == TR::Entity::ENEMY_MUTANT_1 || type == TR::Entity::ENEMY_CENTAUR) {
            if (b.overlap.blockable)
                return false;
        } else
            if (b.overlap.block)
                return false;

        return int(pos.x) < int(b.minX) || int(pos.x) > int(b.maxX) || int(pos.z) < int(b.minZ) || int(pos.z) > int(b.maxZ);
    }

    bool isStalkBox(int box) {
        TR::Box    &b = game->getLevel()->boxes[box];

        int x = (b.minX + b.maxX) / 2 - int(target->pos.x);
        if (abs(x) > STALK_BOX) return false;

        int z = (b.minZ + b.maxZ) / 2 - int(target->pos.z);
        if (abs(z) > STALK_BOX) return false;

        int target_quadrant = angleQuadrant(target->angle.y, 0.0);
        int box_quadrant = z > 0 ? (x > 0 ? 2 : 1) : (x > 0 ? 3 : 0);

        if (target_quadrant == box_quadrant) return false;

        int controller_quadrant = pos.z > target->pos.z ? (pos.x > target->pos.x ? 2 : 1) : (pos.x > target->pos.x ? 3 : 0);

        if (target_quadrant == controller_quadrant && abs(target_quadrant - box_quadrant) == 2) return false;

        return true;
    }

    bool isEscapeBox(int box) {
        TR::Box    &b = game->getLevel()->boxes[box];

        int x = (b.minX + b.maxX) / 2 - int(target->pos.x);
        if (abs(x) < ESCAPE_BOX) return false;

        int z = (b.minZ + b.maxZ) / 2 - int(target->pos.z);
        if (abs(z) < ESCAPE_BOX) return false;

        return !((pos.x > target->pos.x) ^ (x > 0)) || !((pos.z > target->pos.z) ^ (z > 0));
    }

    bool findPath(int ascend, int descend, bool big) {
        delete path;
        path = NULL;

        uint16 *boxes;
        uint16 count = game->findPath(ascend, descend, big, box, targetBox, getZones(), &boxes);
        if (count) {
            path = new Path(level, boxes, count);
            return true;
        }

        return false;
    }

    void shot(TR::Entity::Type type, int joint, const vec3 &offset, float rx, float ry) {
        vec3 from = getJoint(joint) * offset;
        vec3 to   = target->getBoundingBox().center();

        Bullet *bullet = (Bullet*)game->addEntity(type, getRoomIndex(), from);
        if (bullet) {
            vec3 dir = to - from;
            vec3 ang = vec3(-atan2f(dir.y, sqrtf(dir.x * dir.x + dir.z * dir.z)), atan2f(dir.x, dir.z), 0.0f);
            ang += vec3(rx, ry, 0.0f);
            bullet->setAngle(ang);
        }
    }

    void shot(TR::Entity::Type type, int joint, const vec3 &offset) {
        shot(type, joint, offset, (randf() * 2.0f - 1.0f) * (1.5f * DEG2RAD), (randf() * 2.0f - 1.0f) * (1.5f * DEG2RAD));
    }

    bool isVisible() {
        for (int i = 0; i < 2; i++) {
            ICamera *camera = game->getCamera(i);
            if (!camera) continue;

            TR::Location eye = camera->eye;
            TR::Location loc;
            loc.room = getRoomIndex();
            loc.box  = box;
            loc.pos  = pos;
            loc.pos.y -= 1024;

            if (trace(eye, loc))
                return true;
        }
        return false;
    }
};


#define REX_DIST_BITE       1500
#define REX_DIST_BITE_MAX   4096
#define REX_DIST_WALK       5120
#define REX_TURN_FAST       (DEG2RAD * 120)
#define REX_TURN_SLOW       (DEG2RAD * 60)
#define REX_DAMAGE          1000
#define REX_DAMAGE_WALK     1
#define REX_DAMAGE_RUN      10

struct Rex : Enemy {

    enum {
        HIT_MASK = (1 << 12) | (1 << 13),  // head
    };

    enum {
        STATE_NONE,
        STATE_STOP,
        STATE_WALK,
        STATE_RUN,
        STATE_UNUSED,
        STATE_DEATH,
        STATE_BAWL,
        STATE_BITE,
        STATE_FATAL,
    };

    Rex(IGame *game, int entity) : Enemy(game, entity, 100, 341, (game->getLevel()->version & TR::VER_TR2) ? 1800.0f : 2000.0f, 1.0f) {
        jointChest = 10;
        jointHead  = 12;    
        nextState  = STATE_NONE;
    }

    virtual int getStateGround() {
        if (!flags.active)
            return state;

        if (!think(true))
            return state;

        if (nextState == state)
            nextState = STATE_NONE;

        if (targetDead) {
            return (state == STATE_STOP || state == STATE_WALK) ? STATE_WALK : STATE_STOP;
            if (state != STATE_STOP) return STATE_STOP;
            return STATE_WALK;
        }

        int mask = touching();
        if (mask) // any touch hurts, trampling more
            target->hit(state == STATE_RUN ? REX_DAMAGE_RUN : REX_DAMAGE_WALK, this);

        // if Lara is behind and watching Rex we need to rotate
        bool walk = targetFromView && !targetInView && mood != MOOD_ESCAPE;
        if (!walk && targetCanAttack && targetDist > REX_DIST_BITE && targetDist < REX_DIST_BITE_MAX)
            walk = true;

        switch (state) {
            case STATE_STOP : 
                if (nextState != STATE_NONE)                        return nextState;
                if (targetCanAttack && targetDist < REX_DIST_BITE)  return STATE_BITE;
                if (mood == MOOD_SLEEP || walk)                     return STATE_WALK;
                return STATE_RUN;
            case STATE_WALK :
                if (mood != MOOD_SLEEP || !walk)    return STATE_STOP;
                if (targetInView && randf() < 0.015f) {
                    nextState = STATE_BAWL;
                    return STATE_STOP;
                }
                break;
            case STATE_RUN :
                if ((targetCanAttack && targetDist < REX_DIST_WALK) || walk)
                    return STATE_STOP;
                if (targetInView && mood != MOOD_ESCAPE && randf() < 0.015f) {
                    nextState = STATE_BAWL;
                    return STATE_STOP;
                }
                if (mood == MOOD_SLEEP)
                    return STATE_STOP;
                break;
            case STATE_BITE :
                if (mask & HIT_MASK) {
                    target->hit(REX_DAMAGE, this, TR::HIT_REX);
                    return STATE_FATAL;
                }
                nextState = STATE_WALK;
                break;
        }

        return state;
    }

    virtual int getStateDeath() {
        return state == STATE_STOP ? STATE_DEATH : STATE_STOP;
    }

    virtual void updatePosition() {
        if (state == STATE_DEATH) {
            animation.overrideMask = 0;
            angle.z = 0.0f;
            return;
        }

        turn(state == STATE_RUN || state == STATE_WALK, state == STATE_RUN ? REX_TURN_FAST : REX_TURN_SLOW);

        Enemy::updatePosition();
        setOverrides(true, jointChest, jointHead);
        lookAt(target);
    }
};

#define RAPTOR_DIST_BITE        680
#define RAPTOR_DIST_ATTACK      (1024 + 512)

#define RAPTOR_TURN_FAST        (DEG2RAD * 120)
#define RAPTOR_TURN_SLOW        (DEG2RAD * 30)

struct Raptor : Enemy {

    enum {
        HIT_MASK = 0xFF7C00,  // hands and head
    };

    enum {
        ANIM_DEATH_1 = 9,
        ANIM_DEATH_2 = 10,
    };

    enum {
        STATE_NONE = -1,
        STATE_DEATH,
        STATE_STOP,
        STATE_WALK,
        STATE_RUN,
        STATE_ATTACK_1,
        STATE_UNUSED,
        STATE_BAWL,
        STATE_ATTACK_2,
        STATE_BITE,
    };

    Raptor(IGame *game, int entity) : Enemy(game, entity, 20, 341, 400.0f, 0.5f) {
        jointChest = -1;
        jointHead  = 21;    
        nextState  = STATE_NONE;
    }

    virtual int getStateGround() {
        if (!flags.active)
            return state;

        if (!think(true))
            return state;

        if (nextState == state)
            nextState = STATE_NONE;

        if (targetDead) {
            return (state == STATE_STOP || state == STATE_WALK) ? STATE_WALK : STATE_STOP;
            if (state != STATE_STOP) return STATE_STOP;
            return STATE_WALK;
        }

        int mask = collide(target);

        switch (state) {
            case STATE_STOP : 
                if (nextState != STATE_NONE) return nextState;
                if ((mask & HIT_MASK) || (targetCanAttack && targetDist < RAPTOR_DIST_BITE)) return STATE_BITE;
                if (targetCanAttack && targetDist < RAPTOR_DIST_ATTACK) return STATE_ATTACK_1;
                if (mood == MOOD_SLEEP) return STATE_WALK;
                return STATE_RUN;
            case STATE_WALK :
                if (nextState != STATE_NONE) return STATE_STOP;
                if (mood != MOOD_SLEEP) return STATE_STOP;
                if (targetInView && randf() < 0.01f) {
                    nextState = STATE_BAWL;
                    return STATE_STOP;
                }
                break;
            case STATE_RUN :
                if (nextState != STATE_NONE) return STATE_STOP;
                if (mask & HIT_MASK) return STATE_STOP;
                if (targetCanAttack && targetDist < RAPTOR_DIST_ATTACK)
                    return (randf() < 0.25) ? STATE_STOP : STATE_ATTACK_2;
                if (mood == MOOD_ESCAPE && targetInView) {
                    nextState = STATE_BAWL;
                    return STATE_STOP;
                }
                if (mood == MOOD_SLEEP)
                    return STATE_STOP;
                break;
            case STATE_ATTACK_1 :
            case STATE_ATTACK_2 :
            case STATE_BITE     :
                if (nextState == STATE_NONE && targetInView && (mask & HIT_MASK)) {
                    bite(22, vec3(0.0f, 66.0f, 318.0f), 100);                    
                    nextState = state == STATE_ATTACK_2 ? STATE_RUN : STATE_STOP;
                }
                break;
        }

        return state;
    }

    virtual int getStateDeath() {
        if (state == STATE_DEATH) return state;
        return animation.setAnim((rand() % 2) ? ANIM_DEATH_1 : ANIM_DEATH_2);
    }

    virtual void updatePosition() {
        if (state == STATE_DEATH) {
            animation.overrideMask = 0;
            angle.z = 0.0f;
            return;
        }

        turn(state == STATE_RUN || state == STATE_WALK, state == STATE_RUN ? RAPTOR_TURN_FAST : RAPTOR_TURN_SLOW);
        
        Enemy::updatePosition();
        setOverrides(true, jointChest, jointHead);
        lookAt(target);
    }
};

#define HUMAN_WAIT       0.01f
#define HUMAN_DIST_WALK  (1024 * 3)
#define HUMAN_DIST_SHOT  (1024 * 7)
#define HUMAN_TURN_SLOW  (DEG2RAD * 90)
#define HUMAN_TURN_FAST  (DEG2RAD * 180)

struct Human : Enemy {
    enum {
        STATE_NONE,
        STATE_STOP,
        STATE_WALK,
        STATE_RUN,
        STATE_AIM,
        STATE_DEATH,
        STATE_WAIT, // == STATE_FIRE for MrT and Cowboy
        STATE_FIRE
    };

    int jointGun;
    int animDeath;

    Human(IGame *game, int entity, float health) : Enemy(game, entity, health, 100, 0.0f, 1.0f), animDeath(-1) {
        jointGun   = 0;
        jointChest = 7;
        jointHead  = 8;
    }

    virtual void deactivate(bool removeFromList = false) {
        if (health <= 0.0f)
            onDead();
        Enemy::deactivate(removeFromList);
        getRoom().removeDynLight(entity);
    }

    virtual int getStateDeath() {
        return (animDeath == -1 || state == STATE_DEATH || state == STATE_NONE) ? STATE_DEATH : animation.setAnim(animDeath);
    }

    virtual int getStateGround() {
        if (!think(true))
            return state;
        return state;
    }

    virtual void updatePosition() {
        turn(state == STATE_RUN || state == STATE_WALK, state == STATE_RUN ? HUMAN_TURN_FAST : HUMAN_TURN_SLOW);
        
        Enemy::updatePosition();
        setOverrides(true, jointChest, jointHead);
        lookAt(target);
    }

    virtual void onDead() {}

    // TR2: from 87.5% close up down to 25% at 8 blocks, a sidestepping Lara is harder to hit (the 32-bit overflow of
    // the original is part of it)
    bool shotHitTR2() {
        const int32 range = SQR(8 * 1024);
        int32 dist2 = int32(SQR(targetDist));
        if (targetDist >= 8 * 1024)
            return false;

        vec3  d      = target->pos - pos;
        float facing = atan2f(d.x, d.z) - target->angle.y + PI;
        vec3  dir    = target->getDir();
        int32 speed  = int32((target->velocity.x * dir.x + target->velocity.z * dir.z) / 30.0f); // per frame
        int32 lead   = int32(speed * sinf(facing));

        int32 lag = int32(uint32(uint64(int64(lead) * range))) / 300;
        lag = int32(uint32(uint64(int64(dist2) + int64(int32(uint32(uint64(int64(lag) * lag)))))));
        if (lag > range)
            return false;

        int32 chance = 0x2000 + (range - dist2) / (range / 0x5000);
        return randControl() < chance;
    }

    bool doShot(float damage, const vec3 &muzzleOffset) {
        game->addMuzzleFlash(this, jointGun, muzzleOffset, -1);

    // hit or miss, a TR2 shot breaks the window on its way, from the chest height of the shooter to past the target
        if (level->version & TR::VER_TR2) {
            vec3 from = pos - vec3(0.0f, 768.0f, 0.0f);
            vec3 to   = target->pos - vec3(0.0f, 768.0f, 0.0f);
            vec3 dir  = vec3(to.x - from.x, 0.0f, to.z - from.z).normal();
            vec3 point;
            BreakableWindow *window = getWindowHit(level, from, to + dir * 2048.0f, point);
            if (window)
                window->smash();
        }

        bool hit;
        if (level->version & TR::VER_TR2)
            hit = shotHitTR2();
        else
            hit = targetDist < HUMAN_DIST_SHOT && randf() < ((HUMAN_DIST_SHOT - targetDist) / HUMAN_DIST_SHOT - 0.25f);

        if (hit) {
            bite(-1, vec3(0.0f), damage);
            game->addEntity(TR::Entity::BLOOD, target->getRoomIndex(), target->getJoint(rand() % target->getModel()->mCount).pos);
            game->playSound(target->stand == STAND_UNDERWATER ? TR::SND_HIT_UNDERWATER : TR::SND_HIT, target->pos, Sound::PAN);
            return true;
        }

        int16 roomIndex = getRoomIndex();
        TR::Room::Sector *sector = level->getSector(roomIndex, target->pos);
        float floor = level->getFloor(sector, target->pos) - 64.0f;
        vec3 p = vec3(target->pos.x + randf() * 512.0f - 256.0f, floor, target->pos.z + randf() * 512.0f - 256.0f);

        target->addRicochet(p, true);
        return false;
    }
};

#include "enemy_tr1.h"
#include "enemy_tr2.h"

#endif
