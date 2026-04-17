#pragma once
#include "config.h"
#include <stdint.h>

// ----------------------------------------------------------------
//  Goblin ambush system
//  Goblins spawn at the right or top/bottom edge, march toward
//  the nearest dwarf, and attack on contact.
// ----------------------------------------------------------------

#define MAX_GOBLINS              16   // increased for larger late-game waves
#define GOBLIN_AMBUSH_INTERVAL   700  // initial ticks between ambush waves
#define GOBLIN_AMBUSH_MIN_INTERVAL 250 // floor for scaling interval
#define GOBLIN_AMBUSH_START     1200  // first ambush not before this tick
#define GOBLIN_WAVE_MIN           1
#define GOBLIN_WAVE_MAX           4
#define GOBLIN_SCALE_SEASON       2   // every N seasons with barracks: +1 goblin, -100 ticks interval
#define GOBLIN_MOVE_INTERVAL      2   // ticks between goblin moves
#define GOBLIN_ATTACK_DAMAGE     30   // hunger+thirst spike on hit (simulates injury)
#define GOBLIN_DESPAWN_TICKS    200   // ticks before a goblin gives up and leaves
#define GOBLIN_MAX_PATH          80   // max path length for goblin BFS
#define GOBLIN_REPATH_INTERVAL   12   // recompute path every N moves

struct Goblin {
    int8_t  x, y;
    int8_t  lastX, lastY;
    uint8_t moveTimer;
    uint8_t despawnTimer;
    bool    active;
    int8_t  pathX[GOBLIN_MAX_PATH];
    int8_t  pathY[GOBLIN_MAX_PATH];
    uint8_t pathLen;
    uint8_t pathPos;
    uint8_t repathCounter;  // counts down; recomputes path at 0
};

extern Goblin gGoblins[MAX_GOBLINS];
extern int    gNumGoblins;

void goblinsInit();
void goblinsTick();

// Save/load support — exposes internal scaling state
struct GoblinScaleState {
    uint32_t nextAmbush;
    uint32_t ambushInterval;
    int32_t  waveBonus;
    uint32_t lastScaleSeason;
};
void goblinsGetScaleState(GoblinScaleState* out);
void goblinsSetScaleState(const GoblinScaleState* in);
