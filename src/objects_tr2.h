#ifndef H_OBJECTS_TR2
#define H_OBJECTS_TR2

// the objects of Tomb Raider 2 (and the later games reusing them)

#include "objects.h"

struct BreakableWindow : Controller {

    BreakableWindow(IGame *game, int entity) : Controller(game, entity) {
        initMeshOverrides();
        layers[0].mask = 0x00000001;
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

#endif
