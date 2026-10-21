#ifndef H_ENEMY_TR2
#define H_ENEMY_TR2

// the enemies of Tomb Raider 2 (and the later games reusing them)

#include "enemy.h"

struct Dog : Enemy {

    enum {
        ANIM_SLEEP = 5,
        ANIM_DEATH = 13,
    };

    enum {
        STATE_DEATH = 10,
    };

    Dog(IGame *game, int entity) : Enemy(game, entity, 6, 10, 0.0f, 0.0f) {
        jointChest = 19;
        jointHead  = 20;
        animation.setAnim(ANIM_SLEEP);
    }

    virtual int getStateDeath() {
        if (state != STATE_DEATH)
            return animation.setAnim(ANIM_DEATH);
        return state;
    }
};

#define TIGER_WALK           1120
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

    Tiger(IGame *game, int entity) : Enemy(game, entity, 20, 341, 200.0f, 0.25f) {
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
                if (mood == MOOD_ESCAPE)
                    return STATE_RUN;
                if (mood == MOOD_SLEEP) {
                    int r = rand();
                    if (r < TIGER_ROAR) return STATE_ROAR;
                    if (r < TIGER_WALK) return STATE_WALK;
                    return state;
                }
                if (targetInView && targetDist < TIGER_DIST_ATTACK_1)
                    return STATE_ATTACK_1;
                if (targetInView && targetDist < TIGER_DIST_ATTACK_3)
                    return STATE_ATTACK_3;
                if (nextState != STATE_NONE)
                    return nextState;
                if (mood != MOOD_ATTACK && rand() < TIGER_ROAR)
                    return STATE_ROAR;
                return STATE_RUN;
            case STATE_WALK     : 
                if (mood == MOOD_ATTACK || mood == MOOD_ESCAPE)
                    return STATE_RUN;
                if (rand() < TIGER_ROAR) {
                    nextState = STATE_ROAR;
                    return STATE_STOP;
                }
                break;
            case STATE_RUN      : {
                bool melee = flags.unused != 0;
                flags.unused = 0;

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
                if (mood != MOOD_ATTACK && rand() < TIGER_ROAR) {
                    nextState = STATE_ROAR;
                    return STATE_STOP;
                }
                break;
            }
            case STATE_ATTACK_1 :
            case STATE_ATTACK_2 :
            case STATE_ATTACK_3 :
                if (flags.unused == 0 && (collide(target) & HIT_MASK)) {
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
        turn(state == STATE_RUN || state == STATE_WALK || state == STATE_ROAR, state == STATE_RUN ? TIGER_TURN_FAST : TIGER_TURN_SLOW);

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

        if (rand() < 0x100) {
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
