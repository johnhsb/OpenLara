#ifndef H_LEVEL
#define H_LEVEL

#include "core.h"
#include "utils.h"
#include "format.h"
#include "cache.h"
#include "enemy.h"
#include "camera.h"
#include "lara.h"
#include "objects.h"
#include "inventory.h"
#include "savegame.h"
#include "network.h"
#include "extension.h"

#ifdef _WIN32
    #include <direct.h> // _mkdir
#endif

#if defined(_DEBUG) && defined(_GAPI_GL) && !defined(_GAPI_GLES)
    #define DEBUG_RENDER
#endif

#ifdef DEBUG_RENDER
    #include "debug.h"
#endif

#define ANIM_TEX_TIMESTEP (10.0f / 30.0f)
#define SKY_TIME_PERIOD   (1.0f / 0.005f)

extern void loadLevelAsync(Stream *stream, void *userData);

extern Array<SaveSlot> saveSlots;
extern SaveResult saveResult;
extern int loadSlot;

struct Level : IGame {

    TR::Level   level;
    Texture     *atlasRooms;
    Texture     *atlasObjects;
    Texture     *normalRooms;   // the normal maps of the atlases
    Texture     *normalObjects;
    Texture     *atlasSprites;
    Texture     *atlasGlyphs;
    MeshBuilder *mesh;

    Lara        *players[2], *player;
    Camera      *camera;
    Texture     *shadow[2];
    Texture     *scaleTex;

    struct Params {
        float   time;
        float   waterHeight;
        float   reserved[2];
    } *params;

    ZoneCache    *zoneCache;
    AmbientCache *ambientCache;
    WaterCache   *waterCache;

    Sound::Sample *sndTrack, *sndWater;
    bool waitTrack;

    bool lastTitle;
    bool isEnded;
    bool bodiesFall; // a flip may have taken the floor away from under the dead
    bool needRedrawTitleBG;
    bool needRedrawReflections;
    bool needRenderGame;
    bool needRenderInventory;
    bool showStats;
    bool skyIsVisible;
    bool paused;

    TR::LevelID nextLevel;

    TR::Effect::Type effect;
    float      effectTimer;
    int        effectIdx;
    float      cutsceneWaitTimer;
    float      animTexTimer;
    float      statsTimeDelta;

    vec3 underwaterColor;
    vec4 underwaterFogParams;
    vec4 levelFogParams;

    mat4 mLightProj[2];

// IGame implementation ========
    virtual void loadLevel(TR::LevelID id) {
        sndWater = sndTrack = NULL;
        Sound::stopAll();
        nextLevel = id;
    }

    virtual void loadNextLevel() {
        if (nextLevel != TR::LVL_MAX) return;

        TR::LevelID id = TR::LVL_MAX;
    //#ifdef _OS_WEB
    //    if (level.id == TR::LVL_TR1_2 && level.version != TR::VER_TR1_PC)
    //        id = TR::LVL_TR1_TITLE;
    //    else
    //#endif
        id = (level.isEnd() || level.isHome()) ? level.getTitleId() : TR::LevelID(level.id + 1);

        TR::isGameEnded = level.isEnd();

        if (!level.isTitle() && loadSlot == -1) {
        // update statistics info for current level
            if (!TR::isCutsceneLevel(level.id) && !level.isHome())
                saveGame(level.id, false, true);
        // save next level
            if (!TR::isCutsceneLevel(id) && !TR::isTitleLevel(id)) {
                saveGame(id, false, false);
                loadSlot  = getSaveSlot(id, false);
                showStats = true;
            }
        }
        loadLevel(id);
    }

    virtual void invShow(int playerIndex, int page, int itemIndex = -1) {
        if (itemIndex != -1 || page == Inventory::PAGE_SAVEGAME)
            inventory->pageItemIndex[page] = itemIndex;
        inventory->toggle(playerIndex, Inventory::Page(page));
    }

    SaveSlot createSaveSlot(TR::LevelID id, bool checkpoint, bool dummy = false) {
        SaveSlot slot;

        // allocate oversized data for save slot
        slot.data  = new uint8[sizeof(SaveStats) + sizeof(SaveItem) * inventory->itemsCount + // for every save
                               sizeof(SaveState) + sizeof(SaveEntity) * level.entitiesCount]; // only for checkpoints

        uint8 *ptr = slot.data;

    // level progress stats
        SaveStats *stats = (SaveStats*)ptr;
        if (!checkpoint)
            memset(stats, 0, sizeof(*stats));
        else
            *stats = saveStats;
        stats->level      = id;
        stats->checkpoint = checkpoint;
        ptr += sizeof(*stats);

    // inventory items
        int32 *itemsCount = (int32*)ptr;
        ptr += sizeof(*itemsCount);

        *itemsCount = 0;
        if (dummy) {
            SaveItem *item = (SaveItem*)ptr;
            ptr += sizeof(*item);

            item->type  = TR::Entity::INV_PISTOLS;
            item->count = UNLIMITED_AMMO;

            *itemsCount = 1;
        } else {
            for (int i = 0; i < inventory->itemsCount; i++) {
                Inventory::Item *invItem = inventory->items[i];

                if (!TR::Entity::isPickup(TR::Level::convFromInv(invItem->type)))
                    continue;

                if (!checkpoint) {
                    if (!TR::Entity::isCrossLevelItem(TR::Level::convFromInv(invItem->type)))
                        continue;
                    if (TR::isEmptyLevel(id))
                        continue;
                }

                SaveItem *item = (SaveItem*)ptr;
                ptr += sizeof(*item);

                item->type  = invItem->type;
                item->count = invItem->count;

                (*itemsCount)++;
            }
        }

        if (checkpoint) {
        // level state
            SaveState *state = (SaveState*)ptr;
            ptr += sizeof(*state);
            *state = level.state;

        // level entities
            int32 *entitiesCount = (int32*)ptr;
            ptr += sizeof(*entitiesCount);

            *entitiesCount = 0;
            for (int i = 0; i < level.entitiesCount; i++) {
                Controller *controller = (Controller*)level.entities[i].controller;
                SaveEntity *entity = (SaveEntity*)ptr;
                if (!controller || !controller->getSaveData(*entity)) continue;
                ptr += (sizeof(SaveEntity) - sizeof(SaveEntity::Extra)) + entity->extraSize;
                (*entitiesCount)++;
            }
        }

        slot.size = int32(ptr - slot.data);

        return slot;
    }

    void parseSaveSlot(const SaveSlot &slot) {
        clearInventory();

        uint8 *data = slot.data;
        uint8 *ptr  = data;

    // level progress stats
        if (slot.isCheckpoint())
            saveStats = *(SaveStats*)ptr; // start level current position

        ptr += sizeof(saveStats);

    // inventory items
        int32 itemsCount = *(int32*)ptr;
        ptr += sizeof(itemsCount);

        for (int i = 0; i < itemsCount; i++) {
            SaveItem *item = (SaveItem*)ptr;
            inventory->add(TR::Entity::Type(item->type), item->count, false);
            ptr += sizeof(*item);
        }

        if (slot.isCheckpoint()) {
            clearEntities();

        // level state
            level.state = *(SaveState*)ptr;
            ptr += sizeof(level.state);

        // level entities
            int32 entitiesCount = *(int32*)ptr;
            ptr += sizeof(entitiesCount);

            for (int i = 0; i < entitiesCount; i++) {
                SaveEntity *entity = (SaveEntity*)ptr;

                Controller *controller;
                if (i >= level.entitiesBaseCount)
                    controller = addEntity(TR::Entity::Type(entity->type), entity->room, vec3(float(entity->x), float(entity->y), float(entity->z)), TR::angle(entity->rotation));
                else
                    controller = (Controller*)level.entities[i].controller;

                controller->setSaveData(*entity);
                if (Controller::first != controller && controller->flags.state != TR::Entity::asNone) {
                    controller->next = Controller::first;
                    Controller::first = controller;
                }

                if (controller->getEntity().isLara()) {
                    Lara *lara = (Lara*)controller;
                    if (lara->camera)
                        lara->camera->reset();
                }

                ptr += (sizeof(SaveEntity) - sizeof(SaveEntity::Extra)) + entity->extraSize;
            }

            if (level.state.flags.flipped) {
                flipMap();
                level.state.flags.flipped = true;
            }

            uint8 track = level.state.flags.track;
            level.state.flags.track = 0;
            playTrack(track);
        }

        statsTimeDelta = 0.0f;
    }

    static void saveGameWriteAsync(Stream *stream, void *userData) {
        if (stream != NULL) {
            delete[] stream->data;
            delete stream;
            saveResult = SAVE_RESULT_SUCCESS;
            UI::showHint(STR_HINT_SAVING_DONE, 1.0f);
        } else {
            saveResult = SAVE_RESULT_ERROR;
            UI::showHint(STR_HINT_SAVING_ERROR, 3.0f);
        }
    }

    virtual void saveGame(TR::LevelID id, bool checkpoint, bool updateStats) {
        ASSERT(saveResult != SAVE_RESULT_WAIT);

        if (saveResult == SAVE_RESULT_WAIT)
            return;

        LOG("Save Game...\n");

        SaveSlot slot;
        if (updateStats) {
            removeSaveSlot(id, true);
            int index = getSaveSlot(id, false);
            if (index == -1) {
                slot = createSaveSlot(id, false, true);
                saveSlots.push(slot);
            } else
                slot = saveSlots[index];
            SaveStats *stats = (SaveStats*)slot.data;
            stats->level      = level.id;
            stats->checkpoint = checkpoint;
        } else {
            removeSaveSlot(id, checkpoint); // remove checkpoints and level saves
            saveSlots.push(createSaveSlot(id, checkpoint));
        }

        saveSlots.sort();

        if (!updateStats) {
            saveResult = SAVE_RESULT_WAIT;
            UI::showHint(STR_HINT_SAVING, 60.0f);

            int size;
            uint8 *data = writeSaveSlots(size);
            osWriteSlot(new Stream(SAVE_FILENAME, (const char*)data, size, saveGameWriteAsync, this));
        }
    }

    virtual void loadGame(int slot) {
        LOG("Load Game...\n");
        loadSlot = slot;
    }

    void clearInventory() {
        int i = inventory->itemsCount;

        while (i--) {
            if (TR::Entity::isPickup(TR::Level::convFromInv(inventory->items[i]->type)))
                inventory->remove(i);
        }
    }

    void clearEntities() {
        Controller::first = NULL;
        for (int i = 0; i < level.entitiesCount; i++) {
            TR::Entity &e = level.entities[i];
            Controller *controller = (Controller*)e.controller;
            if (controller) {
                controller->next = NULL;
                controller->flags.state = TR::Entity::asNone;
                if (i >= level.entitiesBaseCount) {

                    if (e.type == TR::Entity::ENEMY_SKATEBOARD) {
                        continue;
                    }

                    delete controller;
                    e.controller = NULL;
                }
            }
        }
    }

    void initShadow() {
        delete shadow[0];
        delete shadow[1];
        shadow[0] = shadow[1] = NULL;
    #ifndef FFP
        if (Core::settings.detail.shadows > Core::Settings::LOW) {
            if (level.isTitle())
                shadow[0] = new Texture(32, 32, 1, FMT_SHADOW); // init dummy shadow map
            else
                shadow[0] = new Texture(SHADOW_TEX_SIZE, SHADOW_TEX_SIZE, 1, FMT_SHADOW, OPT_TARGET);
        }
    #endif
    }

    virtual void applySettings(const Core::Settings &settings) {
        if (settings.detail.filter != Core::settings.detail.filter) {
            atlasRooms->setFilterQuality(settings.detail.filter);
            atlasObjects->setFilterQuality(settings.detail.filter);
            atlasSprites->setFilterQuality(settings.detail.filter);
        }

        bool rebuildMesh    = settings.detail.water    != Core::settings.detail.water;
        bool rebuildAmbient = settings.detail.lighting != Core::settings.detail.lighting;
        bool rebuildShadows = settings.detail.shadows  != Core::settings.detail.shadows;
        bool rebuildWater   = settings.detail.water    != Core::settings.detail.water;
        bool switchModels   = settings.detail.simple   != Core::settings.detail.simple;

        bool rebuildBump    = settings.effects.bump     != Core::settings.effects.bump; // the normal maps go in the shaders
        bool changeHaze     = settings.effects.fog      != Core::settings.effects.fog ||
                              settings.effects.fogColor != Core::settings.effects.fogColor;

        bool rebuildShaders = rebuildWater || rebuildAmbient || rebuildShadows || rebuildBump;

        bool redraw = memcmp(&settings.detail, &Core::settings.detail, sizeof(settings.detail)) != 0 || rebuildBump || changeHaze;

        bool toggleVR = (settings.detail.stereo == Core::Settings::STEREO_VR) ^ (Core::settings.detail.stereo == Core::Settings::STEREO_VR);

        Core::settings = settings;

        if (toggleVR) {
            osToggleVR(Core::settings.detail.stereo == Core::Settings::STEREO_VR);
        }

        Core::setVSync(Core::settings.detail.vsync != 0);

        Stream::cacheWrite("settings", (char*)&settings, sizeof(settings));

        if (rebuildShaders) {
        #if !defined(_GAPI_D3D8) && !defined(_GAPI_D3D9) && !defined(_GAPI_D3D11) && !defined(_GAPI_GXM)
            delete shaderCache;
            shaderCache = new ShaderCache();
        #endif
        }

        if (changeHaze)
            setupHaze();

        if (rebuildMesh) {
            delete mesh;
            mesh = new MeshBuilder(&level, atlasRooms);
        }

        if (rebuildAmbient) {
            delete ambientCache;
            ambientCache = Core::settings.detail.lighting > Core::Settings::MEDIUM ? new AmbientCache(this) : NULL;
        }

        if (rebuildShadows)
            initShadow();
            
        if (rebuildWater) {
            delete waterCache;
            waterCache = Core::settings.detail.water > Core::Settings::LOW ? new WaterCache(this) : NULL;
        }

        if (redraw && inventory->active && !level.isTitle())
            needRedrawTitleBG = true;

        if (switchModels)
            resetModels();
    }

    virtual TR::Level* getLevel() {
        return &level;
    }

    virtual MeshBuilder* getMesh() {
        return mesh;
    }

    virtual ICamera* getCamera(int index = -1) {
        if (index == -1)
            return camera;
        if (players[index])
            return players[index]->camera;
        return NULL;
    }

    virtual Controller* getLara(int index = 0) {
        return players[index];
    }

    virtual Controller* getLara(const vec3 &pos) {
        if (!players[1])
            return players[0];
        if (players[0]->health <= 0.0f)
            return players[1];
        if (players[1]->health <= 0.0f)
            return players[0];
        return (players[0]->pos - pos).length2() < (players[1]->pos - pos).length2() ? players[0] : players[1];
    }

    virtual bool isCutscene() {
        if (level.isTitle()) return false;
        return camera->mode == Camera::MODE_CUTSCENE;
    }

    virtual uint16 getRandomBox(uint16 zone, uint16 *zones) { 
        ZoneCache::Item *item = zoneCache->getBoxes(zone, zones);
        return item->boxes[rand() % item->count];
    }
    
    virtual uint16 findPath(int ascend, int descend, bool big, int boxStart, int boxEnd, uint16 *zones, uint16 **boxes) {
        return zoneCache->findPath(ascend, descend, big, boxStart, boxEnd, zones, boxes);
    }

    void updateBlocks(bool rise) {
        for (int i = 0; i < level.entitiesBaseCount; i++) {
            Controller *controller = (Controller*)level.entities[i].controller;
            switch (level.entities[i].type) {
                case TR::Entity::BLOCK_1 :
                case TR::Entity::BLOCK_2 :
                case TR::Entity::BLOCK_3 :
                case TR::Entity::BLOCK_4 :
                case TR::Entity::BLOCK_5 :
                    ((Block*)controller)->updateFloor(rise);
                    break;
                case TR::Entity::MOVING_BLOCK :
                    ((MovingBlock*)controller)->updateFloor(rise);
                    break;
                default : ;
            }
        }
    }

    virtual void flipMap(bool water = true) {
        updateBlocks(false);
        if (water && waterCache) waterCache->flipMap();
        mesh->flipMap();
        level.flipMap();
        updateBlocks(true);
        bodiesFall = true;
    }

    // the dead lie deactivated, those left above the floor fall onto it here
    void updateBodies() {
        if (!bodiesFall)
            return;
        bodiesFall = false;
        for (int i = 0; i < level.entitiesCount; i++) {
            TR::Entity &e = level.entities[i];
            Character *c = (Character*)e.controller;
            if (!c || !e.isEnemy() || c->health > 0.0f || c->flags.state == TR::Entity::asActive || c->flags.invisible)
                continue;
            if (c->fallDead())
                bodiesFall = true;
        }
    }

    virtual void setWaterParams(float height) {
        params->waterHeight = height;
    }

    virtual void waterDrop(const vec3 &pos, float radius, float strength) {
        if (waterCache)
            waterCache->addDrop(pos, radius, strength);
    }

    virtual void setShader(Core::Pass pass, Shader::Type type, bool underwater = false, bool alphaTest = false) {
        shaderCache->bind(pass, type, (underwater ? ShaderCache::FX_UNDERWATER : 0) | (alphaTest ? ShaderCache::FX_ALPHA_TEST : 0));
    }

    virtual void setRoomParams(int roomIndex, Shader::Type type, float diffuse, float ambient, float specular, float alpha, bool alphaTest = false) {
        if (Core::pass == Core::passShadow) {
            setShader(Core::pass, type, false, alphaTest);
            return;
        }

        TR::Room &room = level.rooms[roomIndex];

        if (room.dynLightsCount) {
            Core::lightPos[3]   = room.dynLights[0].pos;
            Core::lightColor[3] = room.dynLights[0].color;
        } else {
            Core::lightPos[3]   = vec4(0);
            Core::lightColor[3] = vec4(0, 0, 0, 1);
        }

        if (type == Shader::SPRITE) {
            alphaTest = true;
        }

    #ifdef FFP
        switch (type) {
            case Shader::SPRITE :
            case Shader::ROOM   :
                ambient = 1.0f;
                Core::lightColor[0].w = 1.0f;
                break;
            case Shader::FLASH  :
            case Shader::MIRROR :
                ambient = 1.0f;
                Core::lightColor[0].w = 
                Core::lightColor[1].w =
                Core::lightColor[2].w =
                Core::lightColor[3].w = 1.0f;
                break;
            default : ;
        }
    #endif

        vec4 material;
        if (Core::pass == Core::passAmbient) {
            if (room.flags.water) {
                material = vec4(underwaterColor, 1.0f);
            } else {
                material = vec4(1.0f);
            }
        } else {
            material = vec4(diffuse, ambient, specular, alpha);
        }
        
        setShader(Core::pass, type, (Core::pass == Core::passAmbient) ? false : room.flags.water, alphaTest);

        if (room.flags.water) {
            Core::setFog(underwaterFogParams);
        } else {
            Core::setFog(levelFogParams);
        }

        #ifdef _GAPI_SW
            GAPI::setPalette(room.flags.water ? GAPI::swPaletteWater : GAPI::swPaletteColor);
            GAPI::setShading(true);
        #endif

        Core::setMaterial(material.x, material.y, material.z, material.w);

        if (room.flags.water) {
            if (waterCache) {
                waterCache->bindCaustics(roomIndex);
            }
            setWaterParams(float(room.waterLevel[level.state.flags.flipped]));
        } else {
            setWaterParams(NO_WATER_HEIGHT);
        }

        Core::active.shader->setParam(uParam, Core::params);

        Core::updateLights();

        if (Core::settings.detail.shadows > Core::Settings::MEDIUM) {
            Core::active.shader->setParam(uContacts, Core::contacts[0], MAX_CONTACTS);
        }
    }

    // the haze of the distance, the setting of the range and of the color (TR1 keeps the colors of its levels), the
    // environment in place of them (OL_FOG=<start>,<range> in blocks, OL_FOG_COLOR=<r>,<g>,<b> in 0..1, OL_FOG=0 none)
    void setupHaze() {
        static const vec3 colors[Core::Settings::FOG_COLOR_MAX] = {
            vec3(0.10f, 0.10f, 0.10f),  // gray
            vec3(0.00f, 0.00f, 0.00f),  // black
            vec3(0.13f, 0.14f, 0.16f),  // blue gray
            vec3(0.25f, 0.20f, 0.15f),  // sand
        };

        vec4 original = TR::getFogParams(level.id);
        float range = Core::envFogRange >= 0.0f ? Core::envFogRange : Core::settings.effects.fog * 3.0f;

        if (range <= 0.0f || original.w == 0.0f) {
            levelFogParams = FOG_NONE;
            return;
        }

        vec3 color = (level.version & TR::VER_TR1) ? original.xyz() : colors[Core::settings.effects.fogColor % Core::Settings::FOG_COLOR_MAX];
        if (Core::envFogColor)
            color = Core::fogColorEnv;

        levelFogParams = vec4(color, 1.0f / (range * 1024.0f));
    }

    // the background above the haze: its color, but black in TR1 as the original shows the open spaces of its levels
    // (the rooms with no ceiling over them, the sanctuary of the sphinx) whatever the color of their haze
    vec3 getHazeBackground() const {
        return (level.version & TR::VER_TR1) ? vec3(0.0f) : levelFogParams.xyz();
    }

    // an atlas of rooms or objects and its normal map
    void bindAtlas(Texture *atlas) {
        atlas->bind(sDiffuse);
        Texture *normals = (atlas == atlasRooms) ? normalRooms : ((atlas == atlasObjects) ? normalObjects : NULL);
        (normals ? normals : Core::flatNormalTex)->bind(sNormal);
    }

    virtual void setupBinding() {
        Core::whiteTex->bind(sNormal);
        Core::whiteTex->bind(sMask);
        Core::whiteTex->bind(sReflect);
        atlasRooms->bind(sDiffuse);

        if (Core::pass != Core::passShadow) {
            Texture *shadowMap = shadow[player ? player->camera->cameraIndex : 0];
            if (shadowMap) shadowMap->bind(sShadow);
        }

        Core::basis.identity();
    }

    virtual void renderEnvironment(int roomIndex, const vec3 &pos, Texture **targets, int stride = 0, Core::Pass pass = Core::passAmbient) {
        #ifdef FFP
            return;
        #endif

        #ifdef _GAPI_SW
            return;
        #endif

        #ifdef _GAPI_C3D
            GAPI::rotate90 = false;
        #endif

        #ifdef _GAPI_D3D8
            GAPI::setFrontFace(false);
        #endif

        PROFILE_MARKER("ENVIRONMENT");
        setupBinding();
        float      tmpEye  = Core::eye;
        Core::Pass tmpPass = Core::pass;
        Core::eye = 0.0f;

        int16 rIndex = roomIndex;
        level.getSector(rIndex, pos); // fix room index for overlapped blocks

    // render level into cube faces or texture images
        for (int i = 0; i < 6; i++) {
            setupCubeCamera(pos, i);
            Core::pass = pass;
            if (targets[0]->opt & OPT_CUBEMAP) {
                Core::setTarget(targets[0], NULL, RT_CLEAR_COLOR | RT_CLEAR_DEPTH | RT_STORE_COLOR, i);
            } else {
                Core::setTarget(targets[i * stride], NULL, RT_CLEAR_COLOR | RT_CLEAR_DEPTH | RT_STORE_COLOR);
            }
            renderView(rIndex, false, false);
        }

        #ifdef _GAPI_D3D8
            GAPI::setFrontFace(true);
        #endif

        #ifdef _GAPI_C3D
            GAPI::rotate90 = true;
        #endif

        Core::pass = tmpPass;
        Core::eye  = tmpEye;
    }

    virtual void renderModelFull(int modelIndex, bool underwater, Basis *joints) {
        vec4 ambient[6] = { vec4(0), vec4(0), vec4(0), vec4(0), vec4(0), vec4(0) };

        // opaque
        Core::setBlendMode(bmPremult); // inventory items has fade-out/in alpha
        mesh->transparent = 0;
        setShader(Core::passCompose, Shader::ENTITY, underwater, false);
        Core::setBasis(joints, level.models[modelIndex].mCount);
        Core::active.shader->setParam(uMaterial, Core::active.material);
        Core::active.shader->setParam(uAmbient, ambient[0], 6);
        Core::setFog(FOG_NONE);
        Core::updateLights();
        mesh->renderModel(modelIndex, underwater);
        // transparent
        mesh->transparent = 1;
        setShader(Core::passCompose, Shader::ENTITY, underwater, true);
        Core::setBasis(joints, level.models[modelIndex].mCount);
        Core::active.shader->setParam(uMaterial, Core::active.material);
        Core::active.shader->setParam(uAmbient, ambient[0], 6);
        Core::setFog(FOG_NONE);
        Core::updateLights();
        mesh->renderModel(modelIndex, underwater);
        // additive
        Core::setBlendMode(bmAdd);
        Core::setDepthWrite(false);
        mesh->transparent = 2;
        setShader(Core::passCompose, Shader::ENTITY, underwater, false);
        Core::setBasis(joints, level.models[modelIndex].mCount);
        Core::active.shader->setParam(uMaterial, Core::active.material);
        Core::active.shader->setParam(uAmbient, ambient[0], 6);
        Core::setFog(FOG_NONE);
        Core::updateLights();
        mesh->renderModel(modelIndex, underwater);
        Core::setDepthWrite(true);
        Core::setBlendMode(bmNone);
        mesh->transparent = 0;
    }
    
    virtual void setEffect(Controller *controller, TR::Effect::Type effect) {
        if (effect >= TR::Effect::TR2_FLOOD && this->effect == effect)
            return; // still going (the trigger is stood on)

        this->effect      = effect;
        this->effectTimer = 0.0f;
        this->effectIdx   = 0;

        switch (effect) {
            case TR::Effect::FLOOR_SHAKE :
                for (int i = 0; i < 2; i++)
                    if (players[i] && players[i]->camera)
                        players[i]->camera->shake = 0.5f * max(0.0f, 1.0f - (controller->pos - players[i]->camera->eye.pos).length2() / (15 * 1024 * 15 * 1024));
                return;
            case TR::Effect::FLOOD : {
                Sound::Sample *sample = playSound(TR::SND_FLOOD);
                if (sample)
                    sample->setVolume(0.0f, 4.0f);
                break;
            }
            case TR::Effect::STAIRS2SLOPE :
                playSound(TR::SND_STAIRS2SLOPE);
                break;
            case TR::Effect::EXPLOSION :
                playSound((level.version & TR::VER_TR2) ? SND_TR2_EXPLOSION_1 : TR::SND_TNT);
                shakeCamera(1.0f);
                break;
        // TR2
            case TR::Effect::TR2_RUBBLE :
                playSound(SND_TR2_MASSIVE_CRASH);
                shakeCamera(1.0f);
                break;
            case TR::Effect::TR2_PISTON :
                playSound(SND_TR2_PULLEY_CRANE);
                break;
            case TR::Effect::TR2_CURTAIN :
                playSound(SND_TR2_CURTAIN);
                break;
            case TR::Effect::TR2_SET_CHANGE :
                playSound(SND_TR2_STAGE_BACKDROP);
                break;
            default : ;
        }
    }

    virtual void checkTrigger(Controller *controller, bool heavy) {
        players[0]->checkTrigger(controller, heavy);
    }

    virtual void shakeCamera(float value, bool add = false) {
        for (int i = 0; i < 2; i++)
            if (players[i] && players[i]->camera) {
                if (add)
                    players[i]->camera->shake += value;
                else
                    players[i]->camera->shake  = value;
            }
    }


    virtual Controller* addEntity(TR::Entity::Type type, int room, const vec3 &pos, float angle) {
        int index;
        for (index = level.entitiesBaseCount; index < level.entitiesCount; index++) {
            TR::Entity &e = level.entities[index];
            if (!e.controller) {
                e.type          = type;
                e.room          = room;
                e.x             = int(pos.x);
                e.y             = int(pos.y);
                e.z             = int(pos.z);
                e.rotation      = TR::angle(normalizeAngle(angle));
                e.intensity     = -1;
                e.flags.value   = 0;
                e.flags.smooth  = true;
                e.modelIndex    = level.getModelIndex(e.type);
                break;
            }
        }

        if (index == level.entitiesCount)
            return NULL;

        TR::Entity &e = level.entities[index];
        if (e.isSprite() && e.modelIndex >= 0) { // no sprites of that type in the level, nothing to show
            e.type = TR::Entity::NONE;
            return NULL;
        }

        if (e.isPickup())
            e.intensity = 4096;
        else
            if (e.isSprite()) {
                if (e.type == TR::Entity::LAVA_PARTICLE || e.type == TR::Entity::FLAME || e.type == TR::Entity::FLAME_STREAM)
                    e.intensity = 0; // emissive
                else
                    e.intensity = 0x1FFF - level.rooms[room].ambient;
            }

        Controller *controller = initController(index);
        e.controller = controller;

        if (e.isEnemy() || e.isSprite()) {
            controller->flags.active = TR::ACTIVE;
            controller->activate();
        }

        return controller;
    }

    virtual void removeEntity(Controller *controller) {
        level.entities[controller->entity].controller = NULL;
        delete controller;
    }

    virtual void addMuzzleFlash(Controller *owner, int joint, const vec3 &offset, int lightIndex, bool forward = false) {
        MuzzleFlash *mf = (MuzzleFlash*)addEntity(TR::Entity::MUZZLE_FLASH, owner->getRoomIndex(), offset, 0);
        if (mf) {
            mf->owner      = owner;
            mf->joint      = joint;
            mf->lightIndex = lightIndex;
            mf->forward    = forward;
        }
    }

    virtual bool invUse(int playerIndex, TR::Entity::Type type) {
        if (!players[playerIndex]->useItem(type))
            return inventory->use(type);
        return true;
    }

    virtual void invAdd(TR::Entity::Type type, int count) {
        inventory->add(type, count);
    }

    virtual int* invCount(TR::Entity::Type type) { 
        return inventory->getCountPtr(type);
    }

    virtual bool invChooseKey(int playerIndex, TR::Entity::Type hole) {
        return inventory->chooseKey(playerIndex, hole);
    }

    virtual Sound::Sample* playSound(int id, const vec3 &pos = vec3(0.0f), int flags = 0) const {
        if (level.version == TR::VER_TR1_PSX && id == TR::SND_SECRET)
            return NULL;

        if (!level.soundsInfo) return NULL;

        int16 a = level.soundsMap[id];
        if (a == -1) return NULL;

        TR::SoundInfo &b = level.soundsInfo[a];
        if (b.chance == 0 || randf() <= b.chance) {
            int   index  = b.index + rand() % b.flags.count;
            float volume = b.volume;
            float pitch  = 1.0f + (b.flags.pitch ? ((randf() - 0.5f) * b.pitch) : 0.0f);

            if (level.version == TR::VER_TR2_PSX) // fix for 8 kHz VAG in PSX TR2
                pitch *= 8000.0f / 11025.0f;

            if (!(flags & Sound::MUSIC)) {
                switch (b.flags.mode) {
                    case 0 : if (level.version & TR::VER_TR1) flags |= Sound::UNIQUE; break;
                    case 1 : flags |= (level.version & TR::VER_TR1) ? Sound::REPLAY : Sound::UNIQUE; break;
                    case 2 : if (level.version & TR::VER_TR1)    flags |= Sound::LOOP; break;
                    case 3 : if (!(level.version & TR::VER_TR1)) flags |= Sound::LOOP | Sound::UNIQUE; break;
                }
            }
            if (b.flags.gain) volume = max(0.0f, volume - randf() * 0.25f);
            //if (b.flags.camera) flags &= ~Sound::PAN;
            return Sound::play(level.getSampleStream(index), &pos, volume, pitch, flags, id);
        }
        return NULL;
    }

    void stopChannel(Sound::Sample *channel) {
        if (channel == sndTrack) {
            sndTrack = NULL;
            if (level.state.flags.track != TR::LEVEL_INFO[level.id].track && TR::LEVEL_INFO[level.id].track != TR::NO_TRACK) // play ambient track
                playTrack(0);
        }
    }

    struct TrackRequest {
        Level *level;
        int   flags;

        TrackRequest(Level *level, int flags) : level(level), flags(flags) {}
    };

    static void playAsync(Stream *stream, void *userData) {
        TrackRequest *req = (TrackRequest*)userData;
        Level *level = req->level;
        level->waitTrack = false;
        if (stream) {
            level->sndTrack = Sound::play(stream, NULL, 0.01f, 1.0f, req->flags);
            if (level->sndTrack) {
                if (level->level.isCutsceneLevel()) {
                    Core::resetTime();
                }
                level->sndTrack->volume = level->sndTrack->volumeTarget = 0.0f;
            }
        }

        delete req;
    }

    static void playAsyncBG(Stream *stream, void *userData) {
        TrackRequest *req = (TrackRequest*)userData;
        if (stream)
            Sound::play(stream, NULL, 1.0f, 1.0f, req->flags);
        delete req;
    }

    virtual void playTrack(uint8 track, bool background = false) {
        if (background) {
            TR::getGameTrack(level.version, track, playAsyncBG, new TrackRequest(this, Sound::MUSIC));
            return;
        }

        if (track == 0) {
            if (sndTrack) return;
            track = TR::LEVEL_INFO[level.id].track;
        }

        if (level.state.flags.track == track) {
        //    if (sndTrack) {
        //        sndTrack->replay();
        //        sndTrack->setVolume(1.0f, 0.2f);
        //    }
            return;
        }

        level.state.flags.track = track;

        if (sndTrack) {
            sndTrack->setVolume(-1.0f, 0.2f);
            sndTrack = NULL;
        }

        if (track == 0xFF) return;

        int flags = Sound::MUSIC;
        if (track == TR::LEVEL_INFO[level.id].track)
            flags |= Sound::LOOP;

        waitTrack = true;
        TR::getGameTrack(level.version, track, playAsync, new TrackRequest(this, flags));

        UI::showSubs(TR::getSubs(level.version, track));
    }

    virtual void stopTrack() {
        playTrack(0xFF);
    }
//==============================

    Level(Stream &stream) : level(stream), waitTrack(false), isEnded(false), bodiesFall(false), cutsceneWaitTimer(0.0f), animTexTimer(0.0f), statsTimeDelta(0.0f), normalRooms(NULL), normalObjects(NULL), hdTiles(NULL), hdScale(1), headTex(NULL), headRectsCount(0) {
        paused = false;

        level.simpleItems = Core::settings.detail.simple == 1;
        level.initModelIndices();

    #ifdef _GAPI_GU
        GAPI::freeEDRAM();
    #endif
        nextLevel = TR::LVL_MAX;
        showStats = false;

        params = (Params*)&Core::params;
        params->time = 0.0f;

        memset(players, 0, sizeof(players));
        player = NULL;

        underwaterColor     = vec3(0.6f, 0.9f, 0.9f);
        underwaterFogParams = vec4(underwaterColor * 0.2f, 1.0f / (6 * 1024));
        levelFogParams      = TR::getFogParams(level.id);
        setupHaze();

        inventory->game = this;

        if (!level.isCutsceneLevel()) {
            inventory->reset();
            memset(&saveStats, 0, sizeof(saveStats));
            saveStats.level = level.id;
        }

        initTextures();
        mesh = new MeshBuilder(&level, atlasRooms);
        initEntities();

        shadow[0] = shadow[1] = NULL;
        scaleTex     = NULL;
        camera       = NULL;
        ambientCache = NULL;
        waterCache   = NULL;
        zoneCache    = NULL;

        needRedrawTitleBG = false;
        needRedrawReflections = true;

        initShadow();

        if (!(lastTitle = level.isTitle())) {
            ASSERT(players[0] != NULL);
            player = players[0];
            camera = player->camera;

            zoneCache    = new ZoneCache(this);
            ambientCache = Core::settings.detail.lighting > Core::Settings::MEDIUM ? new AmbientCache(this) : NULL;
            waterCache   = Core::settings.detail.water    > Core::Settings::LOW    ? new WaterCache(this)   : NULL;

            if (ambientCache) { // at first calculate ambient cube for Lara
                AmbientCache::Cube cube;
                ambientCache->getAmbient(players[0]->getRoomIndex(), players[0]->pos, cube); // add to queue
            }

            for (int i = 0; i < level.soundSourcesCount; i++) {
                TR::SoundSource &src = level.soundSources[i];
                int flags = Sound::PAN;
                if (src.flags & 64)  flags |= Sound::FLIPPED;
                if (src.flags & 128) flags |= Sound::UNFLIPPED;
                playSound(src.id, vec3(float(src.x), float(src.y), float(src.z)), flags);
            }

        }

        effect  = TR::Effect::NONE;

        sndWater = sndTrack = NULL;
        UI::init(this);

        /*
        if (level.id == TR::LVL_TR2_RIG) {
            lara->animation.setAnim(level.models[level.extra.laraSpec].animation);
            camera->doCutscene(lara->pos, lara->angle.y);
        }
        */

    #if DUMP_SAMPLES
        for (int i = 0; i < 256; i++) {
            int16 a = level.soundsMap[i];
            if (a == -1) continue;
            ASSERT(a < level.soundsInfoCount);
            TR::SoundInfo &b = level.soundsInfo[a];
            for (int j = 0; j < b.flags.count; j++) {
                //ASSERT((b.index + j) < level.soundOffsetsCount);
                if ((b.index + j) < level.soundOffsetsCount) {
                    Debug::Level::dumpSample(&level, b.index + j, i, j);
                }
            }
        }
        loadNextLevel();
    #endif

    #if DUMP_PALETTE
        Debug::Level::dumpPalette(&level, level.id);
        loadNextLevel();
    #endif

        saveResult = SAVE_RESULT_SUCCESS;
        if (loadSlot != -1 && saveSlots[loadSlot].getLevelID() == level.id) {
            parseSaveSlot(saveSlots[loadSlot]);
            loadSlot = -1;
        }

        Network::start(this);

        Core::resetTime();
    }

    virtual ~Level() {
        UI::init(NULL);

        Network::stop();

        for (int i = 0; i < level.entitiesCount; i++)
            delete (Controller*)level.entities[i].controller;

        delete shadow[0];
        delete shadow[1];
        delete scaleTex;
        delete ambientCache;
        delete waterCache;
        delete zoneCache;

        delete atlasRooms;
        delete normalRooms;
        delete normalObjects;
        #ifndef SPLIT_BY_TILE
            delete atlasObjects;
            delete atlasSprites;
            delete atlasGlyphs;
        #endif
        delete mesh;

        Sound::stopAll();
    }

    void init(bool playLogo, bool playVideo) {
        inventory->init(playLogo, playVideo);
    }

    // TR2+: a pickup in the room of an enemy at its very position is carried by it (the eels carry nothing)
    void initCarriedItems() {
        if (level.version & TR::VER_TR1)
            return;

        for (int i = 0; i < level.entitiesBaseCount; i++) {
            const TR::Entity &e = level.entities[i];
            if (!e.controller || !e.isEnemy() || e.type == TR::Entity::ENEMY_MORAY_1 || e.type == TR::Entity::ENEMY_MORAY_2)
                continue;

            for (int j = 0; j < level.entitiesBaseCount; j++) {
                const TR::Entity &p = level.entities[j];
                if (p.controller && p.isPickup() && p.room == e.room && p.x == e.x && p.y == e.y && p.z == e.z)
                    ((Controller*)e.controller)->carry((Controller*)p.controller);
            }
        }
    }

    void initEntities() {
        for (int i = 0; i < level.entitiesBaseCount; i++) {
            TR::Entity &e = level.entities[i];
            e.controller = initController(i);
            if (e.type == TR::Entity::LARA || ((level.version & TR::VER_TR1) && e.type == TR::Entity::CUT_1))
                players[0] = (Lara*)e.controller;
        }

        initCarriedItems();

        Sound::listenersCount = 1;
    }

    void resetModels() {
        level.simpleItems = Core::settings.detail.simple == 1;
        level.initModelIndices();
        for (int i = 0; i < level.entitiesCount; i++) {
            TR::Entity &e = level.entities[i];
            if (!e.controller) continue;
            Controller *controller = (Controller*)e.controller;
            controller->updateModel();
        }
    }

    void addPlayer(int index) {
        if (level.isCutsceneLevel()) return;

        Controller *c = Controller::first;
        while (c) {
            Controller *next = c->next;
            if (c->getEntity().type == TR::Entity::FLAME && ((Flame*)c)->owner == players[index])
                removeEntity(c);
            c = next;
        }

        if (!players[index]) {
            players[index] = (Lara*)addEntity(TR::Entity::LARA, 0, vec3(0.0f), 0.0f);
            players[index]->camera->cameraIndex = index;
            Sound::listenersCount = 2;
        } else if (index == 1) {
            removePlayer(index);
            Sound::listenersCount = 1;
            return;
        }

        Lara *lead = players[index ^ 1];
        if (!lead) return;

        players[index]->reset(lead->getRoomIndex(), lead->pos, lead->angle.y, lead->stand);
    }

    void removePlayer(int index) {
        for (int i = 0; i < level.entitiesCount; i++) {
            if (level.entities[i].controller && level.entities[i].isEnemy()) {
                Enemy *e = (Enemy*)level.entities[i].controller;
                if (e->target == players[index]) {
                    e->target = NULL;
                }
            }
        }

        removeEntity(players[index]);
        players[index] = NULL;
    }

    Controller* initController(int index) {
        if (level.entities[index].type == TR::Entity::CUT_1 && (level.version & TR::VER_TR1))
            return new Lara(this, index);

        if (level.entities[index].type == TR::Entity::FLARE && index >= level.entitiesBaseCount)
            return new FlareItem(this, index); // flare thrown by Lara
        if (level.entities[index].type == TR::Entity::FLARE_SPARKLES && index >= level.entitiesBaseCount)
            return new FlareFlame(this, index);

        switch (level.entities[index].type) {
            case TR::Entity::LARA                  : return new Lara(this, index);
            case TR::Entity::ENEMY_DOPPELGANGER    : return new Doppelganger(this, index);
            case TR::Entity::ENEMY_WOLF            : return new Wolf(this, index);
            case TR::Entity::ENEMY_BEAR            : return new Bear(this, index);
            case TR::Entity::ENEMY_BAT             : return new Bat(this, index);
            case TR::Entity::ENEMY_PUMA            :
            case TR::Entity::ENEMY_LION_MALE       :
            case TR::Entity::ENEMY_LION_FEMALE     : return new Lion(this, index);
            case TR::Entity::ENEMY_RAT_LAND        :
            case TR::Entity::ENEMY_RAT_WATER       : return new Rat(this, index);
            case TR::Entity::ENEMY_REX             : return new Rex(this, index);
            case TR::Entity::ENEMY_RAPTOR          : return new Raptor(this, index);
            case TR::Entity::ENEMY_MUTANT_1        :
            case TR::Entity::ENEMY_MUTANT_2        :
            case TR::Entity::ENEMY_MUTANT_3        : return new Mutant(this, index);
            case TR::Entity::ENEMY_CENTAUR         : return new Centaur(this, index);
            case TR::Entity::ENEMY_MUMMY           : return new Mummy(this, index);
            case TR::Entity::ENEMY_CROCODILE_LAND  :
            case TR::Entity::ENEMY_CROCODILE_WATER : return new Crocodile(this, index);
            case TR::Entity::ENEMY_GORILLA         : return new Gorilla(this, index);
            case TR::Entity::ENEMY_LARSON          : return new Larson(this, index);
            case TR::Entity::ENEMY_PIERRE          : return new Pierre(this, index);
            case TR::Entity::ENEMY_SKATEBOY        : return new SkaterBoy(this, index);
            case TR::Entity::ENEMY_COWBOY          : return new Cowboy(this, index);
            case TR::Entity::ENEMY_MR_T            : return new MrT(this, index);
            case TR::Entity::ENEMY_NATLA           : return new Natla(this, index);
            case TR::Entity::ENEMY_GIANT_MUTANT    : return new GiantMutant(this, index);
            case TR::Entity::DOOR_1                :
            case TR::Entity::DOOR_2                :
            case TR::Entity::DOOR_3                :
            case TR::Entity::DOOR_4                :
            case TR::Entity::DOOR_5                :
            case TR::Entity::DOOR_6                :
            case TR::Entity::DOOR_7                :
            case TR::Entity::DOOR_8                : return new Door(this, index);
            case TR::Entity::TRAP_DOOR_1           :
            case TR::Entity::TRAP_DOOR_2           : return new TrapDoor(this, index);
            case TR::Entity::BRIDGE_1              :
            case TR::Entity::BRIDGE_2              :
            case TR::Entity::BRIDGE_3              : return new Bridge(this, index);
            case TR::Entity::GEARS_1               :
            case TR::Entity::GEARS_2               :
            case TR::Entity::GEARS_3               : return new Gear(this, index);
            case TR::Entity::INV_KEY_ITEM_1        :
            case TR::Entity::INV_KEY_ITEM_2        :
            case TR::Entity::INV_KEY_ITEM_3        :
            case TR::Entity::INV_KEY_ITEM_4        : return new KeyItemInv(this, index);
            case TR::Entity::TRAP_FLOOR            :
            case TR::Entity::TRAP_BOARDS           : return new TrapFloor(this, index);
            case TR::Entity::CRYSTAL               : return new Crystal(this, index);
            case TR::Entity::TRAP_SWING_BLADE      : return new TrapSwingBlade(this, index);
            case TR::Entity::TRAP_SPIKES           : return new TrapSpikes(this, index);
            case TR::Entity::TRAP_BOULDER          : 
            case TR::Entity::TRAP_BOULDERS         :
            case TR::Entity::ROLLING_DRUMS         : return new TrapBoulder(this, index);
            case TR::Entity::DART                  : return new Dart(this, index);
            case TR::Entity::TRAP_DART_EMITTER     : return new TrapDartEmitter(this, index);
            case TR::Entity::DRAWBRIDGE            : return new Drawbridge(this, index);
            case TR::Entity::BLOCK_1               :
            case TR::Entity::BLOCK_2               :
            case TR::Entity::BLOCK_3               :
            case TR::Entity::BLOCK_4               :
            case TR::Entity::BLOCK_5               : return new Block(this, index);
            case TR::Entity::MOVING_BLOCK          : return new MovingBlock(this, index);
            case TR::Entity::TRAP_CEILING_1        :
            case TR::Entity::TRAP_CEILING_2        : return new TrapCeiling(this, index);
            case TR::Entity::TRAP_SLAM             : return new TrapSlam(this, index);
            case TR::Entity::TRAP_SWORD            : return new TrapSword(this, index);
            case TR::Entity::HAMMER_HANDLE         : return new ThorHammer(this, index);
            case TR::Entity::HAMMER_BLOCK          : return new ThorHammerBlock(this, index);
            case TR::Entity::LIGHTNING             : return new Lightning(this, index);
            case TR::Entity::MOVING_OBJECT         : return new MovingObject(this, index);
            case TR::Entity::SWITCH                :
            case TR::Entity::SWITCH_WATER          :
            case TR::Entity::SWITCH_BUTTON         : 
            case TR::Entity::SWITCH_BIG            :
            case TR::Entity::WHEEL_KNOB            : return new Switch(this, index);
            case TR::Entity::PUZZLE_HOLE_1         :
            case TR::Entity::PUZZLE_HOLE_2         :
            case TR::Entity::PUZZLE_HOLE_3         :
            case TR::Entity::PUZZLE_HOLE_4         :
            case TR::Entity::KEY_HOLE_1            :
            case TR::Entity::KEY_HOLE_2            :
            case TR::Entity::KEY_HOLE_3            :
            case TR::Entity::KEY_HOLE_4            : return new KeyHole(this, index);
            case TR::Entity::MIDAS_HAND            : return new MidasHand(this, index);
            case TR::Entity::SCION_TARGET          : return new ScionTarget(this, index);
            case TR::Entity::WATERFALL             : return new Waterfall(this, index);
            case TR::Entity::NATLA_BULLET          :
            case TR::Entity::MUTANT_BULLET         :
            case TR::Entity::HARPOON_WTF           :
            case TR::Entity::MISSILE_KNIFE         :
            case TR::Entity::CENTAUR_BULLET        : return new Bullet(this, index);
            case TR::Entity::TRAP_LAVA             : return new TrapLava(this, index);
            case TR::Entity::BUBBLE                : return new Bubble(this, index);
            case TR::Entity::EXPLOSION             : return new Explosion(this, index);
            case TR::Entity::GRENADE_BULLET        :
            case TR::Entity::HARPOON_BULLET        : return new Projectile(this, index);
            case TR::Entity::WATER_SPLASH          :
            case TR::Entity::BLOOD                 :
            case TR::Entity::SMOKE                 :
            case TR::Entity::SPARKLES              : return new Sprite(this, index, true, Sprite::FRAME_ANIMATED);
            case TR::Entity::RICOCHET              : return new Sprite(this, index, true, Sprite::FRAME_RANDOM);
            case TR::Entity::CENTAUR_STATUE        : return new CentaurStatue(this, index);
            case TR::Entity::CABIN                 : return new Cabin(this, index);
            case TR::Entity::MUZZLE_FLASH          : return new MuzzleFlash(this, index);
            case TR::Entity::LAVA_PARTICLE         : ASSERT(false); return NULL;
            case TR::Entity::TRAP_LAVA_EMITTER     : return new TrapLavaEmitter(this, index);
            case TR::Entity::FLAME                 : return new Flame(this, index);
            case TR::Entity::TRAP_FLAME_EMITTER    : return new TrapFlameEmitter(this, index);
            case TR::Entity::BOAT                  : return new Boat(this, index);
            case TR::Entity::EARTHQUAKE            : return new Earthquake(this, index);
            case TR::Entity::MUTANT_EGG_SMALL      :
            case TR::Entity::MUTANT_EGG_BIG        : return new MutantEgg(this, index);

            case TR::Entity::ENEMY_DOG              : return new Dog(this, index);
            case TR::Entity::ENEMY_TIGER            : return new Tiger(this, index);
            case TR::Entity::ENEMY_SPIDER           : return new Spider(this, index);
            case TR::Entity::ENEMY_CROW             :
            case TR::Entity::ENEMY_EAGLE            : return new Crow(this, index); // as in the original
            case TR::Entity::ENEMY_RAT              : return new Mouse(this, index);
            case TR::Entity::ENEMY_GOON_MASK_1      :
            case TR::Entity::ENEMY_GOON_MASK_2      :
            case TR::Entity::ENEMY_GOON_MASK_3      : return new MaskedGoon(this, index);
            case TR::Entity::ENEMY_GOON_STICK_1     :
            case TR::Entity::ENEMY_GOON_STICK_2     : return new ClubThug(this, index); // as in the original
            case TR::Entity::ENEMY_GOON_SHOTGUN     : return new ShotgunGoon(this, index);
            case TR::Entity::ENEMY_GOON_KNIFE       : return new KnifeGoon(this, index);
            case TR::Entity::ENEMY_MORAY_1          :
            case TR::Entity::ENEMY_MORAY_2          : return new Moray(this, index);
            case TR::Entity::ENEMY_SPIDER_GIANT     : return new SpiderGiant(this, index);
            case TR::Entity::ENEMY_MERCENARY_1      :
            case TR::Entity::ENEMY_MERCENARY_2      :
            case TR::Entity::ENEMY_MERCENARY_3      : return new Mercenary(this, index);
            case TR::Entity::ENEMY_YETI             : return new Yeti(this, index);
            case TR::Entity::ENEMY_BIRD_MONSTER     : return new BirdGuardian(this, index);
            case TR::Entity::ENEMY_DIVER            : return new Diver(this, index);
            case TR::Entity::ENEMY_SHARK            : return new Shark(this, index);
            case TR::Entity::ENEMY_BARACUDA         : return new Barracuda(this, index);
            case TR::Entity::ENEMY_GUNMAN_1         : return new Gunman1(this, index);
            case TR::Entity::ENEMY_GUNMAN_2         :
            case TR::Entity::ENEMY_GOON_FLAME       : return new Gunman2(this, index);
            case TR::Entity::ENEMY_DRAGON_FRONT     : return new Dragon(this, index);
            case TR::Entity::ENEMY_DRAGON_BACK      : return new DragonBack(this, index);
            case TR::Entity::UNUSED_23              :
            case TR::Entity::ENEMY_MARCO            : return new Bartoli(this, index);
            case TR::Entity::ENEMY_GUARD_SPEAR        : return new XianSpearman(this, index);
            case TR::Entity::ENEMY_GUARD_SWORD        : return new XianKnight(this, index);
            case TR::Entity::ENEMY_GUARD_SPEAR_STATUE :
            case TR::Entity::ENEMY_GUARD_SWORD_STATUE :
            case TR::Entity::ENEMY_MERCENARY_SNOWMOBILE : return new SkidooDriver(this, index);
            case TR::Entity::ENEMY_MONK_1           :
            case TR::Entity::ENEMY_MONK_2           : return new Monk(this, index);
            case TR::Entity::ENEMY_WINSTON          : return new Winston(this, index);

            case TR::Entity::CRYSTAL_PICKUP         : return new CrystalPickup(this, index);
            case TR::Entity::STONE_ITEM_1           :
            case TR::Entity::STONE_ITEM_2           :
            case TR::Entity::STONE_ITEM_3           :
            case TR::Entity::STONE_ITEM_4           : return new StoneItem(this, index);

            case TR::Entity::WINDOW_1               :
            case TR::Entity::WINDOW_2               : return new BreakableWindow(this, index);

            case TR::Entity::HELICOPTER_FLYING      : return new HelicopterFlying(this, index);

            case TR::Entity::FISH_EMITTER           : return new DummyController(this, index);

            case TR::Entity::TRAP_SPINDLE           : return new TrapSpindle(this, index);
            case TR::Entity::TRAP_BLADE_WALL        : return new TrapBladeWall(this, index);
            case TR::Entity::TRAP_SPIKES_WALL       : return new TrapSpikeWall(this, index);
            case TR::Entity::TRAP_SPIKES_CEILING    : return new TrapSpikeCeiling(this, index);
            case TR::Entity::TRAP_ICICLES           : return new TrapIcicle(this, index);
            case TR::Entity::ZIPLINE_HANDLE         : return new Zipline(this, index);
            case TR::Entity::VEHICLE_BOAT           : return new SpeedBoat(this, index);
            case TR::Entity::VEHICLE_SNOWMOBILE_RED : return new Snowmobile(this, index);
            case TR::Entity::VEHICLE_SNOWMOBILE_BLACK : return new Snowmobile(this, index);
            case TR::Entity::GONDOLA                : return new Gondola(this, index);
            case TR::Entity::TRAP_MINE              : return new TrapMine(this, index);
            case TR::Entity::SOUND_BIRD             :
            case TR::Entity::SOUND_WATER            : return new SoundSource(this, index);
            case TR::Entity::SOUND_CLOCK            : return new ClockChimes(this, index);
            case TR::Entity::DETONATOR              : return new DetonatorBox(this, index);
            case TR::Entity::SOUND_DOOR_BELL        : return new DoorBell(this, index);
            case TR::Entity::SOUND_ALARM_BELL       : return new BurglarAlarm(this, index);
            case TR::Entity::JUMPPAD                : return new Springboard(this, index);
            case TR::Entity::BELL                   : return new Bell(this, index);
            case TR::Entity::GONG                   : return new Gong(this, index);
            case TR::Entity::GONG_STICK             : return new GongBonger(this, index);
            case TR::Entity::TRAP_BLADE_STATUE      : return new TrapBladeStatue(this, index);
            case TR::Entity::TRAP_SWING_BAG         :
            case TR::Entity::TRAP_SWING_BOX         :
            case TR::Entity::HOOK                   : return new TrapPendulum(this, index);
            case TR::Entity::SOUND_ALARM            : return new SoundAlarm(this, index);
            case TR::Entity::PROPELLER_PLANE        :
            case TR::Entity::PROPELLER_WATER        :
            case TR::Entity::PROPELLER_AIR          :
            case TR::Entity::SAW                    : return new TrapPropeller(this, index);
            case TR::Entity::HELICOPTER             : return new Helicopter(this, index);
            case TR::Entity::FLAME_STREAM           : return new FlameStream(this, index);
            case TR::Entity::DYING_MONK             : return new DyingMonk(this, index);
            case TR::Entity::ELEVATOR               : return new Elevator(this, index);
            case TR::Entity::MINISUB                : return new GeneralObject(this, index);
            case TR::Entity::WAKE_BOAT              : return new BoatWake(this, index);
            case TR::Entity::WAKE_SNOWMOBILE        : return new SnowSprite(this, index);
            case TR::Entity::DRAGON_EXPLOSION_1     :
            case TR::Entity::DRAGON_EXPLOSION_2     :
            case TR::Entity::DRAGON_EXPLOSION_3     : return new SphereOfDoom(this, index);
            case TR::Entity::DRAGON_EXPLSION_EMITTER : return new EarthquakeTR2(this, index);
            case TR::Entity::LARA_END_PLACE         : return new CombatEnd(this, index);
            case TR::Entity::LAVA_BOWL              : return new LavaBowl(this, index);
            case TR::Entity::TRAP_EXTRA_FIRE        : return new HotLiquid(this, index);
            case TR::Entity::LARA_END_SHOTGUN       : return new EndShotgun(this, index);

            default                                 : return new Controller(this, index);
        }
    }

    #define ATLAS_PAGE_BARS   4096
    #define ATLAS_PAGE_GLYPHS 8192

    AtlasTile *tileData;
    uint8 *glyphsRU;
    uint8 *glyphsJA;
    uint8 *glyphsGR;
    uint8 *glyphsCN;
    uint8 *glyphsKO;

// an upscaled texture pack: "texture/<LEVEL>_<tile>.png" in the data folder takes the place of a 256x256 texture tile of
// the level with one hdScale times as large a side (all of a level the same), OL_DUMP_TEXTURES=1 writes the original tiles
// to "texture/original/" for an upscaler
    AtlasColor **hdTiles;
    int hdScale;

// a face of Lara of its own: "texture/LARA_HEAD.png" (its layout in "texture/LARA_HEAD.txt") in place of the textures of
// the head of her model, with the textures at the high detail (the pieces of the png are cut as the textures of the head
// of the level the layout was made from, the polygons of the head tell which piece goes on which texture in any level)
    #define HEAD_CLUT 0xFFFF                         // the mark of the copies of the textures the head takes for its own

    struct HeadPiece { int16 w, h, x, y; };          // in texels of the original, where it is in the png
    struct HeadRect  { uint16 tile; short4 mm; int16 piece; };

    Color32   *headTex;
    uint32    headTexW, headTexH;
    int       headScale;                             // texels of the png for a texel of the original
    HeadPiece headPieces[64];
    int       headPiecesCount;
    HeadRect  headRects[64];
    int       headRectsCount;

    // the numbers of the layout in order, the lines of "#" left out
    static int readHeadLayout(const char *text, int size, int *values, int maxCount) {
        int count = 0;
        const char *p = text, *end = text + size;
        while (p < end && count < maxCount) {
            if (*p == '#') {
                while (p < end && *p != '\n') p++;
                continue;
            }
            if ((*p >= '0' && *p <= '9') || *p == '-') {
                values[count++] = int(strtol(p, (char**)&p, 10));
                continue;
            }
            p++;
        }
        return count;
    }

    void loadHeadTex() {
        headTex = NULL;
        headPiecesCount = headRectsCount = 0;

        if (Core::settings.detail.filter < Core::Settings::HIGH)
            return;

        char path[512];
        sprintf(path, "%stexture/LARA_HEAD.txt", contentDir);
        if (!Stream::exists(path))
            return;
        sprintf(path, "%stexture/LARA_HEAD.png", contentDir);
        if (!Stream::exists(path))
            return;

        int modelIndex = -1;
        for (int i = 0; i < level.modelsCount; i++)
            if (level.models[i].type == TR::Entity::LARA) {
                modelIndex = i;
                break;
            }
        if (modelIndex < 0 || level.models[modelIndex].mCount <= 14)
            return;
        const TR::Mesh &mesh = level.meshes[level.meshOffsets[level.models[modelIndex].mStart + 14]];

        int *values = new int[4096];
        int count = 0;
        {
            Stream stream("texture/LARA_HEAD.txt");
            char *text = new char[stream.size];
            stream.raw(text, stream.size);
            count = readHeadLayout(text, stream.size, values, 4096);
            delete[] text;
        }

        int pieces = count >= 3 ? values[1] : 0;
        int polys  = count >= 3 ? values[2] : 0;
        int textured = 0; // the layout has the textured faces only (the head of TR1 has colored ones too)
        for (int i = 0; i < mesh.fCount; i++)
            if (!mesh.faces[i].colored) textured++;
        if (count < 3 + pieces * 4 + polys * 2 || pieces > COUNT(headPieces) || polys != textured) {
            LOG("! texture/LARA_HEAD.txt: %d polygons in the layout, %d in the head of Lara\n", polys, textured);
            delete[] values;
            return;
        }

        headScale = values[0];
        headPiecesCount = pieces;
        for (int i = 0; i < pieces; i++) {
            int *v = values + 3 + i * 4;
            headPieces[i].w = v[0];
            headPieces[i].h = v[1];
            headPieces[i].x = v[2];
            headPieces[i].y = v[3];
        }

        // the head takes copies of its textures of its own (the body shares some of them, its skin keeps the original),
        // the copies marked by their clut (no TR2 PC texture has one)
        {
            TR::Mesh &head = level.meshes[level.meshOffsets[level.models[modelIndex].mStart + 14]];
            TR::TextureInfo *copies = new TR::TextureInfo[head.fCount];
            int16 *copyOf = new int16[head.fCount];
            int copiesCount = 0;
            for (int i = 0; i < head.fCount; i++) {
                TR::Face &f = head.faces[i];
                if (f.colored || f.flags.texture >= level.objectTexturesCount || level.objectTextures[f.flags.texture].clut == HEAD_CLUT)
                    continue;
                int k = 0;
                while (k < copiesCount && copyOf[k] != f.flags.texture) k++;
                if (k == copiesCount) {
                    copies[k] = level.objectTextures[f.flags.texture];
                    copies[k].clut = HEAD_CLUT;
                    copyOf[k] = f.flags.texture;
                    copiesCount++;
                }
                f.flags.texture = level.objectTexturesCount + k;
            }
            delete[] copyOf;
            int32 count = copiesCount;
            if (count)
                level.appendObjectTex(copies, count);
            else
                delete[] copies;
        }

        // the faces of the mesh are its rectangles, then its triangles, as in the layout
        int *poly = values + 3 + pieces * 4;
        int rects = 0;
        for (int i = 0; i < polys; i++)
            if (!poly[i * 2]) rects++;
        int rectIndex = 0, triIndex = 0;
        for (int i = 0; i < mesh.fCount; i++) {
            const TR::Face &f = mesh.faces[i];
            if (f.colored)
                continue;
            int k = f.triangle ? (rects + triIndex++) : rectIndex++;
            int piece = poly[k * 2 + 1];
            if (piece < 0 || piece >= pieces || f.flags.texture >= level.objectTexturesCount)
                continue;

            const TR::TextureInfo &t = level.objectTextures[f.flags.texture];
            short4 mm = t.getMinMax();
            bool found = false;
            for (int j = 0; j < headRectsCount; j++)
                if (headRects[j].tile == t.tile && sameRect(headRects[j].mm, mm))
                    found = true;
            if (found || headRectsCount == COUNT(headRects))
                continue;
            headRects[headRectsCount].tile  = t.tile;
            headRects[headRectsCount].mm    = mm;
            headRects[headRectsCount].piece = piece;
            headRectsCount++;
        }
        delete[] values;

        Stream stream("texture/LARA_HEAD.png");
        headTex = (Color32*)Texture::LoadPNG(stream, headTexW, headTexH);
        if (!headTex) {
            headRectsCount = 0;
            return;
        }
        LOG("Lara head texture: %d pieces x%d\n", headRectsCount, headScale);
    }

    void freeHeadTex() {
        delete[] headTex;
        headTex = NULL;
        headRectsCount = 0;
    }

    static bool sameRect(const short4 &a, const short4 &b) {
        return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
    }

    int getHeadPiece(uint16 tile, const short4 &mm) const {
        for (int i = 0; i < headRectsCount; i++)
            if (headRects[i].tile == tile && sameRect(headRects[i].mm, mm))
                return headRects[i].piece;
        return -1;
    }

    // a texel of the atlas (x, y of w, h) of a texture of the head from its piece of the png, the area it covers there
    // averaged, as opaque as the texel of the original
    void getHeadTexel(int index, int x, int y, int w, int h, Color32 &c) const {
        const HeadPiece &p = headPieces[index];
        int x0 = p.x + x * p.w * headScale / w;
        int y0 = p.y + y * p.h * headScale / h;
        int x1 = max(x0 + 1, p.x + (x + 1) * p.w * headScale / w);
        int y1 = max(y0 + 1, p.y + (y + 1) * p.h * headScale / h);
        x0 = clamp(x0, 0, int(headTexW) - 1); x1 = clamp(x1, x0 + 1, int(headTexW));
        y0 = clamp(y0, 0, int(headTexH) - 1); y1 = clamp(y1, y0 + 1, int(headTexH));

        uint32 r = 0, g = 0, b = 0, n = 0;
        for (int ty = y0; ty < y1; ty++)
            for (int tx = x0; tx < x1; tx++) {
                const Color32 &t = headTex[ty * headTexW + tx];
                r += t.r; g += t.g; b += t.b; n++;
            }
        c.r = uint8(r / n * c.a / 255);
        c.g = uint8(g / n * c.a / 255);
        c.b = uint8(b / n * c.a / 255);
    }

    void getTileName(char *buf, const char *dir, int index, const char *ext = "png") {
        sprintf(buf, "texture/%s%s_%d.%s", dir, TR::LEVEL_INFO[level.id].name, index, ext);
    }

    // a tile of a texture pack, PNG or JPEG (RGBA, NULL for none)
    Color32* loadTile(int index, uint32 &w, uint32 &h, char *name) {
        char path[512];

        getTileName(name, "", index, "png");
        sprintf(path, "%s%s", contentDir, name);
        if (Stream::exists(path)) {
            Stream stream(name);
            return (Color32*)Texture::LoadPNG(stream, w, h);
        }

    #ifdef _GAPI_GL
        getTileName(name, "", index, "jpg");
        sprintf(path, "%s%s", contentDir, name);
        if (Stream::exists(path)) {
            Stream stream(name);
            uint8 *file = new uint8[stream.size];
            stream.raw(file, stream.size);
            int iw, ih, comp;
            uint8 *pixels = stbi_load_from_memory(file, stream.size, &iw, &ih, &comp, 4);
            delete[] file;
            if (!pixels)
                return NULL;
            w = iw;
            h = ih;
            Color32 *data = new Color32[w * h]; // freed as the PNG ones
            memcpy(data, pixels, w * h * 4);
            stbi_image_free(pixels);
            return data;
        }
    #endif

        return NULL;
    }

    // a texture tile of the level as the game shows it, premultiplied
    void getOriginalTile(AtlasTile *tile, int index) {
        memset(tile, 0, sizeof(*tile));
        for (int pass = 0; pass < 2; pass++) {
            int count = pass ? level.spriteTexturesCount : level.objectTexturesCount;
            for (int j = 0; j < count; j++) {
                TR::TextureInfo &t = pass ? level.spriteTextures[j] : level.objectTextures[j];
                if (t.tile != index) continue;
                short4 uv = t.getMinMax();
                uv.z++;
                uv.w++;
                level.fillObjectTexture(tile, uv, &t);
            }
        }
    }

#ifdef _GAPI_GL
    static void makeDir(const char *path) {
        #ifdef _WIN32
            _mkdir(path);
        #else
            mkdir(path, 0777);
        #endif
    }

    // the tiles as the game shows them, the colors run on under the transparent texels so that an upscaler blends no
    // black into the edges of the cut-outs
    void dumpTiles() {
        char path[512];
        sprintf(path, "%stexture", contentDir);
        makeDir(path);
        strcat(path, "/original");
        makeDir(path);

        AtlasTile *tile = new AtlasTile();
        Color32 *tmp = new Color32[256 * 256];

        for (int i = 0; i < level.tilesCount; i++) {
            getOriginalTile(tile, i);

            Color32 *c = (Color32*)tile->color;
            for (int k = 0; k < 256 * 256; k++) // back from the premultiplied alpha
                if (c[k].a && c[k].a < 255) {
                    c[k].r = uint8(min(255, c[k].r * 255 / c[k].a));
                    c[k].g = uint8(min(255, c[k].g * 255 / c[k].a));
                    c[k].b = uint8(min(255, c[k].b * 255 / c[k].a));
                }

            for (int pass = 0; pass < 8; pass++) { // the colors of the edges run on into the transparent texels
                memcpy(tmp, c, sizeof(Color32) * 256 * 256);
                for (int y = 0; y < 256; y++)
                    for (int x = 0; x < 256; x++) {
                        Color32 &p = c[y * 256 + x];
                        if (p.a || p.value) continue;
                        int r = 0, g = 0, b = 0, n = 0;
                        for (int dy = -1; dy <= 1; dy++)
                            for (int dx = -1; dx <= 1; dx++) {
                                int sx = x + dx, sy = y + dy;
                                if (sx < 0 || sy < 0 || sx > 255 || sy > 255) continue;
                                Color32 &q = tmp[sy * 256 + sx];
                                if (!q.a && !q.value) continue;
                                r += q.r; g += q.g; b += q.b; n++;
                            }
                        if (n) {
                            p.r = r / n; p.g = g / n; p.b = b / n;
                            p.a = 0;
                            if (!p.value) p.r = 1; // marked as filled
                        }
                    }
            }

            sprintf(path, "%s", contentDir);
            getTileName(path + strlen(path), "original/", i);
            if (stbi_write_png(path, 256, 256, 4, c, 256 * 4))
                LOG("save %s\n", path);
        }

        delete[] tmp;
        delete tile;
    }
#endif

    void loadHDTiles() {
        hdScale = 1;
        hdTiles = new AtlasColor*[level.tilesCount];
        memset(hdTiles, 0, sizeof(AtlasColor*) * level.tilesCount);

        bool binaryAlpha = (level.version & (TR::VER_TR1 | TR::VER_TR2 | TR::VER_TR3)) != 0;
        AtlasTile *orig = new AtlasTile();

    // how much of the detail of the upscaler shows over the smooth original, OL_TEXTURE_PACK_MIX=0..1
        float mix = 1.0f;
        const char *mixEnv = getenv("OL_TEXTURE_PACK_MIX");
        if (mixEnv)
            mix = clamp(float(atof(mixEnv)), 0.0f, 1.0f);

        for (int i = 0; i < level.tilesCount; i++) {
            char name[256];
            uint32 w, h;
            Color32 *data = loadTile(i, w, h, name);
            if (!data) continue;

            int s = w / 256;
            if (w != h || (w % 256) || s < 2 || (hdScale > 1 && s != hdScale)) {
                LOG("! %s: %dx%d, a tile pack takes the same multiple of 256 for all\n", name, w, h);
                delete[] data;
                continue;
            }
            hdScale = s;

            getOriginalTile(orig, i);
            if (binaryAlpha) // the cut-outs of the original are on or off, they keep its shapes whatever the upscaler made of them
                for (uint32 k = 0; k < w * h; k++)
                    data[k].a = ((Color32*)orig->color)[(k / w / s) * 256 + (k % w) / s].a ? 255 : 0;
            blendHDTile(data, (Color32*)orig->color, i, s, mix, binaryAlpha);

            for (uint32 k = 0; k < w * h; k++) {
                Color32 &c = data[k];
                c.r = uint8((uint16(c.r) * c.a) / 255); // premultiplied as the atlas has it
                c.g = uint8((uint16(c.g) * c.a) / 255);
                c.b = uint8((uint16(c.b) * c.a) / 255);
            }

            hdTiles[i] = (AtlasColor*)data;
        }

        delete orig;

        if (hdScale > 1)
            LOG("texture pack x%d\n", hdScale);
    }

    // the upscaler sees a whole tile and blends the textures next to each other in it into their edges, so the textures
    // of the original that go on from one face to the next don't fit any more, and it makes up light and dark of its own:
    // a texture takes the original smoothly upscaled and only the fine detail of the upscaler over it (the upscaled less
    // its blur of about a texel of the original, both inside the texture), none of it a texel in from the edges, all of
    // it two texels in
    void blendHDTile(Color32 *hd, const Color32 *orig, int tile, int s, float mix, bool binaryAlpha) {
        int size = 256 * s;
        Color32 *out = new Color32[size * size];
        memcpy(out, hd, sizeof(Color32) * size * size);

        float *tmp = new float[size * size * 3];
        float *low = new float[size * size * 3];

        for (int pass = 0; pass < 2; pass++) {
            int count = pass ? level.spriteTexturesCount : level.objectTexturesCount;
            for (int j = 0; j < count; j++) {
                TR::TextureInfo &t = pass ? level.spriteTextures[j] : level.objectTextures[j];
                if (t.tile != tile) continue;
                short4 r = t.getMinMax(); // the texels of the texture: x..z, y..w
                if (r.z < r.x || r.w < r.y) continue;

                int X0 = r.x * s, X1 = (r.z + 1) * s;
                int Y0 = r.y * s, Y1 = (r.w + 1) * s;
                int W  = X1 - X0, H = Y1 - Y0;
                int br = s;
                float bn = 1.0f / (2 * br + 1);

            // the blur of the upscaled inside the texture
                for (int y = 0; y < H; y++) {
                    for (int x = 0; x < W; x++) {
                        float acc[3] = { 0.0f, 0.0f, 0.0f };
                        for (int i = -br; i <= br; i++) {
                            const Color32 &c = hd[(Y0 + y) * size + X0 + clamp(x + i, 0, W - 1)];
                            acc[0] += c.r; acc[1] += c.g; acc[2] += c.b;
                        }
                        float *o = tmp + (y * W + x) * 3;
                        o[0] = acc[0] * bn; o[1] = acc[1] * bn; o[2] = acc[2] * bn;
                    }
                }
                for (int y = 0; y < H; y++) {
                    for (int x = 0; x < W; x++) {
                        float acc[3] = { 0.0f, 0.0f, 0.0f };
                        for (int i = -br; i <= br; i++) {
                            const float *c = tmp + (clamp(y + i, 0, H - 1) * W + x) * 3;
                            acc[0] += c[0]; acc[1] += c[1]; acc[2] += c[2];
                        }
                        float *o = low + (y * W + x) * 3;
                        o[0] = acc[0] * bn; o[1] = acc[1] * bn; o[2] = acc[2] * bn;
                    }
                }

                for (int Y = Y0; Y < Y1; Y++) {
                    float v  = (Y + 0.5f) / s - 0.5f;
                    int   y0 = int(floorf(v));
                    float fy = v - y0;
                    float dy = min(v - r.y + 0.5f, r.w + 0.5f - v); // to the edge, in texels

                    for (int X = X0; X < X1; X++) {
                        float u  = (X + 0.5f) / s - 0.5f;
                        int   x0 = int(floorf(u));
                        float fx = u - x0;
                        float dx = min(u - r.x + 0.5f, r.z + 0.5f - u);

                    // the original upscaled, bilinear over its opaque texels inside the texture
                        float cr = 0.0f, cg = 0.0f, cb = 0.0f, cw = 0.0f;
                        for (int k = 0; k < 4; k++) {
                            int   ox = clamp(x0 + (k & 1), int(r.x), int(r.z));
                            int   oy = clamp(y0 + (k >> 1), int(r.y), int(r.w));
                            float w  = ((k & 1) ? fx : 1.0f - fx) * ((k >> 1) ? fy : 1.0f - fy);
                            const Color32 &c = orig[oy * 256 + ox];
                            if (!c.a) continue;
                            cr += c.r * w;
                            cg += c.g * w;
                            cb += c.b * w;
                            cw += w;
                        }

                    // the outline of a cut-out (a splash, leaves, a grate) along the smoothly upscaled one of the original,
                    // still on or off, so it doesn't show the steps of the texels of the original any more
                        if (binaryAlpha)
                            out[Y * size + X].a = (cw >= 0.5f) ? 255 : 0;

                        if (cw <= 0.0f) continue; // transparent

                        cr /= cw;
                        cg /= cw;
                        cb /= cw;

                        float k = mix * clamp(min(dx, dy) - 1.0f, 0.0f, 1.0f);
                        const Color32 &h = hd[Y * size + X];
                        const float   *l = low + ((Y - Y0) * W + X - X0) * 3;
                        Color32 &o = out[Y * size + X];
                        o.r = uint8(clamp(cr + (h.r - l[0]) * k, 0.0f, 255.0f));
                        o.g = uint8(clamp(cg + (h.g - l[1]) * k, 0.0f, 255.0f));
                        o.b = uint8(clamp(cb + (h.b - l[2]) * k, 0.0f, 255.0f));
                    }
                }
            }
        }

        memcpy(hd, out, sizeof(Color32) * size * size);
        delete[] low;
        delete[] tmp;
        delete[] out;
    }

    void freeHDTiles() {
        if (!hdTiles) return;
        for (int i = 0; i < level.tilesCount; i++)
            delete[] hdTiles[i];
        delete[] hdTiles;
        hdTiles = NULL;
    }

    static int getAdvGlyphPage(int index) {
        index -= UI::advGlyphsStart;
        if (index >= RU_GLYPH_COUNT) {
            index -= RU_GLYPH_COUNT;
            if (index >= JA_GLYPH_COUNT) {
                index -= JA_GLYPH_COUNT;
                if (index >= GR_GLYPH_COUNT) {
                    index -= GR_GLYPH_COUNT;
                    if (index >= CN_GLYPH_COUNT) {
                        index -= CN_GLYPH_COUNT;
                        return 8 + index / 256; // KO
                    } else {
                        return 4 + index / 256; // CN
                    }
                } else {
                    return 3; // GR
                }
            } else {
                return 1 + index / 256; // JA
            }
        }
        return 0; // RU
    }

    static void fillCallback(Atlas *atlas, int id, int tileX, int tileY, int atlasWidth, int atlasHeight, Atlas::Tile &tile, void *userData, void *data) {
        static const uint32 CommonTexData[CTEX_MAX][25] = {
            // flash bar
                { 0x00000000, 0xFFA20058, 0xFFFFFFFF, 0xFFA20058, 0x00000000 },
            // health bar
                { 0xFF2C5D71, 0xFF5E81AE, 0xFF2C5D71, 0xFF1B4557, 0xFF16304F },
            // oxygen bar
                { 0xFF647464, 0xFFA47848, 0xFF647464, 0xFF4C504C, 0xFF303030 },
            // option bar
                { 0x00000000, 0x20202020, 0x20202020, 0x20202020, 0x00000000,
                  0x00000000, 0x60606060, 0x60606060, 0x60606060, 0x00000000,
                  0x00000000, 0x80808080, 0x80808080, 0x80808080, 0x00000000,
                  0x00000000, 0x60606060, 0x60606060, 0x60606060, 0x00000000,
                  0x00000000, 0x20202020, 0x20202020, 0x20202020, 0x00000000 },
            // white room
                { 0xFFFFFFFF },
            // white object
                { 0xFFFFFFFF },
            // white sprite
                { 0xFFFFFFFF },
            };

        int stride = 256, uvCount;
        short2 *uv = NULL;

        Level *owner = (Level*)userData;
        TR::Level *level = &owner->level;

        AtlasColor *src, *dst = (AtlasColor*)data;
        short4 mm;

        bool isSprite = false;
        int  hdTile   = -1;     // the tile of the texture pack to take it from
        int  headPiece = -1;    // the piece of the face of Lara of its own to take it from

        if (id < level->objectTexturesCount) { // textures
            TR::TextureInfo &t = level->objectTextures[id];
            mm      = t.getMinMax();
            headPiece = (owner->headTex && t.clut == HEAD_CLUT) ? owner->getHeadPiece(t.tile, mm) : -1;
            src     = owner->tileData->color;
            uv      = t.texCoordAtlas;
            uvCount = 4;
            hdTile  = t.tile;
            if (data) {
                level->fillObjectTexture(owner->tileData, tile.uv, tile.tex);
            }
        } else {
            id -= level->objectTexturesCount;

            if (id < level->spriteTexturesCount) { // sprites
                TR::TextureInfo &t = level->spriteTextures[id];
                mm       = t.getMinMax();
                src      = owner->tileData->color;
                uv       = t.texCoordAtlas;
                uvCount  = 2;
                isSprite = true;
                hdTile   = t.tile;
                if (data) {
                    if (id < UI::advGlyphsStart) {
                        level->fillObjectTexture(owner->tileData, tile.uv, tile.tex);
                    } else {
                        int page = getAdvGlyphPage(id);
                        int offset = ATLAS_PAGE_GLYPHS + page * 256;
                        short4 uv = tile.uv;
                        uv.y -= offset;
                        uv.w -= offset;
                        Color32 *glyphsData = NULL;

                        switch (page) {
                            case 0  : glyphsData = (Color32*)owner->glyphsRU; break;
                            case 1  :
                            case 2  : glyphsData = (Color32*)owner->glyphsJA + (page - 1) * 256 * 256; break;
                            case 3  : glyphsData = (Color32*)owner->glyphsGR; break;
                            case 4  :
                            case 5  :
                            case 6  :
                            case 7  : glyphsData = (Color32*)owner->glyphsCN + (page - 4) * 256 * 256; break;
                            case 8  :
                            case 9  :
                            case 10 :
                            case 11 : glyphsData = (Color32*)owner->glyphsKO + (page - 8) * 256 * 256; break;
                            default : ASSERT(false);
                        }

                        level->fillObjectTexture32(owner->tileData, glyphsData, uv, tile.tex);
                    }
                }
            } else { // common (generated) textures
                id -= level->spriteTexturesCount;

                TR::TextureInfo *tex;
                mm.x = mm.y = mm.z = mm.w = 0;
                stride  = 1;
                uvCount = 4;

                switch (id) {
                    case CTEX_FLASH        :
                    case CTEX_HEALTH       :
                    case CTEX_OXYGEN       : 
                    case CTEX_OPTION       :
                    case CTEX_WHITE_ROOM   :
                    case CTEX_WHITE_OBJECT :
                    case CTEX_WHITE_SPRITE :
                        src = owner->tileData->color;
                        tex = &CommonTex[id];
                        if (id != CTEX_WHITE_ROOM && id != CTEX_WHITE_OBJECT && id != CTEX_WHITE_SPRITE) {
                            mm.w = 4; // height - 1
                            if (id == CTEX_OPTION) {
                                stride = 5;
                                mm.z   = 4;
                            }
                        }

                        for (int i = 0; i < (mm.z + 1) * (mm.w + 1); i++) {
                            src[i] = ((Color32*)&CommonTexData[id])[i];
                        }
                        break;
                    default : return;
                }

                uv = tex->texCoordAtlas;
                uv[2].y += mm.w;
                uv[3].y += mm.w;
                uv[1].x += mm.z;
                uv[2].x += mm.z;
            }
        }

    // an atlas of a texture pack has scale texels for a texel of the original, from the tile of the pack or the original
        int scale = atlas->scale;
        AtlasColor *hd = (scale > 1 && hdTile >= 0 && hdTile < level->tilesCount && owner->hdTiles) ? owner->hdTiles[hdTile] : NULL;
        int offX = 0, offY = 0;

        if (data) {
            int w  = (mm.z - mm.x + 1) * scale;
            int h  = (mm.w - mm.y + 1) * scale;
            int bx = atlas->border.x * scale;
            int by = atlas->border.y * scale;
            dst += tileY * atlasWidth + tileX;
            for (int y = -by; y < h + atlas->border.w * scale; y++) {
                for (int x = -bx; x < w + atlas->border.z * scale; x++) {
                    ASSERT((x + bx + tileX) >= 0 && (x + bx + tileX) < atlasWidth);
                    ASSERT((y + by + tileY) >= 0 && (y + by + tileY) < atlasHeight);
                    int sx = clamp(x, 0, w - 1);
                    int sy = clamp(y, 0, h - 1);

                    if (isSprite && (y < 0 || y >= h || x < 0 || x >= w)) {
                        dst[x + bx].value = 0;
                    } else if (hd) {
                        dst[x + bx] = hd[(mm.y * scale + sy) * (256 * scale) + mm.x * scale + sx];
                    } else {
                        dst[x + bx] = src[(mm.y + sy / scale) * stride + mm.x + sx / scale];
                    }

                    if (headPiece >= 0 && !isSprite)
                        owner->getHeadTexel(headPiece, sx, sy, w, h, dst[x + bx]);
                }
                dst += atlasWidth;
            }

            offX = tileX + bx;
            offY = tileY + by;
        }

        for (int i = 0; i < uvCount; i++) {
            if (uv[i].x == mm.z) uv[i].x++;
            if (uv[i].y == mm.w) uv[i].y++;
            int32 ux = (uv[i].x - mm.x) * scale + offX;
            int32 uy = (uv[i].y - mm.y) * scale + offY;

            uv[i].x = ux * 32767 / atlasWidth;
            uv[i].y = uy * 32767 / atlasHeight;
        }

    // apply ref for instanced tile
        if (data) return;

        int ref = tileX;
        if (ref < level->objectTexturesCount) { // textures
            mm = level->objectTextures[ref].getMinMaxAtlas();
        } else {
            ref -= level->objectTexturesCount;
            if (ref < level->spriteTexturesCount) { // sprites
                mm = level->spriteTextures[ref].getMinMaxAtlas();
            } else {
                ASSERT(!"only object textures and sprites can be instanced");
            }
        }

        for (int i = 0; i < uvCount; i++) {
            uv[i].x += mm.x;
            uv[i].y += mm.y;
        }
    }

#ifdef _DEBUG
    void dumpGlyphs() {
        ASSERT(level.tiles8);

        TR::SpriteSequence &seq = level.spriteSequences[level.extra.glyphs];
        short2 size = short2(0, 0);
        for (int i = 0; i < seq.sCount; i++) {
            TR::TextureInfo &sprite = level.spriteTextures[seq.sStart + i];
            short w = sprite.texCoord[1].x - sprite.texCoord[0].x + 1;
            short h = sprite.texCoord[1].y - sprite.texCoord[0].y + 1;
            size.y += h + 1;
            size.x = max(size.x, w);
        }
        size.x += 1;
        size.y += 1;
        size.x = (size.x + 3) / 4 * 4;
        Color32 *data = new Color32[int(size.x) * int(size.y)];
        memset(data, 128, int(size.x) * int(size.y) * 4);
        
        short2 pos = short2(1, 1);
        for (int i = 0; i < seq.sCount; i++) {
            TR::TextureInfo &sprite = level.spriteTextures[seq.sStart + i];
            short w = sprite.texCoord[1].x - sprite.texCoord[0].x + 1;
            short h = sprite.texCoord[1].y - sprite.texCoord[0].y + 1;

            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++) {
                    Tile8 &tile = level.tiles8[sprite.tile];
                    
                    data[pos.x + x + (pos.y + y) * size.x] = level.getColor(tile.index[sprite.texCoord[0].x + x + (sprite.texCoord[0].y + y) * 256]);
                }
            pos.y += h + 1;
        }

        Texture::SaveBMP("pc_glyph.bmp", (char*)data, size.x, size.y);
        delete[] data;
    }

    void dumpKanji() {
        Stream stream("DATA/KANJI.PSX");
        int size = stream.readLE32() / 2;
        ColorIndex4 *buffer = new ColorIndex4[size];
        stream.raw(buffer, size);
        int width = 256;
        int height = size / width * 2;

        Color32 *image = new Color32[width * height];

        Tile4 *tile = (Tile4*)buffer;
        CLUT  &clut = level.cluts[level.spriteTextures[level.kanjiSprite].clut];

        ColorIndex4 *idx = buffer;
        Color32 *ptr = image;

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x += 2) {
                *ptr++ = clut.color[idx->a];
                *ptr++ = clut.color[idx->b];
                idx++;
            }
        }

        Texture::SaveBMP("kanji", (char*)image, width, height);

        delete[] buffer;
        delete[] image;
    }
#endif

    void initTextures() {
    #ifndef SPLIT_BY_TILE

        #if defined(_GAPI_SW) || defined(_GAPI_GU)
            #error atlas packing is not allowed for this platform
        #endif

        #ifdef _DEBUG
            //dumpGlyphs();
            //dumpKanji();
        #endif

        UI::patchGlyphs(level);

        {
            uint32 glyphsW, glyphsH;
            Stream stream(NULL, GLYPH_RU, size_GLYPH_RU);
            glyphsRU = Texture::LoadPNG(stream, glyphsW, glyphsH);
        }

        {
            uint32 glyphsW, glyphsH;
            Stream stream(NULL, GLYPH_JA, size_GLYPH_JA);
            glyphsJA = Texture::LoadBMP(stream, glyphsW, glyphsH);
        }

        {
            uint32 glyphsW, glyphsH;
            Stream stream(NULL, GLYPH_GR, size_GLYPH_GR);
            glyphsGR = Texture::LoadBMP(stream, glyphsW, glyphsH);
        }

        {
            uint32 glyphsW, glyphsH;
            Stream stream(NULL, GLYPH_CN, size_GLYPH_CN);
            glyphsCN = Texture::LoadBMP(stream, glyphsW, glyphsH);
        }

        {
            uint32 glyphsW, glyphsH;
            Stream stream(NULL, GLYPH_KO, size_GLYPH_KO);
            glyphsKO = Texture::LoadBMP(stream, glyphsW, glyphsH);
        }

    #ifdef _GAPI_GL
        if (getenv("OL_DUMP_TEXTURES")) {
            dumpTiles();
            Core::isQuit = true; // only the export
        }
    #endif
        loadHDTiles();
        loadHeadTex();

    // the largest atlas the GPU takes, kept to 8192 texels a side for the memory of the texture pack
        int maxSize = 8192;
    #ifdef _GAPI_GL
        GLint glMax = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &glMax);
        if (glMax > 0) maxSize = min(maxSize, int(glMax));
    #endif

    // repack texture tiles
        int maxTiles = level.objectTexturesCount + level.spriteTexturesCount + CTEX_MAX;
        Atlas *rAtlas, *oAtlas, *sAtlas, *gAtlas;
        for (int scale = hdScale; ; scale /= 2) { // a texture pack too large for the GPU at half the scale
            rAtlas = new Atlas(maxTiles, short4(4, 4, 4, 4), this, fillCallback, scale);
            oAtlas = new Atlas(maxTiles, short4(4, 4, 4, 4), this, fillCallback, scale);
            sAtlas = new Atlas(maxTiles, short4(4, 4, 4, 4), this, fillCallback, scale);
            gAtlas = new Atlas(maxTiles, short4(0, 0, 1, 1), this, fillCallback);
            rAtlas->maxSize = oAtlas->maxSize = sAtlas->maxSize = maxSize;
            rAtlas->makeNormals = oAtlas->makeNormals = Core::normalMapping;
            // add textures
            for (int i = 0; i < level.objectTexturesCount; i++) {
                TR::TextureInfo &t = level.objectTextures[i];
                if (t.tile == 0xFFFF) continue;

                short4 uv;
                uv.x = min(min(t.texCoord[0].x, t.texCoord[1].x), t.texCoord[2].x);
                uv.y = min(min(t.texCoord[0].y, t.texCoord[1].y), t.texCoord[2].y);
                uv.z = max(max(t.texCoord[0].x, t.texCoord[1].x), t.texCoord[2].x) + 1;
                uv.w = max(max(t.texCoord[0].y, t.texCoord[1].y), t.texCoord[2].y) + 1;

                if (t.type == TR::TEX_TYPE_ROOM)
                    rAtlas->add(i, uv, &t);
                else
                    oAtlas->add(i, uv, &t);
            }
            // add sprites
            for (int i = 0; i < level.spriteTexturesCount; i++) {
                TR::TextureInfo &t = level.spriteTextures[i];
                if (t.tile == 0xFFFF) continue;

                short4 uv;
                uv.x = t.texCoord[0].x;
                uv.y = t.texCoord[0].y;
                uv.z = t.texCoord[1].x + 1;
                uv.w = t.texCoord[1].y + 1;

                if (i >= UI::advGlyphsStart) {
                 // add virtual UV offset for additional glyph sprites
                    int offset = ATLAS_PAGE_GLYPHS + getAdvGlyphPage(i) * 256;
                    uv.y += offset;
                    uv.w += offset;
                }

                if (level.extra.glyphs != -1) {
                    TR::SpriteSequence &seq = level.spriteSequences[level.extra.glyphs];
                    if ((i >= seq.sStart && i < seq.sStart + seq.sCount) || i >= UI::advGlyphsStart) {
                        gAtlas->add(level.objectTexturesCount + i, uv, &t);
                        continue;
                    }
                }
                sAtlas->add(level.objectTexturesCount + i, uv, &t);
            }
            // add common textures
            const short2 CommonTexOffset[] = { short2(1, 5), short2(1, 5), short2(1, 5), short2(5, 5), short2(1, 1), short2(1, 1), short2(1, 1) };
            ASSERT(COUNT(CommonTexOffset) == CTEX_MAX);
            memset(CommonTex, 0, sizeof(CommonTex));
            for (int i = 0; i < CTEX_MAX; i++) {
                CommonTex[i].type = CommonTex[i].dataType = TR::TEX_TYPE_SPRITE;
                Atlas *dst = (i == CTEX_FLASH || i == CTEX_WHITE_OBJECT) ? oAtlas : ((i == CTEX_WHITE_ROOM) ? rAtlas : gAtlas);
                dst->add(level.objectTexturesCount + level.spriteTexturesCount + i, short4(i * 32, ATLAS_PAGE_BARS, i * 32 + CommonTexOffset[i].x, ATLAS_PAGE_BARS + CommonTexOffset[i].y), &CommonTex[i]);
            }

            if (scale == 1 || (rAtlas->layout() && oAtlas->layout() && sAtlas->layout()))
                break;

            LOG("! texture pack x%d too large\n", scale);
            delete rAtlas;
            delete oAtlas;
            delete sAtlas;
            delete gAtlas;
        }

        // get result texture
        tileData = new AtlasTile();
        
        atlasRooms   = rAtlas->pack(OPT_MIPMAPS | OPT_VRAM_3DS);
        atlasObjects = oAtlas->pack(OPT_MIPMAPS);
        normalRooms   = rAtlas->normals;
        normalObjects = oAtlas->normals;
        atlasSprites = sAtlas->pack(OPT_MIPMAPS);
        atlasGlyphs  = gAtlas->pack(0);

    #ifdef _OS_3DS
        ASSERT(atlasRooms->width   <= 1024 && atlasRooms->height   <= 1024);
        ASSERT(atlasObjects->width <= 1024 && atlasObjects->height <= 1024);
        ASSERT(atlasSprites->width <= 1024 && atlasSprites->height <= 1024);
    #endif

        delete[] tileData;
        tileData = NULL;
        freeHDTiles();
        freeHeadTex();

        delete[] glyphsRU;
        delete[] glyphsJA;
        delete[] glyphsGR;
        delete[] glyphsCN;
        delete[] glyphsKO;

        glyphsRU = NULL;
        glyphsJA = NULL;
        glyphsGR = NULL;
        glyphsCN = NULL;
        glyphsKO = NULL;

        atlasRooms->setFilterQuality(Core::settings.detail.filter);
        atlasObjects->setFilterQuality(Core::settings.detail.filter);
        atlasSprites->setFilterQuality(Core::settings.detail.filter);
        atlasGlyphs->setFilterQuality(Core::Settings::MEDIUM);

        delete rAtlas;
        delete oAtlas;
        delete sAtlas;
        delete gAtlas;

        LOG("rooms   : %d x %d\n", atlasRooms->width, atlasRooms->height);
        LOG("objects : %d x %d\n", atlasObjects->width, atlasObjects->height);
        LOG("sprites : %d x %d\n", atlasSprites->width, atlasSprites->height);
        LOG("glyphs  : %d x %d\n", atlasGlyphs->width, atlasGlyphs->height);
        PROFILE_LABEL(TEXTURE, atlasRooms->ID, "atlas_rooms");
        PROFILE_LABEL(TEXTURE, atlasObjects->ID, "atlas_objects");
        PROFILE_LABEL(TEXTURE, atlasSprites->ID, "atlas_sprites");
        PROFILE_LABEL(TEXTURE, atlasGlyphs->ID, "atlas_glyphs");

    #else
        ASSERT(level.tilesCount);

        #if defined(_GAPI_SW)
            atlasRooms   =
            atlasObjects =
            atlasSprites =
            atlasGlyphs  = new Texture(level.tiles8, level.tilesCount);
            GAPI::initPalette(level.palette, level.lightmap);
        #elif defined(_GAPI_GU)
            atlasRooms   =
            atlasObjects =
            atlasSprites =
            atlasGlyphs  = new Texture(level.tiles4, level.tilesCount, level.cluts, level.clutsCount);
        #else
            Texture::Tile *tiles = new Texture::Tile[level.tilesCount];
            for (int i = 0; i < level.tilesCount; i++) {
                tiles[i].width = tiles[i].height = 256;
                tiles[i].data = new uint32[256 * 256];
            }

            for (int i = 0; i < level.objectTexturesCount; i++) {
                TR::TextureInfo &t = level.objectTextures[i];
                short4 uv = t.getMinMax();
                uv.z++;
                uv.w++;
                level.fillObjectTexture((AtlasTile*)tiles[t.tile].data, uv, &t);
            }

            for (int i = 0; i < level.spriteTexturesCount; i++) {
                TR::TextureInfo &t = level.spriteTextures[i];
                short4 uv = t.getMinMax();
                uv.z++;
                uv.w++;
                level.fillObjectTexture((AtlasTile*)tiles[t.tile].data, uv, &t);
            }

            for (int i = 0; i < level.tilesCount; i++) {
                char buf[256];
                sprintf(buf, "texture/%s_%d.png", TR::LEVEL_INFO[level.id].name, i);
                if (Stream::exists(buf)) {
                    Stream stream(buf);
                    delete[] tiles[i].data;
                    tiles[i].data = (uint32*)Texture::LoadPNG(stream, tiles[i].width, tiles[i].height);
                }
            }

            atlasRooms   =
            atlasObjects =
            atlasSprites =
            atlasGlyphs  = new Texture(tiles, level.tilesCount);

            for (int i = 0; i < level.tilesCount; i++)
                delete[] tiles[i].data;
            delete[] tiles;
        #endif

        #ifndef _GAPI_SW
        for (int i = 0; i < level.objectTexturesCount; i++) {
            TR::TextureInfo &t = level.objectTextures[i];

            t.texCoordAtlas[0].x <<= 7;
            t.texCoordAtlas[0].y <<= 7;
            t.texCoordAtlas[1].x <<= 7;
            t.texCoordAtlas[1].y <<= 7;
            t.texCoordAtlas[2].x <<= 7;
            t.texCoordAtlas[2].y <<= 7;
            t.texCoordAtlas[3].x <<= 7;
            t.texCoordAtlas[3].y <<= 7;

            t.texCoordAtlas[0].x += 64;
            t.texCoordAtlas[0].y += 64;
            t.texCoordAtlas[1].x += 64;
            t.texCoordAtlas[1].y += 64;
            t.texCoordAtlas[2].x += 64;
            t.texCoordAtlas[2].y += 64;
            t.texCoordAtlas[3].x += 64;
            t.texCoordAtlas[3].y += 64;
        }

        for (int i = 0; i < level.spriteTexturesCount; i++) {
            TR::TextureInfo &t = level.spriteTextures[i];

            t.texCoordAtlas[0].x <<= 7;
            t.texCoordAtlas[0].y <<= 7;
            t.texCoordAtlas[1].x <<= 7;
            t.texCoordAtlas[1].y <<= 7;
            /*
            t.texCoord[0].x += 16;
            t.texCoord[0].y += 16;
            t.texCoord[1].x += 16;
            t.texCoord[1].y += 16;
            */
        }
        #endif
    #endif
    }

    void initReflections() {
        for (int i = 0; i < level.entitiesBaseCount; i++) {
            TR::Entity &e = level.entities[i];
            if (e.type == TR::Entity::CRYSTAL) {
                Crystal *c = (Crystal*)e.controller;
                if (c->environment) { // already initialized and baked
                    continue;
                }
                c->bake();

            #ifdef _GAPI_C3D
                // C3D has a limit of GX commands for buffers clearing (GX_MemoryFill), so we limit render to one cubemap per frame
                return;
            #endif
            }
        }
        needRedrawReflections = false;
    }

    void setMainLight(Controller *controller) {
        Core::lightPos[0]   = controller->mainLightPos;
        Core::lightColor[0] = vec4(controller->mainLightColor.xyz(), 1.0f / controller->mainLightColor.w);
    }

    void renderSky() {
        #if !defined(_GAPI_GL) && !defined(_GAPI_D3D11)
            return;
        #endif
        ASSERT(mesh->transparent == 0);

        Shader::Type type;
        TR::SkyParams skyParams;

        if (level.version & TR::VER_TR1) {
            if (Core::settings.detail.lighting < Core::Settings::HIGH || !Core::support.tex3D || !TR::getSkyParams(level.id, skyParams))
                return;
            type = Shader::SKY_AZURE;
        } else { // TR2, TR3
            if (level.extra.sky == -1)
                return;

            if (Core::settings.detail.lighting < Core::Settings::HIGH || !Core::support.tex3D) {
                type = Shader::DEFAULT; // the original sky
            } else {
                type = Shader::SKY_CLOUDS;
                if (!TR::getSkyParams(level.id, skyParams)) {
                    type = Shader::DEFAULT;
                }
            }
        }

        if (type != Shader::DEFAULT && !Core::perlinTex) {
            type = Shader::DEFAULT;
            if (level.version & TR::VER_TR1) {
                return;
            }
        }

        Core::Pass pass = Core::pass;
        mat4 mView = Core::mView;
        mat4 mProj = Core::mProj;

        Core::mView.setPos(vec3(0));
        Core::setViewProj(Core::mView, Core::mProj);

        setShader(Core::passSky, type, false, false);
        Core::active.shader->setParam(uFogParams, camera->isUnderwater() ? FOG_NONE : levelFogParams);

        if (type != Shader::DEFAULT) {
            float time = Core::params.x;
            if (time > SKY_TIME_PERIOD) {
                time /= SKY_TIME_PERIOD;
                time = (time - int(time)) * SKY_TIME_PERIOD;
            }

            Core::active.shader->setParam(uParam,     vec4(skyParams.wind * time * 2.0f, skyParams.seam));
            Core::active.shader->setParam(uLightProj, *(mat4*)&skyParams);
            Core::active.shader->setParam(uPosScale,  skyParams.cloudDownColor, 2);

            Core::perlinTex->bind(sNormal);
            Core::ditherTex->bind(sMask);
        }

        if (level.version & TR::VER_TR1) {
            Core::setCullMode(cmNone);
            mesh->renderBox();
            Core::setCullMode(cmFront);
        } else {
            Basis b;
            Core::setBasis(&b, 1); // unused
            mesh->renderModel(level.extra.sky);
        }

        Core::setViewProj(mView, mProj);
        Core::pass = pass;
    }

    void prepareRooms(RoomDesc *roomsList, int roomsCount) {
        skyIsVisible = (level.version & TR::VER_TR1);

        for (int i = 0; i < level.roomsCount; i++)
            level.rooms[i].flags.visible = false;

        for (int i = 0; i < roomsCount; i++) {
            TR::Room &r = level.rooms[roomsList[i].index];
            skyIsVisible |= r.flags.sky;
            r.flags.visible = true;
        }

        if (Core::pass == Core::passShadow)
            return;

        if (Core::settings.detail.shadows > Core::Settings::MEDIUM) {
            Sphere spheres[MAX_CONTACTS];
            int count = player->getSpheres(spheres);

            for (int i = 0; i < MAX_CONTACTS; i++)
                if (i < count)
                    Core::contacts[i] = vec4(spheres[i].center, PI * spheres[i].radius * spheres[i].radius * 0.25f);
                else
                    Core::contacts[i] = vec4(0.0f);
        }

        setMainLight(player);
    }

    short4 getPortalRect(const vec4 &v, short4 vp) {
        //vec4 s = vec4(v.x, -v.w, v.z, -v.y);
        vec4 sp = (v * 0.5 + 0.5) * vec4(float(vp.z), float(vp.w), float(vp.z), float(vp.w));

        short4 s(short(sp.x) + vp.x, short(sp.y) + vp.y, short(sp.z)+ vp.x, short(sp.w) + vp.y);

        // expand
        s.x -= 2;
        s.y -= 2;
        s.z += 2;
        s.w += 2;

        // clamp
        s.x = max(s.x, vp.x);
        s.y = max(s.y, vp.y);
        s.z = min(s.z, short(vp.x + vp.z));
        s.w = min(s.w, short(vp.y + vp.w));

        // convert from bounds to x,y,w,h
        s.z -= s.x;
        s.w -= s.y;

        // Use the viewport rect if one of the dimensions is the same size
        // as the viewport. This may fix clipping bugs while still allowing
        // impossible geometry tricks.
        if (s.z - s.x >= vp.z - vp.x || s.w - s.y >= vp.w - vp.y) {
           return vp;
        }

        return s;
    }

    void renderRooms(RoomDesc *roomsList, int roomsCount, int transp) {
        PROFILE_MARKER("ROOMS");

        if (Core::pass == Core::passShadow)
            return;

        Basis basis;
        basis.identity();

        Core::mModel.identity();

        switch (transp) {
            case 0 : Core::setBlendMode(bmNone);    break;
            case 1 : Core::setBlendMode(bmPremult); break;
            case 2 : Core::setBlendMode(bmAdd);   Core::setDepthWrite(false); break;
        }

        int i     = 0;
        int end   = roomsCount;
        int dir   = 1;

        if (transp) {
            i   = roomsCount - 1;
            end = -1;
            dir = -1;
        }

        bindAtlas(atlasRooms);

        short4 vp = Core::scissor;

        while (i != end) {
            int roomIndex = roomsList[i].index;
            MeshBuilder::RoomRange &range = mesh->rooms[roomIndex];

            if (!range.geometry[transp].count && !range.dynamic[transp].count) {
                i += dir;
                continue;
            }

            Core::setScissor(getPortalRect(roomsList[i].portal, vp));

            const TR::Room &room = level.rooms[roomIndex];

            vec3 center = room.getCenter();
            int ambient = room.getAmbient(int(center.x), int(center.y), int(center.z));

            setRoomParams(roomIndex, Shader::ROOM, 1.0f, intensityf(ambient), 0.0f, 1.0f, transp == 1);

            basis.pos = room.getOffset();
            Core::setBasis(&basis, 1);

            Core::mModel.setPos(basis.pos);

            mesh->transparent = transp;
            mesh->renderRoomGeometry(roomIndex);

            i += dir;
        }

        Core::setDepthWrite(true);

        if (transp == 1) {
            atlasSprites->bind(sDiffuse);

            Core::setBlendMode(bmPremult);

            #ifdef MERGE_SPRITES
                basis.rot = Core::mViewInv.getRot();
            #else
                basis.rot = quat(0, 0, 0, 1);
            #endif

            for (int i = 0; i < roomsCount; i++) {
                int roomIndex = roomsList[i].index;
                level.rooms[roomIndex].flags.visible = true;

                MeshBuilder::RoomRange &range = mesh->rooms[roomIndex];

                if (!range.sprites.iCount)
                    continue;

                Core::setScissor(getPortalRect(roomsList[i].portal, vp));

                setRoomParams(roomIndex, Shader::SPRITE, 1.0f, 1.0f, 0.0f, 1.0f, true);

                basis.pos = level.rooms[roomIndex].getOffset();
                Core::setBasis(&basis, 1);

                mesh->renderRoomSprites(roomIndex);
            }
        }

        Core::setScissor(vp);
        Core::setBlendMode(bmNone);
    }

    void renderEntity(const TR::Entity &entity) {
        //if (entity.room != lara->getRoomIndex()) return;
        if (Core::pass == Core::passShadow && !entity.castShadow()) return;

        Controller *controller = (Controller*)entity.controller;
        int roomIndex = controller->getRoomIndex();
        TR::Room &room = level.rooms[roomIndex];

        if (controller->flags.invisible)
            return;

        if (!entity.isLara() && !entity.isActor() && !room.flags.visible)
            return;

        bool isModel;

        if (entity.type != TR::Entity::TRAP_LAVA_EMITTER) {
            isModel = entity.modelIndex > 0;
            if (isModel) {
                if (!mesh->models[controller->getModel()->index].geometry[mesh->transparent].count) return;
            } else {
                if (level.spriteSequences[-(entity.modelIndex + 1)].transp != mesh->transparent) return;
            }
        } else {
            if (mesh->transparent != 2) {
                return;
            }
            isModel = false;
        }

        Shader::Type type = isModel ? Shader::ENTITY : Shader::SPRITE;
        if (entity.type == TR::Entity::CRYSTAL)
            type = Shader::MIRROR;

        if (isModel) { // model
            ASSERT(controller->intensity >= 0.0f);

            setMainLight(level.isCutsceneLevel() ? player : controller);
            setRoomParams(roomIndex, type, 1.0f, controller->intensity, controller->specular, 1.0f, mesh->transparent == 1);

            vec3 pos = controller->getPos();
            if (ambientCache) {
                if (!entity.isDoor() && !entity.isBlock() && !entity.isPickup()) { // no advanced ambient lighting for secret (all) doors and blocks
                    AmbientCache::Cube cube;
                    ambientCache->getAmbient(roomIndex, pos, cube);
                    if (cube.status == AmbientCache::Cube::READY) {
                        memcpy(controller->ambient, cube.colors, sizeof(cube.colors)); // store last calculated ambient into controller
                    }
                } else {
                    if (entity.isPickup()) {
                        controller->ambient[0] =
                        controller->ambient[1] =
                        controller->ambient[5] =
                        controller->ambient[4] = vec4(Core::active.material.y * 0.8f);
                        controller->ambient[2] = vec4(Core::active.material.y * 0.1f);
                        controller->ambient[3] = vec4(Core::active.material.y);
                    } else {
                        controller->ambient[0] =
                        controller->ambient[1] =
                        controller->ambient[2] =
                        controller->ambient[3] =
                        controller->ambient[4] =
                        controller->ambient[5] = vec4(Core::active.material.y);
                    }
                }
                Core::active.shader->setParam(uAmbient, controller->ambient[0], 6);
            }
        }

        controller->render(camera->frustum, mesh, type, room.flags.water);
    }

    // the floor of the boat is the hull bottom, below the water surface OpenLara draws (the original draws none), keep
    // the water out of it: the hull goes into the depth buffer only, with the part under the water raised up to the surface
    void renderBoatWaterOccluders() {
        if (!(level.version & TR::VER_TR2) || camera->isUnderwater())
            return;

        Core::Pass pass = Core::pass;
        Core::pass = Core::passCompose;

        for (int i = 0; i < level.entitiesCount; i++) {
            TR::Entity &e = level.entities[i];
            if (e.type != TR::Entity::VEHICLE_BOAT || !e.controller)
                continue;
            SpeedBoat *boat = (SpeedBoat*)e.controller;
            if (!boat->flags.rendered || boat->flags.invisible || boat->isDestroyed())
                continue;

            float surface;
            if (boat->water != 0 && boat->water != BOAT_NO_HEIGHT) {
                surface = float(boat->water - BOAT_SHIFT_Y);
            } else { // not boarded yet
                int16 roomIndex = boat->getRoomIndex();
                TR::Room::Sector *sector = level.getWaterLevelSector(roomIndex, boat->pos);
                if (!sector)
                    continue;
                surface = float(sector->ceiling * 256);
            }

            Basis &joint = boat->getJoint(0);
            int16 top = int16(surface - joint.pos.y - 2.0f); // the surface in the hull space (the hull barely tilts)

            const TR::Model *model = boat->getModel();
            const TR::Mesh  &hull  = level.meshes[level.meshOffsets[model->mStart]];

            setRoomParams(boat->getRoomIndex(), Shader::ENTITY, 1.0f, 1.0f, 0.0f, 1.0f, false);
            Core::whiteTex->bind(sDiffuse);
            Core::setBasis(&joint, 1);
            Core::setBlendMode(bmNone);
            Core::setColorWrite(false, false, false, false);
            Core::setDepthWrite(true);
            Core::setCullMode(cmNone);

            mesh->dynBegin();
            for (int j = 0; j < hull.fCount; j++) {
                const TR::Face &f = hull.faces[j];
                int count = f.triangle ? 3 : 4;
                if (mesh->dynICount + 6 > COUNT(mesh->dynIndices))
                    break;

                int base = mesh->dynVCount;
                for (int k = 0; k < count; k++) {
                    short4 c = hull.vertices[f.vertices[k]].coord;
                    Vertex &v  = mesh->dynVertices[mesh->dynVCount++];
                    v.coord    = short4(c.x, min(c.y, top), c.z, 0);
                    v.normal   = short4(0, -32767, 0, 0);
                    v.texCoord = short4(0, 0, 32767, 32767);
                    v.color    = ubyte4(255, 255, 255, 255);
                    v.light    = ubyte4(255, 255, 255, 255);
                }

                Index *idx = mesh->dynIndices + mesh->dynICount;
                idx[0] = base; idx[1] = base + 1; idx[2] = base + 2;
                mesh->dynICount += 3;
                if (count == 4) {
                    idx[3] = base; idx[4] = base + 2; idx[5] = base + 3;
                    mesh->dynICount += 3;
                }
            }
            mesh->dynEnd();
        }

        Core::setColorWrite(true, true, true, true);
        Core::setCullMode(cmFront);
        Core::pass = pass;
    }

    void loadNextLevelData() {
        isEnded = true;
        char buf[64];
        TR::getGameLevelFile(buf, level.version, nextLevel);
        nextLevel = TR::LVL_MAX;
        new Stream(buf, loadLevelAsync);
    }

    void update() {
        if (isEnded) return;

        bool invRing = inventory->phaseRing != 0.0f && inventory->phaseRing != 1.0f;
        if (inventory->video || inventory->titleTimer >= 1.0f || level.isCutsceneLevel() || invRing) {
            memset(Input::btnEnable, 0, sizeof(Input::btnEnable));
            Input::btnEnable[Input::bInventory] = !invRing;
        } else
            memset(Input::btnEnable, 1, sizeof(Input::btnEnable));

        Input::btnEnable[Input::bWeapon] &= players[0] && players[0]->canDrawWeapon();

        if (inventory->isActive())
            Input::btnEnable[Input::bWalk] = Input::btnEnable[Input::bJump] = Input::btnEnable[Input::bWeapon] = false;

        if (inventory->video) {
            inventory->update();
            UI::update();
            return;
        }

        if (level.isCutsceneLevel() && waitTrack) {
            if (!sndTrack && TR::LEVEL_INFO[level.id].track != TR::NO_TRACK) {
                if (camera->timer > 0.0f) // for the case that audio stops before animation ends
                    loadNextLevel();
                return;
            }

            if (cutsceneWaitTimer > 0.0f) {
                cutsceneWaitTimer -= Core::deltaTime;
                if (cutsceneWaitTimer > 0.0f)
                    return;
                if (sndTrack)
                    sndTrack->setVolume(1.0f, 0.0f);
                cutsceneWaitTimer = 0.0f;
                Core::resetTime();
                LOG("reset timer - %d\n", Core::getTime());
                return;
            }
        }

        if ((Input::lastState[0] == cInventory || Input::lastState[1] == cInventory) && !level.isTitle() && inventory->titleTimer < 1.0f && !inventory->active) {
            int playerIndex = (Input::lastState[0] == cInventory) ? 0 : 1;

            if (getLara(playerIndex)) {
                if (level.isCutsceneLevel()) { // skip cutscene level
                    loadNextLevel();
                    return;
                }

                if (player->health <= 0.0f)
                    inventory->toggle(playerIndex, Inventory::PAGE_OPTION, TR::Entity::INV_PASSPORT);
                else
                    inventory->toggle(playerIndex);
            }
        }

        bool invActive = inventory->isActive();

        inventory->update();

        if (inventory->titleTimer > 1.0f)
            return;

        if (nextLevel != TR::LVL_MAX && !inventory->isActive()) {
            if (showStats) {
                inventory->toggle(0, Inventory::PAGE_LEVEL_STATS);
                showStats = false;
                return;
            }
            loadNextLevelData();
            return;
        }

        if (loadSlot > -1 && nextLevel == TR::LVL_MAX) {
            if (inventory->isActive())
                return;

            loadLevel(saveSlots[loadSlot].getLevelID());
            return;
        }

        if (!inventory->isActive()) {
            UI::update();
        }

        float volWater, volTrack;

        if (invActive || level.isTitle()) {
            Sound::reverb.setRoomSize(vec3(1.0f));
            Sound::listener[0].underwater = false;
            volWater = 0.0f;
            volTrack = level.isTitle() ? 0.9f : 0.0f;
        } else {
            
            if (!level.isCutsceneLevel()) {
                statsTimeDelta += Core::deltaTime;
                while (statsTimeDelta >= 1.0f) {
                    statsTimeDelta -= 1.0f;
                    saveStats.time++;
                }
            }

            if (camera->spectator && Input::lastState[0] == cStart) {
                paused = !paused;
            }

            if (!paused) {
                params->time += Core::deltaTime;
                animTexTimer += Core::deltaTime;

                float timeStep = ANIM_TEX_TIMESTEP;
                if (level.version & TR::VER_TR1)
                    timeStep *= 0.5f;

                if (animTexTimer > timeStep) {
                    level.shiftAnimTex();
                    animTexTimer -= timeStep;
                }

                updateEffect();
                updateBodies();

                Controller *c = Controller::first;
                while (c) {
                    Controller *next = c->next;
                    c->update();
                    c = next;
                }
            } else {
                if (camera->spectator) {
                    camera->update();
                }
            }

            if (waterCache) 
                waterCache->update();

            Controller::clearInactive();

        // underwater ambient sound volume control
            if (camera->isUnderwater()) {
                if (!sndWater && !level.isCutsceneLevel()) {
                    sndWater = playSound(TR::SND_UNDERWATER, vec3(0.0f), Sound::LOOP | Sound::MUSIC);
                    if (sndWater)
                        sndWater->volume = sndWater->volumeTarget = 0.0f;
                }
                volWater = 1.0f;
            } else {
                volWater = 0.0f;
            }

            Sound::listener[0].underwater = (volWater == 1.0f);

            volTrack = 1.0f;
        }

        if (sndWater && sndWater->volumeTarget != volWater)
            sndWater->setVolume(volWater, 0.2f);
        if (sndTrack && sndTrack->volumeTarget != volTrack)
            sndTrack->setVolume(volTrack, 0.2f);

    #ifdef _DEBUG
    #ifdef _NAPI_SOCKET
        if (Input::down[ikJ]) {
            //Network::sayHello();
            NAPI::Peer peer;
            peer.ip = ((93 << 0) | (92 << 8) | (200 << 16) | (163 << 24));
            peer.port = htons(10994);
            LOG("join %s:%d\n", inet_ntoa(*(in_addr*)&peer.ip), ntohs(peer.port));
            Network::joinGame(peer);
            Input::down[ikJ] = false;
        }

        if (Input::down[ikY]) {
            NAPI::requestAddress();
            Input::down[ikY] = false;
        }
    #endif        
    #endif

    #ifdef GEOMETRY_EXPORT
        if (Input::down[ikF1]) {
            Extension::exportGeometry(this, atlasRooms, atlasObjects, atlasSprites);
            Input::down[ikF1] = false;
        }
    #endif
    }

    void updateEffect() {
        if (effect == TR::Effect::NONE)
            return;

        effectTimer += Core::deltaTime;

        switch (effect) {
            case TR::Effect::TR1_FLICKER : {
                int idx = effectIdx;
                switch (effectIdx) {
                    case 0 : if (effectTimer > 3.0f) effectIdx++; break;
                    case 1 : if (effectTimer > 3.1f) effectIdx++; break;
                    case 2 : if (effectTimer > 3.5f) effectIdx++; break;
                    case 3 : if (effectTimer > 3.6f) effectIdx++; break;
                    case 4 : if (effectTimer > 4.1f) { effectIdx++; effect = TR::Effect::NONE; } break;
                }
                if (idx != effectIdx)
                    flipMap();
                break;
            }
            case TR::Effect::EARTHQUAKE : {
                switch (effectIdx) {
                    case 0 : if (effectTimer > 0.0f) { playSound(TR::SND_ROCK);     effectIdx++; shakeCamera(1.0f); } break;
                    case 1 : if (effectTimer > 0.1f) { playSound(TR::SND_STOMP);    effectIdx++; } break;
                    case 2 : if (effectTimer > 0.6f) { playSound(TR::SND_BOULDER);  effectIdx++; shakeCamera(0.5f, true); } break;
                    case 3 : if (effectTimer > 1.1f) { playSound(TR::SND_ROCK);     effectIdx++; } break;
                    case 4 : if (effectTimer > 1.6f) { playSound(TR::SND_BOULDER);  effectIdx++; shakeCamera(0.5f, true); } break;
                    case 5 : if (effectTimer > 2.3f) { playSound(TR::SND_BOULDER);  shakeCamera(0.5f, true); effect = TR::Effect::NONE; } break;
                }
                break;
            }
            case TR::Effect::TR2_FLOOD : { // the rush of water around Lara for 4 seconds
                if (effectTimer > 4.0f) {
                    effect = TR::Effect::NONE;
                    break;
                }
                vec3 p = players[0]->pos;
                p.y = camera->target.pos.y;
                playSound(SND_TR2_FLOOD, p, Sound::PAN | Sound::UNIQUE);
                break;
            }
            case TR::Effect::TR2_CHANDELIER : // the chain of the chandelier for a second
                if (effectTimer > 1.0f) {
                    effect = TR::Effect::NONE;
                    break;
                }
                playSound(SND_TR2_CHAIN_PULLEY, vec3(0.0f), Sound::UNIQUE);
                break;
            default : effect = TR::Effect::NONE; return;
        }
    }

    void setup() {
        camera->setup(Core::pass == Core::passCompose);
        setupBinding();
    }

    void renderEntitiesTransp(int transp) {
        mesh->dynBegin();
        mesh->transparent = transp;

        bindAtlas(atlasObjects);
        for (int i = 0; i < level.entitiesCount; i++) {
            TR::Entity &e = level.entities[i];
            if (!e.controller || e.modelIndex == 0) continue;
            renderEntity(e);
        }

        {
            PROFILE_MARKER("ENTITY_SPRITES");

            if (mesh->dynICount) {
                atlasSprites->bind(sDiffuse);
                Core::lightPos[0]   = vec4(0, 0, 0, 0);
                Core::lightColor[0] = vec4(0, 0, 0, 1);
                setRoomParams(getLara()->getRoomIndex(), Shader::SPRITE, 1.0f, 1.0f, 0.0f, 1.0f, mesh->transparent == 1);

                Basis b;
                b.w   = 1.0f;
                b.pos = Core::viewPos.xyz();
                #ifdef MERGE_SPRITES
                    b.rot = Core::mViewInv.getRot();
                #else
                    b.rot = quat(0, 0, 0, 1);
                #endif
                Core::setBasis(&b, 1);
            }

            mesh->dynEnd();
        }
    }

    void renderEntities(int transp) {
        if (Core::pass == Core::passAmbient) // TODO allow static entities
            return;

        PROFILE_MARKER("ENTITIES");

        if (transp == 0) {
            Core::setBlendMode(bmNone);
            renderEntitiesTransp(transp);
        }

        if (transp == 1) {
            Core::setBlendMode(bmPremult);
            renderEntitiesTransp(transp);

        #ifndef FFP
            Core::setFog(FOG_NONE);
            Core::whiteTex->bind(sDiffuse);
            Core::setBlendMode(bmMult);
            for (int i = 0; i < level.entitiesCount; i++) {
                TR::Entity &entity = level.entities[i];
                Controller *controller = (Controller*)entity.controller;
                if (controller && controller->flags.rendered && controller->getEntity().castShadow())
                    controller->renderShadow(mesh);
            }
            Core::setBlendMode(bmNone);
        #endif
        }

        if (transp == 2) {
            Core::setDepthWrite(false);
            Core::setBlendMode(bmAdd);
            renderEntitiesTransp(transp);
            Core::setDepthWrite(true);
        }
    }

    virtual vec4 projectPoint(const vec4 &p) {
        vec4 res;
        res = Core::mViewProj * p;

        #ifdef _OS_3DS
            if (GAPI::rotate90) {
                res.y = -res.y;
                swap(res.x, res.y);
            }
        #endif

        return res;
    }

    bool checkPortal(const TR::Room &room, const TR::Room::Portal &portal, const vec4 &viewPort, vec4 &clipPort) {
        vec3 n = portal.normal;
        vec3 v = Core::viewPos.xyz() - (room.getOffset() + portal.vertices[0]);

        if (n.dot(v) <= 0.0f)
            return false;

        int  zClip = 0;
        vec4 p[4];

        clipPort = vec4(INF, INF, -INF, -INF);

        for (int i = 0; i < 4; i++) {
            p[i] = projectPoint(vec4(vec3(portal.vertices[i]) + room.getOffset(), 1.0f));

            if (p[i].w > 0.0f) {
                p[i].xy() *= (1.0f / p[i].w);

                clipPort.x = min(clipPort.x, p[i].x);
                clipPort.y = min(clipPort.y, p[i].y);
                clipPort.z = max(clipPort.z, p[i].x);
                clipPort.w = max(clipPort.w, p[i].y);
            } else
                zClip++;
        }

        if (zClip == 4)
            return false;

        if (zClip > 0) {
            for (int i = 0; i < 4; i++) {
                vec4 &a = p[i];
                vec4 &b = p[(i + 1) % 4];

                if ((a.w > 0.0f) ^ (b.w > 0.0f)) {

                    if (a.x < 0.0f && b.x < 0.0f)
                        clipPort.x = -1.0f;
                    else
                        if (a.x > 0.0f && b.x > 0.0f)
                            clipPort.z = 1.0f;
                        else {
                            clipPort.x = -1.0f;
                            clipPort.z =  1.0f;
                        }

                    if (a.y < 0.0f && b.y < 0.0f)
                        clipPort.y = -1.0f;
                    else
                        if (a.y > 0.0f && b.y > 0.0f)
                            clipPort.w = 1.0f;
                        else {
                            clipPort.y = -1.0f;
                            clipPort.w =  1.0f;
                        }

                }
            }
        }

        if (clipPort.x > viewPort.z || clipPort.y > viewPort.w || clipPort.z < viewPort.x || clipPort.w < viewPort.y)
            return false;

        clipPort.x = max(clipPort.x, viewPort.x);
        clipPort.y = max(clipPort.y, viewPort.y);
        clipPort.z = min(clipPort.z, viewPort.z);
        clipPort.w = min(clipPort.w, viewPort.w);

        return true;
    }

    virtual void getVisibleRooms(RoomDesc *roomsList, int &roomsCount, int from, int to, const vec4 &viewPort, bool water, int count = 0) {
        if (roomsCount >= 255 || count > 16) {
            //ASSERT(false);
            return;
        }

        TR::Room &room = level.rooms[to];

        if (Core::pass == Core::passCompose && water && waterCache && from != TR::NO_ROOM && (level.rooms[from].flags.water ^ level.rooms[to].flags.water))
            waterCache->setVisible(from, to);

    // a room seen again through another portal is listed once, its view grows to take in both (as the original does),
    // otherwise the open levels (the Floating Islands) fill the list with copies and lose rooms as the view moves
        vec4 view = viewPort;
        if (room.flags.visible) {
            int i = 0;
            while (i < roomsCount && roomsList[i].index != to)
                i++;
            if (i == roomsCount)
                return; // hidden on purpose (the flipped rooms of a cutscene)

            vec4 &port = roomsList[i].portal;
            view = vec4(min(port.x, viewPort.x), min(port.y, viewPort.y), max(port.z, viewPort.z), max(port.w, viewPort.w));
            if (view == port)
                return; // nothing new to see through it
            port = view;
        } else {
            room.flags.visible = true;
            roomsList[roomsCount++] = RoomDesc(to, view);
        }

        vec4 clipPort;
        for (int i = 0; i < room.portalsCount; i++) {
            TR::Room::Portal &p = room.portals[i];

            if (Core::pass == Core::passCompose && water && waterCache && (level.rooms[to].flags.water ^ level.rooms[p.roomIndex].flags.water))
                waterCache->setVisible(to, p.roomIndex);

            if (from != room.portals[i].roomIndex && checkPortal(room, p, view, clipPort))
                getVisibleRooms(roomsList, roomsCount, to, p.roomIndex, clipPort, water, count + 1);
        }
    }

    void renderOpaque(RoomDesc *roomsList, int roomsCount) {
        renderRooms(roomsList, roomsCount, 0);
        renderEntities(0);
        if (Core::pass != Core::passShadow && skyIsVisible) {
            renderSky();
        }
    }

    void renderTransparent(RoomDesc *roomsList, int roomsCount) {
        renderRooms(roomsList, roomsCount, 1);
        renderEntities(1);
    }

    void renderAdditive(RoomDesc *roomsList, int roomsCount) {
        // don't apply fog for additive geometry
        vec4 oldLevelFogParams = levelFogParams;
        vec4 oldUnderwaterFogParams = underwaterFogParams;
        levelFogParams = FOG_NONE;
        underwaterFogParams = FOG_NONE;

        renderRooms(roomsList, roomsCount, 2);
        renderEntities(2);

        levelFogParams = oldLevelFogParams;
        underwaterFogParams = oldUnderwaterFogParams;
    }

    virtual void renderView(int roomIndex, bool water, bool showUI, int roomsCount = 0, RoomDesc *roomsList = NULL) {
        PROFILE_MARKER("VIEW");

        if (water && waterCache)
            waterCache->reset();

        RoomDesc rList[256];

        if (!roomsList) {
            roomsList = rList;

            // mark all rooms as invisible
            for (int i = 0; i < level.roomsCount; i++)
                level.rooms[i].flags.visible = false;

            if (level.isCutsceneLevel()) { // render all rooms except flipped
            // hide alternative rooms from getVisibleRooms
                for (int i = 0; i < level.roomsCount; i++) {
                    int flipIndex = level.rooms[i].alternateRoom;
                    if (flipIndex > -1)
                        level.rooms[flipIndex].flags.visible = true;
                }

            // get room list through portals (it will change room visible flag)
                getVisibleRooms(roomsList, roomsCount, TR::NO_ROOM, roomIndex, vec4(-1.0f, -1.0f, 1.0f, 1.0f), water);

            // add other non-alternative rooms
                for (int i = 0; i < level.roomsCount; i++)
                    if (!level.rooms[i].flags.visible)
                        roomsList[roomsCount++] = RoomDesc(i, vec4(-1.0f, -1.0f, 1.0f, 1.0f));

            // refresh visible flag
                for (int i = 0; i < level.roomsCount; i++)
                    level.rooms[i].flags.visible = false;

                for (int i = 0; i < roomsCount; i++)
                    level.rooms[roomsList[i].index].flags.visible = true;
            } else
                getVisibleRooms(roomsList, roomsCount, TR::NO_ROOM, roomIndex, vec4(-1.0f, -1.0f, 1.0f, 1.0f), water);
        }

        if (water && waterCache) {
            for (int i = 0; i < roomsCount; i++)
                waterCache->setVisible(roomsList[i].index);

            waterCache->renderReflection();

            Core::Pass pass = Core::pass;
            waterCache->simulate();
            Core::pass = pass;
        }

        // clear entity rendered flag (used for blob shadows)
        if (Core::pass != Core::passAmbient)
            for (int i = 0; i < level.entitiesCount; i++) {
                Controller *controller = (Controller*)level.entities[i].controller;
                if (controller)
                    controller->flags.rendered = false;
            }

        Texture *screen = NULL;
        if (water) {
            screen = (waterCache && waterCache->visible) ? waterCache->getScreenTex() : NULL;

        // under water the background takes the fog color so the distant geometry doesn't end with a sharp edge
            Core::setClearColor(camera->isUnderwater() ? vec4(underwaterFogParams.xyz(), 0.0f) : vec4(getHazeBackground(), 0.0f));

            int clearFlags = RT_STORE_COLOR;

            if (screen) {
                clearFlags |= RT_CLEAR_COLOR | RT_CLEAR_DEPTH | RT_STORE_DEPTH;
            }

            #ifndef EARLY_CLEAR
                clearFlags |= RT_CLEAR_COLOR | RT_CLEAR_DEPTH;
            #endif

            Core::setTarget(screen, NULL, clearFlags); // render to screen texture or back buffer
            Core::validateRenderState();
            Core::setClearColor(vec4(0.0f));
            setupBinding();
        }

        prepareRooms(roomsList, roomsCount);

        renderOpaque(roomsList, roomsCount);
        renderTransparent(roomsList, roomsCount);

        if (camera->isUnderwater())
            renderAdditive(roomsList, roomsCount);

        Core::setFog(FOG_NONE);

        Core::setBlendMode(bmNone);
        if (water && waterCache && waterCache->visible) {
            Core::Pass pass = Core::pass;
            if (!camera->isUnderwater())
                waterCache->renderRays();
            waterCache->renderMask();
            waterCache->copyScreenToRefraction();
            renderBoatWaterOccluders();
            setMainLight(player);
            waterCache->compose();
            if (camera->isUnderwater())
                waterCache->renderRays();

            Core::pass = pass;
            setupBinding();
        }

        if (!camera->isUnderwater())
            renderAdditive(roomsList, roomsCount);
    
        Core::setFog(FOG_NONE);

        Core::setBlendMode(bmNone);

        Core::Pass pass = Core::pass;

        if (water && waterCache && waterCache->visible && screen) {
            Core::setTarget(NULL, NULL, RT_CLEAR_DEPTH | RT_STORE_COLOR);
            Core::validateRenderState();
            waterCache->blitTexture(screen);
        }

        if (showUI) {
            renderUI();
        }

        Core::pass = pass;
    }

    void setupCubeCamera(const vec3 &pos, int face) {
        vec3 up  = vec3(0, -1, 0);
        vec3 dir;
        switch (face) {
            case 0 : dir = vec3( 1,  0,  0); break;
            case 1 : dir = vec3(-1,  0,  0); break;
            case 2 : dir = vec3( 0,  1,  0); up = vec3(0, 0,  1); break;
            case 3 : dir = vec3( 0, -1,  0); up = vec3(0, 0, -1); break;
            case 4 : dir = vec3( 0,  0,  1); break;
            case 5 : dir = vec3( 0,  0, -1); break;
        }

        Core::mViewInv  = mat4(pos, pos + dir, up);
        Core::mView     = Core::mViewInv.inverseOrtho();
        Core::mProj     = GAPI::perspective(90, 1.0f, 32.0f, 45.0f * 1024.0f, 0.0f);

        #ifdef _GAPI_D3D8
            Core::mProj.scale(vec3(1.0f, -1.0f, 1.0f));
        #endif

        Core::mViewProj = Core::mProj * Core::mView;
        Core::viewPos   = Core::mViewInv.offset().xyz();

        camera->setup(false);
    }

    void renderShadowView(int roomIndex) {
        vec3 pos = player->getBoundingBox().center();

        float znear = camera->znear;
        float zfar  = player->mainLightColor.w * 1.5f;

        Core::mViewInv = mat4(player->mainLightPos, pos, vec3(0, -1, 0));
        Core::mView    = Core::mViewInv.inverseOrtho();
        Core::mProj    = GAPI::perspective(90.0f, 1.0f, znear, zfar, 0.0f);

        mat4 &m = mLightProj[player->camera->cameraIndex];
        m = Core::mProj * Core::mView;

        mat4 bias;

        if (GAPI::getProjRange() == mat4::PROJ_ZERO_POS)
            bias = mat4(
                0.5f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.5f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.5f, 0.5f, 0.0f, 1.0f
            );
        else {
            bias = mat4(
                0.5f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.5f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.5f, 0.0f,
                0.5f, 0.5f, 0.5f, 1.0f
            );
        }

    #if defined(_GAPI_D3D8) || defined(_GAPI_D3D9) || defined(_GAPI_D3D11) || defined(_GAPI_GXM)
        bias.e11 = -bias.e11; // vertical flip for UVs
    #endif

        m = bias * m;

        Core::mLightProj = m;

        camera->frustum->pos = Core::viewPos.xyz();
        camera->frustum->calcPlanes(Core::mViewProj);

        setup();
        renderView(roomIndex, false, false);
    }
/*
    void renderShadowEntity(int index, Controller *controller, Controller *player) {
        Box box = controller->getSpheresBox(true);
        mat4 m = controller->getMatrix();

        vec3 pos = m * box.center();
        Core::mViewInv = mat4(player->mainLightPos, pos, vec3(0, -1, 0));
        Core::mView    = Core::mViewInv.inverseOrtho();
        Core::mProj    = mat4(90.0f, 1.0f, 1.0f, 2.0f);

        Box crop = box * (Core::mProj * Core::mView * m);
        crop.min.z = max(0.0f, crop.min.z);

        float sx =  2.0f / (crop.max.x - crop.min.x);
        float sy =  2.0f / (crop.max.y - crop.min.y);
        float sz =  2.0f / (crop.max.z - crop.min.z);
        float ox = -0.5f * (crop.max.x + crop.min.x) * sx;
        float oy = -0.5f * (crop.max.y + crop.min.y) * sy;
        float oz = -0.5f * (crop.max.z + crop.min.z) * sz;

        Core::mProj = mat4(sx,  0,    0,  0,
                            0,  sy,   0,  0,
                            0,   0,  sz,  0,
                            ox,  oy,  oz,  1) * Core::mProj;

        Core::setViewProj(Core::mView, Core::mProj);

        mat4 bias;
        bias.identity();
        bias.e00 = bias.e11 = bias.e22 = bias.e03 = bias.e13 = bias.e23 = 0.5f;
        Core::mLightProj[index] = bias * (Core::mProj * Core::mView);
        
        Core::setBlendMode(bmNone);

        mesh->transparent = 0;
        if (mesh->models[controller->getEntity().modelIndex - 1].geometry[mesh->transparent].count) {
            setShader(Core::pass, Shader::ENTITY, false, false);
            controller->render(NULL, mesh, Shader::ENTITY, false);
        }
        mesh->transparent = 1;
        if (mesh->models[controller->getEntity().modelIndex - 1].geometry[mesh->transparent].count) {
            setShader(Core::pass, Shader::ENTITY, false, true);
            controller->render(NULL, mesh, Shader::ENTITY, false);
        }
        mesh->transparent = 0;
    }

    struct NearObj {
        int   player;
        int   index;
        float dist;
        NearObj() {}
        NearObj(int player, int index, float dist) : player(player), index(index), dist(dist) {}
    };

    int getNearObjects(NearObj *nearObj, int maxCount) {
        int count = 0;

        nearObj[count++] = NearObj(0, players[0]->entity, 0.0f);
        if (players[1])
            nearObj[count++] = NearObj(1, players[1]->entity, 0.0f);
        int base = count;

        for (int i = 0; i < level.entitiesCount; i++) {
            TR::Entity &e = level.entities[i];
            Controller *controller = (Controller*)e.controller;
            if (controller && e.castShadow() && controller != players[0] && controller != players[1]) {
                int pIndex = 0;
                float dist = (players[0]->getPos() - controller->getPos()).length2();
                if (players[1]) {
                    float dist2 = (players[1]->getPos() - controller->getPos()).length2();
                    if (dist2 < dist) {
                        dist = dist2;
                        pIndex = 1;
                    }
                }

                if (dist > 8192 * 8192) continue;

            // get index to insert
                int index = base;
                while (index < count) {
                    if (dist < nearObj[index].dist)
                        break;
                    index++;
                }
            // insertion
                if (index < maxCount) {
                    if (count < maxCount)
                        count++;
                    for (int j = count - 1; j > index; j--)
                        nearObj[j] = nearObj[j - 1];
                    nearObj[index] = NearObj(pIndex, controller->entity, dist);
                }
            }
        }

        return count;
    }
*/
    void renderShadows(int roomIndex, Texture *shadowMap) {
        PROFILE_MARKER("PASS_SHADOW");

        if (Core::settings.detail.shadows == Core::Settings::LOW)
            return;
        ASSERT(shadow);

    // render to shadow map
        float oldEye = Core::eye;
        Core::eye = 0.0f;

        Core::pass = Core::passShadow;
        shadowMap->unbind(sShadow);
        bool colorShadow = shadowMap->fmt == FMT_RGBA ? true : false;
        if (colorShadow)
            Core::setClearColor(vec4(1.0f));
        Core::setTarget(shadowMap, NULL, RT_CLEAR_DEPTH | (colorShadow ? (RT_CLEAR_COLOR | RT_STORE_COLOR) : RT_STORE_DEPTH));
        //Core::setCullMode(cmBack);
        Core::validateRenderState();

        /*
        if (Core::settings.detail.shadows > Core::Settings::MEDIUM) { // per-object shadow map (atlas)
            NearObj nearObj[SHADOW_OBJ_MAX];
            int nearCount = getNearObjects(nearObj, SHADOW_OBJ_MAX);

            for (int i = 0; i < nearCount; i++) {
                Core::setViewport((i % SHADOW_OBJ_COLS) * SHADOW_TEX_TILE, (i / SHADOW_OBJ_COLS) * SHADOW_TEX_TILE, SHADOW_TEX_TILE, SHADOW_TEX_TILE);
                renderShadowEntity(i, (Controller*)level.entities[nearObj[i].index].controller, players[nearObj[i].player]);
            }

            for (int i = nearCount; i < SHADOW_OBJ_MAX; i++)
                Core::mLightProj[i].identity();
        } else // all-in-one shadow map
        */
        renderShadowView(roomIndex);

        //Core::setCullMode(cmFront);
        if (colorShadow)
            Core::setClearColor(vec4(0.0f));

        #ifdef _GAPI_D3D11 // TODO render pass
            Core::setTarget(NULL, NULL, RT_CLEAR_DEPTH | RT_CLEAR_COLOR | RT_STORE_COLOR | RT_STORE_DEPTH);
            Core::validateRenderState();
        #endif

        Core::eye = oldEye;
    }

    #ifdef DEBUG_RENDER
    void renderDebug() {
        if (level.isTitle() || inventory->titleTimer > 1.0f) return;

        Core::setViewport(Core::x, Core::y, Core::width, Core::height);
        camera->setup(true);

        Debug::begin();
        /*
        lara->updateEntity(); // TODO clip angle while rotating

        int q = int(normalizeAngle(lara->angleExt + PI * 0.25f) / (PI * 0.5f));
        float radius = 256.0f;

        const vec2 v[] = {
            { -radius,  radius },
            {  radius,  radius },
            {  radius, -radius },
            { -radius, -radius },
        };

        const vec2 &l = v[q],
                   &r = v[(q + 1) % 4],
                   &f = (q %= 2) ? vec2(l.x, radius * cosf(lara->angleExt)) : vec2(radius * sinf(lara->angleExt), l.y);

        vec3 F = vec3(f.x, 0.0f, f.y);
        vec3 L = vec3(l.x, 0.0f, l.y);
        vec3 R = vec3(r.x, 0.0f, r.y);

        vec3 p, n = lara->pos + vec3(0.0f, -512.0f, 0.0f);
        
        Core::setDepthTest(false);
        glBegin(GL_LINES);
            glColor3f(0, 0, 1); p = n; glVertex3fv((GLfloat*)&p); p += F; glVertex3fv((GLfloat*)&p);
            glColor3f(1, 0, 0); p = n; glVertex3fv((GLfloat*)&p); p += L; glVertex3fv((GLfloat*)&p);
            glColor3f(0, 1, 0); p = n; glVertex3fv((GLfloat*)&p); p += R; glVertex3fv((GLfloat*)&p);
            glColor3f(1, 1, 0); p = lara->pos; glVertex3fv((GLfloat*)&p); p -= vec3(0.0f, LARA_HANG_OFFSET, 0.0f); glVertex3fv((GLfloat*)&p);
        glEnd();
        Core::setDepthTest(true);
        */

        /*
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glLoadIdentity();
            glMatrixMode(GL_PROJECTION);
            glPushMatrix();
            glLoadIdentity();
            glOrtho(0, Core::width, 0, Core::height, 0, 1);

            if (shadow)
                shadow->bind(sDiffuse);
            else
                atlas->bind(sDiffuse);
            glEnable(GL_TEXTURE_2D);
            Core::setCullMode(cmNone);
            Core::setDepthTest(false);
            Core::setBlendMode(bmNone);
            Core::validateRenderState();

            glColor3f(10, 10, 10);
            float w = float(Core::active.textures[sDiffuse]->width);
            float h = float(Core::active.textures[sDiffuse]->height);
            glBegin(GL_QUADS);
                glTexCoord2f(0, 0); glVertex2f(0, 0);
                glTexCoord2f(1, 0); glVertex2f(w, 0);
                glTexCoord2f(1, 1); glVertex2f(w, h);
                glTexCoord2f(0, 1); glVertex2f(0, h);
            glEnd();
            glColor3f(1, 1, 1);

            glDisable(GL_TEXTURE_2D);

            glMatrixMode(GL_PROJECTION);
            glPopMatrix();
            glMatrixMode(GL_MODELVIEW);
            glPopMatrix();
        */

        /*
        Core::setDepthTest(false);
        glBegin(GL_LINES);
            glColor3f(1, 1, 1);
            glVertex3fv((GLfloat*)&lara->pos);
            glVertex3fv((GLfloat*)&lara->mainLightPos);
        glEnd();
        Core::setDepthTest(true);
        */
        //    Debug::Draw::sphere(lara->mainLightPos, lara->mainLightColor.w, vec4(1, 1, 0, 1));

        //    Box bbox = lara->getBoundingBox();
        //    Debug::Draw::box(bbox.min, bbox.max, vec4(1, 0, 1, 1));

            Core::setBlendMode(bmAlpha);
            Core::setDepthTest(false);
            Core::validateRenderState();
        //    Debug::Level::rooms(level, lara->pos, lara->getEntity().room);
        //    Debug::Level::lights(level, player->getRoomIndex(), player);
        //    Debug::Level::sectors(this, players[0]->getRoomIndex(), (int)players[0]->pos.y);
        //    Core::setDepthTest(false);
        //    Debug::Level::portals(level);
        //    Core::setDepthTest(true);
        //    Debug::Level::meshes(level);
        //    Debug::Level::entities(level);
        //    Debug::Level::zones(this, players[0]);
        //    Debug::Level::blocks(level);
        //    Debug::Level::path(level, (Enemy*)level.entities[21].controller);
        //    Debug::Level::debugOverlaps(level, players[0]->box);
        //    Debug::Level::debugBoxes(level, lara->dbgBoxes, lara->dbgBoxesCount);
            Core::setDepthTest(true);
            Core::setBlendMode(bmNone);
        /*// render ambient cube
            Core::validateRenderState();

            static int dbg_ambient = 0;
            dbg_ambient = int(params->time * 2) % 4;

            shadow->unbind(sShadow);
            Core::whiteCube->unbind(sEnvironment);

            glActiveTexture(GL_TEXTURE0);
            glEnable(GL_TEXTURE_2D);
            glDisable(GL_CULL_FACE);
            glColor3f(1, 1, 1);
            for (int j = 0; j < 6; j++) {
                glPushMatrix();
                glTranslatef(player->pos.x, player->pos.y - 1024, player->pos.z);
                switch (j) {
                    case 0 : glRotatef( 90, 0, 1, 0); break;
                    case 1 : glRotatef(-90, 0, 1, 0); break;
                    case 2 : glRotatef(-90, 1, 0, 0); break;
                    case 3 : glRotatef( 90, 1, 0, 0); break;
                    case 4 : glRotatef(  0, 0, 1, 0); break;
                    case 5 : glRotatef(180, 0, 1, 0); break;
                }
                glTranslatef(0, 0, 256);
                
                ambientCache->textures[j * 4 + dbg_ambient]->bind(sDiffuse);
                glBegin(GL_QUADS);
                    glTexCoord2f(0, 0); glVertex3f(-256,  256, 0);
                    glTexCoord2f(1, 0); glVertex3f( 256,  256, 0);
                    glTexCoord2f(1, 1); glVertex3f( 256, -256, 0);
                    glTexCoord2f(0, 1); glVertex3f(-256, -256, 0);
                glEnd();
                glPopMatrix();
            }
            glEnable(GL_CULL_FACE);
            glDisable(GL_TEXTURE_2D);


            glLineWidth(4);
            glBegin(GL_LINES);
            float S = 64.0f;
            for (int i = 0; i < level.roomsCount; i++) {
                if (i != players[0]->getRoomIndex()) continue;
                TR::Room &r = level.rooms[i];                
                for (int j = 0; j < r.xSectors * r.zSectors; j++) {
                    TR::Room::Sector &s = r.sectors[j];
                    vec3 p = vec3(float((j / r.zSectors) * 1024 + 512 + r.info.x),
                                  float(max((s.floor - 2) * 256, (s.floor + s.ceiling) * 256 / 2)),
                                  float((j % r.zSectors) * 1024 + 512 + r.info.z));

                    AmbientCache::Cube &cube = ambientCache->items[ambientCache->offsets[i] + j];
                    if (cube.status == AmbientCache::Cube::READY) {
                        glColor3fv((GLfloat*)&cube.colors[0]);
                        glVertex3f(p.x + 0, p.y + 0, p.z + 0);
                        glVertex3f(p.x + S, p.y + 0, p.z + 0);

                        glColor3fv((GLfloat*)&cube.colors[1]);
                        glVertex3f(p.x + 0, p.y + 0, p.z + 0);
                        glVertex3f(p.x - S, p.y + 0, p.z + 0);

                        glColor3fv((GLfloat*)&cube.colors[2]);
                        glVertex3f(p.x + 0, p.y + 0, p.z + 0);
                        glVertex3f(p.x + 0, p.y + S, p.z + 0);

                        glColor3fv((GLfloat*)&cube.colors[3]);
                        glVertex3f(p.x + 0, p.y + 0, p.z + 0);
                        glVertex3f(p.x + 0, p.y - S, p.z + 0);

                        glColor3fv((GLfloat*)&cube.colors[4]);
                        glVertex3f(p.x + 0, p.y + 0, p.z + 0);
                        glVertex3f(p.x + 0, p.y + 0, p.z + S);

                        glColor3fv((GLfloat*)&cube.colors[5]);
                        glVertex3f(p.x + 0, p.y + 0, p.z + 0);
                        glVertex3f(p.x + 0, p.y + 0, p.z - S);
                    }
                }
            }
            glEnd();
            glLineWidth(1);           
        */

            Debug::Level::info(this, player, player->animation);


        Debug::end();
    }
    #endif

    float setViewport(int view, int eye) {
        int vX = Core::x;
        int vY = Core::y;
        int vW = Core::width;
        int vH = Core::height;

        float aspect = float(vW) / float(vH) * Core::aspectFix;

        if (Core::defaultTarget) {
            vX = 0;
            vY = 0;
            vW = Core::defaultTarget->width;
            vH = Core::defaultTarget->height;
        }

        short4 &vp = Core::viewportDef;
        vp = short4(vX, vY, vW, vH);

        if (players[1] != NULL && view >= 0) {
            vp = short4(vX + vW / 2 * view, vY, vW / 2, vH);

            if (Core::settings.detail.stereo != Core::Settings::STEREO_SPLIT) {
                aspect *= 0.5f;
            }
        }

        if (Core::settings.detail.stereo == Core::Settings::STEREO_SBS) {
            switch (eye) {
                case -1 : vp = short4(vX + vp.x - vp.x / 2, vY + vp.y, vp.z / 2, vp.w);   break;
                case +1 : vp = short4(vX + vW / 2 + vp.x / 2, vY + vp.y, vp.z / 2, vp.w); break;
            }
        }

        Core::setViewport(vp.x, vp.y, vp.z, vp.w);

        return aspect;
    }

    void renderPrepare() {
        setupBinding();

        #ifdef _OS_3DS
            static float sliderState = -1.0f;

            float slider = osGet3DSliderState();
            bool isStereo = slider > 0.0f && !inventory->video;

            if (gfxIs3D() != isStereo) {
                gfxSet3D(isStereo);
                needRedrawTitleBG = inventory->active && !level.isTitle();
            }

            Core::settings.detail.stereo = isStereo ? Core::Settings::STEREO_ANAGLYPH : Core::Settings::STEREO_OFF;

            if (slider != sliderState) {
                sliderState = slider;
                needRedrawTitleBG = inventory->active && !level.isTitle();
            }
        #else
            if (Core::settings.detail.stereo == Core::Settings::STEREO_ANAGLYPH) {
                for (int i = 0; i < 2; i++) {
                    Texture *&tex = Core::eyeTex[i];
                    if (!tex || tex->origWidth != Core::width || tex->origHeight != Core::height) {
                        delete tex;
                        tex = new Texture(Core::width, Core::height, 1, FMT_RGBA, OPT_TARGET | OPT_NEAREST);
                    }
                }
            }
        #endif

        needRenderGame = !inventory->video && !level.isTitle() && ((inventory->phaseRing < 1.0f && inventory->titleTimer <= 1.0f) || needRedrawTitleBG);
        needRenderInventory = inventory->video || level.isTitle() || inventory->phaseRing > 0.0f || inventory->titleTimer > 0.0f;

        bool title  = inventory->isActive() || level.isTitle();
        bool copyBg = title && (lastTitle != title || needRedrawTitleBG);
        lastTitle = title;
        needRedrawTitleBG = false;

        if (!needRenderGame && !copyBg)
            return;

        if (needRedrawReflections) {
            initReflections();
        }

        if (ambientCache) {
            ambientCache->processQueue();
        }

    #ifndef FFP
        if (shadow[0] && players[0]) {
            player = players[0];
            renderShadows(player->getRoomIndex(), shadow[0]);

            if (players[1]) {
                if (!shadow[1]) {
                    shadow[1] = new Texture(shadow[0]->origWidth, shadow[0]->origHeight, 1, shadow[0]->fmt, shadow[0]->opt);
                }

                player = players[1];
                renderShadows(player->getRoomIndex(), shadow[1]);
            }
        }
    #endif

        if (copyBg) {
            inventory->prepareBackground();
        }
    }

    void setDefaultTarget(int eye, int view, bool invBG) {
        int texIndex = eye <= 0 ? 0 : 1;

        if (invBG) {
            if (Core::settings.detail.stereo == Core::Settings::STEREO_SPLIT) {
                Core::defaultTarget = inventory->getBackgroundTarget(view);
            } else {
                Core::defaultTarget = inventory->getBackgroundTarget(texIndex);
            }
        } else {
            if (Core::settings.detail.stereo == Core::Settings::STEREO_ANAGLYPH || Core::settings.detail.stereo == Core::Settings::STEREO_VR) {
                Core::defaultTarget = Core::eyeTex[texIndex];
            } else {
            #ifdef _OS_3DS
                Core::defaultTarget = Core::eyeTex[0];
            #endif
            }
        }

        if (Core::defaultTarget) {
            Core::viewportDef = short4(0, 0, Core::defaultTarget->origWidth, Core::defaultTarget->origHeight);
        } else {
            Core::viewportDef = short4(0, 0, Core::width, Core::height);
        }

        #ifdef EARLY_CLEAR
            if (view == 0 && eye <= 0) {
                if (!invBG) // the distance goes into the haze
                    Core::setClearColor(vec4((camera && camera->isUnderwater()) ? underwaterFogParams.xyz() : getHazeBackground(), 0.0f));
                Core::setTarget(NULL, NULL, RT_CLEAR_COLOR | RT_CLEAR_DEPTH | RT_STORE_COLOR | RT_STORE_DEPTH);
                Core::validateRenderState();
                Core::setClearColor(vec4(0.0f));
            }
        #endif
    }

    void renderEye(int eye, bool showUI, bool invBG) {
        float          oldEye      = Core::eye;
        GAPI::Texture *oldTarget   = Core::defaultTarget;

        Core::eye = float(eye);

        #ifdef _OS_3DS
            Core::eye *= osGet3DSliderState() * 3.25f;
        #endif

        if (needRenderGame || invBG) {
            int viewsCount = players[1] ? 2 : 1;
            for (int view = 0; view < viewsCount; view++) {
                player = players[view];
                camera = player->camera;

                Core::mLightProj = mLightProj[view];

                Core::pass = Core::passCompose;

                short4 oldViewport = Core::viewportDef;

                setDefaultTarget(eye, view, invBG);

                if (Core::settings.detail.stereo == Core::Settings::STEREO_SPLIT) {
                    camera->aspect = setViewport(invBG ? -1 : view, invBG ? 0 : eye);
                } else {
                    camera->aspect = setViewport(view, invBG ? 0 : eye);
                }

                setup();
                renderView(camera->getRoomIndex(), true, showUI);

                Core::viewportDef = oldViewport;
            }
        }

        if (needRenderInventory && !invBG) {
            if (players[1] && Core::settings.detail.stereo == Core::Settings::STEREO_SPLIT) {
                renderInventoryEye(eye, 0);
                renderInventoryEye(eye, 1);
            } else {
                renderInventoryEye(eye, -1);
            }
        }

        Core::defaultTarget = oldTarget;
        Core::eye           = oldEye;

        Core::setViewport(Core::viewportDef);
        Core::setScissor(Core::viewportDef);

        player = players[0];
        if (player) {
            camera = player->camera;
        }
    }

    void renderGame(bool showUI, bool invBG) {
        short4         oldViewport = Core::viewportDef;
        GAPI::Texture *oldTarget   = Core::defaultTarget;

        bool upscale = !invBG && Core::settings.detail.scale != Core::Settings::SCALE_100;

        if (upscale) {
            int scale = (Core::settings.detail.scale + 1) * 25;
            int w = Core::width  * scale / 100;
            int h = Core::height * scale / 100;
            if (!scaleTex || scaleTex->width != w || scaleTex->height != h) {
                delete scaleTex;
                scaleTex = new Texture(w, h, 1, FMT_RGBA, OPT_TARGET);
            }
            Core::defaultTarget = scaleTex;
            Core::viewportDef   = short4(0, 0, w, h);
        }

        if (Core::eye == 0.0f && Core::settings.detail.isStereo()) {
            renderEye(-1, showUI, invBG);
            renderEye(+1, showUI, invBG);
        }  else {
            renderEye(int(Core::eye), showUI, invBG);
        }

    #ifndef _OS_3DS
        if (!invBG && Core::settings.detail.stereo == Core::Settings::STEREO_ANAGLYPH) {
            mat4 mProj, mView;
            mView.identity();
            mProj = GAPI::ortho(-1, +1, -1, +1, 0, 1);
            mProj.scale(vec3(1.0f / 32767.0f));
            Core::setViewProj(mView, mProj);

            Core::setDepthTest(false);
            Core::setDepthWrite(false);

            Core::setTarget(NULL, NULL, RT_STORE_COLOR);
            Core::validateRenderState();
            setShader(Core::passFilter, Shader::FILTER_ANAGLYPH, false, false);
            Core::eyeTex[0]->bind(sDiffuse);
            Core::eyeTex[1]->bind(sNormal);
            Core::setDepthTest(false);
            mesh->renderQuad();

            Core::setDepthTest(true);
            Core::setDepthWrite(true);
        }
    #endif

        Core::defaultTarget = oldTarget;
        Core::viewportDef   = oldViewport;

        if (upscale) {
            mat4 mProj, mView;
            mView.identity();
            mProj = GAPI::ortho(-1, +1, -1, +1, 0, 1);
            mProj.scale(vec3(1.0f / 32767.0f));
            Core::setViewProj(mView, mProj);

            Core::setTarget(NULL, NULL, RT_STORE_COLOR);
            setShader(Core::passFilter, Shader::FILTER_UPSCALE, false, false);
            Core::active.shader->setParam(uParam, vec4(float(scaleTex->width), float(scaleTex->height), 0.0f, 0.0f));
            scaleTex->bind(sDiffuse);
            Core::setDepthTest(false);
            mesh->renderQuad();
            Core::setDepthTest(true);
            Core::setDepthWrite(true);
        }

        // TODO render all UI with native resolution here
    }

    void renderUI() {
        if (inventory->titleTimer > 1.0f || level.isTitle()) return;

        #ifdef _GAPI_SW
            GAPI::setPalette(GAPI::swPaletteColor);
            GAPI::setShading(false);
        #endif

        Core::pushLights();

        UI::begin(camera->aspect);

        bindAtlas(atlasObjects);
        UI::renderPickups();

        atlasGlyphs->bind(sDiffuse);

        Core::resetLights();

        if (!level.isCutsceneLevel() && !camera->spectator) {
        // render health & oxygen bars
            vec2 size = vec2(180, 10);

            float health = player->health / float(LARA_MAX_HEALTH);
            float oxygen = player->oxygen / float(LARA_MAX_OXYGEN);

            if ((params->time - int(params->time)) < 0.5f) { // blinking
                if (health <= 0.2f) health = 0.0f;
                if (oxygen <= 0.2f) oxygen = 0.0f;
            }

            vec2 pos;
            if (Core::settings.detail.stereo == Core::Settings::STEREO_VR)
                pos = vec2((UI::width - size.x) * 0.5f, 96);
            else
                pos = vec2(UI::width - 32 - size.x, 32);

            if (!player->dozy && (player->stand == Lara::STAND_ONWATER || player->stand == Character::STAND_UNDERWATER)) {
                UI::renderBar(CTEX_OXYGEN, pos, size, oxygen);
                pos.y += 16.0f;
            }

            if ((!inventory->active && ((player->wpnReady() && !player->emptyHands()) || player->damageTime > 0.0f || health <= 0.2f))) {
                UI::renderBar(CTEX_HEALTH, pos, size, health);
                pos.y += 32.0f;

                if (!inventory->active && !player->emptyHands()) { // ammo
                    int index = inventory->contains(player->wpnCurrent);
                    if (index > -1)
                        inventory->renderItemCount(inventory->items[index], pos, size.x);
                }
            }

            UI::renderHelp();
        }

        if (!camera->spectator) {
            UI::renderSubs();
        }

        UI::end();

        Core::popLights();
    }

    void renderInventoryEye(int eye, int view) {
        short4 oldViewport = Core::viewportDef;

        setDefaultTarget(eye, view, false);
	#ifdef FFP //fixme: psp framebuffer error?
		Core::setTarget(NULL, NULL, RT_CLEAR_DEPTH | RT_CLEAR_COLOR | RT_STORE_COLOR);
	#else
		Core::setTarget(NULL, NULL, RT_CLEAR_DEPTH | RT_STORE_COLOR);
	#endif

        float aspect = setViewport(view, eye);

        Core::pushLights();
        Core::resetLights();

        if (inventory->video) {
            inventory->render(1.0);

            if (UI::subsStr != STR_EMPTY) {
                UI::begin(float(Core::width) / float(Core::height));
                atlasGlyphs->bind(sDiffuse);
                UI::renderSubs();
                UI::end();
            }
        }

        if (!inventory->video) {
            inventory->renderBackground(max(0, view));
        }

        setupBinding();
        bindAtlas(atlasObjects);
        inventory->render(aspect);

        UI::begin(aspect);
        atlasGlyphs->bind(sDiffuse);
        if (!inventory->video) {
            inventory->renderUI();
        } else {
            UI::renderSubs();
        }
        UI::end();

        Core::popLights();

        Core::viewportDef = oldViewport;
    }

    void render() {
        if (isEnded && !inventory->video) {
            Core::setTarget(NULL, NULL, RT_CLEAR_COLOR | RT_STORE_COLOR);
            UI::begin(float(Core::width) / float(Core::height));
            atlasGlyphs->bind(sDiffuse);
            UI::textOut(vec2(0, 480 - 16), STR_LOADING, UI::aCenter, UI::width);
            UI::end();
            return;
        }

        renderGame(true, false);
    }

};

#endif
