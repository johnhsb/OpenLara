#ifndef H_OBJECTS_TR3
#define H_OBJECTS_TR3

// the objects of Tomb Raider 3

#include "objects.h"

// the small fire of a torch on a wall: a still flame with licks of fire rising from it, it lights the room and does
// no harm. The sprites are the four puffs the level has for all its fire and smoke, the darker a lick the later one
#define TORCH_FORWARD       288.0f
#define TORCH_HEIGHT        192.0f
#define TORCH_LICK_HEIGHT   224.0f
#define TORCH_LICKS         16
#define TORCH_LIGHT_RADIUS  3072.0f

struct TorchFlame : Controller {

    struct Lick {
        vec3  pos, prev, vel;
        vec3  colorA, colorB;
        float gravity, maxVel, size;
        int   life, lifeMax, fadeIn;
    } licks[TORCH_LICKS];

    vec3        origin;
    float       tick;
    int         frame;
    float       coreSize;
    vec3        coreColor;
    EffectLight light;

    TorchFlame(IGame *game, int entity) : Controller(game, entity), tick(0.0f), frame(rand() & 31), coreSize(0.0f) {
        memset(licks, 0, sizeof(licks));
        origin = pos + vec3(sinf(angle.y), 0.0f, cosf(angle.y)) * TORCH_FORWARD;
    }

    virtual ~TorchFlame() {
        light.off(level, entity);
    }

    void addLick() {
        for (int i = 0; i < TORCH_LICKS; i++) {
            Lick &l = licks[i];
            if (l.life > 0)
                continue;
            l.pos     = origin + vec3(float(rand() & 31) - 16.0f, -TORCH_LICK_HEIGHT, float(rand() & 31) - 16.0f);
            l.prev    = l.pos;
            l.vel     = vec3(float((rand() & 255) - 128), -float(16 + (rand() & 15)), float((rand() & 255) - 128)) * (1.0f / 32.0f);
            l.colorA  = vec3(255.0f, float(48 + (rand() & 31)), 48.0f);
            l.colorB  = vec3(float(192 + (rand() & 63)), float(128 + (rand() & 63)), 32.0f);
            l.gravity = -float(16 + (rand() & 31)) / 32.0f;
            l.maxVel  = -float(16 + (rand() & 7));
            l.size    = float(64 + (rand() & 31)) * 2.0f;
            l.fadeIn  = 20 + (rand() & 3);
            l.life    = l.lifeMax = 40 + (rand() & 7);
            return;
        }
    }

    virtual void update() {
        if (!isActive()) {
            if (coreSize > 0.0f) {
                coreSize = 0.0f;
                memset(licks, 0, sizeof(licks));
                light.off(level, entity);
                Sound::stop(TR::SND_FLAME);
            }
            return;
        }

        tick += Core::deltaTime * 30.0f;
        while (tick >= 1.0f) {
            tick -= 1.0f;
            frame++;

            coreSize  = float(32 + (rand() & 15)) * 2.0f;
            coreColor = vec3(float(192 + (rand() & 63)), float(128 + (rand() & 63)), 64.0f);

            if (!(frame & 6)) // two frames of eight
                addLick();

            for (int i = 0; i < TORCH_LICKS; i++) {
                Lick &l = licks[i];
                if (l.life <= 0)
                    continue;
                l.life--;
                l.prev   = l.pos;
                l.vel.y  = max(l.vel.y + l.gravity, l.maxVel);
                l.vel.x -= l.vel.x / 8.0f;
                l.vel.z -= l.vel.z / 8.0f;
                l.vel.y -= l.vel.y / 8.0f;
                l.pos   += l.vel;
            }
        }

        game->playSound(TR::SND_FLAME, origin, Sound::PAN);
        light.set(level, entity, getRoomIndex(), origin - vec3(0.0f, TORCH_HEIGHT, 0.0f), FIRE_LIGHT_COLOR * (0.8f + randf() * 0.2f), TORCH_LIGHT_RADIUS);
    }

    void addPuff(MeshBuilder *mesh, const vec3 &p, float size, const vec3 &c) {
        const TR::SpriteSequence &seq = level->spriteSequences[-(getEntity().modelIndex + 1)];
        float m = max(c.x, max(c.y, c.z));
        int   f = m < 16.0f ? 3 : (m < 64.0f ? 2 : (m < 96.0f ? 1 : 0)); // the darker the later puff
        vec3  v = p - Core::viewPos.xyz();
        Color32 color(uint8(clamp(c.x * 0.5f, 0.0f, 255.0f)), uint8(clamp(c.y * 0.5f, 0.0f, 255.0f)), uint8(clamp(c.z * 0.5f, 0.0f, 255.0f)), 255);
        mesh->addDynSpriteSized(seq.sStart + min(f, int(seq.sCount) - 1), short3(int16(v.x), int16(v.y), int16(v.z)), int16(max(1.0f, size)), color);
    }

    virtual void render(Frustum *frustum, MeshBuilder *mesh, Shader::Type type, bool caustics) {
        if (coreSize <= 0.0f || getEntity().modelIndex >= 0)
            return;

        addPuff(mesh, origin - vec3(0.0f, TORCH_HEIGHT, 0.0f), coreSize, coreColor);

        for (int i = 0; i < TORCH_LICKS; i++) {
            Lick &l = licks[i];
            if (l.life <= 0)
                continue;
            float lived = float(l.lifeMax - l.life) + tick;
            float left  = float(l.life) - tick;
            vec3  c     = l.colorA.lerp(l.colorB, min(1.0f, lived / float(l.fadeIn)));
            if (left < 8.0f)
                c = c * max(0.0f, left / 8.0f);
            float t = min(1.0f, lived / float(l.lifeMax));
            addPuff(mesh, l.prev.lerp(l.pos, tick), l.size * (1.0f - t * (15.0f / 16.0f)), c);
        }
    }
};

// a shoal of piranhas: two dozen fish after a leader that wanders through the range of the shoal. With Lara in the
// water room of the shoal the leader makes for her, and every fish that gets to her bites
#define SHOAL_FISH          24
#define SHOAL_BITE_RANGE    256.0f
#define SHOAL_BITE_DAMAGE   4.0f
#define SHOAL_BLOOD_WAIT    8
#define SHOAL_SPRITE        20      // the pieces of the sprites of the level the fish wear

struct PiranhaShoal : Controller {

    struct Fish {
        vec3 pos, prev;
        int  angle, anglePrev;  // 0..4095
        int  angAdd;
        int  speed;
        int  swim;
        float destY;
    } fish[SHOAL_FISH + 1];     // the leader comes first

    vec3  anchor;
    int   anchorRoom;
    vec3  range;
    bool  on;
    int   leaderAngle, leaderSpeed, angleTime, speedTime;
    int   hitWait;
    float tick;

    PiranhaShoal(IGame *game, int entity) : Controller(game, entity), on(false), hitWait(0), tick(0.0f) {
        anchor     = pos;
        anchorRoom = getRoomIndex();
        // the range comes with the level in the original (a quarter of a sector each)
        range = vec3(1.0f, 1.0f, 1.0f);
        if (level->id == TR::LVL_TR3_JUNGLE)
            range = vec3(10.0f, 3.0f, 22.0f);
        range = range * 256.0f;
    }

    static int rnd(int mask) {
        return rand() & mask;
    }

    static int angleTo(float dx, float dz) {
        return int(atan2f(dx, dz) * (4096.0f / (PI * 2.0f))) & 0xFFF;
    }

    static int angleDiff(int a, int b) {
        int d = a - b;
        if (d > 2048)  d -= 4096;
        if (d < -2048) d += 4096;
        return d;
    }

    static vec3 heading(int angle, float len) {
        float a = float(angle) * (PI * 2.0f / 4096.0f);
        return vec3(sinf(a) * len, 0.0f, cosf(a) * len);
    }

    void setup() {
        Fish &l = fish[0];
        l.pos   = l.prev = vec3(0.0f);
        l.angle = l.anglePrev = l.angAdd = 0;
        l.speed = rnd(0x3F) + 8;
        l.swim  = rnd(0x3F);
        for (int i = 1; i <= SHOAL_FISH; i++) {
            Fish &f = fish[i];
            f.pos    = vec3((randf() * 2.0f - 1.0f) * range.x, randf() * range.y, (randf() * 2.0f - 1.0f) * range.z);
            f.prev   = f.pos;
            f.destY  = randf() * range.y;
            f.angle  = f.anglePrev = rnd(0xFFF);
            f.angAdd = 0;
            f.speed  = rnd(0x1F) + 32;
            f.swim   = rnd(0x3F);
        }
        on = true;
        leaderAngle = 0;
        leaderSpeed = rnd(0x7F) + 32;
        angleTime = speedTime = 0;
        hitWait = 0;
    }

    void steer(Fish &f, int desired, int limit) {
        int diff = angleDiff(f.angle, desired);
        if (diff > 128)
            f.angAdd = max(f.angAdd - 4, -limit);
        else if (diff < -128)
            f.angAdd = min(f.angAdd + 4, limit);
        else {
            f.angAdd -= f.angAdd / 4;
            if (abs(f.angAdd) < 4)
                f.angAdd = 0;
        }
        f.angle = (f.angle + f.angAdd) & 0xFFF;
        if (diff > 1024)
            f.angle = (f.angle + f.angAdd / 4) & 0xFFF;
    }

    void turnBack() { // at the bounds of the range the leader turns away for a while
        leaderAngle = (angleTo(-fish[0].pos.x, -fish[0].pos.z) + rnd(0xFF) - 128) & 0xFFF;
        angleTime   = rnd(0xF) + 8;
        speedTime   = 0;
    }

    void step() {
        Character *lara = (Character*)game->getLara(anchor);
        bool attack = lara && lara->health > 0.0f && lara->getRoomIndex() == anchorRoom;

        if (hitWait)
            hitWait--;

        Fish &l = fish[0];
        for (int i = 0; i <= SHOAL_FISH; i++) {
            fish[i].prev      = fish[i].pos;
            fish[i].anglePrev = fish[i].angle;
        }

        if (attack) {
            l.angle     = angleTo(lara->pos.x - (anchor.x + l.pos.x), lara->pos.z - (anchor.z + l.pos.z));
            leaderAngle = l.angle;
            leaderSpeed = 192 + rnd(0x3F);
        }

        steer(l, leaderAngle, 120);

        int diff = l.speed - leaderSpeed;
        if (diff < -4)
            l.speed = min(255, l.speed + rnd(3) + 1);
        else if (diff > 4)
            l.speed = max(0, l.speed - rnd(3) - 1);

        l.swim = (l.swim + (l.speed >> 4)) & 0x3F;
        l.pos += heading(l.angle, float(l.speed) * 0.5f);

        if (!attack) {
            if (fabsf(l.pos.x) > range.x || fabsf(l.pos.z) > range.z) {
                l.pos.x = clamp(l.pos.x, -range.x, range.x);
                l.pos.z = clamp(l.pos.z, -range.z, range.z);
                turnBack();
            }

            if (!rnd(0xF))
                angleTime = 0;

            if (angleTime)
                angleTime--;
            else {
                angleTime = rnd(0xF) + 8;
                int delta = rnd(0x3F) - 24;
                if (!rnd(3))
                    delta *= 32;
                leaderAngle = (leaderAngle + delta) & 0xFFF;
            }

            if (speedTime)
                speedTime--;
            else {
                speedTime = rnd(0x1F) + 32;
                if (!rnd(7))
                    leaderSpeed = rnd(0x7F) + 128;
                else if (!rnd(3))
                    leaderSpeed += rnd(0x7F) + 32;
                else if (leaderSpeed > 140)
                    leaderSpeed += 208 - rnd(0x1F);
                else {
                    speedTime = rnd(3) + 4;
                    leaderSpeed += rnd(0x1F) - 15;
                }
                leaderSpeed &= 0xFF;
            }
        }

        for (int i = 0; i < SHOAL_FISH; i++) {
            Fish &f = fish[i + 1];

            if (attack) {
                vec3  p = anchor + f.pos;
                float dx = p.x - lara->pos.x, dz = p.z - lara->pos.z, dy = fabsf(p.y - lara->pos.y);
                if (dx * dx + dz * dz <= SQR(SHOAL_BITE_RANGE) && dy <= SHOAL_BITE_RANGE) {
                    if (!hitWait) {
                        game->addEntity(TR::Entity::BLOOD, lara->getRoomIndex(), p);
                        hitWait = SHOAL_BLOOD_WAIT;
                    }
                    lara->hit(SHOAL_BITE_DAMAGE, this);
                }
            }

            float dx = f.pos.x - l.pos.x - 128.0f * i + 3072.0f;
            float dz = f.pos.z - l.pos.z + 128.0f * i - 3072.0f;

            steer(f, angleTo(l.pos.x - f.pos.x, l.pos.z - f.pos.z), (i >> 1) + 92);

            if (dx * dx + dz * dz < 16384.0f * float(i * i) + SQR(1024.0f)) {
                if (f.speed > 2 * i + 32)
                    f.speed -= f.speed >> 5;
            } else {
                if (f.speed < (i >> 1) + 160)
                    f.speed += (i >> 1) + rnd(3) + 1;
                if (f.speed > (i >> 1) - 4 * i + 160)
                    f.speed = ((i >> 1) - 4 * i - 96) & 0xFF;
            }
            f.speed += rnd(1) ? -rnd(1) : rnd(1);
            f.speed  = clamp(f.speed, 32, 200);
            f.swim   = (f.swim + (f.speed >> 4) + (f.speed >> 5)) & 0x3F;

            f.pos += heading(f.angle, float(f.speed) * 0.5f);
            f.pos.x = clamp(f.pos.x, -32000.0f, 32000.0f);
            f.pos.z = clamp(f.pos.z, -32000.0f, 32000.0f);

            if (fabsf(f.pos.y - f.destY) < 16.0f)
                f.destY = attack ? (lara->pos.y - anchor.y + float(rnd(0xFF))) : (randf() * range.y);
            f.pos.y += (f.destY - f.pos.y) / 16.0f;
        }
    }

    virtual void update() {
        if (!isActive()) {
            on = false;
            return;
        }

        if (!on)
            setup();

        tick += Core::deltaTime * 30.0f;
        while (tick >= 1.0f) {
            tick -= 1.0f;
            step();
        }
    }

    virtual void render(Frustum *frustum, MeshBuilder *mesh, Shader::Type type, bool caustics) {
        if (!on || getEntity().modelIndex >= 0)
            return;

        const TR::SpriteSequence &seq = level->spriteSequences[-(getEntity().modelIndex + 1)];
        if (seq.sCount <= SHOAL_SPRITE + 1)
            return;

        vec3 view = Core::viewPos.xyz();

        for (int i = 0; i < SHOAL_FISH; i++) {
            const Fish &f = fish[i + 1];
            vec3 p = anchor + f.prev.lerp(f.pos, tick) - view;
            if (fabsf(p.x) > 30000.0f || fabsf(p.y) > 30000.0f || fabsf(p.z) > 30000.0f)
                continue;

            int   wibble = int(sinf(float(f.swim) * (PI * 2.0f / 64.0f)) * 128.0f);
            int   ang    = (f.anglePrev + int(float(angleDiff(f.angle, f.anglePrev)) * tick) + wibble) & 0xFFF;
            float size   = 192.0f + 128.0f * sinf(float(i) * (PI * 2.0f / 64.0f));
            vec3  back   = p - heading(ang, size);

            // it glints as it turns
            int shade = ang & 1023;
            shade = (shade < 256) ? (shade >> 2) : (shade < 512 ? ((512 - shade) >> 2) : 0);
            shade = min(shade + i, 128) + 80;
            uint8 c = uint8(shade >> 1);

            mesh->addDynSpriteTri(seq.sStart + SHOAL_SPRITE + (i & 1),
                                  short3(int16(p.x), int16(p.y), int16(p.z)),
                                  short3(int16(back.x), int16(back.y - size), int16(back.z)),
                                  short3(int16(back.x), int16(back.y + size), int16(back.z)),
                                  Color32(c, c, c, 255));
        }
    }
};

#endif
