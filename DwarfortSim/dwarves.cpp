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

static const char* kNames[20] = {
    "Urist","Bomrek","Meng","Datan","Sibrek",
    "Fikod","Stukos","Udib","Rigoth","Onget",
    "Kadol","Vucar","Aban","Zulban","Momuz",
    "Likot","Erush","Nish","Ducim","Bembul"
};
static int gNameIdx = 0;  // cycles through names for new migrants

// ----------------------------------------------------------------
static void initOneDwarf(int idx, int spawnX, int spawnY) {
    Dwarf& d = gDwarves[idx];
    memset(&d, 0, sizeof(d));
    d.x      = (int8_t)spawnX;
    d.y      = (int8_t)spawnY;
    d.lastX  = d.x;
    d.lastY  = d.y;
    d.state  = DS_IDLE;
    d.taskIdx = -1;
    d.hunger  = random(10, 30);
    d.thirst  = random(10, 30);
    d.fatigue = random(5, 20);
    d.active  = true;
    d.dead    = false;
    d.carrying    = ITEM_NONE;
    d.placeFurn   = false;
    d.combatSkill = 0;
    strncpy(d.name, kNames[gNameIdx % 20], 9);
    gNameIdx++;
    mapMarkDirty(d.x, d.y);
}

void dwarfInit(int count, int startX, int startY) {
    gNumDwarves = min(count, MAX_DWARVES);
    gNameIdx    = 0;
    for (int i = 0; i < gNumDwarves; i++) {
        initOneDwarf(i, startX, startY - gNumDwarves/2 + i);
    }
}

// ----------------------------------------------------------------
//  Spawn a wave of migrants arriving from the left edge of the map
// ----------------------------------------------------------------
void dwarfSpawnMigrants(int count) {
    for (int i = 0; i < count; i++) {
        if (gNumDwarves >= MAX_DWARVES) break;

        // Find a free slot (may have dead dwarves in the array)
        int slot = -1;
        for (int j = 0; j < MAX_DWARVES; j++) {
            if (!gDwarves[j].active && !gDwarves[j].dead && j >= gNumDwarves) {
                slot = j; break;
            }
        }
        if (slot < 0) slot = gNumDwarves;
        if (slot >= MAX_DWARVES) break;

        // Spawn at left edge, random y on passable grass
        int sy = random(1, MAP_H - 1);
        for (int tries = 0; tries < 20; tries++) {
            int ty = random(1, MAP_H - 1);
            if (mapPassable(0, ty)) { sy = ty; break; }
        }
        initOneDwarf(slot, 0, sy);
        if (slot == gNumDwarves) gNumDwarves++;

        Serial.print("Migrant arrived: ");
        Serial.println(gDwarves[slot].name);
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
    if (dwarfAt(nx, ny, idx)) {
        // If blocker is sleeping, wake them so they move — breaks access deadlocks
        for (int j = 0; j < gNumDwarves; j++) {
            if (j == idx) continue;
            if (gDwarves[j].active && gDwarves[j].x == nx && gDwarves[j].y == ny
                    && gDwarves[j].state == DS_SLEEPING) {
                gDwarves[j].state   = DS_IDLE;
                gDwarves[j].workLeft = 0;
                break;
            }
        }
        // Blocked by another dwarf; unclaim after 20 consecutive blocked ticks
        if (++d.blockedCount >= 20) {
            d.blockedCount = 0;
            if (d.taskIdx >= 0) { taskUnclaim(d.taskIdx); d.taskIdx = -1; }
            d.pathLen = 0; d.pathPos = 0;
            d.state = DS_IDLE;
        }
        return;
    }
    d.blockedCount = 0;

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

    // ---- Happiness decay ----
    if (d.happiness > 0 && gTick % HAPPINESS_DECAY == 0) d.happiness--;

    // ---- Death check (happiness adds up to +3 tolerance) ----
    int happyBonus = d.happiness / (MAX_HAPPINESS / 3);  // 0, 1, 2, or 3
    if (d.starveCount >= STARVE_TICKS + happyBonus || d.dehydCount >= DEHYDRATE_TICKS + happyBonus) {
        const char* cause = (d.dehydCount >= DEHYDRATE_TICKS) ? "dehydration" : "starvation";
        printf("T=%u  %s died of %s  (H:%d T:%d F:%d  supply food=%d drink=%d)\n",
            (unsigned)gTick, d.name, cause,
            (int)d.hunger, (int)d.thirst, (int)d.fatigue,
            gFoodSupply, gDrinkSupply);
        d.state = DS_DEAD;
        d.dead  = true;
        d.active = false;
        if (d.taskIdx >= 0) { taskUnclaim(d.taskIdx); d.taskIdx = -1; }
        if (d.carrying != ITEM_NONE) {
            mapAddItem(d.x, d.y, d.carrying);
            d.carrying = ITEM_NONE;
        }
        mapAddItem(d.x, d.y, ITEM_CORPSE);
        mapMarkDirty(d.x, d.y);
        fortNotifyDeath(d.x, d.y);

        // Check if all dwarves are now dead
        bool anyAlive = false;
        for (int j = 0; j < gNumDwarves; j++)
            if (!gDwarves[j].dead) { anyAlive = true; break; }
        if (!anyAlive && !gFortFallen) {
            gFortFallen = true;
            strncpy(gFortFallReason, cause, sizeof(gFortFallReason) - 1);
        }
        return;
    }

    // ---- Helper: unclaim task and drop any craft material being carried ----
    #define UNCLAIM_TASK() do { \
        if (d.taskIdx >= 0) { \
            if (d.carrying != ITEM_NONE && gTasks[d.taskIdx].type == TASK_CRAFT) { \
                mapAddItem(d.x, d.y, d.carrying); \
                d.carrying = ITEM_NONE; \
                int _sx, _sy; \
                if (stockpileFindSlot(&_sx, &_sy)) taskAdd(TASK_HAUL, d.x, d.y, _sx, _sy); \
            } \
            taskUnclaim(d.taskIdx); d.taskIdx = -1; \
        } \
    } while(0)

    // ---- Critical needs override current task ----
    // DS_SLEEPING during movement phase must still respond to hunger/thirst
    if (d.state != DS_EATING && d.state != DS_DRINKING) {
        if (d.thirst >= THIRST_THRESH && gDrinkSupply > 0) {
            UNCLAIM_TASK();
            d.state    = DS_DRINKING;
            d.workLeft = DRINK_TICKS;
            d.pathLen  = 0; d.pathPos = 0;
            return;
        }
        if (d.hunger >= HUNGER_THRESH && gFoodSupply > 0) {
            UNCLAIM_TASK();
            d.state    = DS_EATING;
            d.workLeft = EAT_TICKS;
            d.pathLen  = 0; d.pathPos = 0;
            return;
        }
        if (d.fatigue >= FATIGUE_THRESH && d.state != DS_SLEEPING) {
            UNCLAIM_TASK();
            d.state    = DS_SLEEPING;
            d.workLeft = SLEEP_TICKS;
            // Find nearest unoccupied TILE_BED and path toward it
            int bestBedX = -1, bestBedY = -1, bestDist = 9999;
            for (int by = 0; by < MAP_H; by++) {
                for (int bx = 0; bx < MAP_W; bx++) {
                    if (mapGet(bx, by) != TILE_BED) continue;
                    if (dwarfAt(bx, by, idx)) continue; // already occupied
                    int dist = abs(bx - d.x) + abs(by - d.y);
                    if (dist < bestDist) { bestDist = dist; bestBedX = bx; bestBedY = by; }
                }
            }
            d.pathLen = 0; d.pathPos = 0;
            if (bestBedX >= 0) {
                int plen = pathFind(d.x, d.y, bestBedX, bestBedY,
                                    d.pathX, d.pathY, MAX_PATH_LEN);
                if (plen > 0) { d.pathLen = (uint8_t)plen; d.pathPos = 0; }
            }
            return;
        }
    }

    // ---- State machine ----
    switch (d.state) {

    // ---- IDLE ----
    case DS_IDLE: {
        d.idleTicks++;
        int ti = taskFindBest(d.x, d.y);

        // When no urgent dig/chop work: let idle dwarves train in barracks
        bool urgentTask = (ti >= 0 &&
            (gTasks[ti].type == TASK_DIG || gTasks[ti].type == TASK_CHOP));
        if (!urgentTask && d.combatSkill < COMBAT_SKILL_MAX && d.idleTicks >= 1) {
            int bx = (FORT_BAR_X1+FORT_BAR_X2)/2, by = (FORT_BAR_Y1+FORT_BAR_Y2)/2;
            if (mapPassable(bx, by) && gMap[by][bx].roomType == ROOM_BARRACKS
                && random(0, 4) == 0) {
                int plen = pathFind(d.x, d.y, bx, by, d.pathX, d.pathY, MAX_PATH_LEN);
                if (plen > 0) {
                    d.pathLen = (uint8_t)plen; d.pathPos = 0;
                    d.state   = DS_TRAINING;
                    d.workLeft = COMBAT_TRAIN_DURATION;
                    d.idleTicks = 0;
                    break;
                }
            }
        }

        if (ti >= 0) {
            d.idleTicks = 0;
            gTasks[ti].claimed   = true;
            gTasks[ti].claimedBy = (int8_t)idx;
            d.taskIdx  = ti;
            d.workLeft = gTasks[ti].workNeeded;
            d.placeFurn = false;

            Task& t = gTasks[ti];
            int plen = 0;
            bool startFetch = false;

            if (t.type == TASK_DIG || t.type == TASK_CHOP) {
                plen = pathFindAdj(d.x, d.y, t.x, t.y, d.pathX, d.pathY, MAX_PATH_LEN);
            } else if (t.type == TASK_PLACE_FURN) {
                // Walk to the stockpile centre — will search for item on arrival
                int sx = (gStockX1+gStockX2)/2, sy = (gStockY1+gStockY2)/2;
                plen = pathFind(d.x, d.y, sx, sy, d.pathX, d.pathY, MAX_PATH_LEN);
                if (plen == 0) plen = 1; // already nearby
            } else if (t.type == TASK_CRAFT) {
                CraftType ct = (CraftType)t.auxType;
                if (craftWoodCost(ct) > 0 || craftBoneCost(ct) > 0 || craftStoneCost(ct) > 0) {
                    // Fetch material from stockpile first
                    int sx = (gStockX1+gStockX2)/2, sy = (gStockY1+gStockY2)/2;
                    plen = pathFind(d.x, d.y, sx, sy, d.pathX, d.pathY, MAX_PATH_LEN);
                    startFetch = true;
                } else {
                    plen = pathFind(d.x, d.y, t.x, t.y, d.pathX, d.pathY, MAX_PATH_LEN);
                }
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
                if (!inPos && !startFetch) { taskUnclaim(ti); d.taskIdx=-1; break; }
            }
            d.pathLen = (uint8_t)plen;
            d.pathPos = 0;
            d.state   = startFetch ? DS_FETCHING : DS_MOVING;
            // Movement timeout: 2× path length, min 40, max 200 ticks
            if (!startFetch)
                d.workLeft = (uint8_t)min(max(plen * 2, 40), 200);
            break;
        }

        // No work — try to pray at shrine when very idle (20% chance per 8-tick burst)
        if (d.idleTicks >= 8 && random(0, 5) == 0) {
            for (int py = 0; py < MAP_H; py++) {
                for (int px = 0; px < MAP_W; px++) {
                    if (gMap[py][px].type != TILE_SHRINE || !mapIsRevealed(px, py)) continue;
                    int plen = pathFind(d.x, d.y, px, py, d.pathX, d.pathY, MAX_PATH_LEN);
                    if (plen > 0) {
                        d.pathLen  = (uint8_t)plen;
                        d.pathPos  = 0;
                        d.workLeft = PRAYER_TICKS;
                        d.state    = DS_PRAYING;
                        d.idleTicks = 0;
                        goto doneIdleChoice;
                    }
                }
            }
        }

        // No work — wander; bias toward hall tiles when the hall exists
        if (d.idleTicks >= 4) {
            d.idleTicks = 0;
            bool tryHall = (gFortStage >= FS_N_CORRIDOR && random(0, 10) < 7);
            int bestX = -1, bestY = -1, bestDist = 0;

            if (tryHall) {
                // Pick a random ROOM_HALL tile to socialize in
                for (int attempt = 0; attempt < 25; attempt++) {
                    int wx = random(0, MAP_W), wy = random(0, MAP_H);
                    if (!mapPassable(wx,wy) || !mapIsRevealed(wx,wy)) continue;
                    if (wx == d.x && wy == d.y) continue;
                    if (gMap[wy][wx].roomType != ROOM_HALL) continue;
                    bestX = wx; bestY = wy; break;
                }
            }

            // Fallback: any passable tile, prefer distant ones
            if (bestX < 0) {
                for (int attempt = 0; attempt < 20; attempt++) {
                    int wx = random(0, MAP_W), wy = random(0, MAP_H);
                    if (!mapPassable(wx,wy) || !mapIsRevealed(wx,wy)) continue;
                    if (wx == d.x && wy == d.y) continue;
                    int dist = abs(wx-d.x)+abs(wy-d.y);
                    if (dist > bestDist) { bestDist=dist; bestX=wx; bestY=wy; }
                }
            }

            if (bestX >= 0) {
                int plen = pathFind(d.x, d.y, bestX, bestY, d.pathX, d.pathY, MAX_PATH_LEN);
                if (plen > 0) { d.pathLen=(uint8_t)plen; d.pathPos=0; d.state=DS_WANDER; }
            }
        }
        doneIdleChoice:;
        break;
    }

    // ---- FETCHING (heading to stockpile to pick up craft material) ----
    case DS_FETCHING: {
        if (d.taskIdx < 0) { d.state=DS_IDLE; break; }
        if (d.pathPos < d.pathLen) { dwarfMove(idx); break; }

        // Arrived at stockpile — find and physically pick up material
        Task& ft = gTasks[d.taskIdx];
        CraftType fct = (CraftType)ft.auxType;
        int woodCost  = craftWoodCost(fct);
        int boneCost  = craftBoneCost(fct);
        ItemType need = (woodCost > 0) ? ITEM_WOOD : (boneCost > 0) ? ITEM_BONE : ITEM_STONE;

        int ix = -1, iy = -1;
        for (int sy2 = gStockY1; sy2 <= gStockY2 && ix < 0; sy2++)
            for (int sx2 = gStockX1; sx2 <= gStockX2 && ix < 0; sx2++)
                if (mapInBounds(sx2,sy2) && gMap[sy2][sx2].item == need) { ix=sx2; iy=sy2; }
        if (ix < 0) mapFindItem(d.x, d.y, need, &ix, &iy);

        if (ix >= 0 && mapRemoveItem(ix, iy, need)) {
            d.carrying = need;
            int plen = pathFind(d.x, d.y, ft.x, ft.y, d.pathX, d.pathY, MAX_PATH_LEN);
            d.pathLen  = (uint8_t)plen; d.pathPos = 0;
            d.workLeft = (uint8_t)min(max(plen * 2, 40), 200);
            d.state    = DS_MOVING;
        } else {
            taskUnclaim(d.taskIdx); d.taskIdx=-1; d.state=DS_IDLE;
        }
        break;
    }

    // ---- TRAINING (moving to / training in barracks) ----
    case DS_TRAINING: {
        // Only urgent dig/chop interrupts training; haul/craft can wait
        int urgTi = taskFindBest(d.x, d.y);
        if (urgTi >= 0 && gTasks[urgTi].type == TASK_DIG) {
            d.state = DS_IDLE; break;
        }
        if (d.pathPos < d.pathLen) { dwarfMove(idx); break; }

        // Inside barracks — train
        if (d.workLeft > 0) {
            d.workLeft--;
            if (gTick % COMBAT_TRAIN_INTERVAL == 0 && d.combatSkill < COMBAT_SKILL_MAX)
                d.combatSkill++;
        } else {
            d.state = DS_IDLE;
        }
        break;
    }

    // ---- PRAYING (walking to shrine, then meditating) ----
    case DS_PRAYING: {
        // Urgent dig interrupts prayer
        int urgPray = taskFindBest(d.x, d.y);
        if (urgPray >= 0 && gTasks[urgPray].type == TASK_DIG) {
            d.pathLen = 0; d.state = DS_IDLE; break;
        }
        // Still walking to shrine
        if (d.pathPos < d.pathLen) { dwarfMove(idx); break; }
        // At destination — pray
        if (d.workLeft > 0) {
            d.workLeft--;
            if (d.happiness < MAX_HAPPINESS) d.happiness++;
            if (d.fatigue > 5) d.fatigue = (uint8_t)(d.fatigue - 1);  // meditation eases fatigue
            break;
        }
        d.state = DS_IDLE;
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
        // Movement timeout: if we've spent too long travelling, release the task
        // so a closer dwarf can claim it. workLeft counts down from pathLen*2.
        if (d.workLeft == 0) {
            taskUnclaim(d.taskIdx); d.taskIdx = -1;
            d.pathLen = 0; d.state = DS_IDLE;
            break;
        }
        if (d.workLeft > 0) d.workLeft--;
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
            // 50% chance to yield a stone boulder
            if (random(0, 2) == 0) {
                mapAddItem(t.x, t.y, ITEM_STONE);
                int sx, sy;
                if (stockpileFindSlot(&sx, &sy)) taskAdd(TASK_HAUL, t.x, t.y, sx, sy);
            }

        // --- CHOP (also handles wagon demolition) ---
        } else if (t.type == TASK_CHOP) {
            bool isWagon = (mapGet(t.x, t.y) == TILE_CART);
            mapSet(t.x, t.y, TILE_GRASS);
            int woodAmt = isWagon ? (3 + random(0, 3)) : 1;  // wagon → 3–5 wood
            for (int wi = 0; wi < woodAmt; wi++) mapAddItem(t.x, t.y, ITEM_WOOD);
            int sx, sy;
            if (stockpileFindSlot(&sx, &sy, ITEM_WOOD)) taskAdd(TASK_HAUL, t.x, t.y, sx, sy);
            if (isWagon) Serial.println("Wagon demolished for wood");

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
                if (ct == CRAFT_MUSHROOM_FOOD) {
                    int cap = (mapCountItemGlobal(ITEM_FOOD) + mapCountItemGlobal(ITEM_BARREL))
                              * BARREL_CAPACITY;
                    if (cap > 0) gFoodSupply = min(gFoodSupply + 3, cap);
                } else {
                    int cap = (mapCountItemGlobal(ITEM_DRINK) + mapCountItemGlobal(ITEM_BARREL))
                              * BARREL_CAPACITY;
                    if (cap > 0) gDrinkSupply = min(gDrinkSupply + 3, cap);
                }
            } else {
                // Dwarf physically carried the primary material (in d.carrying)
                if (d.carrying == ITEM_NONE) {
                    taskUnclaim(d.taskIdx); d.taskIdx=-1; d.state=DS_IDLE; break;
                }
                d.carrying = ITEM_NONE; // consume the carried unit
                // Consume any additional units needed beyond the first
                int boneCost2  = craftBoneCost(ct);
                int stoneCost2 = craftStoneCost(ct);
                if (woodCost > 1 && !mapConsumeFromStockpile(ITEM_WOOD, woodCost - 1)) {
                    taskUnclaim(d.taskIdx); d.taskIdx=-1; d.state=DS_IDLE; break;
                }
                if (boneCost2 > 1 && !mapConsumeFromStockpile(ITEM_BONE, boneCost2 - 1)) {
                    taskUnclaim(d.taskIdx); d.taskIdx=-1; d.state=DS_IDLE; break;
                }
                if (stoneCost2 > 1 && !mapConsumeFromStockpile(ITEM_STONE, stoneCost2 - 1)) {
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

        // --- FISH ---
        } else if (t.type == TASK_FISH) {
            if (random(0, 2) == 0) {  // 50% catch chance
                int barrelCap = (mapCountItemGlobal(ITEM_FOOD) + mapCountItemGlobal(ITEM_BARREL))
                                * BARREL_CAPACITY;
                if (barrelCap > 0)
                    gFoodSupply = min(gFoodSupply + FISH_FOOD_AMOUNT, barrelCap);
            }

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
        // Still walking to bed — move one step
        if (d.pathLen > 0 && d.pathPos < d.pathLen) {
            int nx = d.pathX[d.pathPos], ny = d.pathY[d.pathPos];
            if (!mapPassable(nx, ny)) {
                d.pathLen = 0; // path blocked, sleep in place
            } else if (!dwarfAt(nx, ny, idx)) {
                mapMarkDirty(d.x, d.y);
                d.lastX = d.x; d.lastY = d.y;
                d.x = (int8_t)nx; d.y = (int8_t)ny;
                mapMarkDirty(d.x, d.y);
                d.pathPos++;
            }
            break; // don't sleep this tick, still moving
        }
        // Arrived (or no path) — sleep here
        int restore = nearBed(d) ? SLEEP_RESTORE * 2 : SLEEP_RESTORE;
        if (d.workLeft > 0) {
            d.workLeft--;
            d.fatigue = (uint8_t)max(0, (int)d.fatigue - restore);
            break;
        }
        d.state = DS_IDLE;
        d.pathLen = 0; d.pathPos = 0;
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
