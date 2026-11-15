#ifndef H_SPRITE
#define H_SPRITE

#include "controller.h"

struct Sprite : Controller {

    enum {
        FRAME_ANIMATED = -1,
        FRAME_RANDOM   = -2,
    };

    bool  instant;
    int   frame, flag;
    float time;
    vec3  velocity;

    Sprite(IGame *game, int entity, bool instant = true, int frame = FRAME_ANIMATED) : Controller(game, entity), instant(instant), flag(frame), time(0), velocity(0) {
        if (frame >= 0) { // specific frame
            this->frame = frame;
        } else if (frame == FRAME_RANDOM) { // random frame
            this->frame = rand() % getSequence().sCount;
        } else if (frame == FRAME_ANIMATED) { // animated
            this->frame = 0;
        }
    }

    TR::SpriteSequence& getSequence() {
        return level->spriteSequences[-(getEntity().modelIndex + 1)];
    }

    virtual void update() {
        if (flag >= 0) return;

        bool remove = false;
        time += Core::deltaTime;

        if (flag == FRAME_ANIMATED) {
            frame = int(time * SPRITE_FPS);
            TR::SpriteSequence &seq = getSequence();
            if (instant && frame >= seq.sCount)
                remove = true;
            else
                frame %= max(seq.sCount, int16(1));
        } else
            if (instant && time >= (1.0f / SPRITE_FPS))
                remove = true;

        pos += velocity * (30.0f * Core::deltaTime);

        if (remove)
            game->removeEntity(this);
    }

    virtual void render(Frustum *frustum, MeshBuilder *mesh, Shader::Type type, bool caustics) {
        renderSprite(frame);
    }
};

// blood of TR3 is a spray of small flat particles, the level has no sprite for it
#define BLOOD_SPRAY_MAX   13
#define BLOOD_SPRAY_LIFE  24
#define BLOOD_SPRAY_FADE  8
#define BLOOD_SPRAY_SIZE  8.0f

struct BloodSpray : Sprite {

    struct Particle {
        vec3  pos, prev, vel;
        float gravity, sizeEnd;
    } particles[BLOOD_SPRAY_MAX];

    int   count;
    int   life;
    float tick;

    BloodSpray(IGame *game, int entity) : Sprite(game, entity, true, FRAME_ANIMATED), count(0), life(BLOOD_SPRAY_LIFE), tick(0.0f) {}

    void init() {
        float angle = (velocity.x != 0.0f || velocity.z != 0.0f) ? atan2f(velocity.x, velocity.z) : (randf() * PI * 2.0f);

        count = 6 + (rand() & 7);
        for (int i = 0; i < count; i++) {
            Particle &p = particles[i];
            float a = angle + (randf() - 0.5f) * (PI * 2.0f * 32.0f / 4096.0f);
            float d = float(rand() & 15) * 4.0f;
            p.pos     = pos + vec3(float(rand() & 31) - 16.0f, float(rand() & 31) - 16.0f, float(rand() & 31) - 16.0f);
            p.prev    = p.pos;
            p.vel     = vec3(sinf(a) * d, -float(128 + (rand() & 255)) / 32.0f, cosf(a) * d);
            p.gravity = float(31 + (rand() & 31)) / 32.0f;
            p.sizeEnd = (rand() & 1) ? BLOOD_SPRAY_SIZE : (BLOOD_SPRAY_SIZE * 0.5f);
        }
    }

    virtual void update() {
        if (!count)
            init();

        tick += Core::deltaTime * 30.0f;
        while (tick >= 1.0f) {
            tick -= 1.0f;

            if (--life <= 0) {
                game->removeEntity(this);
                return;
            }

            for (int i = 0; i < count; i++) {
                Particle &p = particles[i];
                p.prev   = p.pos;
                p.vel.y += p.gravity;
                p.vel.x -= p.vel.x / 16.0f;
                p.vel.z -= p.vel.z / 16.0f;
                p.pos   += p.vel;
            }
        }
    }

    virtual void render(Frustum *frustum, MeshBuilder *mesh, Shader::Type type, bool caustics) {
        if (!count) return;

        float lived = float(BLOOD_SPRAY_LIFE - life) + tick;
        float left  = float(life) - tick;

        // 224, 0, 32 at the start, 192, 0, 24 later, black at the end (the sprite shader doubles the color)
        float k = min(1.0f, lived / float(BLOOD_SPRAY_FADE));
        vec3  c = vec3(112.0f, 0.0f, 16.0f).lerp(vec3(96.0f, 0.0f, 12.0f), k);
        if (left < float(BLOOD_SPRAY_FADE))
            c = c * max(0.0f, left / float(BLOOD_SPRAY_FADE));
        Color32 color(uint8(c.x), uint8(c.y), uint8(c.z), 255);

        float t = min(1.0f, lived / float(BLOOD_SPRAY_LIFE));

        for (int i = 0; i < count; i++) {
            Particle &p = particles[i];
            vec3  v = p.prev.lerp(p.pos, tick) - Core::viewPos.xyz();
            float s = BLOOD_SPRAY_SIZE + (p.sizeEnd - BLOOD_SPRAY_SIZE) * t;
            mesh->addDynFlat(short3(int16(v.x), int16(v.y), int16(v.z)), int16(max(1.0f, s)), color);
        }
    }
};

#endif
