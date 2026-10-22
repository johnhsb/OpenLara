#ifndef H_OBJECTS_TR2
#define H_OBJECTS_TR2

// the objects of Tomb Raider 2 (and the later games reusing them)

#include "objects.h"

// TR2 traps

#define SND_TR2_SPIKE_WALL      204
#define SND_TR2_FLESH_WOUND     205
#define SND_TR2_ROLLING_BLADE   231
#define SND_TR2_ZIPLINE_GO      280
#define SND_TR2_ZIPLINE_STOP    281


#define SPINDLE_DAMAGE          100
#define SPINDLE_LOOK_AHEAD      1536.0f

// a blade spinning along the floor, it turns back when it runs out of floor ahead
struct TrapSpindle : Controller {
    enum {
        STATE_STOP = 1,
        STATE_SPIN = 2,
    };

    bool stopping, spinning;

    TrapSpindle(IGame *game, int entity) : Controller(game, entity), stopping(false), spinning(false) {
        animation.setAnim(3);
    }

    virtual void update() {
        if (state == STATE_SPIN) {
            if (!stopping) {
                int16 roomIdx = getRoomIndex();
                vec3 p = pos + getDir() * SPINDLE_LOOK_AHEAD;
                TR::Room::Sector *sector = level->getSector(roomIdx, p);
                if (level->getFloor(sector, p) == TR::NO_FLOOR * 256)
                    stopping = true;
            }

            if (stopping)
                animation.setState(STATE_STOP);

            spinning = true;

            Character *lara = (Character*)game->getLara(pos);
            if (lara->health > 0.0f && touchLara(this, lara))
                lara->hit(SPINDLE_DAMAGE * 30.0f * Core::deltaTime, this, TR::HIT_BLADE);

            game->playSound(SND_TR2_ROLLING_BLADE, pos, Sound::PAN | Sound::UNIQUE);
        } else {
            if (isActive())
                animation.setState(STATE_SPIN);
        }

        updateAnimation(true);

        pos += getDir() * (animation.getSpeed() * 30.0f * Core::deltaTime);
        updateRoom();

        TR::Room::Sector *sector = level->getSector(roomIndex, pos);
        pos.y = level->getFloor(sector, pos);

        if (spinning && state == STATE_STOP) {
            angle.y = clampAngle(angle.y + PI);
            stopping = spinning = false;
        }
    }
};

#define BLADE_WALL_DAMAGE       100
#define BLADE_WALL_MASK         (1 << 1) // the blade mesh

// a blade slicing out of the wall while triggered
struct TrapBladeWall : Controller {
    enum {
        STATE_STOP = 1,
        STATE_CUT  = 2,
    };

    TrapBladeWall(IGame *game, int entity) : Controller(game, entity) {
        animation.setAnim(2);
    }

    virtual void update() {
        if (isActive() && state == STATE_STOP)
            animation.setState(STATE_CUT);
        else
            animation.setState(STATE_STOP);

        if (state == STATE_CUT) {
            Character *lara = (Character*)game->getLara(pos);
            if (lara->health > 0.0f && (touchLara(this, lara) & BLADE_WALL_MASK))
                lara->hit(BLADE_WALL_DAMAGE * 30.0f * Core::deltaTime, this, TR::HIT_BLADE);
        }

        updateAnimation(true);
    }
};

#define BLADE_STATUE_DAMAGE     20
#define BLADE_STATUE_MASK       0x80    // the sword

// the statue swinging its sword while triggered
struct TrapBladeStatue : Controller {
    enum {
        STATE_STOP = 1,
        STATE_CUT  = 2,
    };

    TrapBladeStatue(IGame *game, int entity) : Controller(game, entity) {
        animation.setAnim(3);
    }

    virtual void update() {
        if (isActive() && state == STATE_STOP)
            animation.setState(STATE_CUT);
        else
            animation.setState(STATE_STOP);

        if (state == STATE_CUT) {
            Character *lara = (Character*)game->getLara(pos);
            if (lara->health > 0.0f && (touchLara(this, lara) & BLADE_STATUE_MASK))
                lara->hit(BLADE_STATUE_DAMAGE * 30.0f * Core::deltaTime, this, TR::HIT_BLADE);
        }

        updateAnimation(true);
    }
};

#define PENDULUM_DAMAGE         50

// the sandbags and boxes swinging in the Opera House, they hit whoever they touch while triggered and stop at
// the end of a swing once the trigger is off
struct TrapPendulum : Controller {
    int touchBits;

    TrapPendulum(IGame *game, int entity) : Controller(game, entity), touchBits(0) {}

    virtual void touchedBy(int mask) {
        touchBits |= mask;
    }

    virtual void update() {
        if (!isActive() && animation.frameIndex >= animation.framesCount - 1) {
            animation.setAnim(0);
            touchBits = 0;
            deactivate(true);
            return;
        }

        Character *lara = (Character*)game->getLara(pos);
        int touch = touchBits | touchLara(this, lara);
        touchBits = 0;
        if (touch && lara->health > 0.0f)
            lara->hit(PENDULUM_DAMAGE * 30.0f * Core::deltaTime, this, TR::HIT_BLADE);

        updateAnimation(true);
    }
};

#define PROPELLER_DAMAGE        200
#define PROPELLER_MASK          6       // the blades
#define SND_TR2_AIRPLANE_IDLE   213
#define SND_TR2_UNDERWATER_FAN_ON  215
#define SND_TR2_UNDERWATER_FAN_OFF 216
#define SND_TR2_SMALL_FAN_ON    217
#define SND_TR2_SAW_REVVING     206
#define SND_TR2_SAW_STOP        207

// the propellers of the planes, the underwater ones and the fans: turning while triggered they shred Lara, then slow
// down to a stop, the power saws of the Diving Area as well
struct TrapPropeller : Controller {
    enum {
        STATE_ON,
        STATE_OFF,
    };

    int  touchBits;
    bool turning;

    TrapPropeller(IGame *game, int entity) : Controller(game, entity), touchBits(0), turning(false) {}

    virtual void touchedBy(int mask) {
        touchBits |= mask;
    }

    virtual bool isCollider() { // turning it doesn't push Lara off, it cuts her (a trap collision in the original), a saw is solid
        return !isActive() || getEntity().type == TR::Entity::SAW;
    }

    virtual void update() {
        int touch = touchBits;
        touchBits = 0;

        int sound;
        switch (getEntity().type) {
            case TR::Entity::PROPELLER_PLANE : sound = SND_TR2_AIRPLANE_IDLE;     break;
            case TR::Entity::PROPELLER_WATER : sound = SND_TR2_UNDERWATER_FAN_ON; break;
            case TR::Entity::SAW             : sound = SND_TR2_SAW_REVVING;       break;
            default                          : sound = SND_TR2_SMALL_FAN_ON;
        }

        if (isActive()) {
            animation.setState(STATE_ON);
            turning = true;

            Character *lara = (Character*)game->getLara(pos);
            touch |= touchLara(this, lara);
            if ((touch & PROPELLER_MASK) && lara->health > 0.0f) {
                lara->hit(PROPELLER_DAMAGE * 30.0f * Core::deltaTime, this, TR::HIT_BLADE);
                if (getEntity().type == TR::Entity::SAW)
                    game->playSound(SND_TR2_SAW_STOP, pos, Sound::PAN | Sound::UNIQUE);
            } else
                game->playSound(sound, pos, Sound::PAN | Sound::UNIQUE);
        } else {
            if (state == STATE_ON && getEntity().type == TR::Entity::PROPELLER_PLANE)
                game->playSound(sound, pos, Sound::PAN | Sound::UNIQUE);
            if (turning && getEntity().type == TR::Entity::PROPELLER_WATER)
                game->playSound(SND_TR2_UNDERWATER_FAN_OFF, pos, Sound::PAN);
            turning = false;
            animation.setState(STATE_OFF);
        }

        updateAnimation(true);

        if (state == STATE_OFF && animation.isEnded)
            deactivate(true);
    }
};

#define SND_TR2_PLATFORM_ALARM  332
#define ALARM_PERIOD            13      // frames of one flash
#define ALARM_DARK              7       // frames of it before the light
#define ALARM_LIGHT_RADIUS      2048.0f

// the alarm of the rig rings while triggered, its light flashing
struct SoundAlarm : Controller {
    float phase;

    SoundAlarm(IGame *game, int entity) : Controller(game, entity), phase(0.0f) {}

    virtual void update() {
        getRoom().removeDynLight(entity);

        if (flags.active != TR::ACTIVE) {
            deactivate(true);
            return;
        }

        game->playSound(SND_TR2_PLATFORM_ALARM, pos, Sound::PAN | Sound::UNIQUE);

        phase += 30.0f * Core::deltaTime;
        if (phase >= ALARM_PERIOD)
            phase -= ALARM_PERIOD;
        if (phase >= ALARM_DARK)
            getRoom().addDynLight(entity, vec4(pos, 0.0f), vec4(vec3(0.75f), 1.0f / ALARM_LIGHT_RADIUS));
    }
};

#define LIFT_WAIT       3.0f    // seconds with the door open before it moves
#define LIFT_TRAVEL     (22 * 256)
#define LIFT_SPEED      16.0f   // per frame
#define LIFT_HEIGHT     1280

// the lift of the Opera House: triggered it goes down 22 clicks, back up when the trigger is off, waiting with its
// door open at either end; the 2x2 sectors of its shaft get its floor and ceiling
struct Elevator : Controller {
    enum {
        STATE_DOOR_CLOSED,
        STATE_DOOR_OPEN,
    };

    float startY;
    float waitTime;

    Elevator(IGame *game, int entity) : Controller(game, entity), waitTime(0.0f) {
        startY = float(getEntity().y);
    }

    // the shaft takes the sector of the lift and the ones behind and beside it, as its direction says
    void getShaftOffset(int &ox, int &oz) {
        switch (angleQuadrant(angle.y, 0.25f)) { // north, east, south, west
            case 0  : ox = -1; oz = +1; break;
            case 1  : ox = +1; oz = +1; break;
            case 2  : ox = +1; oz = -1; break;
            default : ox = -1; oz = -1; break;
        }
    }

    bool inShaft(int tx, int tz) {
        int lx = int(pos.x) / 1024, lz = int(pos.z) / 1024;
        int ox, oz;
        getShaftOffset(ox, oz);
        return (tx == lx || tx + ox == lx) && (tz == lz || tz + oz == lz);
    }

    virtual void liftFloor(int sx, int sz, int y, const vec3 &subject, float &floor, float &ceiling) {
        bool pointIn   = inShaft(sx, sz);
        bool subjectIn = inShaft(int(subject.x) / 1024, int(subject.z) / 1024);

        float liftBottom  = pos.y + 256;
        float liftFloor   = pos.y;
        float liftCeiling = pos.y - LIFT_HEIGHT + 256;
        float liftTop     = pos.y - LIFT_HEIGHT;
        bool  inside      = subject.y < liftBottom && subject.y > liftCeiling;

        float newFloor   = 32512.0f;
        float newCeiling = -32512.0f;

        if (subjectIn) {
            if (state == STATE_DOOR_CLOSED && inside) {
                if (pointIn) {
                    newFloor   = liftFloor;
                    newCeiling = liftCeiling;
                } else
                    newFloor = newCeiling = -32512.0f; // the closed door walls her in
            } else if (pointIn) {
                if (subject.y <= liftCeiling)
                    newFloor = liftTop;
                else if (subject.y <= liftBottom) {
                    newFloor   = liftFloor;
                    newCeiling = liftCeiling;
                } else
                    newCeiling = liftBottom;
            }
        } else if (pointIn) {
            if (y <= liftTop)
                newFloor = liftTop;
            else if (y >= liftBottom)
                newCeiling = liftBottom;
            else if (state == STATE_DOOR_OPEN) {
                newFloor   = liftFloor;
                newCeiling = liftCeiling;
            } else
                newFloor = newCeiling = -32512.0f;
        }

        if (newFloor == -32512.0f && newCeiling == -32512.0f) {
            floor = ceiling = -32512.0f;
            return;
        }
        if (newFloor < floor)
            floor = newFloor;
        if (newCeiling > ceiling)
            ceiling = newCeiling;
    }

    virtual void update() {
        float target = startY + (isActive() ? LIFT_TRAVEL : 0.0f);

        if (pos.y == target) {
            animation.setState(STATE_DOOR_OPEN);
            waitTime = 0.0f;
        } else if (waitTime < LIFT_WAIT) {
            animation.setState(STATE_DOOR_OPEN);
            waitTime += Core::deltaTime;
        } else {
            animation.setState(STATE_DOOR_CLOSED);
            float step = LIFT_SPEED * 30.0f * Core::deltaTime;
            float d = target - pos.y;

        // Lara standing on it rides along (the original snaps her down to the floor that drops away)
            Character *lara = (Character*)game->getLara(pos);
            bool riding = lara->stand == Character::STAND_GROUND && fabsf(lara->pos.y - pos.y) < 32.0f &&
                          inShaft(int(lara->pos.x) / 1024, int(lara->pos.z) / 1024);

            pos.y += clamp(d, -step, step);

            if (riding)
                lara->pos.y = pos.y;
        }

        updateAnimation(true);

        // the room a click up, not to sit on a portal
        vec3 p = pos - vec3(0.0f, 256.0f, 0.0f);
        int16 roomIndex = getRoomIndex();
        level->getSector(roomIndex, p);
        this->roomIndex = roomIndex;
    }
};

#define SPIKE_WALL_DAMAGE       20
#define SPIKE_WALL_SPEED        16.0f

// a wall of spikes sliding forward while triggered, until it hits a wall or a step
struct TrapSpikeWall : Controller {
    bool finished;

    TrapSpikeWall(IGame *game, int entity) : Controller(game, entity), finished(false) {}

    virtual void update() {
        if (!isActive()) {
            const TR::Entity &e = getEntity();
            roomIndex = e.room;
            pos       = vec3(float(e.x), float(e.y), float(e.z));
            finished  = false;
            Sound::stop(SND_TR2_SPIKE_WALL);
        } else if (!finished) {
            int16 roomIdx = getRoomIndex();
            vec3 p = pos + getDir() * (SPIKE_WALL_SPEED * 30.0f * Core::deltaTime);
            TR::Room::Sector *sector = level->getSector(roomIdx, p);
            if (level->getFloor(sector, p) != pos.y) {
                finished = true;
                Sound::stop(SND_TR2_SPIKE_WALL);
            } else {
                pos = p;
                roomIndex = roomIdx;
                game->playSound(SND_TR2_SPIKE_WALL, pos, Sound::PAN | Sound::UNIQUE);
            }
        }

        Character *lara = (Character*)game->getLara(pos);
        if (lara->health > 0.0f && touchLara(this, lara)) {
            lara->hit(SPIKE_WALL_DAMAGE * 30.0f * Core::deltaTime, this, TR::HIT_SPIKES);
            game->playSound(SND_TR2_FLESH_WOUND, lara->pos, Sound::PAN | Sound::UNIQUE);
        }
    }
};

#define ZIPLINE_ACCEL           5.0f
#define ZIPLINE_MAX_SPEED       100.0f

// the handle Lara grabs to slide down a cable, it returns to the start after each ride
struct Zipline : Controller {
    enum {
        STATE_GRAB = 1,
        STATE_HANG = 2,
    };

    vec3  startPos;
    int16 startRoom;
    float speed;
    bool  riding;

    Zipline(IGame *game, int entity) : Controller(game, entity), speed(0.0f), riding(false) {
        startPos  = pos;
        startRoom = roomIndex;
    }

    void grab() {
        riding = true;
        speed  = 0.0f;
        animation.setAnim(0);
        activate();
    }

    virtual void update() {
        if (!riding) {
            pos       = startPos;
            roomIndex = startRoom;
            speed     = 0.0f;
            animation.setAnim(0);
            deactivate(true);
            return;
        }

        updateAnimation(true);

        if (state == STATE_GRAB)
            return;

        speed = min(speed + ZIPLINE_ACCEL * 30.0f * Core::deltaTime, ZIPLINE_MAX_SPEED);

        vec3 dir = getDir();
        pos.y += speed * 0.25f * 30.0f * Core::deltaTime;
        pos   += dir * (speed * 30.0f * Core::deltaTime);
        updateRoom();

        int16 roomIdx = getRoomIndex();
        vec3 p = pos + vec3(0.0f, 64.0f, 0.0f) + dir * 1024.0f;
        TR::Room::Sector *sector = level->getSector(roomIdx, p);
        if (level->getFloor(sector, p) > p.y + 256.0f && level->getCeiling(sector, p) < p.y - 256.0f) {
            game->playSound(SND_TR2_ZIPLINE_GO, pos, Sound::PAN | Sound::UNIQUE);
            return;
        }

        riding = false;
        Sound::stop(SND_TR2_ZIPLINE_GO);
        game->playSound(SND_TR2_ZIPLINE_STOP, pos, Sound::PAN);
    }
};


#define GONDOLA_SINK_SPEED      50.0f
#define SND_TR2_BOAT_INTO_WATER 337

// TR2 gondola, the speedboat crashes into it at full speed and it sinks
struct Gondola : Controller {
    enum {
        STATE_FLOATING = 1,
        STATE_CRASH,
        STATE_SINK,
        STATE_LAND,
    };

    bool crashing;

    Gondola(IGame *game, int entity) : Controller(game, entity), crashing(false) {}

    bool isFloating() const {
        return state == STATE_FLOATING && !crashing;
    }

    virtual bool isCollider() {
        return isFloating();
    }

    virtual bool explodeFx() {
        return false;
    }

    void crash() {
        if (!isFloating())
            return;
        crashing = true;
        explode(0xF0, 0.0f); // the bow and the stern fly off, the hull (meshes 0-3) sinks
        game->playSound(SND_TR2_BOAT_INTO_WATER, pos, Sound::PAN);
        activate();
    }

    virtual void update() {
        if (crashing && state == STATE_FLOATING)
            animation.setState(STATE_CRASH);

        if (state == STATE_SINK) {
            pos.y += GONDOLA_SINK_SPEED * 30.0f * Core::deltaTime;
            Box box = animation.getBoundingBox(vec3(0.0f), 0);
            int16 room = getRoomIndex();
            vec3 p = vec3(pos.x, pos.y + box.min.y + 384.0f, pos.z);
            TR::Room::Sector *sector = level->getSector(room, p);
            float floor = level->getFloor(sector, pos);
            roomIndex = room;
            if (pos.y >= floor) {
                pos.y = floor;
                animation.setState(STATE_LAND);
            }
        }

        if (explodeMask)
            updateExplosion();
        updateAnimation(true);
        updateLights();
    }
};

#define BOAT_WAKE_LIFE      20
#define BOAT_WAKE_DIST      700

// the spray behind the speedboat, a water sprite flying off for a moment
struct BoatWake : Sprite {
    int   counter, speed, fallSpeed;
    float stepTime;

    BoatWake(IGame *game, int entity) : Sprite(game, entity, true, 0), counter(BOAT_WAKE_LIFE), speed(0), fallSpeed(0), stepTime(0.0f) {}

    virtual void update() {
        stepTime += Core::deltaTime;
        while (stepTime >= 1.0f / 30.0f) {
            stepTime -= 1.0f / 30.0f;

            counter--;
            if (counter % 4 == 0)
                frame = (frame + 1) % max(getSequence().sCount, int16(1));

            if (counter <= 0 || fallSpeed > 0) {
                game->removeEntity(this);
                return;
            }

            pos += vec3(sinf(angle.y), 0.0f, cosf(angle.y)) * float(speed);
            if (fallSpeed) {
                pos.y += fallSpeed;
                fallSpeed += 6; // gravity
            }
        }
    }
};

// TR2 speedboat, the physics run at 30 fps in the original units (angles: 65536 = 360 degrees)

#define BOAT_RADIUS         500
#define BOAT_SIDE           300
#define BOAT_FRONT          750
#define BOAT_TIP            (BOAT_FRONT + 250)
#define BOAT_MIN_SPEED      20
#define BOAT_MAX_SPEED      90
#define BOAT_SLOW_SPEED     (BOAT_MAX_SPEED / 3)
#define BOAT_FAST_SPEED     (BOAT_MAX_SPEED + 50)
#define BOAT_MAX_BACK       (-20)
#define BOAT_ACCEL          5
#define BOAT_BRAKE          5
#define BOAT_REVERSE        (-5)
#define BOAT_SLOWDOWN       1
#define BOAT_UNDO_TURN      (182 / 4)
#define BOAT_TURN           (182 / 8)
#define BOAT_MAX_TURN       (182 * 4)
#define BOAT_SHIFT_Y        (-5)
#define BOAT_GRAVITY        6
#define BOAT_NO_HEIGHT      (TR::NO_FLOOR * 256)

#define SND_TR2_BOAT_IDLE   195
#define SND_TR2_BOAT_MOVING 197
#define SND_TR2_BOAT_ENGINE 336

struct SpeedBoat : Controller {
    enum {
        STATE_MOUNT,
        STATE_STILL,
        STATE_MOVING,
        STATE_JUMP_R,
        STATE_JUMP_L,
        STATE_HIT,
        STATE_FALL,
        STATE_DEATH = 8,
    };

    // animations of Lara on the boat (LARA_BOAT model), the boat plays the same ones
    enum {
        ANIM_MOUNT_LEFT  = 0,
        ANIM_MOUNT_START = 1,
        ANIM_MOUNT_JUMP  = 6,
        ANIM_MOUNT_RIGHT = 8,
        ANIM_HIT_LEFT    = 11,
        ANIM_HIT_RIGHT   = 12,
        ANIM_HIT_FRONT   = 13,
        ANIM_HIT_BACK    = 14,
        ANIM_FALL        = 15,
        ANIM_DEATH       = 18,
    };

    struct Pos { int x, y, z; };

    enum { FLAG_DESTROYED = 1 };

    Character *driver;   // Lara while she's on the boat
    bool  control;       // the driver may steer (not mounting or jumping off)
    int   x, y, z;
    int   rotX, rotY, rotZ;
    int   speed, fallSpeed, turnSpeed, leftFall, rightFall, water, tilt, extraRot;
    int   collide;       // hit animation of the last step
    float stepTime;

    SpeedBoat(IGame *game, int entity) : Controller(game, entity), driver(NULL), control(false),
        speed(0), fallSpeed(0), turnSpeed(0), leftFall(0), rightFall(0), water(0), tilt(0), extraRot(0), collide(0), stepTime(0.0f) {
        x = int(pos.x);
        y = int(pos.y);
        z = int(pos.z);
        rotX = rotZ = 0;
        rotY = int16(int(angle.y * (32768.0f / PI)));
    }

    static int tsin(int a) { return int(sinf(a * (PI / 32768.0f)) * 16384.0f); }
    static int tcos(int a) { return int(cosf(a * (PI / 32768.0f)) * 16384.0f); }
    static int tatan(int x, int y) { return int(atan2f(float(y), float(x)) * (32768.0f / PI)); }

    int getWaterHeight(int16 &room, const vec3 &p) {
        int16 r = room;
        TR::Room::Sector *sector = level->getWaterLevelSector(r, p);
        return sector ? sector->ceiling * 256 : BOAT_NO_HEIGHT;
    }

    int getHeight(int16 &room, const Pos &p) {
        TR::Room::Sector *sector = level->getSector(room, vec3(float(p.x), float(p.y), float(p.z)));
        return int(level->getFloor(sector, vec3(float(p.x), float(p.y), float(p.z))));
    }

    // the water (or floor) height under a point of the hull
    int testWaterHeight(int zOff, int xOff, Pos &p) {
        int s = tsin(rotY), c = tcos(rotY);
        p.x = x + ((c * xOff + s * zOff) >> 14);
        p.z = z + ((c * zOff - s * xOff) >> 14);
        p.y = y + ((xOff * tsin(rotZ)) >> 14) - ((zOff * tsin(rotX)) >> 14);

        int16 room = getRoomIndex();
        vec3 v = vec3(float(p.x), float(p.y), float(p.z));
        TR::Room::Sector *sector = level->getSector(room, v);
        int h = getWaterHeight(room, v);
        if (h == BOAT_NO_HEIGHT) {
            h = int(level->getFloor(sector, v));
            return h;
        }
        return h + BOAT_SHIFT_Y;
    }

    // pushes the boat back out of a wall, returns the extra rotation
    int doShift(const Pos &p, const Pos &old) {
        int px = p.x >> 10, pz = p.z >> 10;
        int ox = old.x >> 10, oz = old.z >> 10;
        int sx = p.x & 1023, sz = p.z & 1023;

        if (px == ox) {
            if (pz == oz) {
                x += old.x - p.x;
                z += old.z - p.z;
            } else if (pz > oz) {
                z -= sz + 1;
                return p.x - x;
            } else {
                z += 1024 - sz;
                return x - p.x;
            }
        } else if (pz == oz) {
            if (px > ox) {
                x -= sx + 1;
                return z - p.z;
            } else {
                x += 1024 - sx;
                return p.z - z;
            }
        } else {
            int dx = 0, dz = 0;
            int16 room = getRoomIndex();
            Pos t = { old.x, p.y, p.z };
            if (getHeight(room, t) < old.y - 256)
                dz = (p.z > old.z) ? (-sz - 1) : (1024 - sz);

            room = getRoomIndex();
            t.x = p.x; t.z = old.z;
            if (getHeight(room, t) < old.y - 256)
                dx = (p.x > old.x) ? (-sx - 1) : (1024 - sx);

            if (dx && dz) {
                x += dx;
                z += dz;
            } else if (dz) {
                z += dz;
                return dz > 0 ? (x - p.x) : (p.x - x);
            } else if (dx) {
                x += dx;
                return dx > 0 ? (p.z - z) : (z - p.z);
            } else {
                x += old.x - p.x;
                z += old.z - p.z;
            }
        }
        return 0;
    }

    int getCollisionAnim(int mx, int mz) {
        mx = x - mx;
        mz = z - mz;
        if (!mx && !mz)
            return 0;
        int s = tsin(rotY), c = tcos(rotY);
        int front = (mx * s + mz * c) >> 14;
        int side  = (mx * c - mz * s) >> 14;
        if (abs(front) > abs(side))
            return front > 0 ? ANIM_HIT_BACK : ANIM_HIT_FRONT;
        return side > 0 ? ANIM_HIT_LEFT : ANIM_HIT_RIGHT;
    }

    static int doDynamics(int height, int fall, int &py) {
        if (height > py) {
            py += fall;
            if (py > height) {
                py = height;
                fall = 0;
            } else
                fall += BOAT_GRAVITY;
        } else {
            fall += (height - fall - py) >> 3;
            if (fall < -20) fall = -20;
            if (py > height) py = height;
        }
        return fall;
    }

    int dynamics() {
        rotZ -= tilt;

        Pos flOld, frOld, blOld, brOld, fOld;
        int hflOld = testWaterHeight( BOAT_FRONT, -BOAT_SIDE, flOld);
        int hfrOld = testWaterHeight( BOAT_FRONT,  BOAT_SIDE, frOld);
        int hblOld = testWaterHeight(-BOAT_FRONT, -BOAT_SIDE, blOld);
        int hbrOld = testWaterHeight(-BOAT_FRONT,  BOAT_SIDE, brOld);
        int hfOld  = testWaterHeight( BOAT_TIP,    0,         fOld);
        Pos old = { x, y, z };
        blOld.y = min(blOld.y, hblOld);
        brOld.y = min(brOld.y, hbrOld);
        flOld.y = min(flOld.y, hflOld);
        frOld.y = min(frOld.y, hfrOld);
        fOld.y  = min(fOld.y,  hfOld);

        rotY += extraRot + turnSpeed;
        tilt  = turnSpeed * 6;

        x += (speed * tsin(rotY)) >> 14;
        z += (speed * tcos(rotY)) >> 14;

        int slip = (tsin(rotZ) * 30) >> 14;
        if (!slip && rotZ)
            slip = rotZ > 0 ? 1 : -1;
        x += (slip * tcos(rotY)) >> 14;
        z -= (slip * tsin(rotY)) >> 14;

        slip = (tsin(rotX) * 10) >> 14;
        if (!slip && rotX)
            slip = rotX > 0 ? 1 : -1;
        x -= (slip * tsin(rotY)) >> 14;
        z -= (slip * tcos(rotY)) >> 14;

        int movedX = x, movedZ = z;
        doShiftGondolas();
        int rot = 0;
        Pos p;

        if (testWaterHeight(-BOAT_FRONT, -BOAT_SIDE, p) < blOld.y - 128) rot  = doShift(p, blOld);
        if (testWaterHeight(-BOAT_FRONT,  BOAT_SIDE, p) < brOld.y - 128) rot += doShift(p, brOld);
        if (testWaterHeight( BOAT_FRONT, -BOAT_SIDE, p) < flOld.y - 128) rot += doShift(p, flOld);
        if (testWaterHeight( BOAT_FRONT,  BOAT_SIDE, p) < frOld.y - 128) rot += doShift(p, frOld);
        if (!slip && testWaterHeight(BOAT_TIP, 0, p) < fOld.y - 128) doShift(p, fOld);

        int16 room = getRoomIndex();
        vec3 v = vec3(float(x), float(y), float(z));
        TR::Room::Sector *sector = level->getSector(room, v);
        int height = getWaterHeight(room, v);
        if (height == BOAT_NO_HEIGHT)
            height = int(level->getFloor(sector, v));
        if (height < y - 128) {
            Pos cur = { x, y, z };
            doShift(cur, old);
        }

        extraRot = rot;

        int hit = getCollisionAnim(movedX, movedZ);
        if (slip || hit) {
            int dx = x - old.x, dz = z - old.z;
            int newSpeed = (dx * tsin(rotY) + dz * tcos(rotY)) >> 14;

            if (driver && speed > BOAT_MAX_SPEED + BOAT_ACCEL && newSpeed < speed - 10)
                driver->hit(float((speed - newSpeed) / 2));

            if (slip) {
                if (speed <= BOAT_MAX_SPEED + 10)
                    speed = newSpeed;
            } else {
                if ((speed > 0 && newSpeed < speed) || (speed < 0 && newSpeed > speed))
                    speed = newSpeed;
            }
            if (speed < BOAT_MAX_BACK)
                speed = BOAT_MAX_BACK;
        }
        return hit;
    }

    // a floating gondola stops the boat at its stern, unless the boat rams it at full speed
    void doShiftGondolas() {
        for (int i = 0; i < level->entitiesBaseCount; i++) {
            const TR::Entity &e = level->entities[i];
            if (e.type != TR::Entity::GONDOLA || !e.controller)
                continue;
            Gondola *g = (Gondola*)e.controller;
            if (!g->isFloating())
                continue;

            int gy = int(g->angle.y * (32768.0f / PI));
            int sx = int(g->pos.x) - ((512 * tsin(gy)) >> 14);
            int sz = int(g->pos.z) - ((512 * tcos(gy)) >> 14);
            int dx = sx - x;
            int dz = sz - z;
            int dist = dx * dx + dz * dz;
            const int R2 = (BOAT_RADIUS * 2) * (BOAT_RADIUS * 2);

            if (dist >= R2 || dist == 0)
                continue;

            if (speed < BOAT_MAX_SPEED - 10) {
                x = sx - int((int64)R2 * dx / dist);
                z = sz - int((int64)R2 * dz / dist);
            } else if (int(g->pos.y) - y < 2048)
                g->crash();
        }
    }

    // returns true if there's no turn input
    bool userControl() {
        if (y < water - 128 || water == BOAT_NO_HEIGHT)
            return true;

        int input = driver->input;
        if ((input & Character::LOOK) && speed == 0)
            return true;
        if (input & Character::JUMP)
            return true;

        bool left  = (input & Character::LEFT)  != 0;
        bool right = (input & Character::RIGHT) != 0;
        bool back  = (input & Character::BACK)  != 0;
        bool noTurn = true;

        if ((left && !back) || (right && back)) {
            if (turnSpeed > 0)
                turnSpeed -= BOAT_UNDO_TURN;
            else
                turnSpeed = max(turnSpeed - BOAT_TURN, -BOAT_MAX_TURN);
            noTurn = false;
        } else if ((right && !back) || (left && back)) {
            if (turnSpeed < 0)
                turnSpeed += BOAT_UNDO_TURN;
            else
                turnSpeed = min(turnSpeed + BOAT_TURN, BOAT_MAX_TURN);
            noTurn = false;
        }

        if (back) {
            if (speed > 0)
                speed -= BOAT_BRAKE;
            else if (speed > BOAT_MAX_BACK)
                speed += BOAT_REVERSE;
        } else if (input & Character::FORTH) {
            int maxSpeed = (input & Character::ACTION) ? BOAT_FAST_SPEED : ((input & Character::WALK) ? BOAT_SLOW_SPEED : BOAT_MAX_SPEED);
            if (speed < maxSpeed)
                speed += BOAT_ACCEL / 2 + BOAT_ACCEL * speed / (2 * maxSpeed);
            else if (speed > maxSpeed + BOAT_SLOWDOWN)
                speed -= BOAT_SLOWDOWN;
        } else if (speed >= 0 && speed < BOAT_MIN_SPEED && (left || right)) {
            speed = BOAT_MIN_SPEED;
        } else if (speed > BOAT_SLOWDOWN) {
            speed -= BOAT_SLOWDOWN;
        } else
            speed = 0;

        return noTurn;
    }

    void step() {
        collide = dynamics();

        Pos fl, fr;
        int hfl = testWaterHeight(BOAT_FRONT, -BOAT_SIDE, fl);
        int hfr = testWaterHeight(BOAT_FRONT,  BOAT_SIDE, fr);

        int16 room = getRoomIndex();
        vec3 v = vec3(float(x), float(y + BOAT_SHIFT_Y), float(z));
        TR::Room::Sector *sector = level->getSector(room, v);
        v.y = float(y);
        int height = int(level->getFloor(sector, v));
        water = getWaterHeight(room, v);

        bool noTurn = true;
        if (driver && driver->health > 0 && control)
            noTurn = userControl();
        else if (speed > BOAT_SLOWDOWN)
            speed -= BOAT_SLOWDOWN;
        else
            speed = 0;

        if (noTurn) {
            if (turnSpeed < -BOAT_UNDO_TURN)
                turnSpeed += BOAT_UNDO_TURN;
            else if (turnSpeed > BOAT_UNDO_TURN)
                turnSpeed -= BOAT_UNDO_TURN;
            else
                turnSpeed = 0;
        }

        water = (water == BOAT_NO_HEIGHT) ? height : (water + BOAT_SHIFT_Y);

        leftFall  = doDynamics(hfl, leftFall, fl.y);
        rightFall = doDynamics(hfr, rightFall, fr.y);
        fallSpeed = doDynamics(water, fallSpeed, y);

        height = (fr.y + fl.y) / 2;
        int xr = tatan(BOAT_FRONT, y - height);
        int zr = tatan(BOAT_SIDE, height - fl.y);
        rotX += (xr - rotX) / 2;
        rotZ += (zr - rotZ) / 2;
        if (xr == 0 && abs(rotX) < 4) rotX = 0;
        if (zr == 0 && abs(rotZ) < 4) rotZ = 0;

        roomIndex = room;
        rotZ += tilt;

        if (speed && water == y)
            doWake();

        if (driver && control) { // the engine sounds loop, only one of them at a time
            if (speed > 20) {
                Sound::stop(SND_TR2_BOAT_IDLE);
                game->playSound(SND_TR2_BOAT_MOVING, pos, Sound::PAN | Sound::UNIQUE);
            } else {
                Sound::stop(SND_TR2_BOAT_MOVING);
                game->playSound(SND_TR2_BOAT_IDLE, pos, Sound::PAN | Sound::UNIQUE);
            }
        } else
            stopSounds();
    }

    // three water sprites behind the boat: to the left, back and to the right
    void doWake() {
        int seqIndex = level->getModelIndex(TR::Entity::WAKE_BOAT);
        if (seqIndex >= 0)
            return;
        int frames = level->spriteSequences[-(seqIndex + 1)].sCount;
        int frame  = frames > 0 ? ((rand() & 0x7FFF) * frames) >> 15 : 0;
        int s = tsin(rotY), c = tcos(rotY);

        for (int i = 0; i < 3; i++) {
            int w = (1 - i) * BOAT_SIDE;
            int lx = -w, lz = -BOAT_WAKE_DIST;
            vec3 p = vec3(float(x + ((c * lx + s * lz) >> 14)), float(y), float(z + ((c * lz - s * lx) >> 14)));
            float a = int16(rotY + (i << 14) - 0x4000) * (PI / 32768.0f);

            BoatWake *wake = (BoatWake*)game->addEntity(TR::Entity::WAKE_BOAT, getRoomIndex(), p, a);
            if (!wake)
                continue;
            wake->frame = frame;
            wake->angle.y = a;
            wake->speed = speed >> 2;
            wake->fallSpeed = speed < 64 ? (((rand() & 0x7FFF) * (abs(speed) - 64)) >> 15) : 0;
        }
    }

    // Lara gets on, the boat runs from now on
    void mount(Character *lara) {
        driver  = lara;
        control = false;
        activate();
    }

    void stopSounds() {
        Sound::stop(SND_TR2_BOAT_IDLE);
        Sound::stop(SND_TR2_BOAT_MOVING);
    }

    void dismount() {
        driver  = NULL;
        control = false;
        stopSounds();
        animation.setAnim(0);
    }

    virtual void setSaveData(const SaveEntity &data) {
        Controller::setSaveData(data);
        x = int(pos.x);
        y = int(pos.y);
        z = int(pos.z);
        rotX = rotZ = 0;
        rotY = int16(int(angle.y * (32768.0f / PI)));
        speed = fallSpeed = turnSpeed = tilt = extraRot = 0;
        animation.setAnim(0);
        if (isDestroyed())
            flags.invisible = true;
    }

    bool isDestroyed() const {
        return (flags.unused & FLAG_DESTROYED) != 0;
    }

    virtual bool isCollider() {
        return !isDestroyed();
    }

    // blown up by a mine: the boat breaks into its bits (BOAT_BITS) and is gone
    void destroy() {
        if (isDestroyed())
            return;
        flags.unused |= FLAG_DESTROYED;
        stopSounds();
        speed = 0;

        int bits = level->getModelIndex(TR::Entity::BOAT_BITS);
        if (bits > 0) {
            level->entities[entity].modelIndex = bits;
            animation.setModel(getModel());
            initMeshOverrides();
            layers[0].model = bits - 1;
            layers[0].mask  = 0xFFFFFFFF;
        }
        explode(0xFFFFFFFF, 0.0f);
        activate();
    }

    // plays the animation Lara has on it
    void syncAnim(int index, int frame) {
        if (index >= 0 && index < ANIM_DEATH + 1)
            animation.setAnim(index, -frame);
    }

    virtual bool explodeFx() {
        return false;
    }

    virtual void update() {
        if (isDestroyed()) {
            updateExplosion();
            if (!explodeMask) {
                flags.invisible = true;
                deactivate(true);
            }
            return;
        }

        stepTime += Core::deltaTime;
        int steps = 0;
        while (stepTime >= 1.0f / 30.0f && steps < 4) {
            step();
            stepTime -= 1.0f / 30.0f;
            steps++;
        }
        if (steps == 4)
            stepTime = 0.0f;

        pos   = vec3(float(x), float(y), float(z));
        angle = vec3(int16(rotX) * (PI / 32768.0f), int16(rotY) * (PI / 32768.0f), int16(rotZ) * (PI / 32768.0f));
        updateLights();
    }
};


#define MINE_BOAT_DIST      512
#define SND_TR2_EXPLOSION_1 105

// the sounds of the TR2 effects (flipeffects)
#define SND_TR2_MASSIVE_CRASH   24
#define SND_TR2_FLOOD           79
#define SND_TR2_PULLEY_CRANE    190
#define SND_TR2_CURTAIN         191
#define SND_TR2_CHAIN_PULLEY    278
#define SND_TR2_STAGE_BACKDROP  330

// the mines in the Venice canal: a boat coming over one blows up with Lara on it, then all of them go off
struct TrapMine : Controller {
    enum { FLAG_SPENT = 1, FLAG_ALL = 2 };

    TrapMine(IGame *game, int entity) : Controller(game, entity) {}

    bool isSpent() const {
        return (flags.unused & FLAG_SPENT) != 0;
    }

    virtual bool isCollider() {
        return !isSpent();
    }

    SpeedBoat* getBoat() {
        for (int i = 0; i < level->entitiesBaseCount; i++) {
            const TR::Entity &e = level->entities[i];
            if (e.type != TR::Entity::VEHICLE_BOAT || !e.controller)
                continue;
            SpeedBoat *boat = (SpeedBoat*)e.controller;
            if (boat->isDestroyed())
                continue;
            float dx = boat->pos.x - pos.x;
            float dz = boat->pos.z - pos.z;
            if (dx * dx + dz * dz < MINE_BOAT_DIST * MINE_BOAT_DIST)
                return boat;
        }
        return NULL;
    }

    // the boat and Lara on it break apart, the trigger under the mine goes off and all the mines follow
    void detonateAll(SpeedBoat *boat) {
        Character *lara = boat->driver;
        if (lara) {
            lara->explode(0xFFFFFFFF, 0.0f);
            lara->visibleMask = 0; // only her flying pieces are left
            lara->hit(lara->health + 1.0f, this);
        }
        boat->destroy();

        game->checkTrigger(this, true);

        for (int i = 0; i < level->entitiesBaseCount; i++) {
            const TR::Entity &e = level->entities[i];
            if (e.type == TR::Entity::TRAP_MINE && e.controller) {
                TrapMine *mine = (TrapMine*)e.controller;
                mine->flags.unused |= FLAG_ALL;
                mine->activate();
            }
        }
    }

    void blow() {
        flags.unused |= FLAG_SPENT;
        game->addEntity(TR::Entity::EXPLOSION, getRoomIndex(), pos - vec3(0.0f, 1024.0f, 0.0f));
        game->addEntity(TR::Entity::WATER_SPLASH, getRoomIndex(), pos);
        game->playSound(SND_TR2_EXPLOSION_1, pos, Sound::PAN);
        initMeshOverrides();
        layers[0].mask = 1; // the mesh left after the blast
    }

    virtual void update() {
        Controller::update();

        if (isSpent())
            return;

        if (!(flags.unused & FLAG_ALL)) {
            SpeedBoat *boat = getBoat();
            if (!boat)
                return;
            detonateAll(boat);
        } else if ((rand() & 0x7FFF) < 0x7800) // the others go off one after another
            return;

        blow();
    }

    virtual void setSaveData(const SaveEntity &data) {
        Controller::setSaveData(data);
        if (isSpent()) {
            initMeshOverrides();
            layers[0].mask = 1;
        }
    }
};

#define SND_TR2_DOOR_CHIME      208
#define SND_TR2_BIRDS_CHIRP     316
#define SND_TR2_DRIPS_REVERB    329

// TR2 invisible sound sources: the birds singing now and then, the water dripping
struct SoundSource : Controller {
    float stepTime;

    SoundSource(IGame *game, int entity) : Controller(game, entity), stepTime(0.0f) {}

    virtual void update() {
        if (!isActive())
            return;

        stepTime += Core::deltaTime;
        while (stepTime >= 1.0f / 30.0f) { // the chances of the original per 30 fps frame
            stepTime -= 1.0f / 30.0f;
            int r = rand() & 0x7FFF;
            if (getEntity().type == TR::Entity::SOUND_BIRD) {
                if (r < 1024)
                    game->playSound(SND_TR2_BIRDS_CHIRP, pos, Sound::PAN);
            } else if (r < 256)
                game->playSound(SND_TR2_DRIPS_REVERB, pos, Sound::PAN);
        }
    }

    virtual void render(Frustum *frustum, MeshBuilder *mesh, Shader::Type type, bool caustics) {}
};

// the clock of Bartoli's Hideout: triggered with a timer it chimes every two seconds and once more at the end
struct ClockChimes : Controller {
    int   frames;
    float stepTime;

    ClockChimes(IGame *game, int entity) : Controller(game, entity), frames(0), stepTime(0.0f) {}

    virtual bool activate() {
        frames = int(timer * 30.0f);
        stepTime = 0.0f;
        return Controller::activate();
    }

    void chime() { // heard everywhere: near Lara, 1/64 of the way to the clock
        Controller *lara = game->getLara(pos);
        vec3 p = lara->pos + (pos - lara->pos) * (1.0f / 64.0f);
        game->playSound(SND_TR2_DOOR_CHIME, p, Sound::PAN);
    }

    virtual void update() {
        stepTime += Core::deltaTime;
        while (stepTime >= 1.0f / 30.0f) {
            stepTime -= 1.0f / 30.0f;
            if (frames <= 0) {
                deactivate(true);
                return;
            }
            if (frames % 60 == 59)
                chime();
            if (--frames == 0) {
                chime();
                flags.active = 0;
                deactivate(true);
                return;
            }
        }
    }

    virtual void render(Frustum *frustum, MeshBuilder *mesh, Shader::Type type, bool caustics) {}
};

#define DETONATOR_EXPLOSION_FRAME   80

// the detonator box of Bartoli's Hideout: Lara pushes its plunger with the detonator key, the blast goes off midway
struct DetonatorBox : Controller {
    DetonatorBox(IGame *game, int entity) : Controller(game, entity) {}

    void use(float angleY) {
        angle.y = angleY; // it turns to the way Lara faces
        activate();
    }

    virtual void update() {
        if (flags.state == TR::Entity::asNone)
            return;

        int frame = animation.frameIndex;
        if (frame >= animation.framesCount - 1)
            return; // stays pushed down

        updateAnimation(true);

        if (frame < DETONATOR_EXPLOSION_FRAME && animation.frameIndex >= DETONATOR_EXPLOSION_FRAME) {
            game->playSound(SND_TR2_EXPLOSION_1, pos, 0);
            game->shakeCamera(0.5f);
        }
    }
};


#define SND_TR2_GLASS_BREAK     58
#define WINDOW_SMASH_SPEED      50.0f

// TR2 smashable windows: the first mesh is the whole pane, the other ones are its pieces
// WINDOW_1 breaks when shot or when Lara runs into it, WINDOW_2 when triggered
struct BreakableWindow : Controller {
    enum { FLAG_SMASHED = 1 };

    BreakableWindow(IGame *game, int entity) : Controller(game, entity) {
        initMeshOverrides();
        layers[0].mask = 0x00000001;
    }

    bool isSmashed() const {
        return (flags.unused & FLAG_SMASHED) != 0;
    }

    virtual bool isCollider() {
        return !isSmashed();
    }

    virtual bool explodeFx() {
        return false;
    }

    void smash() {
        if (isSmashed())
            return;
        flags.unused |= FLAG_SMASHED;
        layers[0].mask = ~1;
        explode(~((1 << 0) | (1 << 8)), 0.0f); // the frame (mesh 8) stays
        game->playSound(SND_TR2_GLASS_BREAK, pos, Sound::PAN);
        activate(); // to animate the pieces
    }

    virtual void update() {
        if (getEntity().type == TR::Entity::WINDOW_2 && !isSmashed() && isActive())
            smash();
        Controller::update();
        if (isSmashed() && !explodeMask)
            deactivate(true);
    }

    virtual void setSaveData(const SaveEntity &data) {
        Controller::setSaveData(data);
        layers[0].mask = isSmashed() ? (1 << 8) : 1;
    }
};


#define HELICOPTER_SPEED 3000
#define HELICOPTER_RANGE (1024 * 30)

struct HelicopterFlying : Controller {

    HelicopterFlying(IGame *game, int entity) : Controller(game, entity) {}

    virtual void update() {
        pos.z += HELICOPTER_SPEED * Core::deltaTime;
        updateAnimation(false);
        updateRoom();

        Controller *lara = game->getLara(pos);

        float dist = pos.z - lara->pos.z;

        Sound::Sample *sample = game->playSound(TR::SND_HELICOPTER, vec3(0.0), 0);
        if (sample) {
            sample->volume = (1.0f - dist / HELICOPTER_RANGE) * 0.8f;
        }

        if (fabsf(dist) > HELICOPTER_RANGE) {
            Sound::stop(TR::SND_HELICOPTER);
            flags.invisible = true;
            deactivate(true);
        }
    }
};


// TR2 flares
#define FLARE_MAX_AGE           60.0f   // burn time in seconds
#define FLARE_LIGHT_RADIUS      4096.0f
#define SND_TR2_FLARE_IGNITE    11
#define SND_TR2_FLARE_BURN      12

// brightness of the burning flare of "age" seconds, it flares up first and flickers before going out
inline float flareBrightness(float age) {
    if (age < 0.0f || age >= FLARE_MAX_AGE)
        return 0.0f;
    if (age < 1.0f)
        return 0.5f + age * 0.5f;
    if (age > FLARE_MAX_AGE - 2.0f)
        return randf() < 0.75f ? 1.0f - randf() * 0.1f : 0.5f;
    return 1.0f;
}

inline vec4 flareLightColor(float brightness) {
    return vec4(vec3(1.4f, 0.75f, 0.6f) * brightness, 1.0f / FLARE_LIGHT_RADIUS);
}

#define FLARE_LIGHT_ROOMS 8

// the flare light goes to its room and the rooms seen through its portals (a shaft above, the next corridor)
struct FlareLight {
    int rooms[FLARE_LIGHT_ROOMS];
    int count;

    FlareLight() : count(0) {}

    bool has(int room) const {
        for (int i = 0; i < count; i++)
            if (rooms[i] == room)
                return true;
        return false;
    }

    void set(TR::Level *level, int id, int room, const vec3 &pos, const vec4 &color, bool priority = false) {
        FlareLight next;
        next.rooms[next.count++] = room;

        const TR::Room &r = level->rooms[room];
        for (int i = 0; i < r.portalsCount && next.count < FLARE_LIGHT_ROOMS; i++) {
            int index = r.portals[i].roomIndex;
            if (!next.has(index))
                next.rooms[next.count++] = index;
        }

        for (int i = 0; i < count; i++)
            if (!next.has(rooms[i]))
                level->rooms[rooms[i]].removeDynLight(id);

        for (int i = 0; i < next.count; i++)
            level->rooms[next.rooms[i]].addDynLight(id, vec4(pos, 0.0f), color, priority);

        *this = next;
    }

    void off(TR::Level *level, int id) {
        for (int i = 0; i < count; i++)
            level->rooms[rooms[i]].removeDynLight(id);
        count = 0;
    }
};

// the burning end of a flare held by Lara or thrown
struct FlareFlame : Controller {
    Controller *owner;
    int        joint;
    vec3       offset;

    FlareFlame(IGame *game, int entity) : Controller(game, entity), owner(NULL), joint(0), offset(0.0f) {
        activate();
    }

    virtual bool getSaveData(SaveEntity &data) {
        return false;
    }

    virtual void update() {
        if (!owner) {
            game->removeEntity(this);
            return;
        }
        pos = owner->getJoint(joint) * offset;
        updateRoom();
    }

    virtual void render(Frustum *frustum, MeshBuilder *mesh, Shader::Type type, bool caustics) {
        if (!owner) return;

        Basis b = owner->getJoint(joint);
        b.w = 1.0f;
        b.translate(offset);
        b.rotate(quat(vec3(1, 0, 0), -PI * 0.5f));
        b.rotate(quat(vec3(0, 1, 0), randf() * PI * 2.0f));

        float alpha = 0.8f + randf() * 0.2f;
        game->setShader(Core::pass, Shader::FLASH, false, false);
        Core::setMaterial(alpha, 0.0f, 0.0f, alpha);
        Core::setBasis(&b, 1);
        Core::setDepthWrite(false);
        mesh->renderModel(getEntity().modelIndex - 1);
        Core::setDepthWrite(true);
    }
};

inline FlareFlame* flareFlameAdd(IGame *game, Controller *owner, int joint, const vec3 &offset) {
    FlareFlame *flame = (FlareFlame*)game->addEntity(TR::Entity::FLARE_SPARKLES, owner->getRoomIndex(), owner->pos, 0.0f);
    if (flame) {
        flame->owner  = owner;
        flame->joint  = joint;
        flame->offset = offset;
    }
    return flame;
}

inline void flareFlameRemove(IGame *game, FlareFlame *&flame) {
    if (flame)
        game->removeEntity(flame);
    flame = NULL;
}

struct FlareItem : Controller {
    float      age;
    float      speed;     // units per 1/30 s
    float      fallSpeed;
    FlareLight light;
    FlareFlame *flame;

    FlareItem(IGame *game, int entity) : Controller(game, entity), age(FLARE_MAX_AGE), speed(0.0f), fallSpeed(0.0f), flame(NULL) {
        activate();
    }

    virtual ~FlareItem() {
        light.off(level, entity);
        if (flame)
            flame->owner = NULL; // removes itself
    }

    virtual bool getSaveData(SaveEntity &data) {
        return false;
    }

    void launch(float yaw, float speed, float fallSpeed, float age) {
        angle = vec3(0.0f, yaw, 0.0f);
        this->speed     = speed;
        this->fallSpeed = fallSpeed;
        this->age       = age;

        if (flareBrightness(age) > 0.0f) {
            Box box = getBoundingBoxLocal();
            flame = flareFlameAdd(game, this, 0, box.min - box.max);
        }
    }

    void lightOff() {
        light.off(level, entity);
        flareFlameRemove(game, flame);
    }

    virtual void update() {
        float k = Core::deltaTime * 30.0f;

        if (fallSpeed != 0.0f) {
            angle.x += 3.0f * DEG2RAD * k;
            angle.z += 5.0f * DEG2RAD * k;
        } else
            angle.x = angle.z = 0.0f;

        vec3 opos = pos;
        pos.x += sinf(angle.y) * speed * k;
        pos.z += cosf(angle.y) * speed * k;

        if (getRoom().flags.water) {
            float t = min(1.0f, 0.5f * k);
            fallSpeed += (5.0f - fallSpeed) * t;
            speed     += (5.0f - speed)     * t;
        } else
            fallSpeed += GRAVITY * k;

        pos.y += fallSpeed * k;

        int16 room = roomIndex;
        TR::Room::Sector *sector = level->getSector(room, pos);
        float floor = level->getFloor(sector, pos);

        if (floor < opos.y - 128.0f) { // hit the wall, bounce back
            pos.x = opos.x;
            pos.z = opos.z;
            angle.y += PI;
            speed *= 0.5f;
            room   = roomIndex;
            sector = level->getSector(room, pos);
            floor  = level->getFloor(sector, pos);
        }

        float ceiling = level->getCeiling(sector, pos);

        if (pos.y >= floor) {
            pos.y = floor;
            if (fallSpeed > 30.0f) { // bounce off the floor
                fallSpeed = -fallSpeed * 0.25f;
                speed *= 0.5f;
            } else {
                fallSpeed = 0.0f;
                speed = max(0.0f, speed - 2.0f * k);
            }
        } else if (pos.y <= ceiling) {
            pos.y = ceiling + 1.0f;
            if (fallSpeed < 0.0f)
                fallSpeed = 0.0f;
        }

        updateRoom();

        age += Core::deltaTime;

        float brightness = flareBrightness(age);
        if (brightness <= 0.0f) {
            lightOff();
            if (fallSpeed == 0.0f && speed == 0.0f) {
                game->removeEntity(this);
                return;
            }
        } else {
            // raise the light a bit, the lighting depends on the surface angle and the flare lies on the floor
            light.set(level, entity, getRoomIndex(), pos - vec3(0.0f, 256.0f, 0.0f), flareLightColor(brightness));
        }
    }
};

// TR2 grenade launcher and harpoon gun projectiles (speeds are in units per 1/30 s)
#define GRENADE_SPEED       200.0f
#define GRENADE_FALL_SPEED  190.0f // starts to fall below this speed
#define GRENADE_RADIUS      512.0f
#define HARPOON_SPEED       150.0f

struct Projectile : Controller {
    Controller *owner;
    float      speed;
    float      fallSpeed;
    float      damage;

    Projectile(IGame *game, int entity) : Controller(game, entity), owner(NULL), speed(0.0f), fallSpeed(0.0f), damage(0.0f) {
        activate();
    }

    virtual bool getSaveData(SaveEntity &data) {
        return false;
    }

    bool isGrenade() {
        return getEntity().type == TR::Entity::GRENADE_BULLET;
    }

    void launch(Controller *owner, const vec3 &dir, float damage) {
        this->owner  = owner;
        this->damage = damage;
        angle = vec3(-asinf(clamp(dir.y, -1.0f, 1.0f)), atan2f(dir.x, dir.z), 0.0f);
        if (isGrenade()) {
            speed     = GRENADE_SPEED;
            fallSpeed = 0.0f;
        } else {
            speed     = HARPOON_SPEED * cosf(angle.x);
            fallSpeed = HARPOON_SPEED * -sinf(angle.x);
            angle.x   = 0.0f;
        }
    }

    // the enemy on the way from "from" to "to" (expanded by radius)
    Character* getTarget(const vec3 &from, const vec3 &to, float radius) {
        Controller *c = Controller::first;
        while (c) {
            if (c != owner && c->getEntity().isEnemy() && ((Character*)c)->isActiveTarget()) {
                Box box = c->getBoundingBoxLocal();
                box.expand(vec3(radius));
                float t;
                if (box.intersect(c->getMatrix(), from, to - from, t) && t <= 1.0f)
                    return (Character*)c;
            }
            c = c->next;
        }
        return NULL;
    }

    void explode(const vec3 &p) {
        Controller *c = Controller::first;
        while (c) {
            if (c != owner && c->getEntity().isEnemy() && ((Character*)c)->isActiveTarget()) {
                if (c->getBoundingBox().intersect(Sphere(p, GRENADE_RADIUS)))
                    ((Character*)c)->hit(damage, owner);
            }
            c = c->next;
        }

        game->addEntity(TR::Entity::EXPLOSION, getRoomIndex(), p);
        game->removeEntity(this);
    }

    virtual void update() {
        float k = Core::deltaTime * 30.0f;
        bool underwater = getRoom().flags.water;

        if (isGrenade()) {
            speed = max(0.0f, speed - k);
            if (speed < GRENADE_FALL_SPEED)
                fallSpeed += k;
        } else {
            if (!underwater)
                fallSpeed += GRAVITY * 0.5f * k;
            angle.z += 35.0f * DEG2RAD * k; // spin
        }

        vec3 opos = pos;
        pos += getDir() * (speed * k);
        pos.y += fallSpeed * k;

        TR::Room::Sector *sector = level->getSector(roomIndex, pos);
        float floor   = level->getFloor(sector, pos);
        float ceiling = level->getCeiling(sector, pos);

        Character *target = getTarget(opos, pos, 0.0f);

        if (isGrenade()) {
            if (target || pos.y >= floor || pos.y <= ceiling)
                explode(opos);
            return;
        }

        if (target) {
            target->hit(damage, owner);
            game->addEntity(TR::Entity::BLOOD, getRoomIndex(), pos);
            game->removeEntity(this);
            return;
        }

        if (pos.y >= floor || pos.y <= ceiling) {
            game->removeEntity(this);
            return;
        }

        if (underwater && randf() < 0.2f)
            game->addEntity(TR::Entity::BUBBLE, getRoomIndex(), pos);
    }
};

#endif
