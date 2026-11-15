#ifndef H_ENEMY_TR3
#define H_ENEMY_TR3

// Tomb Raider III enemies

#include "enemy.h"

// TR3 monkeys of India: they sit about, eat and scratch, and leave Lara alone till she hurts one. That one comes at
// her from then on with a low or a high bite or a jump, it climbs up to a block and drops from one
// TODO the monkeys that take the medipacks and the keys and the AI objects
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

    bool hurtByLara; // it is at peace with her till then (not saved, as the damage of the monks of TR2)

    Monkey(IGame *game, int entity) : Enemy(game, entity, 8, 102, 0.0f, 1.0f), hurtByLara(false) {
        ai         = AI_FIXED; // once it turned on her it comes for her wherever it can reach her
        stepHeight =  1024;
        dropHeight = -1024;
        jointChest = -1;
        jointHead  = -1;
    }

    virtual bool hasEnemy() {
        return hurtByLara;
    }

    virtual void hit(float damage, Controller *enemy = NULL, TR::HitType hitType = TR::HIT_DEFAULT) {
        if (enemy && enemy->getEntity().isLara() && damage > 0.0f)
            hurtByLara = true;
        Enemy::hit(damage, enemy, hitType);
    }

    void swipe(float damage) {
        if (flags.unused || !(touching() & MONKEY_TOUCH))
            return;
        bite(13, vec3(10.0f, 10.0f, 11.0f), damage);
        flags.unused = 1;
    }

    virtual int getStateGround() {
        if (!think(true))
            return state;

        bool bored  = !hurtByLara || mood == MOOD_SLEEP;
        bool escape = hurtByLara && mood == MOOD_ESCAPE;
        bool canBite = hurtByLara && targetCanAttack;
        // a calm one has no target, so it doesn't walk into her on its way
        bool nearLara = !hurtByLara && target && (target->pos - pos).length2() < SQR(float(MONKEY_DIST_CALM));

        switch (state) {
            case STATE_SIT :
                flags.unused = 0;
                if (escape)
                    return STATE_STAND;
                if (bored) {
                    if (!(randControl() & 0xF) && !nearLara) return STATE_WALK;
                    if (!(randControl() & 0xF)) return (randControl() & 1) ? STATE_SCRATCH : STATE_EAT;
                    break;
                }
                return STATE_STAND;
            case STATE_WALK :
                if (escape)
                    return STATE_RUN;
                if (bored) {
                    if (nearLara || randControl() < MONKEY_SIT_CHANCE)
                        return STATE_SIT;
                    break;
                }
                return STATE_STAND;
            case STATE_STAND :
                flags.unused = 0;
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
                if (canBite && targetDist < MONKEY_DIST_ROLL)
                    return STATE_ROLL;
                return STATE_RUN;
            case STATE_RUN :
                if (escape)
                    break;
                if (bored)
                    return STATE_ROLL;
                if (targetDist < MONKEY_DIST_JUMP)
                    return STATE_STAND;
                if (canBite && targetDist < MONKEY_DIST_ROLL)
                    return STATE_ROLL;
                break;
            case STATE_ATTACK_LOW  :
            case STATE_ATTACK_HIGH : swipe(MONKEY_DAMAGE);      break;
            case STATE_ATTACK_JUMP : swipe(MONKEY_DAMAGE_JUMP); break;
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
