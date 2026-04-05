#include "fortplan.h"
#include "map.h"
#include "tasks.h"
#include "dwarves.h"
#include "config.h"
#include <string.h>
#include <Arduino.h>

FortStage gFortStage  = FS_ARRIVAL;
uint32_t  gTick       = 0;
char      gStageName[24] = "Arriving";

int  gDeadUnburied = 0;
int  gTombSlotUsed = 0;
bool gTombDug      = false;

bool gFortFallen        = false;
char gFortFallReason[48] = "";

// Track which farms are active (TILE_FARM set)
static bool gFarmsActive = false;

// ----------------------------------------------------------------
//  Helpers
// ----------------------------------------------------------------
void fortDesignateRect(int x1, int y1, int x2, int y2) {
    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            if (!mapInBounds(x, y))        continue;
            if (mapGet(x, y) != TILE_WALL) continue;
            if (mapDesignated(x, y))       continue;
            mapDesignate(x, y, true);
            taskAdd(TASK_DIG, x, y);
        }
    }
}

int fortCountRemainingDig(int x1, int y1, int x2, int y2) {
    int count = 0;
    for (int y = y1; y <= y2; y++)
        for (int x = x1; x <= x2; x++)
            if (mapInBounds(x,y) && mapGet(x,y) == TILE_WALL) count++;
    return count;
}

// forward declaration
static bool queueCraft(CraftType ct, int wx, int wy);
static bool queueCraftAt(CraftType ct, int wx, int wy);

// ----------------------------------------------------------------
//  Workshop centre helpers
// ----------------------------------------------------------------
static void workshopCentre(int x1, int y1, int x2, int y2, int* cx, int* cy) {
    *cx = (x1 + x2) / 2;
    *cy = (y1 + y2) / 2;
}

// ----------------------------------------------------------------
//  Embark cart — place items on surface tiles near cartX,cartY
// ----------------------------------------------------------------
void fortPlaceCart(int cartX, int cartY) {
    mapSet(cartX, cartY, TILE_CART);

    static const struct { int dx, dy; ItemType item; } kItems[] = {
        {-1,-1, ITEM_WOOD},  { 0,-1, ITEM_WOOD},  { 1,-1, ITEM_WOOD},
        {-1, 0, ITEM_WOOD},  { 1, 0, ITEM_WOOD},
        {-1, 1, ITEM_STONE}, { 0, 1, ITEM_STONE}, { 1, 1, ITEM_STONE},
    };
    for (int i = 0; i < 8; i++) {
        int ix = cartX + kItems[i].dx;
        int iy = cartY + kItems[i].dy;
        if (!mapInBounds(ix, iy) || !mapPassable(ix, iy)) continue;
        mapAddItem(ix, iy, kItems[i].item);
        int sx, sy;
        if (stockpileFindSlot(&sx, &sy)) taskAdd(TASK_HAUL, ix, iy, sx, sy);
    }
}

// ----------------------------------------------------------------
//  Death notification
// ----------------------------------------------------------------
void fortNotifyDeath(int corpseX, int corpseY) {
    gDeadUnburied++;

    if (!gTombDug) {
        fortDesignateRect(TOMB_X1, TOMB_Y1, TOMB_X2, TOMB_Y2);
        mapSetRoomRect(TOMB_X1, TOMB_Y1, TOMB_X2, TOMB_Y2, ROOM_TOMB);
        gTombDug = true;
    }

    if (!taskExistsCraft(CRAFT_COFFIN)) {
        int wx = (FORT_HALL_X1+FORT_HALL_X2)/2;
        int wy = (FORT_HALL_Y1+FORT_HALL_Y2)/2;
        // Use a workshop if available
        if (mapPassable((FORT_WS_WOOD_X1+FORT_WS_WOOD_X2)/2,
                        (FORT_WS_WOOD_Y1+FORT_WS_WOOD_Y2)/2)) {
            wx = (FORT_WS_WOOD_X1+FORT_WS_WOOD_X2)/2;
            wy = (FORT_WS_WOOD_Y1+FORT_WS_WOOD_Y2)/2;
        }
        queueCraftAt(CRAFT_COFFIN, wx, wy);
    }

    int slot = gTombSlotUsed;
    int cols  = (TOMB_X2 - TOMB_X1 - 1) / 2 + 1;
    int col   = slot % cols;
    int row   = slot / cols;
    int destX = TOMB_X1 + 1 + col * 2;
    int destY = TOMB_Y1 + 1 + row;
    if (destX > TOMB_X2 - 1) destX = TOMB_X2 - 1;
    if (destY > TOMB_Y2 - 1) destY = TOMB_Y2 - 1;

    taskAdd(TASK_BURY, corpseX, corpseY, destX, destY);
}

// ----------------------------------------------------------------
//  Furnishing helpers
// ----------------------------------------------------------------
static bool queueCraftAt(CraftType ct, int wx, int wy) {
    if (taskExistsCraft(ct)) return false;
    int ti = taskAdd(TASK_CRAFT, wx, wy);
    if (ti < 0) return false;
    gTasks[ti].auxType = (uint8_t)ct;
    return true;
}

static bool queueCraft(CraftType ct, int wx, int wy) {
    // Allow multiple queued crafts of the same type (e.g. multiple barrels)
    int ti = taskAdd(TASK_CRAFT, wx, wy);
    if (ti < 0) return false;
    gTasks[ti].auxType = (uint8_t)ct;
    return true;
}

static bool queuePlace(ItemType it, int destX, int destY) {
    if (!mapInBounds(destX, destY)) return false;
    if (taskExistsPlaceFurn(destX, destY)) return false;
    if (mapCountItemGlobal(it) <= 0) return false;
    int ti = taskAdd(TASK_PLACE_FURN, gStockX1, gStockY1, destX, destY);
    if (ti < 0) return false;
    gTasks[ti].auxType = (uint8_t)it;
    return true;
}

// ----------------------------------------------------------------
//  Woodworker workshop centre
// ----------------------------------------------------------------
static int woodWkX() { return (FORT_WS_WOOD_X1+FORT_WS_WOOD_X2)/2; }
static int woodWkY() { return (FORT_WS_WOOD_Y1+FORT_WS_WOOD_Y2)/2; }
static bool woodWkReady() { return mapPassable(woodWkX(), woodWkY()); }

static int hallCx() { return (FORT_HALL_X1+FORT_HALL_X2)/2; }
static int hallCy() { return (FORT_HALL_Y1+FORT_HALL_Y2)/2; }

// Craft location: use woodworker shop if dug, else hall centre
static void getCraftLoc(int* wx, int* wy) {
    if (woodWkReady()) { *wx = woodWkX(); *wy = woodWkY(); }
    else               { *wx = hallCx();  *wy = hallCy(); }
}

// ----------------------------------------------------------------
//  Stage: furnish main hall
// ----------------------------------------------------------------
static const int8_t kHallTableX[HALL_TABLES] = {15, 17};
static const int8_t kHallTableY[HALL_TABLES] = {14, 14};
static const int8_t kHallChairX[HALL_CHAIRS] = {15, 17, 15, 17};
static const int8_t kHallChairY[HALL_CHAIRS] = {13, 13, 15, 15};

static void manageFurnishHall() {
    int wx, wy; getCraftLoc(&wx, &wy);

    int tablesMade = mapCountItemGlobal(ITEM_TABLE_I)
                   + mapCountTileInRect(FORT_HALL_X1,FORT_HALL_Y1,FORT_HALL_X2,FORT_HALL_Y2,TILE_TABLE);
    int chairsMade = mapCountItemGlobal(ITEM_CHAIR)
                   + mapCountTileInRect(FORT_HALL_X1,FORT_HALL_Y1,FORT_HALL_X2,FORT_HALL_Y2,TILE_CHAIR);

    if (!taskExistsCraft(CRAFT_TABLE)) {
        for (int i = tablesMade; i < HALL_TABLES; i++) {
            int ti = taskAdd(TASK_CRAFT, wx, wy);
            if (ti >= 0) gTasks[ti].auxType = (uint8_t)CRAFT_TABLE;
        }
    }
    if (!taskExistsCraft(CRAFT_CHAIR)) {
        for (int i = chairsMade; i < HALL_CHAIRS; i++) {
            int ti = taskAdd(TASK_CRAFT, wx, wy);
            if (ti >= 0) gTasks[ti].auxType = (uint8_t)CRAFT_CHAIR;
        }
    }

    for (int i = 0; i < HALL_TABLES; i++) {
        if (mapGet(kHallTableX[i], kHallTableY[i]) != TILE_TABLE)
            queuePlace(ITEM_TABLE_I, kHallTableX[i], kHallTableY[i]);
    }
    for (int i = 0; i < HALL_CHAIRS; i++) {
        if (mapGet(kHallChairX[i], kHallChairY[i]) != TILE_CHAIR)
            queuePlace(ITEM_CHAIR, kHallChairX[i], kHallChairY[i]);
    }
}

static bool furnishHallComplete() {
    int tables = mapCountTileInRect(FORT_HALL_X1,FORT_HALL_Y1,FORT_HALL_X2,FORT_HALL_Y2,TILE_TABLE);
    int chairs = mapCountTileInRect(FORT_HALL_X1,FORT_HALL_Y1,FORT_HALL_X2,FORT_HALL_Y2,TILE_CHAIR);
    return tables >= HALL_TABLES && chairs >= HALL_CHAIRS;
}

// ----------------------------------------------------------------
//  Stage: furnish bedrooms
// ----------------------------------------------------------------
static void manageFurnishBed() {
    int wx, wy; getCraftLoc(&wx, &wy);

    int placed  = mapCountTileInRect(FORT_BED_X1,FORT_BED_Y1,FORT_BED_X2,FORT_BED_Y2,TILE_BED);
    int inStock = mapCountItemGlobal(ITEM_BED_I);
    int totalNeeded = min((int)BED_COUNT, (FORT_BED_X2 - FORT_BED_X1 - 1));

    if (!taskExistsCraft(CRAFT_BED)) {
        for (int i = placed + inStock; i < totalNeeded; i++) {
            int ti = taskAdd(TASK_CRAFT, wx, wy);
            if (ti >= 0) gTasks[ti].auxType = (uint8_t)CRAFT_BED;
        }
    }

    int slot = 0;
    for (int bx = FORT_BED_X1 + 1; bx <= FORT_BED_X2 - 1 && slot < totalNeeded; bx += 2, slot++) {
        int by = (slot % 2 == 0) ? FORT_BED_Y1 + 1 : FORT_BED_Y2 - 1;
        if (mapGet(bx, by) != TILE_BED) queuePlace(ITEM_BED_I, bx, by);
    }
}

static bool furnishBedComplete() {
    int totalNeeded = min((int)BED_COUNT, (FORT_BED_X2 - FORT_BED_X1 - 1));
    return mapCountTileInRect(FORT_BED_X1,FORT_BED_Y1,FORT_BED_X2,FORT_BED_Y2,TILE_BED) >= totalNeeded;
}

// ----------------------------------------------------------------
//  Burial management
// ----------------------------------------------------------------
static void manageBurials() {
    if (gDeadUnburied <= 0) return;
    if (!gTombDug) return;
    bool burialPending = false;
    for (int i = 0; i < gTaskCount; i++) {
        if (!gTasks[i].done && gTasks[i].type == TASK_BURY) { burialPending = true; break; }
    }
    if (burialPending && mapCountItemGlobal(ITEM_COFFIN) == 0 && !taskExistsCraft(CRAFT_COFFIN)) {
        int wx, wy; getCraftLoc(&wx, &wy);
        int ti = taskAdd(TASK_CRAFT, wx, wy);
        if (ti >= 0) gTasks[ti].auxType = (uint8_t)CRAFT_COFFIN;
    }
}

// ----------------------------------------------------------------
//  Surface foraging
// ----------------------------------------------------------------
static void manageWoodSupply() {
    if (mapCountItemGlobal(ITEM_WOOD) >= LOW_WOOD_THRESHOLD) return;
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < HILL_START_X + 2; x++) {
            if (!mapInBounds(x, y)) continue;
            if (mapGet(x, y) == TILE_TREE && !mapDesignated(x, y)) {
                mapDesignate(x, y, true);
                taskAdd(TASK_CHOP, x, y);
            }
        }
    }
}

// ----------------------------------------------------------------
//  Haul re-queue after stockpile zone moves
// ----------------------------------------------------------------
static void requeueWoodHauls() {
    for (int i = 0; i < gTaskCount; i++) {
        if (gTasks[i].done || gTasks[i].claimed) continue;
        if (gTasks[i].type != TASK_HAUL) continue;
        int dx = gTasks[i].destX, dy = gTasks[i].destY;
        if (!(dx >= gStockX1 && dx <= gStockX2 && dy >= gStockY1 && dy <= gStockY2))
            gTasks[i].done = true;
    }
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            if (gMap[y][x].item == ITEM_NONE) continue;
            if (gMap[y][x].itemCount == 0) continue;
            if (x >= gStockX1 && x <= gStockX2 && y >= gStockY1 && y <= gStockY2) continue;
            bool hasHaul = false;
            for (int i = 0; i < gTaskCount && !hasHaul; i++) {
                if (!gTasks[i].done && gTasks[i].type == TASK_HAUL
                    && gTasks[i].x == (int8_t)x && gTasks[i].y == (int8_t)y)
                    hasHaul = true;
            }
            if (hasHaul) continue;
            int sx, sy;
            if (stockpileFindSlot(&sx, &sy)) taskAdd(TASK_HAUL, x, y, sx, sy);
        }
    }
}

// ----------------------------------------------------------------
//  Workshop stage helpers
// ----------------------------------------------------------------
// Place a workshop machine tile at the centre of the given rect
static void placeWorkshop(int x1, int y1, int x2, int y2, RoomType room) {
    mapSetRoomRect(x1, y1, x2, y2, room);
    int cx = (x1+x2)/2, cy = (y1+y2)/2;
    mapSet(cx, cy, TILE_FORGE);
}

// Convert all TILE_FLOOR in rect to TILE_FARM
static void convertToFarm(int x1, int y1, int x2, int y2) {
    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            if (mapInBounds(x,y) && mapGet(x,y) == TILE_FLOOR) {
                gMap[y][x].type    = TILE_FARM;
                gMap[y][x].roomType = ROOM_FARM;
                mapMarkDirty(x, y);
            }
        }
    }
}

// Count how many workshop rects are fully dug
static int countWorkshopsDug() {
    struct { int x1,y1,x2,y2; } ws[] = {
        {FORT_WS_WOOD_X1, FORT_WS_WOOD_Y1, FORT_WS_WOOD_X2, FORT_WS_WOOD_Y2},
        {FORT_WS_STILL_X1, FORT_WS_STILL_Y1, FORT_WS_STILL_X2, FORT_WS_STILL_Y2},
        {FORT_WS_KITCH_X1, FORT_WS_KITCH_Y1, FORT_WS_KITCH_X2, FORT_WS_KITCH_Y2},
        {FORT_WS_STONE_X1, FORT_WS_STONE_Y1, FORT_WS_STONE_X2, FORT_WS_STONE_Y2},
        {FORT_WS_SMELT_X1, FORT_WS_SMELT_Y1, FORT_WS_SMELT_X2, FORT_WS_SMELT_Y2},
        {FORT_WS_FORGE_X1, FORT_WS_FORGE_Y1, FORT_WS_FORGE_X2, FORT_WS_FORGE_Y2},
        {FORT_WS_FARM_X1,  FORT_WS_FARM_Y1,  FORT_WS_FARM_X2,  FORT_WS_FARM_Y2},
        {FORT_FARM1_X1, FORT_FARM1_Y1, FORT_FARM1_X2, FORT_FARM1_Y2},
        {FORT_FARM2_X1, FORT_FARM2_Y1, FORT_FARM2_X2, FORT_FARM2_Y2},
        {FORT_FARM3_X1, FORT_FARM3_Y1, FORT_FARM3_X2, FORT_FARM3_Y2},
    };
    int done = 0;
    for (int i = 0; i < 10; i++) {
        if (fortCountRemainingDig(ws[i].x1, ws[i].y1, ws[i].x2, ws[i].y2) == 0) done++;
    }
    return done;
}

// Total workshop rects (7 workshops + 3 farms + corridor)
static int workshopTotalRemainingDig() {
    int n = fortCountRemainingDig(FORT_WCORR_X1, FORT_WCORR_Y1, FORT_WCORR_X2, FORT_WCORR_Y2);
    n += fortCountRemainingDig(FORT_WS_WOOD_X1, FORT_WS_WOOD_Y1, FORT_WS_WOOD_X2, FORT_WS_WOOD_Y2);
    n += fortCountRemainingDig(FORT_WS_STILL_X1, FORT_WS_STILL_Y1, FORT_WS_STILL_X2, FORT_WS_STILL_Y2);
    n += fortCountRemainingDig(FORT_WS_KITCH_X1, FORT_WS_KITCH_Y1, FORT_WS_KITCH_X2, FORT_WS_KITCH_Y2);
    n += fortCountRemainingDig(FORT_WS_STONE_X1, FORT_WS_STONE_Y1, FORT_WS_STONE_X2, FORT_WS_STONE_Y2);
    n += fortCountRemainingDig(FORT_WS_SMELT_X1, FORT_WS_SMELT_Y1, FORT_WS_SMELT_X2, FORT_WS_SMELT_Y2);
    n += fortCountRemainingDig(FORT_WS_FORGE_X1, FORT_WS_FORGE_Y1, FORT_WS_FORGE_X2, FORT_WS_FORGE_Y2);
    n += fortCountRemainingDig(FORT_WS_FARM_X1,  FORT_WS_FARM_Y1,  FORT_WS_FARM_X2,  FORT_WS_FARM_Y2);
    n += fortCountRemainingDig(FORT_FARM1_X1, FORT_FARM1_Y1, FORT_FARM1_X2, FORT_FARM1_Y2);
    n += fortCountRemainingDig(FORT_FARM2_X1, FORT_FARM2_Y1, FORT_FARM2_X2, FORT_FARM2_Y2);
    n += fortCountRemainingDig(FORT_FARM3_X1, FORT_FARM3_Y1, FORT_FARM3_X2, FORT_FARM3_Y2);
    return n;
}

static void designateAllWorkshops() {
    // Workshop corridor (single row at y=14)
    fortDesignateRect(FORT_WCORR_X1, FORT_WCORR_Y1, FORT_WCORR_X2, FORT_WCORR_Y2);
    // 7 workshops
    fortDesignateRect(FORT_WS_WOOD_X1,  FORT_WS_WOOD_Y1,  FORT_WS_WOOD_X2,  FORT_WS_WOOD_Y2);
    fortDesignateRect(FORT_WS_STILL_X1, FORT_WS_STILL_Y1, FORT_WS_STILL_X2, FORT_WS_STILL_Y2);
    fortDesignateRect(FORT_WS_KITCH_X1, FORT_WS_KITCH_Y1, FORT_WS_KITCH_X2, FORT_WS_KITCH_Y2);
    fortDesignateRect(FORT_WS_STONE_X1, FORT_WS_STONE_Y1, FORT_WS_STONE_X2, FORT_WS_STONE_Y2);
    fortDesignateRect(FORT_WS_SMELT_X1, FORT_WS_SMELT_Y1, FORT_WS_SMELT_X2, FORT_WS_SMELT_Y2);
    fortDesignateRect(FORT_WS_FORGE_X1, FORT_WS_FORGE_Y1, FORT_WS_FORGE_X2, FORT_WS_FORGE_Y2);
    fortDesignateRect(FORT_WS_FARM_X1,  FORT_WS_FARM_Y1,  FORT_WS_FARM_X2,  FORT_WS_FARM_Y2);
    // 3 farm plots
    fortDesignateRect(FORT_FARM1_X1, FORT_FARM1_Y1, FORT_FARM1_X2, FORT_FARM1_Y2);
    fortDesignateRect(FORT_FARM2_X1, FORT_FARM2_Y1, FORT_FARM2_X2, FORT_FARM2_Y2);
    fortDesignateRect(FORT_FARM3_X1, FORT_FARM3_Y1, FORT_FARM3_X2, FORT_FARM3_Y2);
}

static void finaliseWorkshops() {
    // Set room types and place workshop machines
    mapSetRoomRect(FORT_WCORR_X1, FORT_WCORR_Y1, FORT_WCORR_X2, FORT_WCORR_Y2, ROOM_HALL);
    placeWorkshop(FORT_WS_WOOD_X1,  FORT_WS_WOOD_Y1,  FORT_WS_WOOD_X2,  FORT_WS_WOOD_Y2,  ROOM_WORKSHOP);
    placeWorkshop(FORT_WS_STILL_X1, FORT_WS_STILL_Y1, FORT_WS_STILL_X2, FORT_WS_STILL_Y2, ROOM_WORKSHOP);
    placeWorkshop(FORT_WS_KITCH_X1, FORT_WS_KITCH_Y1, FORT_WS_KITCH_X2, FORT_WS_KITCH_Y2, ROOM_WORKSHOP);
    placeWorkshop(FORT_WS_STONE_X1, FORT_WS_STONE_Y1, FORT_WS_STONE_X2, FORT_WS_STONE_Y2, ROOM_WORKSHOP);
    placeWorkshop(FORT_WS_SMELT_X1, FORT_WS_SMELT_Y1, FORT_WS_SMELT_X2, FORT_WS_SMELT_Y2, ROOM_WORKSHOP);
    placeWorkshop(FORT_WS_FORGE_X1, FORT_WS_FORGE_Y1, FORT_WS_FORGE_X2, FORT_WS_FORGE_Y2, ROOM_WORKSHOP);
    placeWorkshop(FORT_WS_FARM_X1,  FORT_WS_FARM_Y1,  FORT_WS_FARM_X2,  FORT_WS_FARM_Y2,  ROOM_WORKSHOP);
    // Convert farm plot floors to TILE_FARM
    convertToFarm(FORT_FARM1_X1, FORT_FARM1_Y1, FORT_FARM1_X2, FORT_FARM1_Y2);
    convertToFarm(FORT_FARM2_X1, FORT_FARM2_Y1, FORT_FARM2_X2, FORT_FARM2_Y2);
    convertToFarm(FORT_FARM3_X1, FORT_FARM3_Y1, FORT_FARM3_X2, FORT_FARM3_Y2);
    gFarmsActive = true;

    // Queue barrels at Still
    int sx = (FORT_WS_STILL_X1+FORT_WS_STILL_X2)/2;
    int sy = (FORT_WS_STILL_Y1+FORT_WS_STILL_Y2)/2;
    for (int i = 0; i < 2; i++) {
        int ti = taskAdd(TASK_CRAFT, sx, sy);
        if (ti >= 0) gTasks[ti].auxType = (uint8_t)CRAFT_BARREL;
    }

    // Place door at workshop corridor entrance
    int doorX = FORT_ECORR_X1 + 1;
    int doorY = (FORT_ECORR_Y1 + FORT_ECORR_Y2) / 2;
    if (!taskExistsPlaceFurn(doorX, doorY)) {
        int wx, wy; getCraftLoc(&wx, &wy);
        int ti = taskAdd(TASK_CRAFT, wx, wy);
        if (ti >= 0) gTasks[ti].auxType = (uint8_t)CRAFT_DOOR;
        int pi = taskAdd(TASK_PLACE_FURN, gStockX1, gStockY1, doorX, doorY);
        if (pi >= 0) gTasks[pi].auxType = (uint8_t)ITEM_DOOR_I;
    }
}

// ----------------------------------------------------------------
//  Mushroom farm management (runs in FS_DONE)
// ----------------------------------------------------------------
static void manageFarms() {
    if (!gFarmsActive) return;

    // Grow mushrooms on idle farm tiles (rate-limited globally)
    if (gTick % MUSHROOM_GROW_INTERVAL != 0) return;

    // Don't grow if stockpile already saturated with mushrooms
    if (mapCountItemGlobal(ITEM_MUSHROOM) >= MAX_MUSHROOMS_STOCKPILE) return;

    // Grow on each farm plot
    static const struct { int x1,y1,x2,y2; } kFarms[] = {
        {FORT_FARM1_X1,FORT_FARM1_Y1,FORT_FARM1_X2,FORT_FARM1_Y2},
        {FORT_FARM2_X1,FORT_FARM2_Y1,FORT_FARM2_X2,FORT_FARM2_Y2},
        {FORT_FARM3_X1,FORT_FARM3_Y1,FORT_FARM3_X2,FORT_FARM3_Y2},
    };
    for (int fi = 0; fi < 3; fi++) {
        for (int y = kFarms[fi].y1; y <= kFarms[fi].y2; y++) {
            for (int x = kFarms[fi].x1; x <= kFarms[fi].x2; x++) {
                if (!mapInBounds(x,y)) continue;
                if (gMap[y][x].type != TILE_FARM) continue;
                if (gMap[y][x].item != ITEM_NONE) continue;
                mapAddItem(x, y, ITEM_MUSHROOM);
                int sx, sy;
                if (stockpileFindSlot(&sx, &sy)) taskAdd(TASK_HAUL, x, y, sx, sy);
                // Only grow one per farm per grow tick to pace it
                goto nextFarm;
            }
        }
        nextFarm:;
    }
}

// ----------------------------------------------------------------
//  Mushroom processing management (runs in FS_DONE)
// ----------------------------------------------------------------
static void manageMushProcessing() {
    int mushCount = mapCountItemGlobal(ITEM_MUSHROOM);
    if (mushCount < 2) return;

    int kx = (FORT_WS_KITCH_X1+FORT_WS_KITCH_X2)/2;
    int ky = (FORT_WS_KITCH_Y1+FORT_WS_KITCH_Y2)/2;
    int sx = (FORT_WS_STILL_X1+FORT_WS_STILL_X2)/2;
    int sy = (FORT_WS_STILL_Y1+FORT_WS_STILL_Y2)/2;

    // Queue Kitchen cooking if food supply low and kitchen is accessible
    if (gFoodSupply < MAX_FOOD_SUPPLY / 2 && !taskExistsCraft(CRAFT_MUSHROOM_FOOD)
        && mapPassable(kx, ky)) {
        int ti = taskAdd(TASK_CRAFT, kx, ky);
        if (ti >= 0) gTasks[ti].auxType = (uint8_t)CRAFT_MUSHROOM_FOOD;
    }
    // Queue Still brewing if drink supply low and still is accessible
    if (gDrinkSupply < MAX_DRINK_SUPPLY / 2 && !taskExistsCraft(CRAFT_MUSHROOM_BEER)
        && mapPassable(sx, sy)) {
        int ti = taskAdd(TASK_CRAFT, sx, sy);
        if (ti >= 0) gTasks[ti].auxType = (uint8_t)CRAFT_MUSHROOM_BEER;
    }
}

// ----------------------------------------------------------------
//  Init
// ----------------------------------------------------------------
void fortPlanInit() {
    gFortStage     = FS_ARRIVAL;
    gTick          = 0;
    gDeadUnburied  = 0;
    gTombSlotUsed  = 0;
    gTombDug       = false;
    gFarmsActive   = false;
    gFortFallen    = false;
    gFortFallReason[0] = '\0';
    strncpy(gStageName, "Arriving", sizeof(gStageName) - 1);
}

// ----------------------------------------------------------------
//  Main tick
// ----------------------------------------------------------------
void fortPlanTick() {
    gTick++;

    // Periodic food from surface foraging, drink from stream
    if (gTick % FORAGE_FOOD_INTERVAL == 0)
        gFoodSupply = min(gFoodSupply + FORAGE_FOOD_AMOUNT, (int)MAX_FOOD_SUPPLY);
    if (gTick % COLLECT_DRINK_INTERVAL == 0)
        gDrinkSupply = min(gDrinkSupply + COLLECT_DRINK_AMOUNT, (int)MAX_DRINK_SUPPLY);

    manageWoodSupply();
    manageBurials();

    switch (gFortStage) {

    case FS_ARRIVAL:
        if (gTick < 6) break;
        gFortStage = FS_ENTRANCE;
        strncpy(gStageName, "Digging Entrance", sizeof(gStageName)-1);
        fortDesignateRect(FORT_ENTRANCE_X1, FORT_ENTRANCE_Y1,
                          FORT_ENTRANCE_X2, FORT_ENTRANCE_Y2);
        break;

    case FS_ENTRANCE:
        if (fortCountRemainingDig(FORT_ENTRANCE_X1, FORT_ENTRANCE_Y1,
                                  FORT_ENTRANCE_X2, FORT_ENTRANCE_Y2) > 0) break;
        mapSetRoomRect(FORT_ENTRANCE_X1, FORT_ENTRANCE_Y1,
                       FORT_ENTRANCE_X2, FORT_ENTRANCE_Y2, ROOM_HALL);
        gFortStage = FS_MAIN_HALL;
        strncpy(gStageName, "Digging Main Hall", sizeof(gStageName)-1);
        fortDesignateRect(FORT_HALL_X1, FORT_HALL_Y1, FORT_HALL_X2, FORT_HALL_Y2);
        break;

    case FS_MAIN_HALL:
        if (fortCountRemainingDig(FORT_HALL_X1, FORT_HALL_Y1,
                                  FORT_HALL_X2, FORT_HALL_Y2) > 0) break;
        mapSetRoomRect(FORT_HALL_X1, FORT_HALL_Y1, FORT_HALL_X2, FORT_HALL_Y2, ROOM_HALL);
        // Temporary stockpile in hall while real one is being dug
        gStockX1=FORT_HALL_X1+1; gStockY1=FORT_HALL_Y1+1;
        gStockX2=FORT_HALL_X2-1; gStockY2=FORT_HALL_Y2-1;
        requeueWoodHauls();
        gFortStage = FS_FURNISH_HALL;
        strncpy(gStageName, "Furnishing Hall", sizeof(gStageName)-1);
        break;

    case FS_FURNISH_HALL:
        manageFurnishHall();
        if (furnishHallComplete() || mapCountItemGlobal(ITEM_WOOD) == 0) {
            gFortStage = FS_N_CORRIDOR;
            strncpy(gStageName, "N. Corridor", sizeof(gStageName)-1);
            fortDesignateRect(FORT_NCORR_X1, FORT_NCORR_Y1, FORT_NCORR_X2, FORT_NCORR_Y2);
        }
        break;

    case FS_N_CORRIDOR:
        manageFurnishHall();
        if (fortCountRemainingDig(FORT_NCORR_X1, FORT_NCORR_Y1,
                                  FORT_NCORR_X2, FORT_NCORR_Y2) > 0) break;
        mapSetRoomRect(FORT_NCORR_X1, FORT_NCORR_Y1, FORT_NCORR_X2, FORT_NCORR_Y2, ROOM_HALL);
        gFortStage = FS_STOCKPILE;
        strncpy(gStageName, "Stockpile Room", sizeof(gStageName)-1);
        fortDesignateRect(FORT_STOCK_X1, FORT_STOCK_Y1, FORT_STOCK_X2, FORT_STOCK_Y2);
        break;

    case FS_STOCKPILE:
        if (fortCountRemainingDig(FORT_STOCK_X1, FORT_STOCK_Y1,
                                  FORT_STOCK_X2, FORT_STOCK_Y2) > 0) break;
        mapSetRoomRect(FORT_STOCK_X1, FORT_STOCK_Y1, FORT_STOCK_X2, FORT_STOCK_Y2, ROOM_STOCKPILE);
        gStockX1=FORT_STOCK_X1; gStockY1=FORT_STOCK_Y1;
        gStockX2=FORT_STOCK_X2; gStockY2=FORT_STOCK_Y2;
        requeueWoodHauls();
        gFortStage = FS_S_CORRIDOR;
        strncpy(gStageName, "S. Corridor", sizeof(gStageName)-1);
        fortDesignateRect(FORT_SCORR_X1, FORT_SCORR_Y1, FORT_SCORR_X2, FORT_SCORR_Y2);
        break;

    case FS_S_CORRIDOR:
        if (fortCountRemainingDig(FORT_SCORR_X1, FORT_SCORR_Y1,
                                  FORT_SCORR_X2, FORT_SCORR_Y2) > 0) break;
        mapSetRoomRect(FORT_SCORR_X1, FORT_SCORR_Y1, FORT_SCORR_X2, FORT_SCORR_Y2, ROOM_HALL);
        gFortStage = FS_BEDROOMS;
        strncpy(gStageName, "Bedrooms", sizeof(gStageName)-1);
        fortDesignateRect(FORT_BED_X1, FORT_BED_Y1, FORT_BED_X2, FORT_BED_Y2);
        break;

    case FS_BEDROOMS:
        if (fortCountRemainingDig(FORT_BED_X1, FORT_BED_Y1,
                                  FORT_BED_X2, FORT_BED_Y2) > 0) break;
        mapSetRoomRect(FORT_BED_X1, FORT_BED_Y1, FORT_BED_X2, FORT_BED_Y2, ROOM_BEDROOM);
        gFortStage = FS_FURNISH_BED;
        strncpy(gStageName, "Furnishing Beds", sizeof(gStageName)-1);
        break;

    case FS_FURNISH_BED:
        manageFurnishBed();
        if (furnishBedComplete() || mapCountItemGlobal(ITEM_WOOD) == 0) {
            gFortStage = FS_E_CORRIDOR;
            strncpy(gStageName, "E. Corridor", sizeof(gStageName)-1);
            fortDesignateRect(FORT_ECORR_X1, FORT_ECORR_Y1, FORT_ECORR_X2, FORT_ECORR_Y2);
        }
        break;

    case FS_E_CORRIDOR:
        manageFurnishBed();
        if (fortCountRemainingDig(FORT_ECORR_X1, FORT_ECORR_Y1,
                                  FORT_ECORR_X2, FORT_ECORR_Y2) > 0) break;
        mapSetRoomRect(FORT_ECORR_X1, FORT_ECORR_Y1, FORT_ECORR_X2, FORT_ECORR_Y2, ROOM_HALL);
        gFortStage = FS_WORKSHOPS;
        strncpy(gStageName, "Workshop Wing", sizeof(gStageName)-1);
        designateAllWorkshops();
        break;

    case FS_WORKSHOPS:
        manageFurnishBed(); // keep trying beds
        if (workshopTotalRemainingDig() > 0) break;
        finaliseWorkshops();
        gFortStage = FS_DONE;
        strncpy(gStageName, "Fort Complete!", sizeof(gStageName)-1);
        break;

    case FS_DONE:
        manageFarms();
        manageMushProcessing();
        // Keep crafting barrels if wood available
        if (mapCountItemGlobal(ITEM_WOOD) > CRAFT_WOOD_BARREL * 2
            && !taskExistsCraft(CRAFT_BARREL)) {
            int sx = (FORT_WS_STILL_X1+FORT_WS_STILL_X2)/2;
            int sy = (FORT_WS_STILL_Y1+FORT_WS_STILL_Y2)/2;
            int ti = taskAdd(TASK_CRAFT, sx, sy);
            if (ti >= 0) gTasks[ti].auxType = (uint8_t)CRAFT_BARREL;
        }
        break;
    }
}
