#include "dwarves.h"
#include "tasks.h"
#include "map.h"
#include "pathfind.h"
#include "fortplan.h"
#include "config.h"
#include <Arduino.h>
#include <string.h>

Dwarf gDwarves[MAX_DWARVES];
int   gNumDwarves = 0;
int   gFoodSupply  = START_FOOD;
int   gDrinkSupply = START_DRINK;

static const char* kNames[10] = {
    "Urist","Bomrek","Meng","Datan","Sibrek",
    "Fikod","Stukos","Udib","Rigoth","Onget"
};

// ----------------------------------------------------------------
void dwarfInit(int count, int startX, int startY) {
    gNumDwarves = min(count, MAX_DWARVES);
    for (int i = 0; i < gNumDwarves; i++) {
        Dwarf& d = gDwarves[i];
        memset(&d, 0, sizeof(d));
        d.x      = (int8_t)startX;
        d.y      = (int8_t)(startY - gNumDwarves/2 + i);
        d.lastX  = d.x;
        d.lastY  = d.y;
        d.state  = DS_IDLE;
        d.taskIdx = -1;
        d.hunger  = random(10, 30);
        d.thirst  = random(10, 30);
        d.fatigue = random(5, 20);
        d.active  = true;
        d.dead    = false;
        d.carrying = ITEM_NONE;
        d.placeFurn = false;
        strncpy(d.name, kNames[i % 10], 9);
        mapMarkDirty(d.x, d.y);
    }
}

// ----------------------------------------------------------------
//  Helpers
// ----------------------------------------------------------------
static bool dwarfAt(int x, int y, int exceptIdx = -1) {
    for (int i = 0; i < gNumDwarves; i++) {
        if (i == exceptIdx) continue;
        if (gDwarves[i].active && !gDwarves[i].dead
            && gDwarves[i].x == x && gDwarves[i].y == y) return true;
    }
    return false;
}

static void dwarfMove(int idx) {
    Dwarf& d = gDwarves[idx];
    if (d.pathPos >= d.pathLen) return;

    int nx = d.pathX[d.pathPos];
    int ny = d.pathY[d.pathPos];

    if (!mapPassable(nx, ny)) {
        d.pathLen = 0; d.pathPos = 0;
        d.state = DS_IDLE;
        if (d.taskIdx >= 0) { taskUnclaim(d.taskIdx); d.taskIdx = -1; }
        return;
    }
    if (dwarfAt(nx, ny, idx)) return; // blocked — wait this tick

    mapMarkDirty(d.x, d.y);
    d.lastX = d.x; d.lastY = d.y;
    d.x = (int8_t)nx; d.y = (int8_t)ny;
    mapMarkDirty(d.x, d.y);
    d.pathPos++;
}

// Does the dwarf have a bed nearby (in the bedroom zone)?
static bool nearBed(const Dwarf& d) {
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            if (mapGet(d.x+dx, d.y+dy) == TILE_BED) return true;
    return false;
}

// ----------------------------------------------------------------
//  Per-dwarf tick
// ----------------------------------------------------------------
static void tickDwarf(int idx) {
    Dwarf& d = gDwarves[idx];

    // ---- Accumulate needs ----
    d.hunger  = min(100, d.hunger  + HUNGER_RATE);
    d.thirst  = min(100, d.thirst  + THIRST_RATE);
    d.fatigue = min(100, d.fatigue + FATIGUE_RATE);

    // ---- Starvation / dehydration counters ----
    if (d.hunger  >= 100) d.starveCount++;  else d.starveCount = 0;
    if (d.thirst  >= 100) d.dehydCount++;   else d.dehydCount  = 0;

    // ---- Death check ----
    if (d.starveCount >= STARVE_TICKS || d.dehydCount >= DEHYDRATE_TICKS) {
        // Dwarf dies
        d.state = DS_DEAD;
        d.dead  = true;
        d.active = false;
        if (d.taskIdx >= 0) { taskUnclaim(d.taskIdx); d.taskIdx = -1; }
        // Drop anything carried
        if (d.carrying != ITEM_NONE) {
            mapAddItem(d.x, d.y, d.carrying);
            d.carrying = ITEM_NONE;
        }
        // Leave corpse and notify fort planner to begin burial
        mapAddItem(d.x, d.y, ITEM_CORPSE);
        mapMarkDirty(d.x, d.y);
        fortNotifyDeath(d.x, d.y);
        return;
    }

    // ---- Critical needs override current task ----
    if (d.state != DS_EATING && d.state != DS_DRINKING && d.state != DS_SLEEPING) {
        if (d.thirst >= THIRST_THRESH && gDrinkSupply > 0) {
            if (d.taskIdx >= 0) { taskUnclaim(d.taskIdx); d.taskIdx = -1; }
            d.state    = DS_DRINKING;
            d.workLeft = DRINK_TICKS;
            d.pathLen  = 0; d.pathPos = 0;
            return;
        }
        if (d.hunger >= HUNGER_THRESH && gFoodSupply > 0) {
            if (d.taskIdx >= 0) { taskUnclaim(d.taskIdx); d.taskIdx = -1; }
            d.state    = DS_EATING;
            d.workLeft = EAT_TICKS;
            d.pathLen  = 0; d.pathPos = 0;
            return;
        }
        if (d.fatigue >= FATIGUE_THRESH) {
            if (d.taskIdx >= 0) { taskUnclaim(d.taskIdx); d.taskIdx = -1; }
            d.state    = DS_SLEEPING;
            d.workLeft = SLEEP_TICKS;
            d.pathLen  = 0; d.pathPos = 0;
            return;
        }
    }

    // ---- State machine ----
    switch (d.state) {

    // ---- IDLE ----
    case DS_IDLE: {
        d.idleTicks++;
        int ti = taskFindBest(d.x, d.y);
        if (ti >= 0) {
            d.idleTicks = 0;
            gTasks[ti].claimed   = true;
            gTasks[ti].claimedBy = (int8_t)idx;
            d.taskIdx  = ti;
            d.workLeft = gTasks[ti].workNeeded;
            d.placeFurn = false;

            Task& t = gTasks[ti];
            int plen = 0;

            if (t.type == TASK_DIG || t.type == TASK_CHOP) {
                plen = pathFindAdj(d.x, d.y, t.x, t.y, d.pathX, d.pathY, MAX_PATH_LEN);
            } else if (t.type == TASK_PLACE_FURN) {
                // Walk to the stockpile centre — will search for item on arrival
                int sx = (gStockX1+gStockX2)/2, sy = (gStockY1+gStockY2)/2;
                plen = pathFind(d.x, d.y, sx, sy, d.pathX, d.pathY, MAX_PATH_LEN);
                if (plen == 0) plen = 1; // already nearby
            } else {
                plen = pathFind(d.x, d.y, t.x, t.y, d.pathX, d.pathY, MAX_PATH_LEN);
            }

            if (plen == 0) {
                bool inPos = (d.x == t.x && d.y == t.y);
                if (!inPos && (t.type == TASK_DIG || t.type == TASK_CHOP)) {
                    // Also in-position if already adjacent to the target
                    const int8_t adx[4]={0,0,1,-1}, ady[4]={1,-1,0,0};
                    for (int di = 0; di < 4 && !inPos; di++)
                        if (d.x == t.x+adx[di] && d.y == t.y+ady[di]) inPos = true;
                }
                if (!inPos) { taskUnclaim(ti); d.taskIdx=-1; break; }
            }
            d.pathLen = (uint8_t)plen;
            d.pathPos = 0;
            d.state   = DS_MOVING;
            break;
        }

        // No work — wander
        if (d.idleTicks >= 4) {
            d.idleTicks = 0;
            int bestX = -1, bestY = -1, bestDist = 0;
            for (int attempt = 0; attempt < 20; attempt++) {
                int wx = random(0, MAP_W), wy = random(0, MAP_H);
                if (!mapPassable(wx,wy) || !mapIsRevealed(wx,wy)) continue;
                if (wx == d.x && wy == d.y) continue;
                int dist = abs(wx-d.x)+abs(wy-d.y);
                if (dist > bestDist) { bestDist=dist; bestX=wx; bestY=wy; }
            }
            if (bestX >= 0) {
                int plen = pathFind(d.x, d.y, bestX, bestY, d.pathX, d.pathY, MAX_PATH_LEN);
                if (plen > 0) { d.pathLen=(uint8_t)plen; d.pathPos=0; d.state=DS_WANDER; }
            }
        }
        break;
    }

    // ---- WANDER ----
    case DS_WANDER: {
        if (taskFindBest(d.x, d.y) >= 0) {
            d.pathLen=0; d.pathPos=0; d.state=DS_IDLE; return;
        }
        if (d.pathPos >= d.pathLen) { d.state=DS_IDLE; return; }
        dwarfMove(idx);
        break;
    }

    // ---- MOVING ----
    case DS_MOVING: {
        if (d.taskIdx < 0) { d.state=DS_IDLE; break; }

        if (d.pathPos >= d.pathLen) {
            d.state    = DS_WORKING;
            d.workLeft = gTasks[d.taskIdx].workNeeded;
            break;
        }
        dwarfMove(idx);
        break;
    }

    // ---- WORKING ----
    case DS_WORKING: {
        if (d.taskIdx < 0) { d.state=DS_IDLE; break; }
        Task& t = gTasks[d.taskIdx];

        if (d.workLeft > 0) { d.workLeft--; break; }

        // --- DIG ---
        if (t.type == TASK_DIG) {
            mapSet(t.x, t.y, TILE_FLOOR);
            mapDesignate(t.x, t.y, false);
            mapAddItem(t.x, t.y, ITEM_STONE);
            int sx, sy;
            if (stockpileFindSlot(&sx, &sy)) taskAdd(TASK_HAUL, t.x, t.y, sx, sy);

        // --- CHOP ---
        } else if (t.type == TASK_CHOP) {
            mapSet(t.x, t.y, TILE_GRASS);
            mapAddItem(t.x, t.y, ITEM_WOOD);
            int sx, sy;
            if (stockpileFindSlot(&sx, &sy)) taskAdd(TASK_HAUL, t.x, t.y, sx, sy);

        // --- HAUL (phase 1: pick up) ---
        } else if (t.type == TASK_HAUL) {
            if (d.carrying == ITEM_NONE) {
                ItemType it = gMap[t.y][t.x].item;
                if (it != ITEM_NONE && mapRemoveItem(t.x, t.y, it)) {
                    d.carrying  = it;
                    d.placeFurn = false;
                    int plen = pathFind(d.x, d.y, t.destX, t.destY,
                                        d.pathX, d.pathY, MAX_PATH_LEN);
                    d.pathLen=(uint8_t)plen; d.pathPos=0;
                    d.state = DS_HAULING;
                    break;
                } else {
                    taskComplete(d.taskIdx); d.taskIdx=-1; d.state=DS_IDLE; break;
                }
            }

        // --- CRAFT ---
        } else if (t.type == TASK_CRAFT) {
            CraftType ct   = (CraftType)t.auxType;
            int mushCost   = craftMushroomCost(ct);
            int woodCost   = craftWoodCost(ct);
            if (mushCost > 0) {
                // Mushroom processing (Kitchen / Still)
                if (!mapConsumeFromStockpile(ITEM_MUSHROOM, mushCost)) {
                    taskUnclaim(d.taskIdx); d.taskIdx=-1; d.state=DS_IDLE; break;
                }
                if (ct == CRAFT_MUSHROOM_FOOD)
                    gFoodSupply = min(gFoodSupply + 3, (int)MAX_FOOD_SUPPLY);
                else
                    gDrinkSupply = min(gDrinkSupply + 3, (int)MAX_DRINK_SUPPLY);
            } else {
                if (!mapConsumeFromStockpile(ITEM_WOOD, woodCost)) {
                    // No materials yet — unclaim and wait for wood
                    taskUnclaim(d.taskIdx); d.taskIdx=-1; d.state=DS_IDLE; break;
                }
                ItemType product = craftProduct(ct);
                mapAddItem(d.x, d.y, product);
                int sx, sy;
                if (stockpileFindSlot(&sx, &sy)) taskAdd(TASK_HAUL, d.x, d.y, sx, sy);
            }

        // --- PLACE_FURN (phase 1: find and pick up item in stockpile) ---
        } else if (t.type == TASK_PLACE_FURN) {
            ItemType need = (ItemType)t.auxType;
            // Search stockpile zone for the item
            int ix = -1, iy = -1;
            for (int sy2 = gStockY1; sy2 <= gStockY2 && ix < 0; sy2++) {
                for (int sx2 = gStockX1; sx2 <= gStockX2 && ix < 0; sx2++) {
                    if (mapInBounds(sx2,sy2) && gMap[sy2][sx2].item == need) {
                        ix = sx2; iy = sy2;
                    }
                }
            }
            // Also search globally if not in main stockpile
            if (ix < 0) {
                if (!mapFindItem(d.x, d.y, need, &ix, &iy)) {
                    taskUnclaim(d.taskIdx); d.taskIdx=-1; d.state=DS_IDLE; break;
                }
            }
            if (mapRemoveItem(ix, iy, need)) {
                d.carrying  = need;
                d.placeFurn = true;
                int plen = pathFind(d.x, d.y, t.destX, t.destY,
                                    d.pathX, d.pathY, MAX_PATH_LEN);
                d.pathLen=(uint8_t)plen; d.pathPos=0;
                d.state = DS_HAULING;
                break;
            }
            taskUnclaim(d.taskIdx); d.taskIdx=-1; d.state=DS_IDLE; break;

        // --- BURY (phase 1: pick up corpse) ---
        } else if (t.type == TASK_BURY) {
            // Consume a coffin from stockpile
            if (!mapConsumeFromStockpile(ITEM_COFFIN, 1)) {
                taskUnclaim(d.taskIdx); d.taskIdx=-1; d.state=DS_IDLE; break;
            }
            // Pick up corpse
            if (mapRemoveItem(t.x, t.y, ITEM_CORPSE)) {
                d.carrying  = ITEM_CORPSE;
                d.placeFurn = true; // will place as TILE_COFFIN_T at dest
                int plen = pathFind(d.x, d.y, t.destX, t.destY,
                                    d.pathX, d.pathY, MAX_PATH_LEN);
                d.pathLen=(uint8_t)plen; d.pathPos=0;
                d.state = DS_HAULING;
                break;
            }
            taskUnclaim(d.taskIdx); d.taskIdx=-1; d.state=DS_IDLE; break;
        }

        taskComplete(d.taskIdx);
        d.taskIdx  = -1;
        d.state    = DS_IDLE;
        break;
    }

    // ---- HAULING ----
    case DS_HAULING: {
        if (d.pathPos < d.pathLen) { dwarfMove(idx); break; }
        // Arrived at destination
        if (d.carrying != ITEM_NONE) {
            if (d.placeFurn && isFurnitureItem(d.carrying)) {
                // Place as tile
                TileType ft = furnitureItemToTile(d.carrying);
                mapSet(d.x, d.y, ft);
            } else if (d.placeFurn && d.carrying == ITEM_CORPSE) {
                // Lay to rest
                mapSet(d.x, d.y, TILE_COFFIN_T);
                gMap[d.y][d.x].roomType = ROOM_TOMB;
            } else {
                mapAddItem(d.x, d.y, d.carrying);
            }
            d.carrying  = ITEM_NONE;
            d.placeFurn = false;
        }
        if (d.taskIdx >= 0) { taskComplete(d.taskIdx); d.taskIdx=-1; }
        d.state = DS_IDLE;
        break;
    }

    // ---- EATING ----
    case DS_EATING: {
        if (d.workLeft > 0) { d.workLeft--; break; }
        if (gFoodSupply > 0) {
            gFoodSupply--;
            d.hunger = (uint8_t)max(0, (int)d.hunger - EAT_RESTORE);
        }
        d.state = DS_IDLE;
        break;
    }

    // ---- DRINKING ----
    case DS_DRINKING: {
        if (d.workLeft > 0) { d.workLeft--; break; }
        if (gDrinkSupply > 0) {
            gDrinkSupply--;
            d.thirst = (uint8_t)max(0, (int)d.thirst - DRINK_RESTORE);
        }
        d.state = DS_IDLE;
        break;
    }

    // ---- SLEEPING ----
    case DS_SLEEPING: {
        int restore = nearBed(d) ? SLEEP_RESTORE * 2 : SLEEP_RESTORE;
        if (d.workLeft > 0) {
            d.workLeft--;
            d.fatigue = (uint8_t)max(0, (int)d.fatigue - restore);
            break;
        }
        d.state = DS_IDLE;
        break;
    }

    default:
        d.state = DS_IDLE;
        break;
    }
}

// ----------------------------------------------------------------
void dwarvesTick() {
    for (int i = 0; i < gNumDwarves; i++) {
        if (gDwarves[i].active && !gDwarves[i].dead) tickDwarf(i);
    }
}
