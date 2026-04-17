#include "save.h"
#include "map.h"
#include "tasks.h"
#include "dwarves.h"
#include "fortplan.h"
#include "animals.h"
#include "goblins.h"
#include "config.h"
#include <string.h>

#ifdef ARDUINO
#  include <SD.h>
#  include <Arduino.h>
#  define SD_CS_PIN  5
#  define SAVE_PATH  "/save.dat"
#else
#  include <stdio.h>
#  define SAVE_PATH  "DwarfortSim.sav"
#endif

static const uint32_t SAVE_MAGIC   = 0x44574654; // 'DWFT'
static const uint8_t  SAVE_VERSION = 1;

// ----------------------------------------------------------------
//  Compact dwarf — strips the two 200-byte path arrays
// ----------------------------------------------------------------
struct SaveDwarf {
    int8_t  x, y, lastX, lastY;
    uint8_t hunger, thirst, fatigue;
    uint8_t workLeft, idleTicks;
    uint8_t starveCount, dehydCount;
    uint8_t carrying;
    uint8_t placeFurn;
    uint8_t combatSkill;
    uint8_t happiness;
    uint8_t active, dead;
    uint8_t hasAxe, hasArmor;
    char    name[10];
};

// ----------------------------------------------------------------
//  Global snapshot
// ----------------------------------------------------------------
struct SaveGlobals {
    uint32_t tick;
    int32_t  foodSupply;
    int32_t  drinkSupply;
    int32_t  beerSupply;
    uint8_t  fortStage;
    uint8_t  season;
    uint32_t doneTick;
    int32_t  deadUnburied;
    int32_t  tombSlotUsed;
    uint8_t  tombDug;
    uint8_t  fortFallen;
    char     fortFallReason[48];
    char     stageName[24];
    int32_t  stockX1, stockY1, stockX2, stockY2;
};

// ----------------------------------------------------------------
//  Platform I/O wrappers
// ----------------------------------------------------------------
#ifdef ARDUINO

static File sSaveFile;

static bool sOpen(bool write) {
    if (write) {
        if (SD.exists(SAVE_PATH)) SD.remove(SAVE_PATH);
        sSaveFile = SD.open(SAVE_PATH, FILE_WRITE);
    } else {
        sSaveFile = SD.open(SAVE_PATH, FILE_READ);
    }
    return (bool)sSaveFile;
}
static void sClose() { sSaveFile.close(); }
static bool sWrite(const void* buf, size_t len) {
    return sSaveFile.write((const uint8_t*)buf, len) == len;
}
static bool sRead(void* buf, size_t len) {
    return (size_t)sSaveFile.read((uint8_t*)buf, len) == len;
}

#else

static FILE* sSaveFile = nullptr;

static bool sOpen(bool write) {
    sSaveFile = fopen(SAVE_PATH, write ? "wb" : "rb");
    return sSaveFile != nullptr;
}
static void sClose() { if (sSaveFile) { fclose(sSaveFile); sSaveFile = nullptr; } }
static bool sWrite(const void* buf, size_t len) {
    return fwrite(buf, 1, len, sSaveFile) == len;
}
static bool sRead(void* buf, size_t len) {
    return fread(buf, 1, len, sSaveFile) == len;
}

#endif

// ----------------------------------------------------------------
void saveInit() {
#ifdef ARDUINO
    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("[save] SD init failed — saves disabled");
    } else {
        Serial.println("[save] SD ready");
    }
#endif
}

// ----------------------------------------------------------------
bool hasSave() {
#ifdef ARDUINO
    return SD.exists(SAVE_PATH);
#else
    FILE* f = fopen(SAVE_PATH, "rb");
    if (!f) return false;
    fclose(f);
    return true;
#endif
}

// ----------------------------------------------------------------
void deleteSave() {
#ifdef ARDUINO
    if (SD.exists(SAVE_PATH)) SD.remove(SAVE_PATH);
#else
    remove(SAVE_PATH);
#endif
}

// ----------------------------------------------------------------
bool saveGame() {
    if (!sOpen(true)) {
#ifdef ARDUINO
        Serial.println("[save] open for write failed");
#endif
        return false;
    }

#define SW(x) if (!sWrite(&(x), sizeof(x))) { sClose(); return false; }

    SW(SAVE_MAGIC);
    SW(SAVE_VERSION);

    // Globals
    SaveGlobals g;
    g.tick         = gTick;
    g.foodSupply   = (int32_t)gFoodSupply;
    g.drinkSupply  = (int32_t)gDrinkSupply;
    g.beerSupply   = (int32_t)gBeerSupply;
    g.fortStage    = (uint8_t)gFortStage;
    g.season       = gSeason;
    g.doneTick     = gDoneTick;
    g.deadUnburied = (int32_t)gDeadUnburied;
    g.tombSlotUsed = (int32_t)gTombSlotUsed;
    g.tombDug      = gTombDug   ? 1 : 0;
    g.fortFallen   = gFortFallen ? 1 : 0;
    memcpy(g.fortFallReason, gFortFallReason, 48);
    memcpy(g.stageName,      gStageName,      24);
    g.stockX1 = (int32_t)gStockX1; g.stockY1 = (int32_t)gStockY1;
    g.stockX2 = (int32_t)gStockX2; g.stockY2 = (int32_t)gStockY2;
    SW(g);

    // Map tiles
    if (!sWrite(gMap, sizeof(gMap))) { sClose(); return false; }

    // Tasks
    int16_t tc = (int16_t)gTaskCount;
    SW(tc);
    if (gTaskCount > 0)
        if (!sWrite(gTasks, sizeof(Task) * (size_t)gTaskCount)) { sClose(); return false; }

    // Dwarves (compact)
    int8_t nd = (int8_t)gNumDwarves;
    SW(nd);
    for (int i = 0; i < gNumDwarves; i++) {
        const Dwarf& d = gDwarves[i];
        SaveDwarf sd;
        sd.x = d.x; sd.y = d.y; sd.lastX = d.lastX; sd.lastY = d.lastY;
        sd.hunger      = d.hunger;      sd.thirst     = d.thirst;
        sd.fatigue     = d.fatigue;     sd.workLeft   = d.workLeft;
        sd.idleTicks   = d.idleTicks;   sd.starveCount = d.starveCount;
        sd.dehydCount  = d.dehydCount;
        sd.carrying    = (uint8_t)d.carrying;
        sd.placeFurn   = d.placeFurn ? 1 : 0;
        sd.combatSkill = d.combatSkill;
        sd.happiness   = d.happiness;
        sd.active      = d.active ? 1 : 0;
        sd.dead        = d.dead   ? 1 : 0;
        sd.hasAxe      = d.hasAxe   ? 1 : 0;
        sd.hasArmor    = d.hasArmor ? 1 : 0;
        memcpy(sd.name, d.name, 10);
        SW(sd);
    }

    // Animals
    int8_t na = (int8_t)gNumAnimals;
    SW(na);
    if (gNumAnimals > 0)
        if (!sWrite(gAnimals, sizeof(Animal) * (size_t)gNumAnimals)) { sClose(); return false; }

    // Goblins + scaling state
    int8_t ng = (int8_t)gNumGoblins;
    SW(ng);
    if (gNumGoblins > 0)
        if (!sWrite(gGoblins, sizeof(Goblin) * (size_t)gNumGoblins)) { sClose(); return false; }
    GoblinScaleState gs;
    goblinsGetScaleState(&gs);
    SW(gs);

    // Tail magic — detects truncated writes
    SW(SAVE_MAGIC);

#undef SW

    sClose();
#ifdef ARDUINO
    Serial.println("[save] saved");
#endif
    return true;
}

// ----------------------------------------------------------------
bool loadGame() {
    if (!sOpen(false)) return false;

#define SR(x) if (!sRead(&(x), sizeof(x))) { sClose(); return false; }

    uint32_t magic   = 0;
    uint8_t  version = 0;
    SR(magic);   if (magic   != SAVE_MAGIC)   { sClose(); return false; }
    SR(version); if (version != SAVE_VERSION) { sClose(); return false; }

    // Globals
    SaveGlobals g;
    SR(g);
    gTick         = g.tick;
    gFoodSupply   = (int)g.foodSupply;
    gDrinkSupply  = (int)g.drinkSupply;
    gBeerSupply   = (int)g.beerSupply;
    gFortStage    = (FortStage)g.fortStage;
    gSeason       = g.season;
    gDoneTick     = g.doneTick;
    gDeadUnburied = (int)g.deadUnburied;
    gTombSlotUsed = (int)g.tombSlotUsed;
    gTombDug      = g.tombDug   != 0;
    gFortFallen   = g.fortFallen != 0;
    memcpy(gFortFallReason, g.fortFallReason, 48);
    memcpy(gStageName,      g.stageName,      24);
    gStockX1 = (int)g.stockX1; gStockY1 = (int)g.stockY1;
    gStockX2 = (int)g.stockX2; gStockY2 = (int)g.stockY2;

    // Map tiles
    if (!sRead(gMap, sizeof(gMap))) { sClose(); return false; }

    // Tasks
    int16_t tc = 0;
    SR(tc);
    gTaskCount = (int)tc;
    if (gTaskCount > 0)
        if (!sRead(gTasks, sizeof(Task) * (size_t)gTaskCount)) { sClose(); return false; }
    // Clear claimed flags so dwarves re-claim cleanly on first tick
    for (int i = 0; i < gTaskCount; i++) {
        gTasks[i].claimed   = false;
        gTasks[i].claimedBy = -1;
    }

    // Dwarves
    int8_t nd = 0;
    SR(nd);
    gNumDwarves = (int)nd;
    for (int i = 0; i < gNumDwarves; i++) {
        SaveDwarf sd;
        SR(sd);
        Dwarf& d = gDwarves[i];
        memset(&d, 0, sizeof(d));
        d.x = sd.x; d.y = sd.y; d.lastX = sd.lastX; d.lastY = sd.lastY;
        d.state        = DS_IDLE;  // re-path on first tick
        d.taskIdx      = -1;
        d.hunger       = sd.hunger;      d.thirst     = sd.thirst;
        d.fatigue      = sd.fatigue;
        d.workLeft     = 0;
        d.idleTicks    = 0;
        d.starveCount  = sd.starveCount; d.dehydCount = sd.dehydCount;
        d.carrying     = (ItemType)sd.carrying;
        d.placeFurn    = sd.placeFurn != 0;
        d.combatSkill  = sd.combatSkill;
        d.happiness    = sd.happiness;
        d.pathLen      = 0; d.pathPos = 0; d.blockedCount = 0;
        d.active       = sd.active  != 0;
        d.dead         = sd.dead    != 0;
        d.hasAxe       = sd.hasAxe  != 0;
        d.hasArmor     = sd.hasArmor != 0;
        memcpy(d.name, sd.name, 10);
    }

    // Animals
    int8_t na = 0;
    SR(na);
    gNumAnimals = (int)na;
    if (gNumAnimals > 0)
        if (!sRead(gAnimals, sizeof(Animal) * (size_t)gNumAnimals)) { sClose(); return false; }

    // Goblins + scaling state
    int8_t ng = 0;
    SR(ng);
    gNumGoblins = (int)ng;
    if (gNumGoblins > 0)
        if (!sRead(gGoblins, sizeof(Goblin) * (size_t)gNumGoblins)) { sClose(); return false; }
    GoblinScaleState gs;
    SR(gs);
    goblinsSetScaleState(&gs);

    // Verify tail magic
    uint32_t tail = 0;
    SR(tail);
    if (tail != SAVE_MAGIC) { sClose(); return false; }

#undef SR

    sClose();
#ifdef ARDUINO
    Serial.println("[save] loaded");
#endif
    return true;
}
