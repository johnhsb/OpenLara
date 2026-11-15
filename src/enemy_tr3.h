#ifndef H_ENEMY_TR3
#define H_ENEMY_TR3

// Tomb Raider III enemies

#include "enemy.h"

// TR3 monkeys of India: they sit about, eat and scratch, and leave Lara alone till she hurts one. That one comes at
// her from then on with a low or a high bite or a jump, it climbs up to a block and drops from one.
// One with nothing to do goes for a small medipack it can reach, takes it to an ambush marker and leaves it there, the
// one of the key marker takes the key and keeps it till it dies. The patrol markers send one to and fro between them
#define MONKEY_DAMAGE         40
#define MONKEY_DAMAGE_JUMP    50
#define MONKEY_TOUCH          0x2400
#define MONKEY_DIST_ROLL      1024
#define MONKEY_WALK_TURN      (DEG2RAD * 7 * 30)
#define MONKEY_RUN_TURN       (DEG2RAD * 11 * 30)
#define MONKEY_VAULT_SHIFT    128
#define MONKEY_DIST_ATTACK    341
#define MONKEY_DIST_JUMP      682
#define MONKEY_DIST_CALM      1024
#define MONKEY_SIT_CHANCE     0x100
#define MONKEY_MARKER_REACH   768
#define MONKEY_PICKUP_FRAME   12
#define MONKEY_SWAP_KEY       0x9090    // the meshes of the one of the key
#define MONKEY_SWAP_HELD      0x0100    // the head with the thing it took

struct Monkey : Enemy {

    enum {
        ANIM_DEATH   = 14,
        ANIM_CLIMB_4 = 17,
        ANIM_CLIMB_3 = 18,
        ANIM_CLIMB_2 = 19,
        ANIM_FALL_4  = 20,
        ANIM_FALL_3  = 21,
        ANIM_FALL_2  = 22,
    };

    enum {
        STATE_EMPTY,
        STATE_STOP,
        STATE_WALK,
        STATE_STAND,
        STATE_RUN,
        STATE_PICKUP,
        STATE_SIT,
        STATE_EAT,
        STATE_SCRATCH,
        STATE_ROLL,
        STATE_ANGRY,
        STATE_DEATH,
        STATE_ATTACK_LOW,
        STATE_ATTACK_HIGH,
        STATE_ATTACK_JUMP,
        STATE_CLIMB_4,
        STATE_CLIMB_3,
        STATE_CLIMB_2,
        STATE_FALL_4,
        STATE_FALL_3,
        STATE_FALL_2,
    };

    enum { // the markers of the level on its place
        BIT_GUARD  = 1,
        BIT_AMBUSH = 2,
        BIT_PATROL = 4,
        BIT_MODIFY = 8,
        BIT_FOLLOW = 16,
    };

    bool   hurtByLara;  // it is at peace with her till then
    int    aiBits;
    uint16 aiTag;       // the facing of its marker, the markers it goes to have the same
    bool   patrol2;     // on its way to the second patrol marker
    int    goal;        // the entity it goes to, -1 for none (Lara, once she hurt it)
    vec3   goalP;
    int    goalB;
    int    held;        // the pickup it carries, -1 for none

    Monkey(IGame *game, int entity) : Enemy(game, entity, 8, 102, 0.0f, 1.0f), hurtByLara(false), aiBits(0), aiTag(0), patrol2(false), goal(-1), goalB(TR::NO_BOX), held(-1) {
        ai         = AI_FIXED; // it goes for what it is after wherever it can reach it
        stepHeight =  1024;
        dropHeight = -1024;
        jointChest = -1;
        jointHead  = -1;

        const TR::Entity &me = getEntity();
        for (int i = 0; i < level->entitiesBaseCount; i++) {
            const TR::Entity &e = level->entities[i];
            int bit = markerBit(e.type);
            if (!bit || (aiBits & bit) || e.room != me.room || e.x != me.x || e.z != me.z)
                continue;
            aiBits |= bit;
            aiTag   = e.rotation.value;
        }
        updateSwap();
    }

    static int markerBit(TR::Entity::Type type) {
        switch (type) {
            case TR::Entity::AI_GUARD    : return BIT_GUARD;
            case TR::Entity::AI_AMBUSH   : return BIT_AMBUSH;
            case TR::Entity::AI_PATROL_1 : return BIT_PATROL;
            case TR::Entity::AI_MODIFY   : return BIT_MODIFY;
            case TR::Entity::AI_FOLLOW   : return BIT_FOLLOW;
            default : return 0;
        }
    }

    // a marker on the place of a creature is its own and nowhere to go to
    bool markerSpent(const TR::Entity &m) {
        if (!markerBit(m.type))
            return false;
        for (int i = 0; i < level->entitiesBaseCount; i++) {
            const TR::Entity &e = level->entities[i];
            if (e.isEnemy() && e.room == m.room && e.x == m.x && e.z == m.z)
                return true;
        }
        return false;
    }

    int boxAt(int room, const vec3 &p) {
        int dx, dz;
        return level->getSector(room, int(p.x), int(p.z), dx, dz).boxIndex;
    }

    // the first thing of the type it can reach: a marker of its tag (any for an ambush one) or a pickup on the floor
    // no monkey left there
    bool findGoal(TR::Entity::Type type, bool tagged) {
        uint16 *zones = getZones();
        for (int i = 0; i < level->entitiesBaseCount; i++) {
            const TR::Entity &e = level->entities[i];
            if (e.type != type)
                continue;
            vec3 p;
            int  room;
            if (e.isPickup()) {
                Controller *c = (Controller*)e.controller;
                if (!c || c->flags.invisible || c->flags.unused)
                    continue;
                p    = c->pos;
                room = c->getRoomIndex();
            } else {
                if ((tagged && e.rotation.value != aiTag) || markerSpent(e))
                    continue;
                p    = vec3(float(e.x), float(e.y), float(e.z));
                room = e.room;
            }
            int b = boxAt(room, p);
            if (b == TR::NO_BOX || zones[b] != zone)
                continue;
            goal  = i;
            goalP = p;
            goalB = b;
            return true;
        }
        return false;
    }

    TR::Entity::Type goalType() {
        return goal < 0 ? TR::Entity::NONE : level->entities[goal].type;
    }

    bool goalIsPickup() {
        return goal >= 0 && level->entities[goal].isPickup();
    }

    bool goalReached() {
        return fabsf(goalP.x - pos.x) < MONKEY_MARKER_REACH && fabsf(goalP.y - pos.y) < MONKEY_MARKER_REACH && fabsf(goalP.z - pos.z) < MONKEY_MARKER_REACH;
    }

    void updateGoal() {
        if (goalIsPickup()) { // Lara or another monkey was first
            Controller *c = (Controller*)level->entities[goal].controller;
            if (!c || c->flags.invisible)
                goal = -1;
            else
                goalP = c->pos;
        }

        if (aiBits & BIT_GUARD) {
            goal = -1;
            if (hurtByLara)
                aiBits &= ~BIT_GUARD;
        } else if (aiBits & BIT_PATROL) {
            TR::Entity::Type want = patrol2 ? TR::Entity::AI_PATROL_2 : TR::Entity::AI_PATROL_1;
            if (hurtByLara) {
                aiBits &= ~BIT_PATROL;
                goal = -1;
            } else if (goalType() != want)
                findGoal(want, true);
            else if (goalReached())
                patrol2 = !patrol2;
        } else if (aiBits & BIT_AMBUSH) {
            if ((aiBits & BIT_MODIFY) || hurtByLara) {
                if (goalType() != TR::Entity::AI_AMBUSH)
                    findGoal(TR::Entity::AI_AMBUSH, false);
            } else
                goal = -1;
        } else if (aiBits & BIT_FOLLOW) {
            goal = -1;
        } else if (held < 0) {
            if (hurtByLara)
                goal = -1; // she comes first
            else if (aiBits == BIT_MODIFY) {
                if (goalType() != TR::Entity::KEY_ITEM_4)
                    findGoal(TR::Entity::KEY_ITEM_4, false);
            } else if (goalType() != TR::Entity::MEDIKIT_SMALL)
                findGoal(TR::Entity::MEDIKIT_SMALL, false);
        } else if (goalIsPickup())
            goal = -1;
    }

    // the one of the key has meshes of its own, the head of both shows the thing they took
    void updateSwap() {
        bool   key  = aiBits == BIT_MODIFY;
        uint32 mask = (key ? MONKEY_SWAP_KEY : 0) | (held >= 0 ? MONKEY_SWAP_HELD : 0);
        int16  swap = int16(level->getModelIndex(key ? TR::Entity::MONKEY_KEY : TR::Entity::MONKEY_MEDIPACK) - 1);
        if (mask && swap > -1)
            meshSwap(1, swap, mask);
        else if (layers)
            layers[1].mask = 0;
    }

    void take() {
        Controller *c = (Controller*)level->entities[goal].controller;
        if (c && !c->flags.invisible) {
            c->flags.invisible = true;
            held = goal;
            if (aiBits != BIT_MODIFY)
                aiBits |= BIT_AMBUSH | BIT_MODIFY; // off to an ambush marker with it
        }
        goal = -1;
        updateSwap();
    }

    // at the marker the next monkey leaves it alone, the one of a dead monkey is free for all
    void leave(bool guarded) {
        if (held < 0)
            return;
        Controller *c = (Controller*)level->entities[held].controller;
        if (c) {
            TR::Level::FloorInfo info;
            getFloorInfo(getRoomIndex(), pos, info);
            c->pos       = vec3(pos.x, info.floor, pos.z);
            c->roomIndex = getRoomIndex();
            c->updateRoom();
            c->flags.invisible = false;
            c->flags.unused    = guarded;
        }
        held = -1;
        updateSwap();
    }

    virtual bool hasEnemy() {
        return hurtByLara || goal >= 0;
    }

    virtual vec3 goalPos() {
        return goal >= 0 ? goalP : target->pos;
    }

    virtual int goalBox() {
        return goal >= 0 ? goalB : target->box;
    }

    virtual bool getSaveData(SaveEntity &data) {
        Enemy::getSaveData(data);
        data.extraSize = sizeof(data.extra.monkey);
        data.extra.monkey.held = uint16(held + 1);
        data.extra.monkey.ai   = uint16(aiBits | (hurtByLara ? 0x20 : 0) | (patrol2 ? 0x40 : 0));
        return true;
    }

    virtual void setSaveData(const SaveEntity &data) {
        Enemy::setSaveData(data);
        if (data.extraSize < sizeof(data.extra.monkey)) // a save from before it kept these
            return;
        held       = int(data.extra.monkey.held) - 1;
        aiBits     = data.extra.monkey.ai & 0x1F;
        hurtByLara = (data.extra.monkey.ai & 0x20) != 0;
        patrol2    = (data.extra.monkey.ai & 0x40) != 0;
        goal       = -1;
        updateSwap();
    }

    virtual void hit(float damage, Controller *enemy = NULL, TR::HitType hitType = TR::HIT_DEFAULT) {
        if (enemy && enemy->getEntity().isLara() && damage > 0.0f)
            hurtByLara = true;
        Enemy::hit(damage, enemy, hitType);
        if (health <= 0.0f)
            leave(false);
    }

    void swipe(float damage) {
        if (flags.unused || !(touching() & MONKEY_TOUCH))
            return;
        bite(13, vec3(10.0f, 10.0f, 11.0f), damage);
        flags.unused = 1;
    }

    void face() { // it turns to what it is at without a step
        float w = DEG2RAD * 7.0f;
        angle.y += clamp(targetAngle, -w, w);
    }

    virtual int getStateGround() {
        updateGoal();

        if (!think(true))
            return state;

        bool toLara  = goal < 0 && hurtByLara;
        bool bored   = !hasEnemy() || mood == MOOD_SLEEP;
        bool escape  = toLara && mood == MOOD_ESCAPE;
        bool canBite = toLara && targetCanAttack;
        bool patrol  = (aiBits & BIT_PATROL) != 0;
        bool guard   = (aiBits & BIT_GUARD) != 0;
        // near enough to a pickup or an ambush marker to bend down for it
        bool atGoal  = goal >= 0 && !patrol && targetDist < MONKEY_DIST_JUMP && fabsf(goalP.y - pos.y) < 256.0f;
        // a calm one has no target, so it doesn't walk into her on its way
        bool nearLara = !hasEnemy() && target && (target->pos - pos).length2() < SQR(float(MONKEY_DIST_CALM));

        if (state != STATE_WALK)
            nextState = STATE_EMPTY;

        switch (state) {
            case STATE_SIT :
                flags.unused = 0;
                if (guard) {
                    if (!(randControl() & 0xF)) return (randControl() & 1) ? STATE_SCRATCH : STATE_EAT;
                    break;
                }
                if (patrol)
                    return STATE_WALK;
                if (escape)
                    return STATE_STAND;
                if (bored) {
                    if (!(randControl() & 0xF) && !nearLara) return STATE_WALK;
                    if (!(randControl() & 0xF)) return (randControl() & 1) ? STATE_SCRATCH : STATE_EAT;
                    break;
                }
                return STATE_STAND;
            case STATE_WALK :
                if (patrol)
                    break;
                if (escape)
                    return STATE_RUN;
                if (bored) {
                    if (nearLara || randControl() < MONKEY_SIT_CHANCE)
                        nextState = STATE_SIT; // it sits down at the end of the step
                    if (nextState == STATE_SIT)
                        return STATE_SIT;
                    break;
                }
                return STATE_STAND;
            case STATE_STAND :
                flags.unused = 0;
                if (guard) {
                    if (!(randControl() & 0xF)) return (randControl() & 1) ? STATE_ANGRY : STATE_SIT;
                    break;
                }
                if (patrol)
                    return STATE_WALK;
                if (escape)
                    return STATE_RUN;
                if (bored) {
                    if (!(randControl() & 0xF) && !nearLara) return STATE_WALK;
                    if (!(randControl() & 0xF)) return (randControl() & 1) ? STATE_ANGRY : STATE_SIT;
                    break;
                }
                if (canBite && targetDist < MONKEY_DIST_ATTACK)
                    return (target->pos.y < pos.y) ? STATE_ATTACK_HIGH : STATE_ATTACK_LOW;
                if (canBite && targetDist < MONKEY_DIST_JUMP)
                    return STATE_ATTACK_JUMP;
                if (atGoal)
                    return STATE_PICKUP;
                if (canBite && targetDist < MONKEY_DIST_ROLL)
                    return STATE_ROLL;
                return STATE_RUN;
            case STATE_RUN :
                if (guard)
                    return STATE_STAND;
                if (escape)
                    break;
                if (bored)
                    return STATE_ROLL;
                if (targetDist < MONKEY_DIST_JUMP)
                    return STATE_STAND;
                if (canBite && targetDist < MONKEY_DIST_ROLL)
                    return STATE_ROLL;
                break;
            case STATE_PICKUP :
                if (goal < 0)
                    break;
                if (!flags.unused && animation.frameIndex >= MONKEY_PICKUP_FRAME) {
                    flags.unused = 1;
                    if (goalIsPickup())
                        take();
                    else if (goalType() == TR::Entity::AI_AMBUSH && held >= 0) {
                        leave(true);
                        aiBits = 0;
                        goal   = -1;
                    }
                } else
                    face();
                break;
            case STATE_ATTACK_LOW  :
            case STATE_ATTACK_HIGH : face(); swipe(MONKEY_DAMAGE);      break;
            case STATE_ATTACK_JUMP : face(); swipe(MONKEY_DAMAGE_JUMP); break;
        }
        return state;
    }

    virtual int getStateDeath() {
        if (state == STATE_DEATH)
            return state;
        return animation.setAnim(ANIM_DEATH);
    }

    virtual void updatePosition() {
        if (state == STATE_DEATH) {
            animation.overrideMask = 0;
            return;
        }

        bool vaulting = state >= STATE_CLIMB_4;

        if (!vaulting) {
            float w = 0.0f;
            if (state == STATE_WALK) w = MONKEY_WALK_TURN;
            if (state == STATE_RUN || state == STATE_ROLL || state == STATE_ATTACK_JUMP) w = MONKEY_RUN_TURN;
            turn(state == STATE_RUN, w);
        }

        vec3 old = pos;
        TR::Level::FloorInfo infoA, infoB;
        getFloorInfo(getRoomIndex(), old, infoA);
        old.y = infoA.floor;

        Enemy::updatePosition();

        if (!vaulting) {
            getFloorInfo(getRoomIndex(), pos, infoB);
            int clicks = int(floorf((old.y - infoB.floor) / 256.0f + 0.5f));
            if (((clicks >= 2 && clicks <= 4) || (clicks <= -2 && clicks >= -4)) && vaultTo(old, MONKEY_VAULT_SHIFT)) {
                switch (clicks) {
                    case  2 : animation.setAnim(ANIM_CLIMB_2); break;
                    case  3 : animation.setAnim(ANIM_CLIMB_3); break;
                    case  4 : animation.setAnim(ANIM_CLIMB_4); break;
                    case -2 : animation.setAnim(ANIM_FALL_2);  break;
                    case -3 : animation.setAnim(ANIM_FALL_3);  break;
                    default : animation.setAnim(ANIM_FALL_4);
                }
            }
        }
    }
};

#endif
