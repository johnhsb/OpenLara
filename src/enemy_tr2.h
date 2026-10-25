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
        int ox = int(old.x) / 1024;
        int oz = int(old.z) / 1024;
        int cx = int(pos.x) / 1024;
        int cz = int(pos.z) / 1024;

        if ((ox == cx) == (oz == cz))
            return; // not exactly one sector away

        if (oz == cz) {
            if (ox < cx) {
                pos.x   = float(cx * 1024 - THUG_VAULT_SHIFT);
                angle.y = PI * 0.5f;
            } else {
                pos.x   = float(ox * 1024 + THUG_VAULT_SHIFT);
                angle.y = -PI * 0.5f;
            }
        } else {
            if (oz < cz) {
                pos.z   = float(cz * 1024 - THUG_VAULT_SHIFT);
                angle.y = 0.0f;
            } else {
                pos.z   = float(oz * 1024 + THUG_VAULT_SHIFT);
                angle.y = -PI;
            }
        }
        pos.y = old.y;

        switch (clicks) {
            case  2 : animation.setAnim(ANIM_CLIMB_1 + climbShift); break;
            case  3 : animation.setAnim(ANIM_CLIMB_2 + climbShift); break;
            case  4 : animation.setAnim(ANIM_CLIMB_3 + climbShift); break;
            case -4 : animation.setAnim(ANIM_FALL_3  + climbShift); break;
        }
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

    virtual void update() {
        if (health <= 0.0f && !flags.invisible) { // burst with a single small explosion
            game->playSound(SND_TR2_SPIDER_EXPLODE, pos, Sound::PAN);
            Explosion *explosion = (Explosion*)game->addEntity(TR::Entity::EXPLOSION, getRoomIndex(), getBoundingBox().center());
            if (explosion)
                explosion->volume = 0.5f; // the original spider sound is too quiet to be heard over the shots
            flags.invisible = true;
            deactivate(true);
            return;
        }

        Enemy::update();
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
    }

    virtual int getStateGround() {
        if (!think(true))
            return state;

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
                if (randControl() < TIGER_ROAR) {
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
                if (mood != MOOD_ATTACK && randControl() < TIGER_ROAR) {
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

#endif
