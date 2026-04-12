#pragma once
#include "config.h"
#include <stdint.h>

// ----------------------------------------------------------------
//  Goblin ambush system
//  Goblins spawn at the right or top/bottom edge, march toward
//  the nearest dwarf, and attack on contact.
// ----------------------------------------------------------------

#define MAX_GOBLINS              12   // increased for larger late-game waves
#define GOBLIN_AMBUSH_INTERVAL  1200  // initial ticks between ambush waves
#define GOBLIN_AMBUSH_MIN_INTERVAL 300 // floor for scaling interval
#define GOBLIN_AMBUSH_START     1500  // first ambush not before this tick
#define GOBLIN_WAVE_MIN           1
#define GOBLIN_WAVE_MAX           3
#define GOBLIN_SCALE_SEASON       5   // every N seasons with barracks: +1 goblin, -100 ticks interval
#define GOBLIN_MOVE_INTERVAL      2   // ticks between goblin moves
#define GOBLIN_ATTACK_DAMAGE     30   // hunger+thirst spike on hit (simulates injury)
#define GOBLIN_DESPAWN_TICKS    200   // ticks before a goblin gives up and leaves

struct Goblin {
    int8_t  x, y;
    int8_t  lastX, lastY;
    uint8_t moveTimer;
    uint8_t despawnTimer;
    bool    active;
};

extern Goblin gGoblins[MAX_GOBLINS];
extern int    gNumGoblins;

void goblinsInit();
void goblinsTick();
