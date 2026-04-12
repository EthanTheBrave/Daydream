#include "goblins.h"
#include "map.h"
#include "tasks.h"
#include "dwarves.h"
#include "fortplan.h"
#include "config.h"
#include "renderer.h"
#include <Arduino.h>
#include <string.h>

Goblin gGoblins[MAX_GOBLINS];
int    gNumGoblins = 0;

// Scaling state
static uint32_t sNextAmbush    = GOBLIN_AMBUSH_START;
static uint32_t sAmbushInterval = GOBLIN_AMBUSH_INTERVAL;
static int      sWaveBonus      = 0;   // extra goblins per wave due to scaling
static uint32_t sLastScaleSeason = 0xFFFFFFFF;

// ----------------------------------------------------------------
static void spawnGoblinWave(int count) {
    for (int i = 0; i < count; i++) {
        // Find a free slot
        int slot = -1;
        for (int j = 0; j < MAX_GOBLINS; j++) {
            if (!gGoblins[j].active) { slot = j; break; }
        }
        if (slot < 0) return;

        // Spawn at right edge or top/bottom edge (outside the map view)
        int x, y;
        int edge = random(0, 3); // 0=right, 1=top, 2=bottom
        if (edge == 0) {
            x = MAP_W - 1;
            y = random(1, MAP_H - 1);
        } else if (edge == 1) {
            x = random(HILL_START_X, MAP_W - 1);
            y = 0;
        } else {
            x = random(HILL_START_X, MAP_W - 1);
            y = MAP_H - 1;
        }

        // Snap to a passable tile nearby
        bool placed = false;
        for (int tries = 0; tries < 20 && !placed; tries++) {
            int tx = x + random(-2, 3);
            int ty = y + random(-2, 3);
            if (mapInBounds(tx, ty) && mapPassable(tx, ty)) {
                x = tx; y = ty; placed = true;
            }
        }
        if (!placed) continue;

        Goblin& g = gGoblins[slot];
        g.x           = (int8_t)x;
        g.y           = (int8_t)y;
        g.lastX       = g.x;
        g.lastY       = g.y;
        g.moveTimer   = GOBLIN_MOVE_INTERVAL;
        g.despawnTimer = GOBLIN_DESPAWN_TICKS;
        g.active      = true;
        if (slot == gNumGoblins) gNumGoblins++;
        mapMarkDirty(x, y);

        Serial.println("Goblin ambush!");
        char buf[53];
        snprintf(buf, sizeof(buf), "Goblins have been spotted near the fortress!");
        tickerPush(buf);
    }
}

// ----------------------------------------------------------------
//  Find nearest alive dwarf, return index or -1
static int nearestDwarf(int gx, int gy) {
    int bestDist = 99999, bestIdx = -1;
    for (int i = 0; i < gNumDwarves; i++) {
        if (!gDwarves[i].active || gDwarves[i].dead) continue;
        int d = abs(gDwarves[i].x - gx) + abs(gDwarves[i].y - gy);
        if (d < bestDist) { bestDist = d; bestIdx = i; }
    }
    return bestIdx;
}

// ----------------------------------------------------------------
static void moveGoblin(int idx) {
    Goblin& g = gGoblins[idx];

    if (g.moveTimer > 0) { g.moveTimer--; return; }
    g.moveTimer = GOBLIN_MOVE_INTERVAL;

    g.despawnTimer--;
    if (g.despawnTimer == 0) {
        g.active = false; mapMarkDirty(g.x,g.y);
        if (random(0, 2) == 0) {
            mapAddItem(g.x, g.y, ITEM_BONE);
            int bsx, bsy;
            if (stockpileFindSlot(&bsx, &bsy)) taskAdd(TASK_HAUL, g.x, g.y, bsx, bsy);
        }
        return;
    }

    int ti = nearestDwarf(g.x, g.y);
    if (ti < 0) return; // no dwarves left

    Dwarf& target = gDwarves[ti];

    // Attack if adjacent
    int dist = abs(target.x - g.x) + abs(target.y - g.y);
    if (dist <= 1) {
        // Combat skill reduces damage (0 skill = full, max skill = 25% damage)
        int damage = GOBLIN_ATTACK_DAMAGE - (target.combatSkill * GOBLIN_ATTACK_DAMAGE / (COMBAT_SKILL_MAX * 4 / 3));
        if (damage < GOBLIN_ATTACK_DAMAGE / 4) damage = GOBLIN_ATTACK_DAMAGE / 4;
        // Injure the dwarf — spike hunger and thirst to simulate wounding
        target.hunger  = min(100, (int)target.hunger  + damage);
        target.thirst  = min(100, (int)target.thirst  + damage);
        // Also drain supply (goblin ransacks food)
        gFoodSupply  = max(0, gFoodSupply  - 3);
        gDrinkSupply = max(0, gDrinkSupply - 3);
        // Goblin despawns after a successful attack, dropping bones
        g.active = false;
        mapMarkDirty(g.x, g.y);
        if (random(0, 2) == 0) {
            mapAddItem(g.x, g.y, ITEM_BONE);
            int bsx, bsy;
            if (stockpileFindSlot(&bsx, &bsy)) taskAdd(TASK_HAUL, g.x, g.y, bsx, bsy);
        }

        // Ticker: report the attack
        {
            char buf[53];
            snprintf(buf, sizeof(buf), "%s was wounded by a goblin!", target.name);
            tickerPush(buf);
        }

        // Update fall reason if this kills the last dwarf
        int alive = 0;
        for (int j = 0; j < gNumDwarves; j++) if (!gDwarves[j].dead) alive++;
        if (alive <= 1 && !gFortFallen) {
            // Will be caught by dwarves.cpp death check, but pre-set the reason
            strncpy(gFortFallReason, "goblin attack", sizeof(gFortFallReason)-1);
        }
        return;
    }

    // Step toward target — greedy move
    int bestX = g.x, bestY = g.y, bestDist = dist;
    static const int8_t dx[4] = {1,-1,0,0};
    static const int8_t dy[4] = {0,0,1,-1};
    for (int d = 0; d < 4; d++) {
        int nx = g.x + dx[d];
        int ny = g.y + dy[d];
        if (!mapInBounds(nx, ny)) continue;
        if (!mapPassable(nx, ny)) continue;
        int nd = abs(target.x - nx) + abs(target.y - ny);
        if (nd < bestDist) { bestDist = nd; bestX = nx; bestY = ny; }
    }

    if (bestX != g.x || bestY != g.y) {
        mapMarkDirty(g.x, g.y);
        g.lastX = g.x; g.lastY = g.y;
        g.x = (int8_t)bestX; g.y = (int8_t)bestY;
        mapMarkDirty(g.x, g.y);
    }
}

// ----------------------------------------------------------------
void goblinsInit() {
    memset(gGoblins, 0, sizeof(gGoblins));
    gNumGoblins      = 0;
    sNextAmbush      = GOBLIN_AMBUSH_START;
    sAmbushInterval  = GOBLIN_AMBUSH_INTERVAL;
    sWaveBonus       = 0;
    sLastScaleSeason = 0xFFFFFFFF;
}

// ----------------------------------------------------------------
void goblinsTick() {
    if (gFortFallen) return;

    // Scale difficulty when barracks exists: every GOBLIN_SCALE_SEASON seasons,
    // reduce interval by 100 (floor GOBLIN_AMBUSH_MIN_INTERVAL) and +1 wave size.
    if (gFortStage >= FS_DONE) {
        // Check if a barracks has been built
        bool hasBarracks = false;
        for (int rx = 0; rx < MAP_W && !hasBarracks; rx++)
            for (int ry = 0; ry < MAP_H && !hasBarracks; ry++)
                if (gMap[ry][rx].roomType == ROOM_BARRACKS) hasBarracks = true;

        if (hasBarracks) {
            // Compute completed seasons since FS_DONE
            uint32_t seasons = (gTick >= gDoneTick)
                               ? (gTick - gDoneTick) / TICKS_PER_SEASON
                               : 0;
            uint32_t scaleLevel = seasons / GOBLIN_SCALE_SEASON;
            if (scaleLevel != sLastScaleSeason) {
                sLastScaleSeason = scaleLevel;
                uint32_t newInterval = GOBLIN_AMBUSH_INTERVAL
                                       - (uint32_t)(scaleLevel * 100);
                if (newInterval < GOBLIN_AMBUSH_MIN_INTERVAL || newInterval > GOBLIN_AMBUSH_INTERVAL)
                    newInterval = GOBLIN_AMBUSH_MIN_INTERVAL;
                sAmbushInterval = newInterval;
                sWaveBonus      = (int)scaleLevel;
            }
        }
    }

    // Fire ambush wave when timer expires
    if (gTick >= sNextAmbush) {
        int count = GOBLIN_WAVE_MIN + sWaveBonus
                    + random(0, GOBLIN_WAVE_MAX - GOBLIN_WAVE_MIN + 1);
        spawnGoblinWave(count);
        sNextAmbush = gTick + sAmbushInterval;
    }

    for (int i = 0; i < gNumGoblins; i++) {
        if (gGoblins[i].active) moveGoblin(i);
    }
}
