#ifndef H_ENEMY_TR3
#define H_ENEMY_TR3

// Tomb Raider III enemies

#include "enemy.h"

// TR3 monkeys of India: they sit about, eat and scratch, and leave Lara alone till she hurts them. A hurt one comes
// at her with a low or a high swipe or a jump, it climbs up to a block and drops from one
// TODO the monkeys that take the medipacks and the keys (MONKEY_MEDIPACK, MONKEY_KEY) and the AI objects
#define MONKEY_DAMAGE         40
#define MONKEY_WALK_TURN      (DEG2RAD * 7 * 30)
#define MONKEY_RUN_TURN       (DEG2RAD * 11 * 30)
#define MONKEY_VAULT_SHIFT    128
#define MONKEY_DIST_ATTACK    341
#define MONKEY_DIST_JUMP      682
#define MONKEY_DIST_CALM      1024
#define MONKEY_WALK_CHANCE    0x100
#define MONKEY_EAT_CHANCE     0x200
#define MONKEY_SCRATCH_CHANCE 0x300
#define MONKEY_SIT_CHANCE     0x100
#define MONKEY_ROLL_CHANCE    0x100
#define MONKEY_ANGRY_CHANCE   0x200

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

    Monkey(IGame *game, int entity) : Enemy(game, entity, 8, 102, 0.0f, 1.0f) {
        stepHeight =  1024;
        dropHeight = -1024;
        jointChest = -1;
        jointHead  = -1;
    }

    void swipe() {
        if (flags.unused || !touching())
            return;
        bite(-1, vec3(0.0f), MONKEY_DAMAGE);
        flags.unused = 1;
    }

    virtual int getStateGround() {
        if (!think(false))
            return state;

        bool angry = wound; // hurt by Lara
        if (!angry)
            mood = MOOD_SLEEP;
        bool nearLara = targetInView && targetDist < MONKEY_DIST_CALM; // a calm one doesn't walk into her

        switch (state) {
            case STATE_SIT :
                if (angry)
                    return STATE_STAND;
                {
                    int r = randControl();
                    if (r < MONKEY_WALK_CHANCE && !nearLara) return STATE_WALK;
                    if (r < MONKEY_EAT_CHANCE)     return STATE_EAT;
                    if (r < MONKEY_SCRATCH_CHANCE) return STATE_SCRATCH;
                }
                break;
            case STATE_WALK :
                if (angry)
                    return STATE_RUN;
                if (nearLara || randControl() < MONKEY_SIT_CHANCE)
                    return STATE_SIT;
                break;
            case STATE_STAND :
                flags.unused = 0;
                if (!angry)
                    return (nearLara || randControl() < 0x4000) ? STATE_SIT : STATE_WALK;
                if (mood == MOOD_ESCAPE)
                    return STATE_RUN;
                if (targetInView && targetDist < MONKEY_DIST_ATTACK)
                    return (randControl() < 0x4000) ? STATE_ATTACK_LOW : STATE_ATTACK_HIGH;
                if (targetInView && targetDist < MONKEY_DIST_JUMP)
                    return STATE_ATTACK_JUMP;
                if (randControl() < MONKEY_ANGRY_CHANCE)
                    return STATE_ANGRY;
                return STATE_RUN;
            case STATE_RUN :
                if (!angry)
                    return STATE_STAND;
                if (mood == MOOD_ESCAPE)
                    break;
                if (targetInView && targetDist < MONKEY_DIST_JUMP)
                    return STATE_STAND;
                if (randControl() < MONKEY_ROLL_CHANCE)
                    return STATE_ROLL;
                break;
            case STATE_ATTACK_LOW  :
            case STATE_ATTACK_HIGH :
            case STATE_ATTACK_JUMP :
                swipe();
                break;
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
