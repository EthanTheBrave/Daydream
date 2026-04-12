#include "tasks.h"
#include "config.h"
#include "map.h"
#include "pathfind.h"
#include <string.h>
#include <Arduino.h>

Task gTasks[MAX_TASKS];
int  gTaskCount = 0;

// ----------------------------------------------------------------
void tasksInit() {
    gTaskCount = 0;
    memset(gTasks, 0, sizeof(gTasks));
}

// ----------------------------------------------------------------
int taskAdd(TaskType type, int x, int y, int destX, int destY) {
    // Deduplicate: don't add the same dig/chop/fish at the same tile twice
    if (type == TASK_DIG || type == TASK_CHOP || type == TASK_FISH) {
        for (int i = 0; i < gTaskCount; i++) {
            if (!gTasks[i].done && gTasks[i].type == type
                && gTasks[i].x == x && gTasks[i].y == y) {
                return i;
            }
        }
    }
    // Find a free slot: prefer extending the array, but recycle done slots when full
    int idx = -1;
    if (gTaskCount < MAX_TASKS) {
        idx = gTaskCount++;
    } else {
        for (int i = 0; i < MAX_TASKS; i++) {
            if (gTasks[i].done) { idx = i; break; }
        }
        if (idx < 0) return -1; // truly full
    }
    gTasks[idx].type       = type;
    gTasks[idx].x          = (int8_t)x;
    gTasks[idx].y          = (int8_t)y;
    gTasks[idx].destX      = (int8_t)destX;
    gTasks[idx].destY      = (int8_t)destY;
    gTasks[idx].claimed    = false;
    gTasks[idx].claimedBy  = -1;
    gTasks[idx].done       = false;

    switch (type) {
        case TASK_DIG:   gTasks[idx].workNeeded = DIG_TICKS;   break;
        case TASK_CHOP:  gTasks[idx].workNeeded = CHOP_TICKS;  break;
        case TASK_HAUL:  gTasks[idx].workNeeded = HAUL_TICKS;  break;
        case TASK_EAT:   gTasks[idx].workNeeded = EAT_TICKS;   break;
        case TASK_DRINK: gTasks[idx].workNeeded = DRINK_TICKS; break;
        case TASK_SLEEP:       gTasks[idx].workNeeded = SLEEP_TICKS;  break;
        case TASK_CRAFT:       gTasks[idx].workNeeded = CRAFT_TICKS;  break;
        case TASK_PLACE_FURN:  gTasks[idx].workNeeded = HAUL_TICKS;   break;
        case TASK_BURY:        gTasks[idx].workNeeded = HAUL_TICKS;   break;
        case TASK_FISH:        gTasks[idx].workNeeded = FISH_TICKS;   break;
        default:               gTasks[idx].workNeeded = 1;            break;
    }
    return idx;
}

// ----------------------------------------------------------------
void taskComplete(int idx) {
    if (idx < 0 || idx >= gTaskCount) return;
    gTasks[idx].done = true;
}

void taskUnclaim(int idx) {
    if (idx < 0 || idx >= gTaskCount) return;
    gTasks[idx].claimed   = false;
    gTasks[idx].claimedBy = -1;
}

// ----------------------------------------------------------------
// Priority order: structural first, then haul/place, crafting, fishing last
static const TaskType kPriority[] = {
    TASK_DIG, TASK_CHOP, TASK_BURY, TASK_HAUL, TASK_PLACE_FURN, TASK_CRAFT, TASK_FISH
};
static const int kNumPriority = 7;

int taskFindBest(int cx, int cy) {
    static int8_t tmpX[MAX_PATH_LEN], tmpY[MAX_PATH_LEN];

    for (int pi = 0; pi < kNumPriority; pi++) {
        TaskType want = kPriority[pi];
        int bestDist = 99999, bestIdx = -1;
        for (int i = 0; i < gTaskCount; i++) {
            if (gTasks[i].done)    continue;
            if (gTasks[i].claimed) continue;
            if (gTasks[i].type != want) continue;
            if (!taskIsReady(i))   continue;
            // For DIG/CHOP: verify the dwarf can actually reach an adjacent tile
            if (want == TASK_DIG || want == TASK_CHOP) {
                int plen = pathFindAdj(cx, cy, gTasks[i].x, gTasks[i].y,
                                       tmpX, tmpY, MAX_PATH_LEN);
                // plen==0 is also "in position" (already adjacent) — allow that
                bool adj = false;
                static const int8_t adx[4] = {0,0,1,-1};
                static const int8_t ady[4] = {1,-1,0,0};
                for (int d = 0; d < 4 && !adj; d++)
                    adj = (cx == gTasks[i].x+adx[d] && cy == gTasks[i].y+ady[d]);
                if (plen == 0 && !adj) continue; // unreachable from here
            }
            int d = abs(gTasks[i].x - cx) + abs(gTasks[i].y - cy);
            if (d < bestDist) { bestDist = d; bestIdx = i; }
        }
        if (bestIdx >= 0) return bestIdx;
    }
    return -1;
}

// ----------------------------------------------------------------
void taskCancelAt(int x, int y) {
    for (int i = 0; i < gTaskCount; i++) {
        if (!gTasks[i].done && gTasks[i].x == x && gTasks[i].y == y) {
            gTasks[i].done = true;
            mapDesignate(x, y, false);
        }
    }
}

bool taskExistsAt(int x, int y, TaskType type) {
    for (int i = 0; i < gTaskCount; i++) {
        if (!gTasks[i].done && gTasks[i].type == type
            && gTasks[i].x == x && gTasks[i].y == y) return true;
    }
    return false;
}

bool taskExistsCraft(CraftType ct) {
    for (int i = 0; i < gTaskCount; i++) {
        if (!gTasks[i].done && gTasks[i].type == TASK_CRAFT
            && gTasks[i].auxType == (uint8_t)ct) return true;
    }
    return false;
}

bool taskExistsPlaceFurn(int destX, int destY) {
    for (int i = 0; i < gTaskCount; i++) {
        if (!gTasks[i].done && gTasks[i].type == TASK_PLACE_FURN
            && gTasks[i].destX == destX && gTasks[i].destY == destY) return true;
    }
    return false;
}

bool taskIsReady(int idx) {
    if (idx < 0 || idx >= gTaskCount) return false;
    Task& t = gTasks[idx];
    // PLACE_FURN only ready if item exists somewhere accessible
    if (t.type == TASK_PLACE_FURN) {
        ItemType need = (ItemType)t.auxType;
        return mapCountItemGlobal(need) > 0;
    }
    // CRAFT only ready if enough materials exist
    if (t.type == TASK_CRAFT) {
        CraftType ct = (CraftType)t.auxType;
        int mushCost = craftMushroomCost(ct);
        if (mushCost > 0) return mapCountItemGlobal(ITEM_MUSHROOM) >= mushCost;
        int boneCost = craftBoneCost(ct);
        if (boneCost > 0) return mapCountItemGlobal(ITEM_BONE) >= boneCost;
        int stoneCost = craftStoneCost(ct);
        if (stoneCost > 0) return mapCountItemGlobal(ITEM_STONE) >= stoneCost;
        int woodCost = craftWoodCost(ct);
        return mapCountItemGlobal(ITEM_WOOD) >= woodCost;
    }
    // BURY only ready if a coffin is available
    if (t.type == TASK_BURY) {
        return mapCountItemGlobal(ITEM_COFFIN) > 0;
    }
    return true;
}
