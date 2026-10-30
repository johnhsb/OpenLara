#ifndef H_ENEMY_TR2
#define H_ENEMY_TR2

// the enemies of Tomb Raider 2 (and the later games reusing them)

#include "enemy.h"

// TR2 masked goons of Venice and the Opera House ("cult_1" in the original), they shoot pistols
#define GOON_DAMAGE         50
#define GOON_TURN           (DEG2RAD * 5 * 30)
#define GOON_DIST_RUN       (1024 * 2)
#define GOON_DIST_SHOT      (1024 * 8)
#define GOON_POSE           0x500
#define GOON_UNPOSE         0x100
#define GOON_WALK           0xA00
#define GOON_UNWALK         0x300

struct MaskedGoon : Human {

    enum {
        ANIM_DEATH = 20,
    };

    enum {
        STATE_EMPTY,
        STATE_WALK,
        STATE_RUN,
        STATE_STOP,
        STATE_WAIT_1,
        STATE_WAIT_2,
        STATE_AIM_1,
        STATE_SHOOT_1,
        STATE_AIM_2,
        STATE_SHOOT_2,
        STATE_AIM_3,
        STATE_SHOOT_3,
        STATE_DEATH,
        STATE_DEATH_2, // the second death animation has a state of its own
    };

    MaskedGoon(IGame *game, int entity) : Human(game, entity, 25) {
        jointGun   = 10;
        jointChest = -1;
        jointHead  = 1;
        animDeath  = ANIM_DEATH + (randControl() < 0x4000 ? 0 : 1);
        nextState  = STATE_EMPTY;

    // the meshes of the variants
        if (randControl() < 0x4000)
            visibleMask &= ~0x30;
        if (getEntity().type == TR::Entity::ENEMY_GOON_MASK_3)
            visibleMask &= ~0x1F8000;
    }

    bool canShoot() {
        return targetIsVisible(GOON_DIST_SHOT);
    }

    void fire() {
        doShot(GOON_DAMAGE, vec3(3.0f, -56.0f, 151.0f)); // the muzzle (3, 331, 56) of the gun mesh in the flash space
    }

    virtual int getStateGround() {
        if (!think(false))
            return state;

        bool bored = mood == MOOD_SLEEP;

        if (nextState == state)
            nextState = STATE_EMPTY;

        switch (state) {
            case STATE_STOP :
                return nextState != STATE_EMPTY ? nextState : STATE_WAIT_1;
            case STATE_WAIT_1 :
                if (mood == MOOD_ESCAPE)
                    nextState = STATE_RUN;
                else if (canShoot())
                    nextState = randControl() < 0x4000 ? STATE_AIM_1 : STATE_AIM_3;
                else if (bored && targetInView) {
                    int r = randControl();
                    if (r < GOON_POSE)
                        nextState = STATE_WAIT_2;
                    else if (r < GOON_WALK)
                        nextState = STATE_WALK;
                    else
                        break;
                } else if (bored || targetDist < GOON_DIST_RUN)
                    nextState = STATE_WALK;
                else
                    nextState = STATE_RUN;
                return STATE_STOP;
            case STATE_WAIT_2 :
                if (canShoot()) {
                    nextState = STATE_AIM_1;
                    return STATE_STOP;
                }
                if (!bored || randControl() < GOON_UNPOSE || !targetInView)
                    return STATE_STOP;
                break;
            case STATE_WALK :
                if (mood == MOOD_ESCAPE)
                    return STATE_RUN;
                if (canShoot()) {
                    nextState = randControl() < 0x4000 ? STATE_AIM_1 : STATE_AIM_3;
                    return STATE_STOP;
                }
                if (targetDist > GOON_DIST_RUN || !targetInView)
                    return STATE_RUN;
                if (bored && randControl() < GOON_UNWALK)
                    return STATE_STOP;
                break;
            case STATE_RUN :
                flags.unused = 0;
                if (mood == MOOD_ESCAPE)
                    return canShoot() ? STATE_SHOOT_2 : state;
                if (canShoot())
                    return (targetDist < GOON_DIST_RUN || zone != targetZone()) ? STATE_STOP : STATE_SHOOT_2;
                if (bored)
                    return STATE_STOP;
                break;
            case STATE_AIM_1 :
            case STATE_AIM_3 :
                flags.unused = 0;
                if (mood == MOOD_ESCAPE || !canShoot())
                    return STATE_STOP;
                return state == STATE_AIM_1 ? STATE_SHOOT_1 : STATE_SHOOT_3;
            case STATE_SHOOT_1 :
            case STATE_SHOOT_3 :
                if (flags.unused == 0) {
                    fire();
                    flags.unused = 1;
                }
                return state == STATE_SHOOT_1 ? STATE_AIM_1 : STATE_AIM_3; // aim again to check where Lara is
            case STATE_SHOOT_2 : // shooting on the run, one shot and run again
                if (flags.unused == 0 && canShoot()) {
                    fire();
                    flags.unused = 1;
                }
                return STATE_RUN;
        }
        return state;
    }

    bool isDying() const {
        return state == STATE_DEATH || state == STATE_DEATH_2;
    }

    virtual int getStateDeath() {
        if (isDying())
            return state;
        return animation.setAnim(animDeath);
    }

    virtual void updatePosition() {
        if (isDying()) {
            animation.overrideMask = 0;
            return;
        }

        turn(state == STATE_RUN, (state == STATE_WALK || state == STATE_RUN) ? GOON_TURN : 0.0f); // no turning while aiming, the head follows Lara

        Enemy::updatePosition();
        setOverrides(true, jointChest, jointHead);
        lookAt(target);
    }
};

// TR2 shotgun goons of the Opera House and the home ("cult_3" in the original), a double shotgun
#define SHOTGUN_GOON_DAMAGE     50
#define SHOTGUN_GOON_TURN       (DEG2RAD * 3 * 30)
#define SHOTGUN_GOON_DIST_STOP  (1024 * 3)
#define SHOTGUN_GOON_DIST_RUN   (1024 * 5)
#define SHOTGUN_GOON_DIST_SHOT  (1024 * 8)

struct ShotgunGoon : Human {

    enum {
        ANIM_WAIT  = 3,
        ANIM_DEATH = 32,
    };

    enum {
        STATE_EMPTY,
        STATE_STOP,
        STATE_WAIT,
        STATE_WALK,
        STATE_RUN,
        STATE_AIM_L,
        STATE_AIM_R,
        STATE_SHOOT_L,
        STATE_SHOOT_R,
        STATE_AIM_2,
        STATE_SHOOT_2,
        STATE_DEATH,
    };

    ShotgunGoon(IGame *game, int entity) : Human(game, entity, 150) {
        jointChest = -1;
        jointHead  = 11;
        animDeath  = ANIM_DEATH;
        animation.setAnim(ANIM_WAIT);
    }

    bool canShoot() {
        return targetIsVisible(SHOTGUN_GOON_DIST_SHOT);
    }

    void fire(bool left) { // the muzzle (-2/2, 275, 23) of the hand mesh in the flash space
        jointGun = left ? 6 : 10;
        doShot(SHOTGUN_GOON_DAMAGE, vec3(left ? -2.0f : 2.0f, -23.0f, 95.0f));
    }

    virtual int getStateGround() {
        if (!think(true))
            return state;

        bool bored    = mood == MOOD_SLEEP;
        bool laraDead = target->health <= 0.0f;

        switch (state) {
            case STATE_STOP :
            case STATE_WAIT :
                if (bored && laraDead)
                    return STATE_WAIT;
                if (canShoot())
                    return targetDist > SHOTGUN_GOON_DIST_STOP ? STATE_WALK : STATE_AIM_2;
                if (mood == MOOD_ESCAPE)
                    return STATE_RUN;
                if (mood == MOOD_ATTACK)
                    return (targetDist > SHOTGUN_GOON_DIST_RUN || !targetInView) ? STATE_RUN : STATE_WALK;
                if (mood == MOOD_STALK || !targetInView)
                    return STATE_WALK;
                break;
            case STATE_WALK :
                if (canShoot()) {
                    if (targetDist < SHOTGUN_GOON_DIST_STOP || zone != targetZone())
                        return STATE_STOP;
                    return targetAngle < 0.0f ? STATE_AIM_L : STATE_AIM_R;
                }
                if (mood == MOOD_ESCAPE)
                    return STATE_RUN;
                if (mood == MOOD_STALK || mood == MOOD_ATTACK) {
                    if (targetDist > SHOTGUN_GOON_DIST_RUN || !targetInView)
                        return STATE_RUN;
                } else if (laraDead)
                    return STATE_WAIT;
                else if (targetInView)
                    return STATE_STOP;
                break;
            case STATE_RUN :
                if (canShoot()) {
                    if (zone != targetZone())
                        return STATE_STOP;
                    return targetAngle < 0.0f ? STATE_AIM_L : STATE_AIM_R;
                }
                if (bored)
                    return laraDead ? STATE_WAIT : STATE_STOP;
                if (targetInView && targetDist < SHOTGUN_GOON_DIST_RUN)
                    return STATE_WALK;
                break;
            case STATE_AIM_L :
            case STATE_AIM_R :
                flags.unused = 0;
                if (canShoot())
                    return state == STATE_AIM_L ? STATE_SHOOT_L : STATE_SHOOT_R;
                return STATE_WALK;
            case STATE_AIM_2 :
                flags.unused = 0;
                return canShoot() ? STATE_SHOOT_2 : STATE_STOP;
            case STATE_SHOOT_L :
            case STATE_SHOOT_R :
                if (flags.unused == 0) {
                    fire(state == STATE_SHOOT_L);
                    flags.unused = 1;
                }
                break;
            case STATE_SHOOT_2 :
                if (flags.unused == 0) {
                    fire(true);
                    fire(false);
                    flags.unused = 1;
                }
                break;
        }
        return state;
    }

    virtual int getStateDeath() {
        if (state == STATE_DEATH)
            return state;
        return animation.setAnim(animDeath);
    }

    virtual void updatePosition() {
        if (state == STATE_DEATH) {
            animation.overrideMask = 0;
            return;
        }

        turn(state == STATE_RUN, (state == STATE_WALK || state == STATE_RUN) ? SHOTGUN_GOON_TURN : 0.0f);

        Enemy::updatePosition();
        setOverrides(true, jointChest, jointHead);
        lookAt(target);
    }
};

// TR2 club thugs of Venice, the Opera House and the rig ("worker_3" in the original), they climb up to a block
#define THUG_HIT_DAMAGE     80
#define THUG_SWIPE_DAMAGE   100
#define THUG_WALK_TURN      (DEG2RAD * 5 * 30)
#define THUG_RUN_TURN       (DEG2RAD * 6 * 30)
#define THUG_DIST_ATTACK_0  512
#define THUG_DIST_ATTACK_1  1024
#define THUG_DIST_ATTACK_2  1536
#define THUG_DIST_WALK      2048
#define THUG_WALK_CHANCE    0x100
#define THUG_WAIT_CHANCE    0x100
#define THUG_VAULT_SHIFT    260
#define SND_TR2_ENEMY_HIT_1 71
#define SND_TR2_ENEMY_HIT_2 72

struct ClubThug : Enemy {

    enum {
        HIT_MASK = 0x600,
    };

    enum {
        ANIM_DEATH   = 26,
        ANIM_CLIMB_3 = 27,
        ANIM_CLIMB_1 = 28,
        ANIM_CLIMB_2 = 29,
        ANIM_FALL_3  = 30,
    };

    enum {
        STATE_EMPTY,
        STATE_STOP,
        STATE_WALK,
        STATE_PUNCH_2,
        STATE_AIM_2,
        STATE_WAIT,
        STATE_AIM_1,
        STATE_AIM_0,
        STATE_PUNCH_1,
        STATE_PUNCH_0,
        STATE_RUN,
        STATE_DEATH,
        STATE_CLIMB_3,
        STATE_CLIMB_1,
        STATE_CLIMB_2,
        STATE_FALL_3,
    };

    int climbShift; // the second thugs (worker_4) have no climbs of their own, they take the ones of the first

    ClubThug(IGame *game, int entity) : Enemy(game, entity, 27, 102, 0.0f, 1.0f), climbShift(0) {
        stepHeight =  1024;
        dropHeight = -1024;
        jointChest = 1;
        jointHead  = 5;
        nextState  = STATE_EMPTY;

        int index = level->getModelIndex(TR::Entity::ENEMY_GOON_STICK_1);
        if (getEntity().type == TR::Entity::ENEMY_GOON_STICK_2 && index > 0)
            climbShift = level->models[index - 1].animation - getModel()->animation;
    }

    void punch(float damage, int hitFlag, int sound) {
        if (flags.unused == hitFlag || !(touching() & HIT_MASK))
            return;
        bite(10, vec3(247.0f, 10.0f, 11.0f), damage);
        game->playSound(sound, pos, Sound::PAN);
        flags.unused = hitFlag;
    }

    virtual int getStateGround() {
        if (!think(false))
            return state;

        if (nextState == state)
            nextState = STATE_EMPTY;

        bool bored = mood == MOOD_SLEEP;
        bool reach = targetCanAttack; // ahead and about Lara's height

        switch (state) {
            case STATE_STOP :
                flags.unused = 0;
                if (mood == MOOD_ESCAPE)
                    return STATE_RUN;
                if (bored) {
                    if (nextState != STATE_EMPTY)
                        return nextState;
                    return randControl() < 0x4000 ? STATE_WALK : STATE_WAIT;
                }
                if (!reach)                            return STATE_RUN;
                if (targetDist < THUG_DIST_ATTACK_0)   return STATE_AIM_0;
                if (targetDist < THUG_DIST_ATTACK_1)   return STATE_AIM_1;
                if (targetDist < THUG_DIST_WALK)       return STATE_WALK;
                return STATE_RUN;
            case STATE_WAIT :
                if (!bored)
                    return STATE_STOP;
                if (randControl() < THUG_WALK_CHANCE) {
                    nextState = STATE_WALK;
                    return STATE_STOP;
                }
                break;
            case STATE_WALK :
                if (mood == MOOD_ESCAPE)
                    return STATE_RUN;
                if (bored) {
                    if (randControl() < THUG_WAIT_CHANCE) {
                        nextState = STATE_WAIT;
                        return STATE_STOP;
                    }
                    break;
                }
                if (!reach)                            return STATE_RUN;
                if (targetDist < THUG_DIST_ATTACK_0)   return STATE_STOP;
                if (targetDist < THUG_DIST_ATTACK_2)   return STATE_AIM_2;
                return STATE_RUN;
            case STATE_RUN :
                if (mood == MOOD_ESCAPE)
                    break;
                if (bored || (targetInView && targetDist < THUG_DIST_WALK))
                    return STATE_WALK;
                break;
            case STATE_AIM_0 :
                flags.unused = 0;
                return (reach && targetDist < THUG_DIST_ATTACK_0) ? STATE_PUNCH_0 : STATE_STOP;
            case STATE_AIM_1 :
                flags.unused = 0;
                return (targetInView && targetDist < THUG_DIST_ATTACK_1) ? STATE_PUNCH_1 : STATE_STOP;
            case STATE_AIM_2 :
                flags.unused = 0;
                return (reach && targetDist < THUG_DIST_ATTACK_2) ? STATE_PUNCH_2 : STATE_WALK;
            case STATE_PUNCH_0 :
                punch(THUG_HIT_DAMAGE, 1, SND_TR2_ENEMY_HIT_2);
                break;
            case STATE_PUNCH_1 :
                punch(THUG_HIT_DAMAGE, 1, SND_TR2_ENEMY_HIT_1);
                if (targetInView && targetDist > THUG_DIST_ATTACK_1 && targetDist < THUG_DIST_ATTACK_2)
                    return STATE_PUNCH_2;
                break;
            case STATE_PUNCH_2 :
                punch(THUG_SWIPE_DAMAGE, 2, SND_TR2_ENEMY_HIT_1);
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

        bool vaulting = state >= STATE_CLIMB_3;

        if (!vaulting)
            turn(state == STATE_RUN, state == STATE_RUN ? THUG_RUN_TURN : (state == STATE_WALK ? THUG_WALK_TURN : 0.0f));

        vec3 old = pos;
        TR::Level::FloorInfo infoA, infoB;
        getFloorInfo(getRoomIndex(), old, infoA);
        old.y = infoA.floor;

        Enemy::updatePosition();

        if (!vaulting) {
            getFloorInfo(getRoomIndex(), pos, infoB);
            int clicks = int(floorf((old.y - infoB.floor) / 256.0f + 0.5f));
            if (clicks >= 2 && clicks <= 4)
                vault(old, clicks);
            else if (clicks <= -4 && clicks >= -5)
                vault(old, -4);
        }

        setOverrides(true, jointChest, jointHead);
        lookAt(target);
    }


// climb up 2..4 clicks or jump down a block at the edge of the sector
    void vault(const vec3 &old, int clicks) {
        if (!vaultTo(old, THUG_VAULT_SHIFT))
            return;

        switch (clicks) {
            case  2 : animation.setAnim(ANIM_CLIMB_1 + climbShift); break;
            case  3 : animation.setAnim(ANIM_CLIMB_2 + climbShift); break;
            case  4 : animation.setAnim(ANIM_CLIMB_3 + climbShift); break;
            case -4 : animation.setAnim(ANIM_FALL_3  + climbShift); break;
        }
    }
};

// TR2 yetis of Tibet: they punch, thump or charge Lara and climb up to a block or jump down from one like the club
// thugs, a yeti killing her beats her to death in its hands (the yeti kill of the special animations)
#define YETI_PUNCH_DAMAGE   100
#define YETI_THUMP_DAMAGE   150
#define YETI_CHARGE_DAMAGE  200
#define YETI_HIT_R          0x0700
#define YETI_HIT_L          0x3800
#define YETI_WALK_TURN      (DEG2RAD * 4 * 30)
#define YETI_RUN_TURN       (DEG2RAD * 6 * 30)
#define YETI_VAULT_SHIFT    300
#define YETI_DIST_ATTACK_1  512
#define YETI_DIST_ATTACK_2  682
#define YETI_DIST_ATTACK_3  2048
#define YETI_DIST_RUN       2048
#define YETI_WAIT_1_CHANCE  0x100
#define YETI_WAIT_2_CHANCE  0x200
#define YETI_WALK_CHANCE    0x300
#define YETI_ROAR_CHANCE    0x200

struct Yeti : Enemy {

    enum {
        ANIM_DEATH   = 31,
        ANIM_CLIMB_3 = 32,
        ANIM_CLIMB_2 = 33,
        ANIM_CLIMB_1 = 34,
        ANIM_FALL    = 35,
        ANIM_KILL    = 36,
    };

    enum {
        STATE_EMPTY,
        STATE_RUN,
        STATE_STOP,
        STATE_WALK,
        STATE_ATTACK_1,
        STATE_ATTACK_2,
        STATE_ATTACK_3,
        STATE_WAIT_1,
        STATE_DEATH,
        STATE_WAIT_2,
        STATE_CLIMB_1,
        STATE_CLIMB_2,
        STATE_CLIMB_3,
        STATE_FALL,
        STATE_KILL,
    };

    float maxTurn; // kept by the attacks from the state they come from, as in the original

    Yeti(IGame *game, int entity) : Enemy(game, entity, 30, 128, 0.0f, 1.0f), maxTurn(0.0f) {
        stepHeight =  1024;
        dropHeight = -1024;
        jointChest = 7;
        jointHead  = 15;
        nextState  = STATE_EMPTY;
    }

    // a blow of the hands touching her, a killing one plays the yeti kill
    void blow(int mask, float damage) {
        int touch = touching();
        if (flags.unused || !(touch & mask))
            return;
        if (touch & YETI_HIT_L)
            game->addEntity(TR::Entity::BLOOD, target->getRoomIndex(), getJoint(13) * vec3(12.0f, 101.0f, 19.0f));
        if (touch & YETI_HIT_R)
            game->addEntity(TR::Entity::BLOOD, target->getRoomIndex(), getJoint(10) * vec3(12.0f, 101.0f, 19.0f));
        bool alive = target->health > 0.0f;
        target->hit(damage, this, target->health <= damage ? TR::HIT_YETI : TR::HIT_DEFAULT);
        if (alive && target->health <= 0.0f)
            animation.setAnim(ANIM_KILL);
        flags.unused = 1;
    }

    virtual int getStateGround() {
        if (!think(true) || state == STATE_KILL)
            return state;

        if (nextState == state)
            nextState = STATE_EMPTY;

        bool alive  = target->health > 0.0f;
        bool bored  = mood == MOOD_SLEEP;
        bool ahead  = targetInView;
        int  r;

        switch (state) {
            case STATE_STOP :
                flags.unused = 0;
                maxTurn = 0.0f;
                if (mood == MOOD_ESCAPE)
                    return STATE_RUN;
                if (nextState != STATE_EMPTY)
                    return nextState;
                if (bored) {
                    r = randControl();
                    if (r < YETI_WAIT_1_CHANCE || !alive) return STATE_WAIT_1;
                    if (r < YETI_WAIT_2_CHANCE)           return STATE_WAIT_2;
                    if (r < YETI_WALK_CHANCE)             return STATE_WALK;
                    break;
                }
                if (ahead && targetDist < YETI_DIST_ATTACK_1 && randControl() < 0x4000)
                    return STATE_ATTACK_1;
                if (ahead && targetDist < YETI_DIST_ATTACK_2)
                    return STATE_ATTACK_2;
                return mood == MOOD_STALK ? STATE_WALK : STATE_RUN;
            case STATE_WAIT_1 :
            case STATE_WAIT_2 :
                if (mood == MOOD_ESCAPE || wound)
                    return STATE_STOP;
                if (bored) {
                    r = randControl();
                    if (state == STATE_WAIT_1) {
                        if (!alive) break;
                        if (r < YETI_WAIT_1_CHANCE) return STATE_STOP;
                        if (r < YETI_WAIT_2_CHANCE) return STATE_WAIT_2;
                    } else {
                        if (r < YETI_WAIT_1_CHANCE || !alive) return STATE_WAIT_1;
                        if (r < YETI_WAIT_2_CHANCE)           return STATE_STOP;
                    }
                    if (r < YETI_WALK_CHANCE) {
                        nextState = STATE_WALK;
                        return STATE_STOP;
                    }
                    break;
                }
                if (randControl() < YETI_ROAR_CHANCE)
                    return STATE_STOP;
                break;
            case STATE_WALK :
                maxTurn = YETI_WALK_TURN;
                if (mood == MOOD_ESCAPE)
                    return STATE_RUN;
                if (bored) {
                    r = randControl();
                    if (r < YETI_WAIT_1_CHANCE || !alive) { nextState = STATE_WAIT_1; return STATE_STOP; }
                    if (r < YETI_WAIT_2_CHANCE)           { nextState = STATE_WAIT_2; return STATE_STOP; }
                    if (r < YETI_WALK_CHANCE)             return STATE_STOP;
                    break;
                }
                if (mood == MOOD_ATTACK) {
                    if (ahead && targetDist < YETI_DIST_ATTACK_2)
                        return STATE_STOP;
                    if (targetDist > YETI_DIST_RUN)
                        return STATE_RUN;
                }
                break;
            case STATE_RUN :
                flags.unused = 0;
                maxTurn = YETI_RUN_TURN;
                if (mood == MOOD_ESCAPE)
                    break;
                if (bored)
                    return STATE_WALK;
                if (ahead && targetDist < YETI_DIST_ATTACK_2)
                    return STATE_STOP;
                if (ahead && targetDist < YETI_DIST_ATTACK_3)
                    return STATE_ATTACK_3;
                if (mood == MOOD_STALK)
                    return STATE_WALK;
                break;
            case STATE_ATTACK_1 : blow(YETI_HIT_R, YETI_PUNCH_DAMAGE); break;
            case STATE_ATTACK_2 : maxTurn = YETI_WALK_TURN; blow(YETI_HIT_R | YETI_HIT_L, YETI_THUMP_DAMAGE); break;
            case STATE_ATTACK_3 : blow(YETI_HIT_R | YETI_HIT_L, YETI_CHARGE_DAMAGE); break;
        }
        return state;
    }

    virtual int getStateDeath() {
        if (state == STATE_DEATH)
            return state;
        return animation.setAnim(ANIM_DEATH);
    }

    virtual void updatePosition() {
        if (state == STATE_DEATH || state == STATE_KILL) { // Lara is in its hands
            animation.overrideMask = 0;
            return;
        }

        bool vaulting = state >= STATE_CLIMB_1;

        if (!vaulting)
            turn(state == STATE_RUN, maxTurn);

        vec3 old = pos;
        TR::Level::FloorInfo infoA, infoB;
        getFloorInfo(getRoomIndex(), old, infoA);
        old.y = infoA.floor;

        Enemy::updatePosition();

        if (!vaulting) {
            getFloorInfo(getRoomIndex(), pos, infoB);
            int clicks = int(floorf((old.y - infoB.floor) / 256.0f + 0.5f));
            if (((clicks >= 2 && clicks <= 4) || (clicks <= -4 && clicks >= -5)) && vaultTo(old, YETI_VAULT_SHIFT)) {
                switch (clicks) {
                    case  2 : animation.setAnim(ANIM_CLIMB_1); break;
                    case  3 : animation.setAnim(ANIM_CLIMB_2); break;
                    case  4 : animation.setAnim(ANIM_CLIMB_3); break;
                    default : animation.setAnim(ANIM_FALL);
                }
            }
        }

        setOverrides(true, jointChest, jointHead);
        lookAt(target);
    }
};

// the guardian of the Talion in the Ice Palace ("bird_guardian" in the original): it walks to Lara and punches her with
// either fist, 200 a fist
#define GUARDIAN_DAMAGE         200
#define GUARDIAN_HIT_L          0x0C0000
#define GUARDIAN_HIT_R          0x600000
#define GUARDIAN_WALK_TURN      (DEG2RAD * 4 * 30)
#define GUARDIAN_DIST_ATTACK_1  1024
#define GUARDIAN_DIST_ATTACK_2  2048

struct BirdGuardian : Enemy {

    enum {
        ANIM_DEATH = 20,
    };

    enum {
        STATE_EMPTY,
        STATE_WAIT,
        STATE_WALK,
        STATE_AIM_1,
        STATE_PUNCH_1,
        STATE_AIM_2,
        STATE_PUNCH_2,
        STATE_PUNCH_R,
        STATE_WAIT_2,
        STATE_DEATH,
        STATE_AIM_3,
        STATE_PUNCH_3,
    };

    BirdGuardian(IGame *game, int entity) : Enemy(game, entity, 200, 341, 0.0f, 1.0f) {
        jointChest = -1;
        jointHead  = 15;
    }

    // either fist touching her hurts her once a punch
    void punch() {
        int touch = touching();
        if (!(flags.unused & 1) && (touch & GUARDIAN_HIT_R)) {
            target->hit(GUARDIAN_DAMAGE, this);
            game->addEntity(TR::Entity::BLOOD, target->getRoomIndex(), getJoint(22) * vec3(0.0f, 224.0f, 0.0f));
            flags.unused |= 1;
        }
        if (!(flags.unused & 2) && (touch & GUARDIAN_HIT_L)) {
            target->hit(GUARDIAN_DAMAGE, this);
            game->addEntity(TR::Entity::BLOOD, target->getRoomIndex(), getJoint(19) * vec3(0.0f, 224.0f, 0.0f));
            flags.unused |= 2;
        }
    }

    virtual int getStateGround() {
        if (!think(true))
            return state;

        bool ahead = targetInView;

        switch (state) {
            case STATE_WAIT :
                if (ahead && targetDist < GUARDIAN_DIST_ATTACK_1)
                    return randControl() < 0x4000 ? STATE_AIM_1 : STATE_AIM_3;
                if (ahead && (mood == MOOD_SLEEP || mood == MOOD_STALK))
                    return STATE_WAIT_2;
                return STATE_WALK;
            case STATE_WAIT_2 :
                if (mood != MOOD_SLEEP || !ahead)
                    return STATE_WAIT;
                break;
            case STATE_WALK :
                if (ahead && targetDist < GUARDIAN_DIST_ATTACK_2)
                    return STATE_AIM_2;
                if (ahead && (mood == MOOD_SLEEP || mood == MOOD_STALK))
                    return STATE_WAIT;
                break;
            case STATE_AIM_1 :
                flags.unused = 0;
                return (ahead && targetDist < GUARDIAN_DIST_ATTACK_1) ? STATE_PUNCH_1 : STATE_WAIT;
            case STATE_AIM_2 :
                flags.unused = 0;
                return (ahead && targetDist < GUARDIAN_DIST_ATTACK_2) ? STATE_PUNCH_2 : STATE_WALK;
            case STATE_AIM_3 :
                flags.unused = 0;
                return (ahead && targetDist < GUARDIAN_DIST_ATTACK_1) ? STATE_PUNCH_3 : STATE_WAIT;
            case STATE_PUNCH_1 :
            case STATE_PUNCH_2 :
            case STATE_PUNCH_R :
            case STATE_PUNCH_3 :
                punch();
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
        turn(false, state == STATE_WALK ? GUARDIAN_WALK_TURN : 0.0f);
        Enemy::updatePosition();
        setOverrides(true, jointChest, jointHead);
        lookAt(target);
    }
};

// TR2 knife throwers ("cult_2" in the original), they throw a knife from either hand walking, or both standing
#define KNIFE_GOON_WALK_TURN    (DEG2RAD * 3 * 30)
#define KNIFE_GOON_RUN_TURN     (DEG2RAD * 6 * 30)
#define KNIFE_GOON_DIST_WALK    (1024 * 4)
#define KNIFE_GOON_DIST_KNIFE   (1024 * 6)
#define KNIFE_GOON_DIST_STOP    (1024 * 5 / 2)
#define KNIFE_GOON_DIST_THROW   (1024 * 8)

struct KnifeGoon : Human {

    enum {
        ANIM_DEATH = 23,
    };

    enum {
        STATE_EMPTY,
        STATE_STOP,
        STATE_WALK,
        STATE_RUN,
        STATE_AIM_1_L,
        STATE_THROW_1_L,
        STATE_AIM_1_R,
        STATE_THROW_1_R,
        STATE_AIM_2,
        STATE_THROW_2,
        STATE_DEATH,
    };

    KnifeGoon(IGame *game, int entity) : Human(game, entity, 60) {
        jointChest = 1;
        jointHead  = 9;
        animDeath  = ANIM_DEATH;
    }

    bool canThrow() {
        return targetIsVisible(KNIFE_GOON_DIST_THROW);
    }

    void throwKnife(int joint) { // from the hand at Lara, a little off
        vec3 from = getJoint(joint).pos;
        Box  box  = target->getBoundingBox();
        vec3 to   = vec3(target->pos.x, box.max.y + (box.min.y - box.max.y) * 0.75f, target->pos.z);

        Bullet *knife = (Bullet*)game->addEntity(TR::Entity::MISSILE_KNIFE, getRoomIndex(), from);
        if (knife) {
            vec3 d = to - from;
            vec3 a = vec3(-atan2f(d.y, sqrtf(d.x * d.x + d.z * d.z)), atan2f(d.x, d.z), 0.0f);
            a.x += (randf() * 2.0f - 1.0f) * (1.4f * DEG2RAD);
            a.y += (randf() * 2.0f - 1.0f) * (1.4f * DEG2RAD);
            knife->setAngle(a);
        }
    }

    virtual int getStateGround() {
        if (!think(false))
            return state;

        bool bored = mood == MOOD_SLEEP;

        switch (state) {
            case STATE_STOP :
                if (mood == MOOD_ESCAPE)
                    return STATE_RUN;
                if (canThrow())
                    return STATE_AIM_2;
                if (bored) {
                    if (!targetInView || targetDist > KNIFE_GOON_DIST_KNIFE)
                        return STATE_WALK;
                    break;
                }
                return (targetInView && targetDist < KNIFE_GOON_DIST_WALK) ? STATE_WALK : STATE_RUN;
            case STATE_WALK :
                if (mood == MOOD_ESCAPE)
                    return STATE_RUN;
                if (canThrow()) {
                    if (targetDist < KNIFE_GOON_DIST_STOP || zone != targetZone())
                        return STATE_STOP;
                    return randControl() < 0x4000 ? STATE_AIM_1_L : STATE_AIM_1_R;
                }
                if (bored) {
                    if (targetInView && targetDist < KNIFE_GOON_DIST_KNIFE)
                        return STATE_STOP;
                    break;
                }
                if (!targetInView || targetDist > KNIFE_GOON_DIST_WALK)
                    return STATE_RUN;
                break;
            case STATE_RUN :
                if (mood == MOOD_ESCAPE)
                    break;
                if (canThrow())
                    return STATE_WALK;
                if (bored)
                    return (targetInView && targetDist < KNIFE_GOON_DIST_KNIFE) ? STATE_STOP : STATE_WALK;
                if (targetInView && targetDist < KNIFE_GOON_DIST_WALK)
                    return STATE_WALK;
                break;
            case STATE_AIM_1_L :
                flags.unused = 0;
                return canThrow() ? STATE_THROW_1_L : STATE_WALK;
            case STATE_AIM_1_R :
                flags.unused = 0;
                return canThrow() ? STATE_THROW_1_R : STATE_WALK;
            case STATE_AIM_2 :
                flags.unused = 0;
                return canThrow() ? STATE_THROW_2 : STATE_STOP;
            case STATE_THROW_1_L :
            case STATE_THROW_1_R :
            case STATE_THROW_2 :
                if (flags.unused == 0) {
                    if (state != STATE_THROW_1_R)
                        throwKnife(5);
                    if (state != STATE_THROW_1_L)
                        throwKnife(8);
                    flags.unused = 1;
                }
                break;
        }
        return state;
    }

    virtual int getStateDeath() {
        if (state == STATE_DEATH)
            return state;
        return animation.setAnim(animDeath);
    }

    virtual void updatePosition() {
        if (state == STATE_DEATH) {
            animation.overrideMask = 0;
            return;
        }

        float w = 0.0f;
        if (state == STATE_RUN)
            w = KNIFE_GOON_RUN_TURN;
        else if (state == STATE_WALK)
            w = KNIFE_GOON_WALK_TURN;
        turn(state == STATE_RUN, w);

        Enemy::updatePosition();
        setOverrides(true, jointChest, jointHead);
        lookAt(target);
    }
};

// TR2 gunmen of the rig and the diving area ("worker_1" and "worker_2" in the original), the first one fires single
// shots, the second one bursts of them
#define GUNMAN_1_DAMAGE     150
#define GUNMAN_2_DAMAGE     30
#define GUNMAN_2_RELOAD     5               // frames between the shots of a burst
#define GUNMAN_WALK_TURN    (DEG2RAD * 3 * 30)
#define GUNMAN_RUN_TURN     (DEG2RAD * 5 * 30)
#define GUNMAN_DIST_RUN     (1024 * 2)
#define GUNMAN_DIST_STAND   (1024 * 3)      // closer it shoots standing
#define GUNMAN_DIST_SHOT    (1024 * 8)

struct Gunman1 : Human {

    enum {
        ANIM_DEATH = 18,
    };

    enum {
        STATE_EMPTY,
        STATE_WALK,
        STATE_STOP,
        STATE_WAIT,
        STATE_SHOOT_1,
        STATE_RUN,
        STATE_SHOOT_2,  // walking
        STATE_DEATH,
        STATE_AIM_1,
        STATE_AIM_2,
        STATE_SHOOT_3,
    };

    Gunman1(IGame *game, int entity) : Human(game, entity, 25) {
        jointChest = 5;
        jointHead  = 14;
        jointGun   = 9;
        animDeath  = ANIM_DEATH;
    }

    bool canShoot() {
        return targetIsVisible(GUNMAN_DIST_SHOT);
    }

    void fire() { // the muzzle (0, 281, 40) of the gun mesh in the flash space
        if (flags.unused == 0) {
            doShot(GUNMAN_1_DAMAGE, vec3(0.0f, -40.0f, 101.0f));
            flags.unused = 1;
        }
    }

    virtual int getStateGround() {
        if (!think(true))
            return state;

        bool bored = mood == MOOD_SLEEP;

        switch (state) {
            case STATE_STOP :
                flags.unused = 0;
                if (mood == MOOD_ESCAPE)
                    return STATE_RUN;
                if (canShoot()) {
                    if (targetDist >= GUNMAN_DIST_STAND && zone == targetZone())
                        return STATE_WALK;
                    return randControl() < 0x4000 ? STATE_AIM_1 : STATE_AIM_2;
                }
                if (bored && targetInView)
                    return STATE_WAIT;
                return targetDist > GUNMAN_DIST_RUN ? STATE_RUN : STATE_WALK;
            case STATE_WAIT :
                if (canShoot())
                    return STATE_SHOOT_1;
                if (!bored || !targetInView)
                    return STATE_STOP;
                break;
            case STATE_WALK :
                flags.unused = 0;
                if (mood == MOOD_ESCAPE)
                    return STATE_RUN;
                if (canShoot())
                    return (targetDist < GUNMAN_DIST_STAND || zone != targetZone()) ? STATE_STOP : STATE_SHOOT_2;
                if (bored && targetInView)
                    return STATE_STOP;
                if (targetDist > GUNMAN_DIST_RUN)
                    return STATE_RUN;
                break;
            case STATE_RUN :
                if (mood == MOOD_ESCAPE)
                    break;
                if (canShoot() || bored || mood == MOOD_STALK)
                    return STATE_WALK;
                break;
            case STATE_AIM_1 :
                flags.unused = 0;
                break;
            case STATE_AIM_2 :
                flags.unused = 0;
                if (canShoot())
                    return STATE_SHOOT_3;
                break;
            case STATE_SHOOT_1 :
            case STATE_SHOOT_3 :
                fire();
                if (mood == MOOD_ESCAPE || targetDist > GUNMAN_DIST_STAND || !canShoot())
                    return STATE_STOP;
                break;
            case STATE_SHOOT_2 :
                fire();
                break;
        }
        return state;
    }

    virtual int getStateDeath() {
        if (state == STATE_DEATH)
            return state;
        return animation.setAnim(animDeath);
    }

    virtual void updatePosition() {
        if (state == STATE_DEATH) {
            animation.overrideMask = 0;
            return;
        }

        float w = 0.0f;
        if (state == STATE_RUN)
            w = GUNMAN_RUN_TURN;
        else if (state == STATE_WALK || state == STATE_SHOOT_2)
            w = GUNMAN_WALK_TURN;
        turn(state == STATE_RUN, w);

        Enemy::updatePosition();
        setOverrides(true, jointChest, jointHead);
        lookAt(target);
    }
};

struct Gunman2 : Human {

    enum {
        ANIM_DEATH = 19,
    };

    enum {
        STATE_EMPTY,
        STATE_STOP,
        STATE_WALK,
        STATE_RUN,
        STATE_WAIT,
        STATE_SHOOT_1,
        STATE_SHOOT_2,  // walking
        STATE_DEATH,
        STATE_AIM_1,
        STATE_AIM_2,    // walking
        STATE_AIM_3,
        STATE_SHOOT_3,
    };

    bool  flamer;    // the flamethrower one of the Diving Area
    float flameTime;

    Gunman2(IGame *game, int entity) : Human(game, entity, 20), flameTime(0.0f) {
        jointChest = 5;
        jointHead  = 14;
        jointGun   = 9;
        animDeath  = ANIM_DEATH;
        flamer     = getEntity().type == TR::Entity::ENEMY_GOON_FLAME;
    }

    bool canShoot() {
        return targetIsVisible(GUNMAN_DIST_SHOT);
    }

    void fire() { // the muzzle (0, 308, 32) of the gun mesh in the flash space
        if (flamer) { // a gout of fire every frame
            flameTime += Core::deltaTime;
            for (; flameTime >= 1.0f / 30.0f; flameTime -= 1.0f / 30.0f) {
                FlameStream *flame = (FlameStream*)game->addEntity(TR::Entity::FLAME_STREAM, getRoomIndex(), getJoint(jointGun) * vec3(0.0f, 308.0f, 32.0f));
                if (flame)
                    flame->aim(target);
            }
            return;
        }

        if (flags.unused == 0) {
            doShot(GUNMAN_2_DAMAGE, vec3(0.0f, -32.0f, 128.0f));
            flags.unused = GUNMAN_2_RELOAD;
        } else
            flags.unused--;
    }

    virtual int getStateGround() {
        if (!think(true))
            return state;

        bool bored = mood == MOOD_SLEEP;

        switch (state) {
            case STATE_STOP :
                flags.unused = 0;
                if (mood == MOOD_ESCAPE)
                    return STATE_RUN;
                if (canShoot()) {
                    if (targetDist >= GUNMAN_DIST_STAND && zone == targetZone())
                        return STATE_WALK;
                    return (flamer || randControl() < 0x4000) ? STATE_AIM_1 : STATE_AIM_3;
                }
                if (bored && targetInView)
                    return STATE_WAIT;
                return targetDist > GUNMAN_DIST_RUN ? STATE_RUN : STATE_WALK;
            case STATE_WAIT :
                if (canShoot())
                    return STATE_SHOOT_1;
                if (!bored || !targetInView)
                    return STATE_STOP;
                break;
            case STATE_WALK :
                flags.unused = 0;
                if (mood == MOOD_ESCAPE)
                    return STATE_RUN;
                if (canShoot())
                    return (targetDist < GUNMAN_DIST_STAND || zone != targetZone()) ? STATE_STOP : STATE_AIM_2;
                if (bored && targetInView)
                    return STATE_STOP;
                if (targetDist > GUNMAN_DIST_RUN)
                    return STATE_RUN;
                break;
            case STATE_RUN :
                if (mood == MOOD_ESCAPE)
                    break;
                if (canShoot() || bored || mood == MOOD_STALK)
                    return STATE_WALK;
                break;
            case STATE_AIM_1 :
            case STATE_AIM_3 :
                flags.unused = 0;
                if (targetInView) {
                    if (!canShoot())
                        return STATE_STOP;
                    return state == STATE_AIM_1 ? STATE_SHOOT_1 : STATE_SHOOT_3;
                }
                break;
            case STATE_AIM_2 :
                flags.unused = 0;
                if (targetInView)
                    return canShoot() ? STATE_SHOOT_2 : STATE_WALK;
                break;
            case STATE_SHOOT_1 :
            case STATE_SHOOT_2 :
                fire();
                break;
            case STATE_SHOOT_3 :
                fire();
                if (mood == MOOD_ESCAPE || targetDist > GUNMAN_DIST_STAND || !canShoot())
                    return STATE_STOP;
                break;
        }
        return state;
    }

    virtual int getStateDeath() {
        if (state == STATE_DEATH)
            return state;
        return animation.setAnim(animDeath);
    }

    virtual void updatePosition() {
        if (state == STATE_DEATH) {
            animation.overrideMask = 0;
            return;
        }

        float w = 0.0f;
        if (state == STATE_RUN)
            w = GUNMAN_RUN_TURN;
        else if (state == STATE_WALK || state == STATE_AIM_2 || state == STATE_SHOOT_2)
            w = GUNMAN_WALK_TURN;
        turn(state == STATE_RUN, w);

        Enemy::updatePosition();
        setOverrides(true, jointChest, jointHead);
        lookAt(target);
    }
};

// TR2 mercenaries of Tibet ("bandit_1", "bandit_2" and "bandit_2b" in the original): the first ones fire bursts
// from their machine guns, the others single shots of their rifles, the third ones with the animations of the second
#define MERC_1_DAMAGE       8               // a frame of a burst
#define MERC_2_DAMAGE       50
#define MERC_WALK_TURN      (DEG2RAD * 4 * 30)
#define MERC_RUN_TURN       (DEG2RAD * 6 * 30)
#define MERC_DIST_WALK      (1024 * 2)
#define MERC_DIST_SHOT      (1024 * 8)

struct Mercenary : Human {

    enum { // the first ones
        M1_STATE_EMPTY,
        M1_STATE_WAIT,
        M1_STATE_WALK,
        M1_STATE_RUN,
        M1_STATE_AIM_1,
        M1_STATE_SHOOT_1,
        M1_STATE_AIM_2,
        M1_STATE_SHOOT_2,
        M1_STATE_SHOOT_3A,
        M1_STATE_SHOOT_3B,
        M1_STATE_SHOOT_4A,
        M1_STATE_AIM_3,
        M1_STATE_AIM_4,
        M1_STATE_DEATH,
        M1_STATE_SHOOT_4B,
    };

    enum { // the second and third ones
        M2_STATE_EMPTY,
        M2_STATE_AIM_4,
        M2_STATE_WAIT,
        M2_STATE_WALK,
        M2_STATE_RUN,
        M2_STATE_AIM_1,
        M2_STATE_AIM_2,
        M2_STATE_SHOOT_1,
        M2_STATE_SHOOT_2,
        M2_STATE_SHOOT_4A,
        M2_STATE_SHOOT_4B,
        M2_STATE_DEATH,
        M2_STATE_AIM_5,
        M2_STATE_SHOOT_5,
    };

    bool rifle;     // the second and third ones
    int  goal;      // a goal set at a shot, kept till the state changes

    Mercenary(IGame *game, int entity) : Human(game, entity, 45), goal(-1) {
        rifle      = getEntity().type != TR::Entity::ENEMY_MERCENARY_1;
        jointChest = 7;
        jointHead  = 9;
        jointGun   = 17;
        animDeath  = rifle ? 9 : 14;
        if (rifle)
            health = 50;

        // the third ones have no animations of their own, they play the ones of the second (as in the original)
        int index = level->getModelIndex(TR::Entity::ENEMY_MERCENARY_2);
        if (getEntity().type == TR::Entity::ENEMY_MERCENARY_3 && index > 0) {
            animation.setModel(&level->models[index - 1]);
            animation.setAnim(0);
        }
    }

    bool canShoot() {
        return targetIsVisible(MERC_DIST_SHOT);
    }

    // the first and second ones go for the nearest of Lara and the monks (the third ones for Lara only)
    virtual Character* chooseTarget() {
        Character *lara = (Character*)game->getLara(pos);
        if (getEntity().type == TR::Entity::ENEMY_MERCENARY_3)
            return lara;
        static const TR::Entity::Type monks[] = { TR::Entity::ENEMY_MONK_1, TR::Entity::ENEMY_MONK_2 };
        Character *monk = nearestOf(monks, COUNT(monks));
        if (!monk || (lara->health > 0.0f && (lara->pos - pos).length2() < (monk->pos - pos).length2()))
            return lara;
        return monk;
    }

    // the muzzle (-2, 150, 19) or (-1, 230, 9) of the gun mesh in the flash space
    bool fire(float damage) {
        return doShot(damage, rifle ? vec3(-1.0f, -9.0f, 50.0f) : vec3(-2.0f, -19.0f, -30.0f));
    }

    int getStateMachineGun() {
        bool shoot = canShoot();

        switch (state) {
            case M1_STATE_WAIT :
                if (mood == MOOD_ESCAPE)
                    return M1_STATE_RUN;
                if (shoot) {
                    if (targetDist > MERC_DIST_WALK)
                        return M1_STATE_WALK;
                    int r = randControl();
                    return r < 0x2000 ? M1_STATE_SHOOT_1 : (r < 0x4000 ? M1_STATE_SHOOT_2 : M1_STATE_AIM_3);
                }
                if (mood == MOOD_SLEEP)
                    return targetInView ? M1_STATE_WAIT : M1_STATE_WALK;
                return M1_STATE_RUN;
            case M1_STATE_WALK :
                if (mood == MOOD_ESCAPE)
                    return M1_STATE_RUN;
                if (shoot)
                    return (targetDist > MERC_DIST_WALK && zone == targetZone()) ? M1_STATE_AIM_4 : M1_STATE_WAIT;
                if (mood == MOOD_SLEEP)
                    return targetInView ? M1_STATE_WALK : M1_STATE_WAIT;
                return M1_STATE_RUN;
            case M1_STATE_RUN :
                if (mood == MOOD_ESCAPE)
                    break;
                if (shoot)
                    return M1_STATE_WAIT;
                if (mood == MOOD_SLEEP)
                    return M1_STATE_WALK;
                break;
            case M1_STATE_SHOOT_1  :
            case M1_STATE_SHOOT_2  :
            case M1_STATE_SHOOT_3A :
            case M1_STATE_SHOOT_3B : // a burst, a shot every frame
                if (!shoot)
                    return M1_STATE_WAIT;
                fire(MERC_1_DAMAGE);
                break;
            case M1_STATE_SHOOT_4A :
            case M1_STATE_SHOOT_4B :
                if (shoot)
                    fire(MERC_1_DAMAGE);
                if (!shoot || targetDist < MERC_DIST_WALK)
                    return M1_STATE_WALK;
                break;
        }
        return state;
    }

    bool shooting() {
        return state == M2_STATE_SHOOT_1 || state == M2_STATE_SHOOT_2 || state == M2_STATE_SHOOT_5 ||
               state == M2_STATE_SHOOT_4A || state == M2_STATE_SHOOT_4B;
    }

    int getStateRifle() {
        bool shoot = canShoot();

        if (!shooting())
            goal = -1;

        switch (state) {
            case M2_STATE_WAIT :
                if (mood == MOOD_ESCAPE)
                    return M2_STATE_RUN;
                if (shoot) {
                    if (targetDist > MERC_DIST_WALK && randControl() < 0x4000)
                        return M2_STATE_WALK;
                    int r = randControl();
                    return r < 0x2000 ? M2_STATE_SHOOT_1 : (r < 0x5000 ? M2_STATE_SHOOT_2 : M2_STATE_AIM_5);
                }
                if (mood == MOOD_SLEEP)
                    return (!targetInView || randControl() < 0x100) ? M2_STATE_WALK : state;
                return M2_STATE_RUN;
            case M2_STATE_WALK :
                if (mood == MOOD_ESCAPE)
                    return M2_STATE_RUN;
                if (shoot)
                    return (targetDist < MERC_DIST_WALK || zone == targetZone() || randControl() < 0x400) ? M2_STATE_WAIT : M2_STATE_AIM_4;
                if (mood != MOOD_SLEEP)
                    return M2_STATE_RUN;
                if (targetInView && randControl() < 0x400)
                    return M2_STATE_WAIT;
                break;
            case M2_STATE_RUN :
                if (mood == MOOD_ESCAPE)
                    break;
                if (mood == MOOD_SLEEP || shoot)
                    return M2_STATE_WAIT;
                break;
            case M2_STATE_AIM_1 :
            case M2_STATE_AIM_2 :
            case M2_STATE_AIM_4 :
                flags.unused = 0;
                break;
            case M2_STATE_AIM_5 :
                flags.unused = 0;
                return shoot ? M2_STATE_SHOOT_5 : M2_STATE_WAIT;
            case M2_STATE_SHOOT_1 :
            case M2_STATE_SHOOT_2 :
            case M2_STATE_SHOOT_5 : // a single shot
                if (flags.unused == 0) {
                    if (shoot)
                        fire(MERC_2_DAMAGE);
                    if (!shoot || randControl() < 0x2000)
                        goal = M2_STATE_WAIT;
                    flags.unused = 1;
                }
                break;
            case M2_STATE_SHOOT_4A :
            case M2_STATE_SHOOT_4B : { // walking, a shot each step
                int step = state == M2_STATE_SHOOT_4A ? 1 : 2;
                if (flags.unused != step) {
                    if (shoot)
                        fire(MERC_2_DAMAGE);
                    else
                        goal = M2_STATE_WALK;
                    flags.unused = step;
                }
                if (targetDist < MERC_DIST_WALK)
                    return M2_STATE_WALK;
                break;
            }
        }
        return goal != -1 ? goal : state;
    }

    virtual int getStateGround() {
        if (!think(rifle))
            return state;

        return rifle ? getStateRifle() : getStateMachineGun();
    }

    virtual int getStateDeath() {
        int death = rifle ? M2_STATE_DEATH : M1_STATE_DEATH;
        if (state == death)
            return state;
        return animation.setAnim(animDeath);
    }

    virtual void updatePosition() {
        if (health <= 0.0f) {
            animation.overrideMask = 0;
            return;
        }

        float w = 0.0f;
        int walk = rifle ? M2_STATE_WALK : M1_STATE_WALK;
        int run  = rifle ? M2_STATE_RUN  : M1_STATE_RUN;
        if (state == run)
            w = MERC_RUN_TURN;
        else if (state == walk || (rifle ? (state == M2_STATE_AIM_4 || state == M2_STATE_SHOOT_4A || state == M2_STATE_SHOOT_4B)
                                         : (state == M1_STATE_AIM_4 || state == M1_STATE_SHOOT_4A || state == M1_STATE_SHOOT_4B)))
            w = MERC_WALK_TURN;
        turn(state == run, w);

        Enemy::updatePosition();
        setOverrides(true, jointChest, jointHead);
        lookAt(target);
    }
};

// TR2 monks of the Barkhang Monastery ("monk_1" and "monk_2" in the original): allies of Lara, they fight the
// mercenaries with their staffs and leave her alone as long as she aims at none of them, they all turn on her once she
// hurts one of them badly (more than 10 in all), kills one or hurts one with no foe around (as in the original)
#define MONK_DAMAGE         150
#define MONK_ENEMY_DAMAGE   5               // a blow at a mercenary
#define MONK_HIT_MASK       0x4000
#define MONK_WALK_TURN      (DEG2RAD * 3 * 30)
#define MONK_RUN_TURN       (DEG2RAD * 4 * 30)
#define MONK_RUN_TURN_FAST  (DEG2RAD * 5 * 30)
#define MONK_DIST_CLOSE     512
#define MONK_DIST_LONG      1024
#define MONK_DIST_ATTACK_5  (1024 * 3)
#define MONK_DIST_WALK      (1024 * 2)
#define MONK_DIST_HIT       512
#define MONK_FRIENDLY_FIRE  10.0f
#define SND_TR2_MONK_CRUNCH 245

struct Monk : Enemy {

    enum {
        ANIM_DEATH = 20,
    };

    enum {
        STATE_EMPTY,
        STATE_WAIT_1,
        STATE_WALK,
        STATE_RUN,
        STATE_ATTACK_1,
        STATE_ATTACK_2,
        STATE_ATTACK_3,
        STATE_ATTACK_4,
        STATE_AIM_3,
        STATE_DEATH,
        STATE_ATTACK_5,
        STATE_WAIT_2,
    };

    float maxTurn;
    float laraDamage; // the damage Lara did it while the monks were at peace with her

    Monk(IGame *game, int entity) : Enemy(game, entity, 30, 102, 0.0f, 1.0f), maxTurn(0.0f), laraDamage(0.0f) {
        jointChest = -1;
        jointHead  = 7;
    }

    bool isHostile() {
        return level->state.flags.alliesHostile != 0;
    }

    // the nearest mercenary of the first or second kind, or Lara once the monks turned on her if she is nearer
    virtual Character* chooseTarget() {
        static const TR::Entity::Type foes[] = { TR::Entity::ENEMY_MERCENARY_1, TR::Entity::ENEMY_MERCENARY_2 };
        Character *lara = (Character*)game->getLara(pos);
        Character *foe  = nearestOf(foes, COUNT(foes));
        if (!foe || (isHostile() && lara->health > 0.0f && (lara->pos - pos).length2() < (foe->pos - pos).length2()))
            return lara;
        return foe;
    }

    virtual bool hasEnemy() {
        return !target->getEntity().isLara() || isHostile();
    }

    virtual void hit(float damage, Controller *enemy = NULL, TR::HitType hitType = TR::HIT_DEFAULT) {
        if (enemy && enemy->getEntity().isLara() && !isHostile() && damage > 0.0f) {
            laraDamage += damage;
            if (health <= damage || laraDamage > MONK_FRIENDLY_FIRE || mood == MOOD_SLEEP)
                level->state.flags.alliesHostile = 1;
        }
        Enemy::hit(damage, enemy, hitType);
    }

    // a blow of the staff: at Lara as the other creatures bite her, at a mercenary when it is near
    void blow() {
        if (flags.unused)
            return;
        if (target->getEntity().isLara()) {
            if (!isHostile() || !(touching() & MONK_HIT_MASK))
                return;
            target->hit(MONK_DAMAGE, this);
            game->addEntity(TR::Entity::BLOOD, target->getRoomIndex(), getJoint(14) * vec3(-23.0f, 16.0f, 265.0f));
        } else {
            vec3 d = target->pos - pos;
            if (fabsf(d.x) >= MONK_DIST_HIT || fabsf(d.y) >= MONK_DIST_HIT || fabsf(d.z) >= MONK_DIST_HIT)
                return;
            target->hit(MONK_ENEMY_DAMAGE, this);
        }
        game->playSound(SND_TR2_MONK_CRUNCH, pos, Sound::PAN);
        flags.unused = 1;
    }

    virtual int getStateGround() {
        if (!think(true))
            return state;

        bool ahead = targetInView;
        bool bored = mood == MOOD_SLEEP;
        // a monk Lara aims at while at peace with her stands still
        Character *lara = (Character*)game->getLara(pos);
        bool aimedAt = !isHostile() && lara->viewTarget == this;

        switch (state) {
            case STATE_WAIT_1 :
                flags.unused = 0;
                if (aimedAt && ahead) break;
                if (bored)                 return STATE_WALK;
                if (mood == MOOD_ESCAPE)   return STATE_RUN;
                if (ahead && targetDist < MONK_DIST_CLOSE)
                    return randControl() < 0x7000 ? STATE_ATTACK_1 : STATE_WAIT_2;
                if (!ahead)                return STATE_RUN;
                if (targetDist < MONK_DIST_LONG) return STATE_ATTACK_4;
                if (targetDist < MONK_DIST_WALK) return STATE_WALK;
                return STATE_RUN;
            case STATE_WAIT_2 :
                flags.unused = 0;
                if (aimedAt && ahead) break;
                if (bored)                 return STATE_WALK;
                if (mood == MOOD_ESCAPE)   return STATE_RUN;
                if (ahead && targetDist < MONK_DIST_CLOSE) {
                    int r = randControl();
                    return r < 0x3000 ? STATE_ATTACK_2 : (r < 0x6000 ? STATE_AIM_3 : STATE_WAIT_1);
                }
                if (ahead && targetDist < MONK_DIST_WALK) return STATE_WALK;
                return STATE_RUN;
            case STATE_WALK :
                maxTurn = MONK_WALK_TURN;
                if (bored) {
                    if (aimedAt && ahead)
                        return randControl() < 0x4000 ? STATE_WAIT_1 : STATE_WAIT_2;
                    break;
                }
                if (mood == MOOD_ESCAPE)   return STATE_RUN;
                if (ahead && targetDist < MONK_DIST_CLOSE)
                    return randControl() < 0x4000 ? STATE_WAIT_1 : STATE_WAIT_2;
                if (!ahead || targetDist > MONK_DIST_WALK) return STATE_RUN;
                break;
            case STATE_RUN :
                flags.unused = 0;
                maxTurn = isHostile() ? MONK_RUN_TURN_FAST : MONK_RUN_TURN;
                if (bored)                 return STATE_WAIT_1;
                if (mood == MOOD_ESCAPE)   break;
                if (ahead && targetDist < MONK_DIST_CLOSE)
                    return randControl() < 0x4000 ? STATE_WAIT_1 : STATE_WAIT_2;
                if (ahead && targetDist < MONK_DIST_ATTACK_5) return STATE_ATTACK_5;
                break;
            case STATE_AIM_3 :
                return (!ahead || targetDist > MONK_DIST_CLOSE) ? STATE_WAIT_2 : STATE_ATTACK_3;
            case STATE_ATTACK_1 :
            case STATE_ATTACK_2 :
            case STATE_ATTACK_3 :
            case STATE_ATTACK_4 :
            case STATE_ATTACK_5 :
                blow();
                break;
        }
        return state;
    }

    virtual int getStateDeath() {
        if (state == STATE_DEATH)
            return state;
        return animation.setAnim(ANIM_DEATH + randControl() / 0x4000); // either of the two deaths
    }

    virtual void updatePosition() {
        if (state == STATE_DEATH) {
            animation.overrideMask = 0;
            return;
        }
        if (state == STATE_WAIT_1 || state == STATE_WAIT_2)
            maxTurn = 0.0f;
        turn(state == STATE_RUN, maxTurn);
        Enemy::updatePosition();
        setOverrides(true, jointChest, jointHead);
        lookAt(target);
    }
};

// TR2 morays ("eel" and "big_eel" in the original): out of their hole along the way they face they lunge at Lara near
// ahead, the small one turns to her, they draw back after a bite and slide out dead. Lara can't target them
#define MORAY_MOVE          102.0f              // per frame
#define MORAY_ANGLE         (DEG2RAD * 10)
#define MORAY_TURN          (DEG2RAD * 0.5f * 30)
#define MORAY_HIT_MASK      0x180

struct Moray : Enemy {

    enum {
        STATE_EMPTY,
        STATE_ATTACK,
        STATE_STOP,
        STATE_DEATH,
    };

    bool  big;
    float slide;    // how far out of its hole
    float range, length, damage;
    int   animDeath;

    Moray(IGame *game, int entity) : Enemy(game, entity, 5, 102, 0.0f, 1.0f), slide(0.0f) {
        big = getEntity().type == TR::Entity::ENEMY_MORAY_2;
        if (big) {
            health = 20;
            range  = 6144.0f;
            length = 2560.0f;
            damage = 500.0f;
            animDeath = 2;
        } else {
            range  = 2048.0f;
            length = 512.0f;
            damage = 50.0f;
            animDeath = 3;
        }
        jointChest = jointHead = -1;
    }

    virtual bool isActiveTarget() {
        return false;
    }

    virtual bool walledFrom(Controller *c) { // it lives in a hole in a wall, it reaches out of it
        return false;
    }

    virtual void setSaveData(const SaveEntity &data) { // back in its hole, as the original saves no position
        Enemy::setSaveData(data);
        const TR::Entity &e = getEntity();
        pos   = vec3(float(e.x), float(e.y), float(e.z));
        slide = 0.0f;
    }

    virtual void update() {
        vec3  dir  = getDir();
        float step = MORAY_MOVE * 30.0f * Core::deltaTime;
        pos -= dir * slide;

        if (health <= 0.0f) {
            slide = min(range - length, slide + step);
            if (state != STATE_DEATH)
                animation.setAnim(animDeath);
        } else {
            target = (Character*)game->getLara(pos);
            vec3  d    = target->pos - pos;
            float dist = sqrtf(d.x * d.x + d.z * d.z);
            float a    = atan2f(d.x, d.z);
            int   goal = state;

            switch (state) {
                case STATE_STOP : {
                    flags.unused = 0;
                    slide = max(0.0f, slide - step);
                    // the small one looks the way of the nearest quarter of its turn
                    float ahead = big ? angle.y : angleQuadrant(angle.y, 0.25f) * (PI * 0.5f);
                    goal = (dist <= range && fabsf(shortAngle(ahead, a)) < MORAY_ANGLE) ? STATE_ATTACK : STATE_STOP;
                    break;
                }
                case STATE_ATTACK :
                    if (slide < dist - length)
                        slide = min(dist - length, slide + step);
                    if (!big) {
                        float w = shortAngle(angle.y, a);
                        angle.y += clamp(w, -MORAY_TURN * Core::deltaTime, MORAY_TURN * Core::deltaTime);
                    }
                    // the meshes touching her now or when she was pushed off them (the touch bits of the original)
                    if (flags.unused == 0 && ((touchBits | collide(target)) & MORAY_HIT_MASK)) {
                        target->hit(damage, this);
                        game->addEntity(TR::Entity::BLOOD, target->getRoomIndex(), getJoint(7) * vec3(7.0f, 157.0f, 333.0f));
                        flags.unused = 1;
                    }
                    goal = flags.unused ? STATE_STOP : STATE_ATTACK;
                    break;
            }
            animation.setState(goal);
            dir = getDir();
        }
        touchBits = 0;

        pos += dir * slide;
        updateAnimation(true);
        updateRoom();
    }
};

// TR2 divers, they swim after Lara and fire harpoons at her, up to the surface when she is out of the water
#define DIVER_SWIM_TURN     (DEG2RAD * 3 * 30)
#define DIVER_FRONT_ARC     (DEG2RAD * 45)
#define DIVER_FLY_SPEED     (16.0f * 30.0f)
#define DIVER_FLOAT_SPEED   (32.0f * 30.0f) // the body comes up

struct Diver : Enemy {

    enum {
        ANIM_DEATH = 16,
    };

    enum {
        STATE_EMPTY,
        STATE_SWIM_1,   // under the water
        STATE_SWIM_2,   // at the surface
        STATE_SHOOT_1,
        STATE_AIM_1,
        STATE_NULL_1,
        STATE_AIM_2,
        STATE_SHOOT_2,
        STATE_NULL_2,
        STATE_DEATH,
    };

    Diver(IGame *game, int entity) : Enemy(game, entity, 20, 341, 50.0f, 1.0f) {
        jointChest = -1;
        jointHead  = 11;
        flying     = true;
        stand      = STAND_UNDERWATER;
    }

    // the surface of the water it swims in (none for a flooded room under no air)
    float getSurface() {
        int16 roomIndex = getRoomIndex();
        TR::Room::Sector *sector = level->getWaterLevelSector(roomIndex, pos);
        return sector ? sector->ceiling * 256.0f : -32512.0f;
    }

    // it sees Lara ahead, from its eyes to hers out of the water
    bool canShoot() {
        if (fabsf(targetAngle) >= DIVER_FRONT_ARC || target->health <= 0.0f)
            return false;

        TR::Location from, to;
        from.room = getRoomIndex();
        from.pos  = pos;
        to.pos    = target->pos;
        if (target->stand != STAND_UNDERWATER) {
            from.pos.y -= 256.0f;
            to.pos.y   -= 762.0f - 150.0f; // Lara's height
        }
        return trace(from, to);
    }

    void fire() {
        if (flags.unused)
            return;
        flags.unused = 1;

        vec3 from = getJoint(18) * vec3(17.0f, 164.0f, 44.0f);
        Box  box  = target->getBoundingBox();
        vec3 to   = vec3(target->pos.x, box.max.y + (box.min.y - box.max.y) * 0.75f, target->pos.z);

        Bullet *harpoon = (Bullet*)game->addEntity(TR::Entity::HARPOON_WTF, getRoomIndex(), from);
        if (harpoon) {
            vec3 d = to - from;
            vec3 a = vec3(-atan2f(d.y, sqrtf(d.x * d.x + d.z * d.z)), atan2f(d.x, d.z), 0.0f);
            a.x += (randf() * 2.0f - 1.0f) * (1.4f * DEG2RAD);
            a.y += (randf() * 2.0f - 1.0f) * (1.4f * DEG2RAD);
            harpoon->setAngle(a);
        }
    }

    virtual int getStateUnderwater() {
        if (!think(false))
            return state;

        bool  shoot   = canShoot();
        float surface = getSurface() + 512.0f;
        bool  up      = waypoint.y < surface; // Lara is out of the water

        switch (state) {
            case STATE_SWIM_1 :
                if (up && pos.y < surface + 16.0f)
                    return STATE_SWIM_2;
                if (mood != MOOD_ESCAPE && shoot)
                    return STATE_AIM_1;
                break;
            case STATE_SWIM_2 :
                if (!up)
                    return STATE_SWIM_1;
                if (mood != MOOD_ESCAPE && shoot)
                    return STATE_AIM_2;
                break;
            case STATE_SHOOT_1 :
            case STATE_SHOOT_2 :
                fire();
                break;
            case STATE_AIM_1 :
                flags.unused = 0;
                if (!shoot || mood == MOOD_ESCAPE || (up && pos.y < surface + 16.0f))
                    return STATE_SWIM_1;
                return STATE_SHOOT_1;
            case STATE_AIM_2 :
                flags.unused = 0;
                if (!shoot || mood == MOOD_ESCAPE || !up)
                    return STATE_SWIM_2;
                return STATE_SHOOT_2;
        }
        return state;
    }

    virtual int getStateDeath() {
        if (state == STATE_DEATH)
            return state;
        return animation.setAnim(ANIM_DEATH);
    }

    virtual void updatePosition() {
        float surface = getSurface();

        if (state == STATE_DEATH) { // up to the surface, or to the ceiling of a flooded room
            animation.overrideMask = 0;
            TR::Level::FloorInfo info;
            getFloorInfo(getRoomIndex(), pos, info);
            float top = max(surface, info.ceiling);
            if (pos.y > top)
                pos.y = max(top, pos.y - DIVER_FLOAT_SPEED * Core::deltaTime);
            updateRoom();
            return;
        }

        turn(false, (state == STATE_SWIM_1 || state == STATE_SWIM_2) ? DIVER_SWIM_TURN : 0.0f);
        angle.z = 0.0f;

        Enemy::updatePosition();

        if (state == STATE_SWIM_1 || state == STATE_AIM_1 || state == STATE_SHOOT_1) {
            lift(waypoint.y - pos.y, DIVER_FLY_SPEED);

            TR::Level::FloorInfo info;
            getFloorInfo(getRoomIndex(), pos, info);
            pos.y = clamp(pos.y, max(info.ceiling, surface) + 512.0f, info.floor);
        } else
            pos.y = surface;

        updateRoom();
        setOverrides(true, jointChest, jointHead);
        lookAt(target);
    }
};

// TR2 fishes, they swim after Lara under the water and bite her, a dead one floats up to the surface
#define FISH_FLY_SPEED      (16.0f * 30.0f)
#define FISH_FLOAT_SPEED    (32.0f * 30.0f)

struct Fish : Enemy {
    int   stateDeath, animDeath;
    float depth;    // how far under the surface it keeps

    Fish(IGame *game, int entity, float health, int radius, float depth) : Enemy(game, entity, health, radius, 200.0f, 1.0f), depth(depth) {
        jointChest = -1;
        flying     = true;
        stand      = STAND_UNDERWATER;
    }

    float getSurface() {
        int16 roomIndex = getRoomIndex();
        TR::Room::Sector *sector = level->getWaterLevelSector(roomIndex, pos);
        return sector ? sector->ceiling * 256.0f : -32512.0f;
    }

    // a bite of the meshes of the mask touching Lara, once an attack
    void attack(int mask, int joint, const vec3 &offset, float damage, TR::HitType hitType = TR::HIT_DEFAULT) {
        if (flags.unused || !(touching() & mask))
            return;
        target->hit(damage, this, hitType);
        game->addEntity(TR::Entity::BLOOD, target->getRoomIndex(), getJoint(joint) * offset);
        flags.unused = 1;
    }

    virtual int getStateDeath() {
        if (state == stateDeath)
            return state;
        return animation.setAnim(animDeath);
    }

    void swim(float w) {
        float surface = getSurface();

        if (state == stateDeath) { // up to the surface, or to the ceiling of a flooded room
            animation.overrideMask = 0;
            TR::Level::FloorInfo info;
            getFloorInfo(getRoomIndex(), pos, info);
            float top = max(surface, info.ceiling);
            if (pos.y > top)
                pos.y = max(top, pos.y - FISH_FLOAT_SPEED * Core::deltaTime);
            updateRoom();
            return;
        }

        turn(false, w);
        angle.z = 0.0f;

        Enemy::updatePosition();
        lift(waypoint.y - pos.y, FISH_FLY_SPEED);

        TR::Level::FloorInfo info;
        getFloorInfo(getRoomIndex(), pos, info);
        pos.y = clamp(pos.y, max(info.ceiling, surface + depth), info.floor);

        updateRoom();
        setOverrides(true, jointChest, jointHead);
        lookAt(target);
    }
};

#define SHARK_DAMAGE            400
#define SHARK_HIT_MASK          0x3400
#define SHARK_SWIM_1_TURN       (DEG2RAD * 0.5f * 30)
#define SHARK_SWIM_2_TURN       (DEG2RAD * 2 * 30)
#define SHARK_DIST_SWIM_2       (1024 * 3)
#define SHARK_DIST_ATTACK_1     768
#define SHARK_DIST_ATTACK_2     1365
#define SHARK_STOP_CHANCE       0x800

// the shark of the sunken ship, one bite too many it shakes Lara to death in its jaws
struct Shark : Fish {

    enum {
        ANIM_DEATH = 4,
        ANIM_KILL  = 19,
    };

    enum {
        STATE_STOP,
        STATE_SWIM_1,
        STATE_SWIM_2,
        STATE_ATTACK_1,
        STATE_ATTACK_2,
        STATE_DEATH,
        STATE_KILL,
    };

    Shark(IGame *game, int entity) : Fish(game, entity, 30, 341, 341.0f) {
        jointHead  = 9;
        stateDeath = STATE_DEATH;
        animDeath  = ANIM_DEATH;
        ai         = AI_FIXED;
    }

    virtual int getStateUnderwater() {
        if (!think(false) || state == STATE_KILL)
            return state;

        bool inZone = zone == targetZone();
        bool bored  = mood == MOOD_SLEEP;

        switch (state) {
            case STATE_STOP :
                flags.unused = 0;
                if (targetInView && targetDist < SHARK_DIST_ATTACK_1 && inZone)
                    return STATE_ATTACK_1;
                return STATE_SWIM_1;
            case STATE_SWIM_1 :
                if (bored)
                    break;
                if (targetInView && targetDist < SHARK_DIST_ATTACK_1)
                    return STATE_STOP;
                if (mood == MOOD_ESCAPE || targetDist > SHARK_DIST_SWIM_2 || !targetInView)
                    return STATE_SWIM_2;
                break;
            case STATE_SWIM_2 :
                flags.unused = 0;
                if (bored)
                    return STATE_SWIM_1;
                if (mood == MOOD_ESCAPE)
                    break;
                if (targetInView && targetDist < SHARK_DIST_ATTACK_2 && inZone) {
                    if (randControl() < SHARK_STOP_CHANCE)
                        return STATE_STOP;
                    if (targetDist < SHARK_DIST_ATTACK_1)
                        return STATE_ATTACK_2;
                }
                break;
            case STATE_ATTACK_1 :
            case STATE_ATTACK_2 : {
                bool alive = target->health > 0.0f;
                attack(SHARK_HIT_MASK, 12, vec3(17.0f, -22.0f, 344.0f), SHARK_DAMAGE, target->health <= SHARK_DAMAGE ? TR::HIT_SHARK : TR::HIT_DEFAULT);
                if (alive && target->health <= 0.0f)
                    return animation.setAnim(ANIM_KILL);
                break;
            }
        }
        return state;
    }

    virtual void updatePosition() {
        if (state == STATE_KILL) { // it holds still, Lara is in its jaws
            animation.overrideMask = 0;
            return;
        }

        float w = 0.0f;
        if (state == STATE_SWIM_1)
            w = SHARK_SWIM_1_TURN;
        else if (state == STATE_SWIM_2)
            w = SHARK_SWIM_2_TURN;
        swim(w);
    }
};

#define BARRACUDA_DAMAGE        100
#define BARRACUDA_HIT_MASK      0xE0
#define BARRACUDA_SWIM_1_TURN   (DEG2RAD * 2 * 30)
#define BARRACUDA_SWIM_2_TURN   (DEG2RAD * 4 * 30)
#define BARRACUDA_DIST_ATTACK_1 682
#define BARRACUDA_DIST_ATTACK_2 341

struct Barracuda : Fish {

    enum {
        ANIM_DEATH = 6,
    };

    enum {
        STATE_EMPTY,
        STATE_STOP,
        STATE_SWIM_1,
        STATE_SWIM_2,
        STATE_ATTACK_1,
        STATE_ATTACK_2,
        STATE_DEATH,
    };

    Barracuda(IGame *game, int entity) : Fish(game, entity, 12, 204, 256.0f) {
        jointHead  = 6;
        stateDeath = STATE_DEATH;
        animDeath  = ANIM_DEATH;
    }

    virtual int getStateUnderwater() {
        if (!think(false))
            return state;

        bool bored = mood == MOOD_SLEEP;

        switch (state) {
            case STATE_STOP :
                flags.unused = 0;
                if (bored)
                    return STATE_SWIM_1;
                if (targetInView && targetDist < BARRACUDA_DIST_ATTACK_1)
                    return STATE_ATTACK_1;
                return mood == MOOD_STALK ? STATE_SWIM_1 : STATE_SWIM_2;
            case STATE_SWIM_1 :
                if (bored)
                    break;
                if (targetInView && (touching() & BARRACUDA_HIT_MASK))
                    return STATE_STOP;
                if (mood != MOOD_STALK)
                    return STATE_SWIM_2;
                break;
            case STATE_SWIM_2 :
                flags.unused = 0;
                if (bored)
                    return STATE_SWIM_1;
                if (targetInView && targetDist < BARRACUDA_DIST_ATTACK_2)
                    return STATE_ATTACK_2;
                if (targetInView && targetDist < BARRACUDA_DIST_ATTACK_1)
                    return STATE_STOP;
                if (mood == MOOD_STALK)
                    return STATE_SWIM_1;
                break;
            case STATE_ATTACK_1 :
            case STATE_ATTACK_2 :
                attack(BARRACUDA_HIT_MASK, 7, vec3(2.0f, -60.0f, 121.0f), BARRACUDA_DAMAGE);
                break;
        }
        return state;
    }

    virtual void updatePosition() {
        float w = 0.0f;
        if (state == STATE_SWIM_1)
            w = BARRACUDA_SWIM_1_TURN;
        else if (state == STATE_SWIM_2)
            w = BARRACUDA_SWIM_2_TURN;
        swim(w);
    }
};

// TR2 Doberman of Venice and the Opera House
#define DOG_BITE_DAMAGE     100
#define DOG_LUNGE_DAMAGE    100
#define DOG_WALK_TURN       (DEG2RAD * 3 * 30)
#define DOG_RUN_TURN        (DEG2RAD * 6 * 30)
#define DOG_DIST_ATTACK_1   341
#define DOG_DIST_ATTACK_2   768
#define DOG_DIST_ATTACK_3   682
#define DOG_BARK            0x300
#define DOG_CROUCH          0x600
#define DOG_STAND           0xB00
#define DOG_WALK            0x2600
#define DOG_UNCROUCH        0x100
#define DOG_UNSTAND         0x200
#define DOG_UNBARK          0x500

struct Dog : Enemy {

    enum {
        HIT_MASK = 0x13F7000,
    };

    enum {
        ANIM_DEATH = 13,
    };

    enum {
        STATE_EMPTY,
        STATE_WALK,
        STATE_RUN,
        STATE_STOP,
        STATE_BARK,
        STATE_CROUCH,
        STATE_STAND,
        STATE_ATTACK_1,
        STATE_ATTACK_2,
        STATE_ATTACK_3,
        STATE_DEATH,
    };

    Dog(IGame *game, int entity) : Enemy(game, entity, 10, 341, 300.0f, 1.0f) {
        jointChest = -1;
        jointHead  = 20;
        nextState  = STATE_EMPTY;
    }

    void bite(float damage, int hitFlag) {
        if (flags.unused == hitFlag || !(touching() & HIT_MASK))
            return;
        Enemy::bite(20, vec3(0.0f, 30.0f, 141.0f), damage);
        flags.unused = hitFlag;
    }

    virtual int getStateGround() {
        if (!think(false))
            return state;

        if (nextState == state)
            nextState = STATE_EMPTY;

        bool bored = mood == MOOD_SLEEP;

        switch (state) {
            case STATE_WALK :
                if (!bored)
                    return STATE_RUN;
                {
                    int r = randControl();
                    if (r < DOG_BARK)   { nextState = STATE_BARK;   return STATE_STOP; }
                    if (r < DOG_CROUCH) { nextState = STATE_CROUCH; return STATE_STOP; }
                    if (r < DOG_STAND)  return STATE_STOP;
                }
                break;
            case STATE_RUN :
                if (bored)
                    return STATE_STOP;
                if (targetDist < DOG_DIST_ATTACK_2)
                    return STATE_ATTACK_2;
                break;
            case STATE_STOP :
                flags.unused = 0;
                if (!bored) {
                    if (mood != MOOD_ESCAPE && targetInView && targetDist < DOG_DIST_ATTACK_1)
                        return STATE_ATTACK_1;
                    return STATE_RUN;
                }
                if (nextState != STATE_EMPTY)
                    return nextState;
                {
                    int r = randControl();
                    if (r < DOG_BARK)   return STATE_BARK;
                    if (r < DOG_CROUCH) return STATE_CROUCH;
                    if (r < DOG_WALK)   return STATE_WALK;
                }
                break;
            case STATE_BARK :
                if (!bored || randControl() < DOG_UNBARK)
                    return STATE_STOP;
                break;
            case STATE_CROUCH :
                if (!bored || randControl() < DOG_UNCROUCH)
                    return STATE_STOP;
                break;
            case STATE_STAND :
                if (!bored || randControl() < DOG_UNSTAND)
                    return STATE_STOP;
                break;
            case STATE_ATTACK_1 :
                if (targetInView)
                    bite(DOG_BITE_DAMAGE, 1);
                if (targetDist > DOG_DIST_ATTACK_1 && targetDist < DOG_DIST_ATTACK_3)
                    return STATE_ATTACK_3;
                return STATE_STOP;
            case STATE_ATTACK_2 :
                bite(DOG_LUNGE_DAMAGE, 2);
                if (targetDist < DOG_DIST_ATTACK_1)
                    return STATE_ATTACK_1;
                if (targetDist < DOG_DIST_ATTACK_3)
                    return STATE_ATTACK_3;
                break;
            case STATE_ATTACK_3 :
                bite(DOG_LUNGE_DAMAGE, 3);
                if (targetDist < DOG_DIST_ATTACK_1)
                    return STATE_ATTACK_1;
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

        float w = 0.0f; // no turning when it stops or bites
        if (state == STATE_WALK) w = DOG_WALK_TURN;
        if (state == STATE_RUN || state == STATE_ATTACK_2 || state == STATE_ATTACK_3) w = DOG_RUN_TURN;
        turn(state == STATE_RUN, w);

        Enemy::updatePosition();

        setOverrides(true, jointChest, jointHead);
        lookAt(target);
    }
};

// TR2 crow, the eagles act as the crows (as in the original) with animations and a beak of their own
#define CROW_DAMAGE          20
#define CROW_DIST_ATTACK     512
#define CROW_TURN            (DEG2RAD * 3 * 30)
#define CROW_LIFT_SPEED      512.0f

struct Crow : Enemy {

    enum {
        ANIM_DEATH = 1,
        ANIM_FLY   = 8,
        ANIM_STOP  = 14,
    };

    enum { // the eagle
        ANIM_EAGLE_FLY   = 3,
        ANIM_EAGLE_STOP  = 5,
        ANIM_EAGLE_DEATH = 8,
    };

    enum {
        STATE_EMPTY,
        STATE_FLY,
        STATE_STOP,
        STATE_GLIDE,
        STATE_FALL,
        STATE_DEATH,
        STATE_ATTACK,
        STATE_EAT,
    };

    int  requiredState;
    bool eagle;

    Crow(IGame *game, int entity) : Enemy(game, entity, 15, 204, 0.0f, 1.0f), requiredState(STATE_EMPTY) {
        stand      = STAND_AIR;
        stepHeight =  20 * 1024;
        dropHeight = -20 * 1024;
        jointHead  = -1;

        eagle = getEntity().type == TR::Entity::ENEMY_EAGLE;
        if (eagle)
            health = 20;

        TR::Level::FloorInfo info;
        getFloorInfo(getRoomIndex(), pos, info);
        if (info.boxIndex != TR::NO_BOX) { // sits on the floor
            pos.y = info.floor;
            animation.setAnim(eagle ? ANIM_EAGLE_STOP : ANIM_STOP);
        } else
            animation.setAnim(eagle ? ANIM_EAGLE_FLY : ANIM_FLY);
    }

    bool onFloor() {
        return state == STATE_STOP || state == STATE_EAT;
    }

    virtual int getStateAir() {
        if (!think(false))
            return state;

        bool attack = targetInView && targetDist < CROW_DIST_ATTACK;

        if (mood == MOOD_ATTACK) // flying, it heads straight for Lara
            waypoint = target->pos;

        switch (state) {
            case STATE_FLY :
                flags.unused = 0;
                if (requiredState != STATE_EMPTY) {
                    int s = requiredState;
                    requiredState = STATE_EMPTY;
                    return s;
                }
                if (mood == MOOD_SLEEP)
                    return STATE_STOP;
                if (attack)
                    return STATE_ATTACK;
                return STATE_GLIDE;
            case STATE_STOP :
            case STATE_EAT  :
                if (mood != MOOD_SLEEP)
                    return STATE_FLY;
                break;
            case STATE_GLIDE :
                if (mood == MOOD_SLEEP) {
                    requiredState = STATE_STOP;
                    return STATE_FLY;
                }
                if (attack)
                    return STATE_ATTACK;
                break;
            case STATE_ATTACK :
                if (flags.unused == 0 && touching()) {
                    if (eagle)
                        bite(6, vec3(15.0f, 46.0f, 21.0f), CROW_DAMAGE);
                    else
                        bite(14, vec3(2.0f, 10.0f, 60.0f), CROW_DAMAGE);
                    flags.unused = 1;
                }
                break;
        }
        return state;
    }

    virtual int getStateGround() {
        return getStateAir();
    }

    virtual int getStateDeath() {
        switch (state) {
            case STATE_FALL  :
                if (stand == STAND_GROUND) // landed
                    return STATE_DEATH;
                return state;
            case STATE_DEATH :
                return state;
            default :
                flying = false;
                stand  = STAND_AIR;
                updateZone();
                return animation.setAnim(eagle ? ANIM_EAGLE_DEATH : ANIM_DEATH);
        }
    }

    virtual void updatePosition() {
        if (health > 0.0f) {
            turn(false, CROW_TURN);

            if (!target)
                target = (Character*)game->getLara(pos);

            if (onFloor()) {
                TR::Level::FloorInfo info;
                getFloorInfo(getRoomIndex(), pos, info);
                pos.y = info.floor;
            } else {
                float wy = waypoint.y - (target->stand != STAND_ONWATER ? 765.0f : 64.0f);
                lift(wy - pos.y, CROW_LIFT_SPEED);
            }
        }
        Enemy::updatePosition();
    }








    virtual void deactivate(bool removeFromList = false) {
        if (health <= 0.0f && stand != STAND_GROUND)
            return; // wait for landing
        Enemy::deactivate(removeFromList);
    }
};

// TR2 rat ("mouse" in the original)
#define MOUSE_DAMAGE          20
#define MOUSE_DIST_ATTACK     341
#define MOUSE_TURN            (DEG2RAD * 6 * 30)
#define MOUSE_WAIT_1          0x500
#define MOUSE_WAIT_2          0xA00

struct Mouse : Enemy {

    enum {
        ANIM_DEATH = 9,
    };

    enum {
        STATE_EMPTY,
        STATE_RUN,
        STATE_STOP,
        STATE_WAIT_1,
        STATE_WAIT_2,
        STATE_ATTACK,
        STATE_DEATH,
    };

    Mouse(IGame *game, int entity) : Enemy(game, entity, 4, 102, 0.0f, 1.0f) {
        stepHeight = 256;
        dropHeight = -1024;
        jointChest = -1;
        jointHead  = 4;
        nextState  = STATE_EMPTY;
    }

    virtual int getStateGround() {
        if (!think(false))
            return state;

        bool bored  = mood == MOOD_SLEEP || mood == MOOD_STALK;
        bool inRange = targetInView && targetDist < MOUSE_DIST_ATTACK;

        if (nextState == state)
            nextState = STATE_EMPTY;

        switch (state) {
            case STATE_RUN : {
                if (bored) {
                    int r = randControl();
                    if (r < MOUSE_WAIT_1) {
                        nextState = STATE_WAIT_1;
                        return STATE_STOP;
                    }
                    if (r < MOUSE_WAIT_2)
                        return STATE_STOP;
                } else if (inRange)
                    return STATE_STOP;
                break;
            }
            case STATE_STOP : {
                if (nextState != STATE_EMPTY)
                    return nextState;
                if (bored)
                    return STATE_WAIT_2;
                return inRange ? STATE_ATTACK : STATE_RUN;
            }
            case STATE_WAIT_1 :
                if (randControl() < MOUSE_WAIT_1)
                    return STATE_STOP;
                break;
            case STATE_WAIT_2 :
                if (bored) {
                    int r = randControl();
                    if (r < MOUSE_WAIT_1)
                        nextState = STATE_WAIT_1;
                    else if (r > MOUSE_WAIT_2)
                        nextState = STATE_RUN;
                } else
                    nextState = inRange ? STATE_ATTACK : STATE_RUN;
                if (nextState != STATE_EMPTY)
                    return STATE_STOP;
                break;
            case STATE_ATTACK :
                if (nextState == STATE_EMPTY && targetInView && touching()) {
                    bite(2, vec3(0.0f, 0.0f, 57.0f), MOUSE_DAMAGE);
                    nextState = STATE_STOP;
                }
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

        turn(false, state == STATE_RUN ? MOUSE_TURN : 0.0f);

        Enemy::updatePosition();
        setOverrides(true, jointChest, jointHead);
        lookAt(target);
    }
};

// TR2 small spider
#define SPIDER_DAMAGE        25
#define SPIDER_DIST_ATTACK_2 512
#define SPIDER_DIST_ATTACK_3 204
#define SPIDER_TURN          (DEG2RAD * 8 * 30)
#define SPIDER_WALK          256
#define SPIDER_LEAP_HEIGHT   384
#define SND_TR2_SPIDER_EXPLODE 349

struct Spider : Enemy {

    enum {
        ANIM_LEAP = 2,
    };

    enum {
        STATE_EMPTY,
        STATE_STOP,
        STATE_WALK,
        STATE_RUN,
        STATE_ATTACK_1,
        STATE_ATTACK_2,
        STATE_ATTACK_3,
        STATE_DEATH,
    };

    Spider(IGame *game, int entity) : Enemy(game, entity, 5, 102, 0.0f, 1.0f) {
        stepHeight = 512;
        dropHeight = -1024;
        jointChest = jointHead = -1;
    }

    virtual bool explodeFx() {
        return false; // its parts fall apart without a blast
    }

    virtual void setSaveData(const SaveEntity &data) {
        Enemy::setSaveData(data);
        if (health <= 0.0f) { // gone already
            flags.invisible = true;
            deactivate(true);
        }
    }

    // shot down it bursts apart, as in the original: its parts fly off and vanish on the floor
    virtual void update() {
        bool exploded = explodeMask != 0;

        if (health <= 0.0f && !exploded) {
            game->playSound(SND_TR2_SPIDER_EXPLODE, pos, Sound::PAN);
            explode(0xFFFFFFFF, 0.0f);
        }

        Enemy::update();

        if (exploded && !explodeMask) {
            flags.invisible = true;
            deactivate(true);
        }
    }

    virtual int getStateGround() {
        if (!think(false))
            return state;

        bool touch = targetInView && touching() != 0;

        switch (state) {
            case STATE_STOP :
                flags.unused = 0;
                if (mood == MOOD_SLEEP)
                    return randControl() < SPIDER_WALK ? STATE_WALK : state;
                if (touch)
                    return STATE_ATTACK_1;
                if (mood == MOOD_STALK)
                    return STATE_WALK;
                return STATE_RUN;
            case STATE_WALK :
                if (mood == MOOD_SLEEP)
                    return randControl() < SPIDER_WALK ? STATE_STOP : state;
                if (mood == MOOD_ESCAPE || mood == MOOD_ATTACK)
                    return STATE_RUN;
                break;
            case STATE_RUN :
                flags.unused = 0;
                if (mood == MOOD_SLEEP || mood == MOOD_STALK)
                    return STATE_WALK;
                if (touch)
                    return STATE_STOP;
                if (targetInView && targetDist < SPIDER_DIST_ATTACK_3)
                    return STATE_ATTACK_3;
                if (targetInView && targetDist < SPIDER_DIST_ATTACK_2)
                    return STATE_ATTACK_2;
                break;
            case STATE_ATTACK_1 :
            case STATE_ATTACK_2 :
            case STATE_ATTACK_3 :
                if (flags.unused == 0 && touch) {
                    bite(1, vec3(0.0f, 0.0f, 41.0f), SPIDER_DAMAGE);
                    flags.unused = 1;
                }
                break;
        }
        return state;
    }

    virtual int getStateDeath() {
        return state;
    }

    virtual void updatePosition() {
        if (health <= 0.0f)
            return;

        turn(false, SPIDER_TURN);

        vec3 opos = pos;
        int  oroom = roomIndex;
        Enemy::updatePosition();

    // too high to step up, leap there
        if (pos.y < opos.y - SPIDER_LEAP_HEIGHT) {
            pos = opos;
            roomIndex = oroom;
            animation.setAnim(ANIM_LEAP);
        }
    }
};

// TR2 giant spider ("big_spider" in the original), it walks to Lara and bites her, no leaps
#define SPIDER_GIANT_DAMAGE  100
#define SPIDER_GIANT_TURN    (DEG2RAD * 4 * 30)

struct SpiderGiant : Enemy {

    enum {
        ANIM_DEATH = 2,
    };

    enum {
        STATE_EMPTY,
        STATE_STOP,
        STATE_WALK_1,
        STATE_WALK_2,
        STATE_ATTACK_1,
        STATE_ATTACK_2,
        STATE_ATTACK_3,
        STATE_DEATH,
    };

    SpiderGiant(IGame *game, int entity) : Enemy(game, entity, 40, 341, 0.0f, 1.0f) {
        stepHeight =  256;
        dropHeight = -512;
        jointChest = jointHead = -1;
    }

    virtual int getStateGround() {
        if (!think(true))
            return state;

        bool touch = targetInView && touching() != 0;
        bool hurry = mood == MOOD_ESCAPE || mood == MOOD_ATTACK;

        switch (state) {
            case STATE_STOP :
                flags.unused = 0;
                if (mood == MOOD_SLEEP)
                    break;
                if (touch)
                    return STATE_ATTACK_1;
                if (mood == MOOD_STALK)
                    return STATE_WALK_1;
                if (hurry)
                    return STATE_WALK_2;
                break;
            case STATE_WALK_1 :
                if (mood == MOOD_SLEEP)
                    break;
                if (touch)
                    return STATE_STOP;
                if (hurry)
                    return STATE_WALK_2;
                break;
            case STATE_WALK_2 :
                flags.unused = 0;
                if (touch)
                    return STATE_STOP;
                if (mood == MOOD_SLEEP || mood == MOOD_STALK)
                    return STATE_WALK_1;
                break;
            case STATE_ATTACK_1 :
                if (flags.unused == 0 && touching()) {
                    bite(1, vec3(0.0f, 0.0f, 41.0f), SPIDER_GIANT_DAMAGE);
                    flags.unused = 1;
                }
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
        if (health > 0.0f)
            turn(false, SPIDER_GIANT_TURN);
        Enemy::updatePosition();
    }
};

#define TIGER_ROAR           96
#define TIGER_DIST_ATTACK_1  341
#define TIGER_DIST_ATTACK_2  1536
#define TIGER_DIST_ATTACK_3  1024
#define TIGER_DAMAGE         100
#define TIGER_TURN_FAST      (DEG2RAD * 180)
#define TIGER_TURN_SLOW      (DEG2RAD * 90)

struct Tiger : Enemy {

    enum {
        HIT_MASK = 0x7FDC000,
    };

    enum {
        ANIM_DEATH = 11,
    };

    enum {
        STATE_NONE = -1,
        STATE_DEATH    ,
        STATE_STOP     ,
        STATE_WALK     ,
        STATE_RUN      ,
        STATE_IDLE     ,
        STATE_ROAR     ,
        STATE_ATTACK_1 ,
        STATE_ATTACK_2 ,
        STATE_ATTACK_3 ,
    };

    float maxTurn; // kept from the state that leads to an attack, as in the original

    Tiger(IGame *game, int entity) : Enemy(game, entity, 20, 341, 200.0f, 1.0f), maxTurn(0.0f) {
        dropHeight = -1024;
        jointChest = -1;//21;
        jointHead  = -1;//22;
        nextState  = STATE_NONE;
        ai         = AI_FIXED; // the violent mood of the original: it attacks Lara anywhere in its zone
    }

    virtual int getStateGround() {
        // the stop to roar asks again till the walk or the run comes to it (the goal of the original stays till
        // reached), else it got lost out of the few frames that lead to the stop and the tiger prowled on
        bool roarAsked = nextState == STATE_ROAR && (state == STATE_WALK || state == STATE_RUN);

        if (!think(true))
            return roarAsked ? STATE_STOP : state;

        if (nextState == state)
            nextState = STATE_NONE;

        switch (state) {
            case STATE_STOP    :
                maxTurn = 0.0f;
                flags.unused = 0;
                if (mood == MOOD_ESCAPE)
                    return STATE_RUN;
                if (mood == MOOD_SLEEP)
                    return STATE_WALK; // the roar the original rolls for here is overwritten
                if (targetInView && targetDist < TIGER_DIST_ATTACK_1)
                    return STATE_ATTACK_1;
                if (targetInView && targetDist < TIGER_DIST_ATTACK_3) {
                    maxTurn = TIGER_TURN_SLOW;
                    return STATE_ATTACK_3;
                }
                if (nextState != STATE_NONE)
                    return nextState;
                if (mood != MOOD_ATTACK && randControl() < TIGER_ROAR)
                    return STATE_ROAR;
                return STATE_RUN;
            case STATE_WALK     : 
                maxTurn = TIGER_TURN_SLOW;
                if (roarAsked || randControl() < TIGER_ROAR) {
                    nextState = STATE_ROAR;
                    return STATE_STOP;
                }
                break;
            case STATE_RUN      : {
                bool melee = flags.unused != 0;
                flags.unused = 0;
                maxTurn = TIGER_TURN_FAST;

                if (mood == MOOD_SLEEP)
                    return STATE_STOP;
                if (targetInView && melee)
                    return STATE_STOP;
                if (targetInView && targetDist < TIGER_DIST_ATTACK_2) {
                    if (target->velocity.length2() < SQR(16.0f))
                        return STATE_STOP;
                    else
                        return STATE_ATTACK_2;
                }
                if (roarAsked || (mood != MOOD_ATTACK && randControl() < TIGER_ROAR)) {
                    nextState = STATE_ROAR;
                    return STATE_STOP;
                }
                break;
            }
            case STATE_ATTACK_1 :
            case STATE_ATTACK_2 :
            case STATE_ATTACK_3 :
                if (flags.unused == 0 && (touching() & HIT_MASK)) {
                    bite(26, vec3(19.0f, -13.0f, 3.0f), TIGER_DAMAGE);
                    flags.unused = 1;
                }
        }
        return state;
    }

    virtual int getStateDeath() {
        if (state == STATE_DEATH)
            return state;
        return animation.setAnim(ANIM_DEATH);
    }

    virtual void updatePosition() {
        turn(state == STATE_RUN, maxTurn);

        if (state == STATE_DEATH) {
            animation.overrideMask = 0;
            return;
        }

        Enemy::updatePosition();
        setOverrides(true, jointChest, jointHead);
        lookAt(target);
    }
};


#define WINSTON_DIST        1536.0f
#define WINSTON_TURN_SLOW   (DEG2RAD * 60)
#define WINSTON_FREEZE_TIME 60.0f

struct Winston : Enemy {

    enum {
        STATE_NONE ,
        STATE_STOP ,
        STATE_WALK ,
    };

    Texture *environment;

    Winston(IGame *game, int entity) : Enemy(game, entity, 20, 341, 200.0f, 0.25f), environment(NULL) {
        dropHeight = -1024;
        jointChest = 11;
        jointHead  = 25;
        nextState  = STATE_NONE;
        lookAtSpeed = 1.0f;
        timer = 0.0f;
    }

    virtual ~Winston() {
        delete environment;
    }

    virtual int getStateGround() {
        if (getRoomIndex() == 94) {
            int doorIndex = (level->version & TR::VER_TR2) ? 38 : 68;

            Controller *door = (Controller*)game->getLevel()->entities[doorIndex].controller;
            if (!door->isActive()) {
                if (timer > WINSTON_FREEZE_TIME) {
                    flags.unused |= 4;
                    if (!environment) {
                        bakeEnvironment(environment);
                    }
                } else {
                    timer += Core::deltaTime;
                }
                return STATE_STOP;
            } else {
                if (!(flags.unused & 4)) {
                    timer = 0.0f;
                }
            }
        }

        if (flags.unused & 4) {
            timer -= Core::deltaTime;
            if (timer < 0.0f) {
                timer = 0.0f;
                flags.unused &= ~4;
            }
            return STATE_STOP;
        }

        if (!think(false))
            return state;

        if (nextState == state)
            nextState = STATE_NONE;

        if (nextState != STATE_NONE)
            return nextState;

        switch (state) {
            case STATE_STOP    :
                if ((targetDist > WINSTON_DIST || !targetInView) && nextState != STATE_WALK) {
                    nextState = STATE_WALK;
                    game->playSound(TR::SND_WINSTON_WALK, pos, Sound::PAN);
                }
            case STATE_WALK     :
                if (targetDist < WINSTON_DIST) {
                    if (targetInView) {
                        nextState = STATE_STOP;
                        flags.unused &= ~1;
                    } else if (!(flags.unused & 1)) {
                        game->playSound(TR::SND_WINSTON_SCARED, pos, Sound::PAN);
                        game->playSound(TR::SND_WINSTON_TRAY, pos, Sound::PAN);
                        flags.unused |= 1;
                    }
                }
        }

        bool touch = collide(target) != 0;
        bool push  = (flags.unused & 2) != 0;

        if (!push && touch) {
            game->playSound(TR::SND_WINSTON_PUSH, pos, Sound::PAN);
            game->playSound(TR::SND_WINSTON_TRAY, pos, Sound::PAN);
            flags.unused |= 2;
        }

        if (push && !touch) {
            flags.unused &= ~2;
        }

        if (randControl() < 0x100) {
            game->playSound(TR::SND_WINSTON_TRAY, pos, Sound::PAN);
        }

        return state;
    }

    virtual void updatePosition() {
        if (flags.unused & 4) {
            animation.time = 0.0f;
            animation.updateInfo();
            return;
        }

        turn(state == STATE_WALK, WINSTON_TURN_SLOW);
        angle.z = 0.0f;

        Enemy::updatePosition();
        setOverrides(true, jointChest, jointHead);
        lookAt(target);
    }

    virtual void render(Frustum *frustum, MeshBuilder *mesh, Shader::Type type, bool caustics) {
        if (environment && (flags.unused & 4)) {
            game->setRoomParams(getRoomIndex(), Shader::MIRROR, 1.5f, 2.0f, 2.5f, 1.0f, false);
            GAPI::Texture *dtex = Core::active.textures[sDiffuse];
            environment->bind(sDiffuse);
            Controller::render(frustum, mesh, type, caustics);
            if (dtex) dtex->bind(sDiffuse);
        } else {
            Enemy::render(frustum, mesh, type, caustics);
        }
    }
};

// TR2 mercenaries on the black snowmobiles of the Tibetan Foothills ("skidoo_driver" and "skidoo_armed" in the
// original): the snowmobile is the creature, it chases Lara and fires its two guns, the driver only rides along.
// Shot down, he falls off, it rolls to a stop and Lara can ride it with its guns (as in the original)
#define SKIDOO_DRIVER_TURN          (DEG2RAD * 3 * 30)   // half the turn of a snowmobile a frame
#define SKIDOO_DRIVER_MIN_TURN      (DEG2RAD * 2)        // a third of it
#define SKIDOO_DRIVER_TARGET_ANGLE  (DEG2RAD * 15)
#define SKIDOO_DRIVER_WAIT_RANGE    (1024 * 4)
#define SKIDOO_DRIVER_DAMAGE        50                   // Lara on foot
#define SKIDOO_DRIVER_DAMAGE_RIDING 10                   // Lara on a snowmobile
#define SKIDOO_DRIVER_RUN_OVER      100
#define SKIDOO_DRIVER_RADIUS        341
#define SKIDOO_DRIVER_BURST         5

struct SkidooDriver : Human {
    enum {
        STATE_EMPTY,
        STATE_WAIT,
        STATE_MOVING,
        STATE_START_LEFT,
        STATE_START_RIGHT,
        STATE_LEFT,
        STATE_RIGHT,
        STATE_DEATH,
    };

    enum { ANIM_DEATH = 10 };       // of the driver
    enum { FLAG_LEFT = 4 };         // flags.unused: the snowmobile was left to Lara, he's only a body now

    Animation rider;                // the driver on it
    mat4      riderMatrix;          // where he fell off
    bool      riderDead;
    int       burst;                // the frames the guns keep firing
    Basis     riderBasis[MAX_JOINTS]; // kept, the renderer holds on to the joints it is given

    SkidooDriver(IGame *game, int entity) : Human(game, entity, 100), riderDead(false), burst(0) {
        radius     = SKIDOO_DRIVER_RADIUS;
        stepHeight = 512;
        dropHeight = -1024;
        jointChest = jointHead = -1;
        jointGun   = 0;
        rider      = Animation(level, Controller::getModel());
        rider.setAnim(0);
        animation.setModel(getModel());
        animation.setAnim(0);
    }

    bool isLeft() const {
        return (flags.unused & FLAG_LEFT) != 0;
    }

    // his snowmobile till it's left to Lara, his body then
    virtual const TR::Model* getModel() {
        int index = level->getModelIndex(TR::Entity::VEHICLE_SNOWMOBILE_BLACK);
        if (isLeft() || index <= 0)
            return Controller::getModel();
        return &level->models[index - 1];
    }

    virtual int getInput() {
        return 0; // no death state of its own, it rolls on with him dead
    }

    virtual void updateVelocity() {
        if (stand == STAND_AIR)
            applyGravity(velocity.y);
        else
            velocity = getDir() * animation.getSpeed();
    }

    // Lara riding a snowmobile takes less from its guns
    bool isRiding(Controller *c) {
        for (int i = 0; i < level->entitiesCount; i++) {
            const TR::Entity &e = level->entities[i];
            if ((e.type == TR::Entity::VEHICLE_SNOWMOBILE_RED || e.type == TR::Entity::VEHICLE_SNOWMOBILE_BLACK) && e.controller && ((Snowmobile*)e.controller)->driver == c)
                return true;
        }
        return false;
    }

    int getStateDead() {
        if (!riderDead) { // he falls off where he is
            riderDead   = true;
            riderMatrix = getMatrix();
            rider.setAnim(ANIM_DEATH);
        }
        return (state == STATE_MOVING || state == STATE_WAIT) ? STATE_WAIT : STATE_MOVING;
    }

    virtual int getStateGround() {
        if (!think(true))
            return state;

        if (health <= 0.0f)
            return getStateDead();

        vec3  d = waypoint - pos;
        float a = clamp(clampAngle(normalizeAngle(PIH - d.angleY() - angle.y)), -SKIDOO_DRIVER_TURN / 30.0f, SKIDOO_DRIVER_TURN / 30.0f);

        bool near = fabsf(targetAngle) < SKIDOO_DRIVER_TARGET_ANGLE && targetDist < SKIDOO_DRIVER_WAIT_RANGE;
        int  next = state;

        switch (state) {
            case STATE_WAIT :
                if (mood != MOOD_SLEEP && !near)
                    next = STATE_MOVING;
                break;
            case STATE_MOVING :
                if (mood == MOOD_SLEEP || near)
                    next = STATE_WAIT;
                else if (a < -SKIDOO_DRIVER_MIN_TURN)
                    next = STATE_START_LEFT;
                else if (a > SKIDOO_DRIVER_MIN_TURN)
                    next = STATE_START_RIGHT;
                break;
            case STATE_START_LEFT :
            case STATE_LEFT :
            case STATE_START_RIGHT :
            case STATE_RIGHT : // the right turn falls back as the left one does (as in the original)
                next = (a >= -SKIDOO_DRIVER_MIN_TURN) ? STATE_MOVING : STATE_LEFT;
                break;
        }

        // both guns at once when Lara is ahead and in sight, a short burst
        if (!burst && fabsf(targetAngle) < SKIDOO_DRIVER_TARGET_ANGLE && target->health > 0.0f && targetIsVisible(8 * 1024)) {
            float damage = isRiding(target) ? SKIDOO_DRIVER_DAMAGE_RIDING : SKIDOO_DRIVER_DAMAGE;
            doShot(damage, vec3(-235.0f, -71.0f, 550.0f - 180.0f), true);
            doShot(damage, vec3( 219.0f, -71.0f, 550.0f - 180.0f), true);
            burst = SKIDOO_DRIVER_BURST;
        }
        if (burst) {
            game->playSound(SND_TR2_UZI_FIRE, pos, Sound::PAN);
            burst--;
        }

        return next;
    }

    virtual void updatePosition() {
        turn(false, (health > 0.0f && state != STATE_WAIT) ? SKIDOO_DRIVER_TURN : 0.0f);
        Enemy::updatePosition();
    }

    // run over by it, Lara on foot dies
    void runOver() {
        Character *lara = (Character*)game->getLara(pos);
        if (!lara || lara->health <= 0.0f || animation.getSpeed() <= 0.0f || isRiding(lara))
            return;
        Box box = getBoundingBoxLocal();
        box.expand(vec3(100.0f, 0.0f, 100.0f)); // Lara's radius
        vec3 p = (lara->pos - pos).rotateY(angle.y);
        if (box.contains(p))
            lara->hit(SKIDOO_DRIVER_RUN_OVER, this);
    }

    void engine() {
        int x = int(pos.x), y = int(pos.y), z = int(pos.z);
        int rotY = int(angle.y * (32768.0f / PI)), rotX = int(angle.x * (32768.0f / PI));
        int speed = int(animation.getSpeed());
        if (state == STATE_WAIT) {
            game->playSound(SND_TR2_SKIDOO_IDLE, pos, Sound::PAN);
        } else {
            Snowmobile::throwSnow(this, x, y, z, rotY, rotX, speed);
            Sound::Sample *s = game->playSound(SND_TR2_SKIDOO_MOVING, pos, Sound::PAN);
            if (s) s->pitch = max(0.1f, 1.0f - (SKIDOO_MAX_SPEED - speed) * 100 / 65536.0f);
        }
    }

    // his body stays where he fell, Lara can take the snowmobile
    void leave() {
        vec3 p = pos;
        float a = angle.y;
        int16 room = getRoomIndex();

        flags.unused |= FLAG_LEFT;
        animation.setModel(getModel());
        animation.setAnim(ANIM_DEATH);
        animation.goEnd(false);
        if (riderDead) {
            pos     = riderMatrix.getPos();
            angle.y = atan2f(riderMatrix.dir().x, riderMatrix.dir().z);
        }

        Controller *skidoo = game->addEntity(TR::Entity::VEHICLE_SNOWMOBILE_BLACK, room, p, a);
        if (skidoo)
            skidoo->angle.x = skidoo->angle.z = 0.0f;
    }

    virtual void setSaveData(const SaveEntity &data) {
        Human::setSaveData(data);
        rider.setAnim(0);
        riderDead = false;
        animation.setModel(getModel());
        animation.setAnim(data.animIndex, -data.animFrame);
        if (health <= 0.0f && !isLeft()) // saved rolling on with him dead, it's left where it was
            leave();
    }

    virtual void update() {
        if (isLeft())
            return;

        Human::update();

        if (riderDead) {
            rider.update();
            if (rider.isEnded && animation.getSpeed() == 0.0f && stand == STAND_GROUND)
                leave();
            return;
        }

        if (rider.index != animation.index)
            rider.setAnim(animation.index);
        rider.time = animation.time;
        rider.updateInfo();

        if (flags.active && thinkTime < Core::deltaTime) { // once a step
            runOver();
            engine();
        }
    }

    virtual void render(Frustum *frustum, MeshBuilder *mesh, Shader::Type type, bool caustics) {
        Human::render(frustum, mesh, type, caustics);
        if (isLeft())
            return;

        const TR::Model *model = Controller::getModel();
        mat4 m = riderDead ? riderMatrix : getMatrix();
        rider.getJoints(m, -1, true, riderBasis);
        Core::mModel = m;
        Core::setBasis(riderBasis, model->mCount);
        mesh->renderModel(model->index, caustics);
    }
};

// TR2 warriors of Xian in the Floating Islands and the Dragon's Lair ("xian_spearman" and "xian_knight" in the
// original): stone statues till their trigger, they come to life part by part, shot down they turn back to stone
// from the head down and shatter (as in the original)
#define XIAN_WAKE_TICKS         4   // a mesh comes to life every 4 frames
#define XIAN_SPEAR_WALK_TURN    (DEG2RAD * 3 * 30)
#define XIAN_SPEAR_RUN_TURN     (DEG2RAD * 5 * 30)
#define XIAN_SPEAR_DIST_1       1024
#define XIAN_SPEAR_DIST_2       1536
#define XIAN_SPEAR_DIST_3       2048
#define XIAN_SPEAR_DIST_RUN     3072
#define XIAN_SPEAR_DAMAGE       75
#define XIAN_SPEAR_DAMAGE_RUN   120
#define XIAN_SPEAR_STOP_CHANCE  0x200
#define XIAN_SPEAR_WALK_CHANCE  0x400
#define XIAN_SPEAR_TOUCH_L      0x00800     // the spear in the left hand (mesh 11)
#define XIAN_SPEAR_TOUCH_R      0x40000     // and in the right one (mesh 18)
#define XIAN_KNIGHT_WALK_TURN   (DEG2RAD * 5 * 30)
#define XIAN_KNIGHT_FLY_TURN    (DEG2RAD * 4 * 30)
#define XIAN_KNIGHT_DIST_1      1024
#define XIAN_KNIGHT_DIST_3      2048
#define XIAN_KNIGHT_DAMAGE      300
#define XIAN_KNIGHT_TOUCH       0xC000      // the sword
#define XIAN_KNIGHT_LIFT_SPEED  (64.0f * 30.0f)
#define SND_TR2_WARRIOR_HOVER   312
#define SND_TR2_CRUNCH_2        318

struct XianWarrior : Enemy {
    enum { FLAG_GONE = 1 };         // flags.unused: shattered, nothing left of it

    TR::Entity::Type statueType;    // the stone look of the parts not alive (the same meshes)
    uint32 meshBits;                // the parts alive
    int    wakeCount;
    int    hits;                    // the weapons that hit in this attack
    float  maxTurn;                 // kept by the attacks from the state they come from, as in the original
    int    stateStart;
    float  stepTime;

    XianWarrior(IGame *game, int entity, float health, TR::Entity::Type statueType, int stateStart) :
        Enemy(game, entity, health, 204, 0.0f, 1.0f), statueType(statueType), meshBits(0), wakeCount(0), hits(0),
        maxTurn(0.0f), stateStart(stateStart), stepTime(0.0f) {
        initMeshOverrides();
        meshSwap(1, level->getModelIndex(statueType) - 1);
        updateMeshes();
        flags.invisible = false; // the statue stands there before its trigger
    }

    uint32 allMeshes() {
        return (1u << getModel()->mCount) - 1;
    }

    void updateMeshes() {
        if (layers[1].model >= 0)
            layers[1].mask = ~meshBits & allMeshes();
    }

    bool isGone() const {
        return (flags.unused & FLAG_GONE) != 0;
    }

    virtual bool isCollider() {
        return !isGone();
    }

    virtual int getInput() {
        return 0; // no death animation, it turns back to stone where it stands
    }

    virtual bool explodeFx() {
        return false; // only the stone flies apart
    }

    // it comes to life part by part while it wakes up, it's whole after that
    void wake() {
        if (state != stateStart) {
            meshBits = allMeshes();
        } else if (wakeCount == 0) {
            meshBits = ((meshBits << 1) | 1) & allMeshes();
            wakeCount = XIAN_WAKE_TICKS - 1;
        } else
            wakeCount--;
        updateMeshes();
    }

    // a weapon touching her hurts her once in an attack
    bool strike(int touchMask, int weapon, int joint, const vec3 &offset, float damage, TR::HitType lethal = TR::HIT_DEFAULT) {
        if ((hits & weapon) || !(touching() & touchMask))
            return false;
        hits |= weapon;
        bool alive = target->health > 0.0f;
        target->hit(damage, this, target->health <= damage ? lethal : TR::HIT_DEFAULT);
        game->addEntity(TR::Entity::BLOOD, target->getRoomIndex(), getJoint(joint) * offset);
        return alive && target->health <= 0.0f;
    }

    // shot down it turns back to stone from the last part on, then shatters
    void crumble() {
        meshBits >>= 1;
        updateMeshes();
        if (meshBits)
            return;
        game->playSound(SND_TR2_EXPLOSION_1, pos, Sound::PAN);
        layers[0].model = layers[1].model; // the stone flies apart
        layers[1].mask  = 0;
        explode(0xFFFFFFFF, 0.0f);
        flags.unused |= FLAG_GONE;
    }

    virtual void update() {
        if (isGone()) {
            if (explodeMask) {
                updateExplosion();
                if (!explodeMask) {
                    flags.invisible = true;
                    deactivate(true);
                }
            }
            return;
        }

        if (health <= 0.0f) { // its pose stays as it turns to stone
            stepTime += Core::deltaTime;
            while (stepTime >= 1.0f / 30.0f && !isGone()) {
                stepTime -= 1.0f / 30.0f;
                crumble();
            }
            return;
        }

        Enemy::update();
    }

    virtual void updatePosition() {
        turn(false, maxTurn);
        Enemy::updatePosition();
        setOverrides(true, jointChest, jointHead);
        lookAt(target);
    }

    virtual void setSaveData(const SaveEntity &data) {
        Enemy::setSaveData(data);
        flags.invisible = isGone() || health <= 0.0f;
        if (health <= 0.0f) {
            flags.unused |= FLAG_GONE;
            deactivate(true);
        }
        meshBits = (flags.active || state != stateStart) ? allMeshes() : 0;
        updateMeshes();
    }
};

struct XianSpearman : XianWarrior {
    enum {
        ANIM_START = 48,
        ANIM_KILL  = 49,
    };

    enum {
        STATE_EMPTY,
        STATE_STOP,
        STATE_STOP_2,
        STATE_WALK,
        STATE_RUN,
        STATE_AIM_1,
        STATE_HIT_1,
        STATE_AIM_2,
        STATE_HIT_2,
        STATE_AIM_3,
        STATE_HIT_3,
        STATE_AIM_4,
        STATE_HIT_4,
        STATE_AIM_5,
        STATE_HIT_5,
        STATE_AIM_6,
        STATE_HIT_6,
        STATE_DEATH,
        STATE_START,
        STATE_KILL,
    };

    XianSpearman(IGame *game, int entity) : XianWarrior(game, entity, 100, TR::Entity::ENEMY_GUARD_SPEAR_STATUE, STATE_START) {
        stepHeight =  256;
        dropHeight = -512;
        jointChest = 7;
        jointHead  = 13;
        animation.setAnim(ANIM_START);
    }

    // both spears, a killing thrust lifts her up on the spear
    void thrust(float damage) {
        int before = hits;
        bool killed = strike(XIAN_SPEAR_TOUCH_R, 1, 18, vec3(0.0f, 0.0f, 920.0f), damage, TR::HIT_GUARD);
        killed |= strike(XIAN_SPEAR_TOUCH_L, 2, 11, vec3(0.0f, 0.0f, 920.0f), damage, TR::HIT_GUARD);
        if (hits != before)
            game->playSound(SND_TR2_CRUNCH_2, pos, Sound::PAN);
        if (killed)
            animation.setAnim(ANIM_KILL);
    }

    virtual int getStateGround() {
        if (!think(true) || state == STATE_KILL)
            return state;

        wake();

        bool bored = mood == MOOD_SLEEP;
        bool ahead = targetInView;
        float d = targetDist;
        int r;

        switch (state) {
            case STATE_STOP :
                maxTurn = 0.0f;
                if (bored) {
                    r = randControl();
                    if (r < XIAN_SPEAR_STOP_CHANCE) return STATE_STOP_2;
                    if (r < XIAN_SPEAR_WALK_CHANCE) return STATE_WALK;
                    break;
                }
                if (ahead && d < XIAN_SPEAR_DIST_1) return STATE_AIM_1;
                return STATE_WALK;
            case STATE_STOP_2 :
                maxTurn = 0.0f;
                if (mood == MOOD_ESCAPE) return STATE_WALK;
                if (bored) {
                    r = randControl();
                    if (r < XIAN_SPEAR_STOP_CHANCE) return STATE_STOP;
                    if (r < XIAN_SPEAR_WALK_CHANCE) return STATE_WALK;
                    break;
                }
                if (ahead && d < XIAN_SPEAR_DIST_1) return STATE_AIM_5;
                return STATE_WALK;
            case STATE_WALK :
                maxTurn = XIAN_SPEAR_WALK_TURN;
                if (mood == MOOD_ESCAPE) return STATE_RUN;
                if (bored) {
                    r = randControl();
                    if (r < XIAN_SPEAR_STOP_CHANCE) return STATE_STOP;
                    if (r < XIAN_SPEAR_WALK_CHANCE) return STATE_STOP_2;
                    break;
                }
                if (ahead && d < XIAN_SPEAR_DIST_3) {
                    if (d < XIAN_SPEAR_DIST_2) return STATE_AIM_2;
                    return randControl() < 0x4000 ? STATE_AIM_3 : STATE_AIM_4;
                }
                if (!ahead || d > XIAN_SPEAR_DIST_RUN) return STATE_RUN;
                break;
            case STATE_RUN :
                maxTurn = XIAN_SPEAR_RUN_TURN;
                if (mood == MOOD_ESCAPE) break;
                if (bored) return randControl() < 0x4000 ? STATE_STOP : STATE_STOP_2;
                if (ahead && d < XIAN_SPEAR_DIST_3) return STATE_AIM_6;
                break;
            case STATE_AIM_1 :
                hits = 0;
                return (!ahead || d > XIAN_SPEAR_DIST_1) ? STATE_STOP : STATE_HIT_1;
            case STATE_AIM_2 :
                hits = 0;
                return (!ahead || d > XIAN_SPEAR_DIST_2) ? STATE_WALK : STATE_HIT_2;
            case STATE_AIM_3 :
            case STATE_AIM_4 :
                hits = 0;
                return (!ahead || d > XIAN_SPEAR_DIST_3) ? STATE_WALK : STATE_HIT_2;
            case STATE_AIM_5 :
                hits = 0;
                return (!ahead || d > XIAN_SPEAR_DIST_1) ? STATE_STOP_2 : STATE_HIT_5;
            case STATE_AIM_6 :
                hits = 0;
                return (!ahead || d > XIAN_SPEAR_DIST_3) ? STATE_RUN : STATE_HIT_6;
            case STATE_HIT_1 :
                thrust(XIAN_SPEAR_DAMAGE);
                break;
            case STATE_HIT_2 :
            case STATE_HIT_3 :
            case STATE_HIT_4 :
                thrust(XIAN_SPEAR_DAMAGE);
                if (state == STATE_KILL) break;
                if (ahead && d < XIAN_SPEAR_DIST_1)
                    return randControl() < 0x4000 ? STATE_STOP : STATE_STOP_2;
                return STATE_WALK;
            case STATE_HIT_5 :
                thrust(XIAN_SPEAR_DAMAGE);
                if (state == STATE_KILL) break;
                return (ahead && d < XIAN_SPEAR_DIST_1) ? STATE_STOP : STATE_STOP_2;
            case STATE_HIT_6 :
                thrust(XIAN_SPEAR_DAMAGE_RUN);
                if (state == STATE_KILL) break;
                if (ahead && d < XIAN_SPEAR_DIST_1)
                    return randControl() < 0x4000 ? STATE_STOP : STATE_STOP_2;
                return (ahead && d < XIAN_SPEAR_DIST_3) ? STATE_WALK : STATE_RUN;
        }
        return state;
    }

    virtual void updatePosition() {
        if (state == STATE_KILL) { // Lara is on its spear
            animation.overrideMask = 0;
            return;
        }
        XianWarrior::updatePosition();
    }
};

struct XianKnight : XianWarrior {
    enum {
        STATE_EMPTY,
        STATE_STOP,
        STATE_WALK,
        STATE_AIM_1,
        STATE_SLASH_1,
        STATE_AIM_2,
        STATE_SLASH_2,
        STATE_WAIT,
        STATE_FLY,
        STATE_START,
        STATE_AIM_3,
        STATE_SLASH_3,
        STATE_DEATH,
    };

    XianKnight(IGame *game, int entity) : XianWarrior(game, entity, 80, TR::Entity::ENEMY_GUARD_SWORD_STATUE, STATE_START) {
        jointChest = 7;
        jointHead  = 17;
        walk();
    }

    void walk() {
        flying     = false;
        stepHeight =  256;
        dropHeight = -256;
    }

    // Lara's ground zone differs from its own one, it has to fly there
    bool apart() {
        uint16 *ground = level->zones[level->state.flags.flipped].ground1;
        return box != TR::NO_BOX && target && target->box != TR::NO_BOX && ground[box] != ground[target->box];
    }

    void sparkle() {
        vec3 p = pos + vec3(randf() * 256.0f - 128.0f, randf() * 256.0f - 256.0f, randf() * 256.0f - 128.0f);
        game->addEntity(TR::Entity::SPARKLES, getRoomIndex(), p);
        game->playSound(SND_TR2_WARRIOR_HOVER, pos, Sound::PAN);
    }

    virtual int getStateGround() {
        walk();
        if (state == STATE_FLY && target && apart()) { // it flies over the gaps
            flying     = true;
            stepHeight =  20 * 1024;
            dropHeight = -20 * 1024;
        }

        if (!think(true))
            return state;

        wake();

        bool laraDead = target->health <= 0.0f;
        bool bite = targetCanAttack;
        float d = targetDist;

        switch (state) {
            case STATE_STOP :
                maxTurn = 0.0f;
                if (laraDead) return STATE_WAIT;
                if (bite && d < XIAN_KNIGHT_DIST_1) return randControl() < 0x4000 ? STATE_AIM_1 : STATE_AIM_2;
                if (apart()) return STATE_FLY;
                return STATE_WALK;
            case STATE_WALK :
                maxTurn = XIAN_KNIGHT_WALK_TURN;
                if (laraDead) return STATE_STOP;
                if (bite && d < XIAN_KNIGHT_DIST_3) return STATE_AIM_3;
                if (apart()) return STATE_STOP;
                break;
            case STATE_FLY :
                maxTurn = XIAN_KNIGHT_FLY_TURN;
                sparkle();
                if (!flying) return STATE_STOP;
                break;
            case STATE_AIM_1 :
                hits = 0;
                return (bite && d < XIAN_KNIGHT_DIST_1) ? STATE_SLASH_1 : STATE_STOP;
            case STATE_AIM_2 :
                hits = 0;
                return (bite && d < XIAN_KNIGHT_DIST_1) ? STATE_SLASH_2 : STATE_STOP;
            case STATE_AIM_3 :
                hits = 0;
                return (bite && d < XIAN_KNIGHT_DIST_3) ? STATE_SLASH_3 : STATE_WALK;
            case STATE_SLASH_1 :
            case STATE_SLASH_2 :
            case STATE_SLASH_3 :
                strike(XIAN_KNIGHT_TOUCH, 1, 15, vec3(0.0f, 37.0f, 550.0f), XIAN_KNIGHT_DAMAGE);
                break;
        }
        return state;
    }

    virtual void updatePosition() {
        if (flying) {
            stand = STAND_AIR;
            lift(waypoint.y - pos.y, XIAN_KNIGHT_LIFT_SPEED);
        }
        XianWarrior::updatePosition();
        stand = STAND_GROUND; // it decides on the ground states in the air too (as Natla)
    }
};

// TR2 dragon of the Dragon's Lair ("dragon_front" and "dragon_back" in the original): Bartoli turns into it, the front
// half is the creature, the back half follows it. Knocked down it lies there a while, Lara has to pull the dagger out of
// it before it comes back to life with half its health, then it melts away into its bones (as in the original)
#define DRAGON_HEALTH           300
#define DRAGON_TOUCH_DAMAGE     10
#define DRAGON_SWIPE_DAMAGE     250
#define DRAGON_RADIUS           341
#define DRAGON_DIST_CLOSE       (1024 * 3)
#define DRAGON_DIST_STOP        (1024 * 6)
#define DRAGON_WALK_TURN        (2.0f * DEG2RAD)    // a frame
#define DRAGON_NEED_TURN        (1.0f * DEG2RAD)
#define DRAGON_TOUCH_L          0x7F000000
#define DRAGON_TOUCH_R          0x000000FE
#define DRAGON_FIRE_TIME        30
#define DRAGON_LIVE_TIME        330     // frames it lies knocked down before it gets up
#define DRAGON_ALMOST_LIVE      100     // and getting up, the dagger can still be pulled out
#define DRAGON_LIGHT_TIME       (-20)
#define DRAGON_BONE_TIME        (-100)
#define DRAGON_DISSOLVE_TIME    (-240)
#define DRAGON_DISSOLVE_SHIFT   10
#define DRAGON_SIDE_DAGGER      512     // the reach along it to the dagger
#define DRAGON_CLOSE_SHIFT      900     // Lara pushed off its side to this far
#define DRAGON_FAR_SHIFT        2300
#define DRAGON_MID_SHIFT        ((DRAGON_CLOSE_SHIFT + DRAGON_FAR_SHIFT) / 2)

struct Dragon;

// the back half, it's where the front half is and plays its animations
struct DragonBack : Enemy {
    Dragon *front;

    DragonBack(IGame *game, int entity) : Enemy(game, entity, 1, DRAGON_RADIUS, 0.0f, 0.0f), front(NULL) {}

    virtual bool isActiveTarget() { return false; }
    virtual int  getInput()       { return 0; }
    virtual bool activate()       { return false; } // the front half moves it, it isn't among the active ones pushing each other
    virtual void update()         {}
    virtual void hit(float damage, Controller *enemy = NULL, TR::HitType hitType = TR::HIT_DEFAULT) {}
};

struct Dragon : Enemy {
    enum {
        STATE_EMPTY,
        STATE_WALK,
        STATE_LEFT,
        STATE_RIGHT,
        STATE_AIM,
        STATE_FIRE,
        STATE_STOP,
        STATE_TURN_LEFT,
        STATE_TURN_RIGHT,
        STATE_SWIPE_LEFT,
        STATE_SWIPE_RIGHT,
        STATE_DEATH,
    };

    enum {
        ANIM_DIE       = 21,
        ANIM_DEAD      = 22,
        ANIM_RESURRECT = 23,
    };

    enum { FLAG_GONE = 1 };

    DragonBack *back;
    int   phase;        // the flags of the original: the fire left, then the time knocked down (counting down once the dagger is out)
    int   goal;
    float turnRate;     // a frame
    float stepTime;

    Dragon(IGame *game, int entity) : Enemy(game, entity, DRAGON_HEALTH, DRAGON_RADIUS, 300.0f, 1.0f), back(NULL), phase(0), goal(-1), turnRate(0.0f), stepTime(0.0f) {
        jointHead = -1;
    }

    bool isGone() const {
        return (flags.unused & FLAG_GONE) != 0;
    }

    virtual int getInput() {
        return 0; // it doesn't die the usual way
    }

    void link() {
        if (back)
            return;
        for (int i = level->entitiesBaseCount; i < level->entitiesCount; i++) {
            TR::Entity &e = level->entities[i];
            if (e.type == TR::Entity::ENEMY_DRAGON_BACK && e.controller) {
                back = (DragonBack*)e.controller;
                back->front = this;
                return;
            }
        }
    }

    // Lara may pull the dagger out of it lying down or just getting up
    bool canPullDagger() {
        return health <= 0.0f && phase >= 0 && !isGone() && (animation.index == ANIM_DEAD || (animation.index == ANIM_RESURRECT && animation.frameIndex <= DRAGON_ALMOST_LIVE));
    }

    bool isDown() {
        return health <= 0.0f && !isGone();
    }

    void daggerPulled() {
        phase = -1;
        dropCarried();
    }

    void bones() {
        Controller *b = game->addEntity(TR::Entity::DRAGON_BONES_BACK, getRoomIndex(), pos, angle.y);
        Controller *f = game->addEntity(TR::Entity::DRAGON_BONES_FRONT, getRoomIndex(), pos, angle.y);
        if (f) {
            f->initMeshOverrides();
            f->layers[0].mask = ~0xC00000u; // no dagger in it
        }
        if (b) b->flags.invisible = false;
    }

    void gone() {
        flags.unused |= FLAG_GONE;
        flags.invisible = true;
        if (back) {
            game->checkTrigger(back, true);
            back->flags.invisible = true;
            back->deactivate(true);
        }
        deactivate(true);
    }

    // a frame of it knocked down
    void stepDown() {
        if (state != STATE_DEATH) {
            animation.setAnim(ANIM_DIE);
            goal  = -1;
            phase = 0;
            return;
        }

        if (phase >= 0) {
            phase++;
            vec3 heart = pos + vec3(0.0f, -540.0f, 0.0f) + vec3(cosf(angle.y), 0.0f, -sinf(angle.y)) * 1100.0f + vec3(sinf(angle.y), 0.0f, cosf(angle.y)) * 490.0f;
            mysticLight(this, heart);
            if (phase == DRAGON_LIVE_TIME)
                goal = STATE_STOP;
            if (phase > DRAGON_LIVE_TIME + DRAGON_ALMOST_LIVE) {
                health = DRAGON_HEALTH / 2; // up again
                goal   = -1;
            }
            return;
        }

        if (phase == DRAGON_BONE_TIME)
            bones();
        else if (phase == DRAGON_DISSOLVE_TIME) {
            gone();
            return;
        } else if (phase < DRAGON_BONE_TIME)
            pos.y += DRAGON_DISSOLVE_SHIFT;
        phase--;
    }

    void fire() {
        FlameStream *flame = (FlameStream*)game->addEntity(TR::Entity::FLAME_STREAM, getRoomIndex(), getJoint(12) * vec3(35.0f, 171.0f, 1168.0f));
        if (flame) {
            flame->aim(target);
            flame->life = 0x4000 / 30.0f; // it flies on till it hits something
        }
    }

    virtual int getStateGround() {
        if (!think(true))
            return state;

        float d = targetDist;
        bool ahead = targetInView && d > DRAGON_DIST_CLOSE && d < DRAGON_DIST_STOP;
        int touch = touching();
        if (touch)
            target->hit(DRAGON_TOUCH_DAMAGE, this);

        vec3  w = waypoint - pos;
        float a = clamp(clampAngle(normalizeAngle(PIH - w.angleY() - angle.y)), -DRAGON_WALK_TURN, DRAGON_WALK_TURN);
        turnRate = animation.getSpeed() > 0.0f ? a : 0.0f;

        switch (state) {
            case STATE_WALK :
                phase = 0;
                if (ahead) return STATE_STOP;
                if (a < -DRAGON_NEED_TURN) return (d < DRAGON_DIST_STOP && targetInView) ? STATE_STOP : STATE_LEFT;
                if (a >  DRAGON_NEED_TURN) return (d < DRAGON_DIST_STOP && targetInView) ? STATE_STOP : STATE_RIGHT;
                break;
            case STATE_LEFT :
                if (a > -DRAGON_NEED_TURN || ahead) return STATE_WALK;
                break;
            case STATE_RIGHT :
                if (a < DRAGON_NEED_TURN || ahead) return STATE_WALK;
                break;
            case STATE_AIM :
                turnRate = 0.0f;
                if (ahead) {
                    phase = DRAGON_FIRE_TIME;
                    return STATE_FIRE;
                }
                phase = 0;
                break;
            case STATE_FIRE :
                turnRate = 0.0f;
                game->playSound(SND_TR2_DRAGON_FIRE, pos, Sound::PAN);
                if (phase) {
                    if (targetInView)
                        fire();
                    phase--;
                } else
                    return STATE_STOP;
                break;
            case STATE_STOP :
                turnRate = 0.0f;
                if (ahead) return STATE_AIM;
                if (d > DRAGON_DIST_STOP || !targetInView) return STATE_WALK;
                if (d >= DRAGON_DIST_CLOSE || phase)
                    return targetAngle < 0.0f ? STATE_TURN_LEFT : STATE_TURN_RIGHT;
                phase = 1;
                return targetAngle < 0.0f ? STATE_SWIPE_LEFT : STATE_SWIPE_RIGHT;
            case STATE_TURN_LEFT :
                phase = 0;
                turnRate = -DRAGON_NEED_TURN;
                break;
            case STATE_TURN_RIGHT :
                phase = 0;
                turnRate = DRAGON_NEED_TURN;
                break;
            case STATE_SWIPE_LEFT :
                if (touch & DRAGON_TOUCH_L) {
                    target->hit(DRAGON_SWIPE_DAMAGE, this);
                    phase = 0;
                }
                break;
            case STATE_SWIPE_RIGHT :
                if (touch & DRAGON_TOUCH_R) {
                    target->hit(DRAGON_SWIPE_DAMAGE, this);
                    phase = 0;
                }
                break;
        }
        return state;
    }

    virtual void updatePosition() {
        angle.y += turnRate * 30.0f * Core::deltaTime;
        Enemy::updatePosition();
    }

    virtual void update() {
        link();
        if (isGone())
            return;

        if (health <= 0.0f) {
            stepTime += Core::deltaTime;
            while (stepTime >= 1.0f / 30.0f && !isGone()) {
                stepTime -= 1.0f / 30.0f;
                stepDown();
            }
            if (isGone())
                return;
            if (goal > -1)
                animation.setState(goal);
            Controller::updateAnimation(true);
            updateRoom();
            updateLights();
        } else
            Enemy::update();

        if (back) { // the back half follows
            back->pos       = pos;
            back->angle     = angle;
            back->roomIndex = roomIndex;
            if (back->animation.index != animation.index)
                back->animation.setAnim(animation.index);
            back->animation.time = animation.time;
            back->animation.updateInfo();
            back->updateLights();
        }
    }

    virtual void setSaveData(const SaveEntity &data) {
        Enemy::setSaveData(data);
        phase = 0;
        goal  = -1;
        if (isGone())
            flags.invisible = true;
    }
};

// TR2 Marco Bartoli in the Dragon's Lair ("bartoli" in the original): Lara coming near, the magic gathers round him, he
// blows up in three spheres of doom and the dragon stands there (as in the original)
#define BARTOLI_RANGE       (1024 * 5)
#define BARTOLI_BOOM_TIME   130

struct Bartoli : Enemy {
    enum { FLAG_CHANGED = 1 };

    int   timer;
    float stepTime;

    Bartoli(IGame *game, int entity) : Enemy(game, entity, 1, 0, 0.0f, 0.0f), timer(0), stepTime(0.0f) {
        flags.invisible = false; // he stands there waiting, not lying in ambush
    }

    virtual bool isActiveTarget() { return false; }
    virtual int  getInput()       { return 0; }
    virtual void hit(float damage, Controller *enemy = NULL, TR::HitType hitType = TR::HIT_DEFAULT) {}
    virtual bool isCollider()     { return false; }

    bool hasChanged() const {
        return (flags.unused & FLAG_CHANGED) != 0;
    }


    virtual void deactivate(bool removeFromList = false) {
        if (hasChanged())
            Enemy::deactivate(removeFromList); // not at the end of his animation, he changes later
    }

    void boom(TR::Entity::Type type) {
        Controller *sphere = game->addEntity(type, getRoomIndex(), pos + vec3(0.0f, 256.0f, 0.0f), 0.0f);
        if (sphere)
            sphere->activate();
    }

    // he's the dragon now
    void change() {
        Controller *front = game->addEntity(TR::Entity::ENEMY_DRAGON_FRONT, getRoomIndex(), pos, angle.y);
        Controller *back  = game->addEntity(TR::Entity::ENEMY_DRAGON_BACK,  getRoomIndex(), pos, angle.y);
        if (front && back) {
            front->flags.invisible = back->flags.invisible = false;
            ((Dragon*)front)->back = (DragonBack*)back;
            ((DragonBack*)back)->front = (Dragon*)front;
            ((Dragon*)front)->update(); // the back half there at once
        }
        flags.unused |= FLAG_CHANGED;
        flags.invisible = true;
        deactivate(true);
    }

    virtual void update() {
        if (hasChanged())
            return;

        stepTime += Core::deltaTime;
        while (stepTime >= 1.0f / 30.0f) {
            stepTime -= 1.0f / 30.0f;

            if (timer == 0) {
                Controller *lara = game->getLara(pos);
                if (fabsf(lara->pos.x - pos.x) < BARTOLI_RANGE && fabsf(lara->pos.z - pos.z) < BARTOLI_RANGE)
                    timer = 1;
                continue;
            }

            timer++;
            if ((timer & 7) == 0)
                game->shakeCamera(timer / 400.0f);
            mysticLight(this, pos);

            if (timer == BARTOLI_BOOM_TIME)
                boom(TR::Entity::DRAGON_EXPLOSION_1);
            else if (timer == BARTOLI_BOOM_TIME + 10)
                boom(TR::Entity::DRAGON_EXPLOSION_2);
            else if (timer == BARTOLI_BOOM_TIME + 20)
                boom(TR::Entity::DRAGON_EXPLOSION_3);
            else if (timer > BARTOLI_BOOM_TIME + 20) {
                change();
                return;
            }
        }

        if (timer)
            Controller::updateAnimation(true);
    }

    virtual void setSaveData(const SaveEntity &data) {
        Enemy::setSaveData(data);
        timer = 0;
        flags.invisible = hasChanged();
    }
};

#endif
