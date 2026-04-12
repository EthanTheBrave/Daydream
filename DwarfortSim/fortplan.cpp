#include "fortplan.h"
#include "map.h"
#include "tasks.h"
#include "dwarves.h"
#include "config.h"
#include <string.h>
#include <Arduino.h>

// ----------------------------------------------------------------
//  Per-run layout — randomized each game from the map seed
// ----------------------------------------------------------------
static int sHX1, sHY1, sHX2, sHY2;          // Main hall
static int sNX1, sNY1, sNX2, sNY2;          // N corridor
static int sPX1, sPY1, sPX2, sPY2;          // Stockpile room
static int sSX1, sSY1, sSX2, sSY2;          // S corridor
static int sBX1, sBY1, sBX2, sBY2, sBCY;   // Bedrooms + corridor row
static int sEX1, sEY1, sEX2, sEY2;          // E corridor
static int sHallCx;                          // Hall horizontal centre

// Furniture positions (computed from layout, filled in computeLayout)
static int8_t gHallTableX[HALL_TABLES];
static int8_t gHallTableY[HALL_TABLES];
static int8_t gHallChairX[HALL_CHAIRS];
static int8_t gHallChairY[HALL_CHAIRS];
static int8_t gBedX[BED_COUNT];
static int8_t gBedY[BED_COUNT];

static void computeLayout() {
    // ---- Hall: odd width/height, centered on y=14, starts at x=13 ----
    int hallW = 5 + 2 * random(0, 3);   // 5, 7, or 9
    int hallH = 5 + 2 * random(0, 3);   // 5, 7, or 9
    sHX1  = FORT_ENTRANCE_X2 + 1;        // always x=13
    sHX2  = sHX1 + hallW - 1;
    sHY1  = 14 - hallH / 2;
    sHY2  = 14 + hallH / 2;
    sHallCx = (sHX1 + sHX2) / 2;

    // ---- N corridor: 1 or 3 wide, 2-3 tiles tall, above hall ----
    int ncorrW   = random(0, 2) ? 3 : 1;
    int ncorrLen = 2 + random(0, 2);
    sNX1 = sHallCx - ncorrW / 2;
    sNX2 = sHallCx + ncorrW / 2;
    sNY2 = sHY1 - 1;
    sNY1 = sNY2 - ncorrLen + 1;

    // ---- Stockpile: 6 or 8 wide, 3 or 4 tall, above N corridor ----
    int stockW = 6 + 2 * random(0, 2);
    int stockH = 3 + random(0, 2);
    sPX1 = sHallCx - stockW / 2;
    sPX2 = sPX1 + stockW - 1;
    sPY2 = sNY1 - 1;
    sPY1 = sPY2 - stockH + 1;
    if (sPY1 < 1) sPY1 = 1;
    if (sPX1 < 1) sPX1 = 1;
    if (sPX2 >= MAP_W) sPX2 = MAP_W - 2;

    // ---- S corridor: same x as N corridor, below hall ----
    int scorrLen = 2 + random(0, 2);
    sSX1 = sNX1;
    sSX2 = sNX2;
    sSY1 = sHY2 + 1;
    sSY2 = sSY1 + scorrLen - 1;

    // ---- Bedrooms: 3N + corridor + 3S, below S corridor ----
    // Connecting passage aligns with sHallCx (= sBX1+3)
    sBX1 = sHallCx - 3;
    sBX2 = sBX1 + 9;
    sBY1 = sSY2 + 1;    // immediately below S corridor
    sBCY = sBY1 + 2;    // bedroom corridor row
    sBY2 = sBY1 + 4;
    if (sBY2 >= MAP_H - 1) { sBY2 = MAP_H - 2; sBCY = sBY1 + (sBY2 - sBY1) / 2; }
    if (sBX1 < 1) sBX1 = 1;
    if (sBX2 >= MAP_W) sBX2 = MAP_W - 2;

    // ---- E corridor: hall east wall → x=22, y=12-14 ----
    sEX1 = sHX2 + 1;
    sEX2 = FORT_ECORR_X2;
    sEY1 = FORT_ECORR_Y1;
    sEY2 = FORT_ECORR_Y2;

    // ---- Hall furniture ----
    gHallTableX[0] = (int8_t)(sHX1 + 1);      gHallTableY[0] = (int8_t)(sHY1 + 1);
    gHallTableX[1] = (int8_t)(sHX2 - 1);      gHallTableY[1] = (int8_t)(sHY2 - 1);
    gHallChairX[0] = (int8_t)(sHX1 + 2);      gHallChairY[0] = (int8_t)(sHY1 + 1);
    gHallChairX[1] = (int8_t)(sHX1 + 1);      gHallChairY[1] = (int8_t)(sHY1 + 2);
    gHallChairX[2] = (int8_t)(sHX2 - 2);      gHallChairY[2] = (int8_t)(sHY2 - 1);
    gHallChairX[3] = (int8_t)(sHX2 - 1);      gHallChairY[3] = (int8_t)(sHY2 - 2);

    // ---- Bed positions (one per 2×2 room) ----
    static const int kOff[3] = {0, 4, 7};
    for (int i = 0; i < 3; i++) {
        gBedX[i]   = (int8_t)(sBX1 + kOff[i]);  gBedY[i]   = (int8_t)sBY1;
        gBedX[i+3] = (int8_t)(sBX1 + kOff[i]);  gBedY[i+3] = (int8_t)sBY2;
    }
}

FortStage gFortStage  = FS_ARRIVAL;
uint32_t  gTick       = 0;
char      gStageName[24] = "Arriving";

int  gDeadUnburied = 0;
int  gTombSlotUsed = 0;
bool gTombDug      = false;

bool gFortFallen        = false;
char gFortFallReason[48] = "";

bool     gFortWon  = false;
uint32_t gDoneTick = 0;

// Track which farms are active (TILE_FARM set)
static bool gFarmsActive = false;
// Wagon demolition (one-shot, after wagon area is cleared)
static bool sWagonDemolished = false;
// Temple state
static bool sTempleDug = false;
static bool sShrineQueued = false;

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

    // Starting materials: wood + stone (as before)
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
        if (stockpileFindSlot(&sx, &sy, kItems[i].item)) taskAdd(TASK_HAUL, ix, iy, sx, sy);
    }

    // Starting food + drink stored as barrels (8 each = BARREL_CAPACITY * 8 supply units)
    // plus 2 empty spare barrels. All placed to the WEST of the cart (away from mountain).
    // gFoodSupply/gDrinkSupply already start at START_FOOD/START_DRINK.
    static const struct { int dx, dy; ItemType item; } kBarrels[] = {
        // food barrels — south-west of cart
        {-3, 2, ITEM_FOOD},  {-2, 2, ITEM_FOOD},  {-1, 2, ITEM_FOOD},  { 0, 2, ITEM_FOOD},
        {-3, 3, ITEM_FOOD},  {-2, 3, ITEM_FOOD},  {-1, 3, ITEM_FOOD},  { 0, 3, ITEM_FOOD},
        // drink barrels — north-west of cart
        {-3,-2, ITEM_DRINK}, {-2,-2, ITEM_DRINK},  {-1,-2, ITEM_DRINK},  { 0,-2, ITEM_DRINK},
        {-3,-3, ITEM_DRINK}, {-2,-3, ITEM_DRINK},  {-1,-3, ITEM_DRINK},  { 0,-3, ITEM_DRINK},
        // spare empty barrels — directly west of cart
        {-1,-1, ITEM_BARREL}, { 0,-1, ITEM_BARREL},
    };
    for (int i = 0; i < 18; i++) {
        int ix = cartX + kBarrels[i].dx;
        int iy = cartY + kBarrels[i].dy;
        if (!mapInBounds(ix, iy) || !mapPassable(ix, iy)) continue;
        mapAddItem(ix, iy, kBarrels[i].item);
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
        // Mark tomb for digging; tiles are designated progressively each tick
        // (same tile-by-tile approach as workshops) so no unreachable tasks.
        mapSetRoomRect(TOMB_X1, TOMB_Y1, TOMB_X2, TOMB_Y2, ROOM_TOMB);
        gTombDug = true;
    }

    if (!taskExistsCraft(CRAFT_COFFIN)) {
        int wx = (sHX1+sHX2)/2;
        int wy = (sHY1+sHY2)/2;
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

static int hallCx() { return (sHX1+sHX2)/2; }
static int hallCy() { return (sHY1+sHY2)/2; }

// Craft location: use woodworker shop if dug, else hall centre
static void getCraftLoc(int* wx, int* wy) {
    if (woodWkReady()) { *wx = woodWkX(); *wy = woodWkY(); }
    else               { *wx = hallCx();  *wy = hallCy(); }
}

// ----------------------------------------------------------------
//  Stage: furnish main hall
// ----------------------------------------------------------------
static void manageFurnishHall() {
    int wx, wy; getCraftLoc(&wx, &wy);

    int tablesMade = mapCountItemGlobal(ITEM_TABLE_I)
                   + mapCountTileInRect(sHX1,sHY1,sHX2,sHY2,TILE_TABLE);
    int chairsMade = mapCountItemGlobal(ITEM_CHAIR)
                   + mapCountTileInRect(sHX1,sHY1,sHX2,sHY2,TILE_CHAIR);

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
        if (mapGet(gHallTableX[i], gHallTableY[i]) != TILE_TABLE)
            queuePlace(ITEM_TABLE_I, gHallTableX[i], gHallTableY[i]);
    }
    for (int i = 0; i < HALL_CHAIRS; i++) {
        if (mapGet(gHallChairX[i], gHallChairY[i]) != TILE_CHAIR)
            queuePlace(ITEM_CHAIR, gHallChairX[i], gHallChairY[i]);
    }
}

static bool furnishHallComplete() {
    int tables = mapCountTileInRect(sHX1,sHY1,sHX2,sHY2,TILE_TABLE);
    int chairs = mapCountTileInRect(sHX1,sHY1,sHX2,sHY2,TILE_CHAIR);
    return tables >= HALL_TABLES && chairs >= HALL_CHAIRS;
}

// ----------------------------------------------------------------
//  Stage: furnish bedrooms — 6 beds, one per small room
// ----------------------------------------------------------------
// gBedX/gBedY are filled by computeLayout() using sBX1, sBY1, sBY2

static void manageFurnishBed() {
    int wx, wy; getCraftLoc(&wx, &wy);

    int placed  = mapCountTileInRect(sBX1,sBY1,sBX2,sBY2,TILE_BED);
    int inStock = mapCountItemGlobal(ITEM_BED_I);

    if (!taskExistsCraft(CRAFT_BED)) {
        // Only queue as many beds as current wood supply can fund
        int woodAvail = mapCountItemGlobal(ITEM_WOOD);
        int canMake   = (CRAFT_WOOD_BED > 0) ? woodAvail / CRAFT_WOOD_BED : BED_COUNT;
        int toQueue   = min(BED_COUNT - placed - inStock, canMake);
        for (int i = 0; i < toQueue; i++) {
            int ti = taskAdd(TASK_CRAFT, wx, wy);
            if (ti >= 0) gTasks[ti].auxType = (uint8_t)CRAFT_BED;
        }
    }

    for (int i = 0; i < BED_COUNT; i++) {
        if (mapGet(gBedX[i], gBedY[i]) != TILE_BED)
            queuePlace(ITEM_BED_I, gBedX[i], gBedY[i]);
    }
}

static bool furnishBedComplete() {
    return mapCountTileInRect(sBX1,sBY1,sBX2,sBY2,TILE_BED) >= BED_COUNT;
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
    // Designate existing trees when wood is low
    if (mapCountItemGlobal(ITEM_WOOD) < LOW_WOOD_THRESHOLD) {
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
    // Regrow a tree on a random surface grass tile every 400 ticks
    if (gTick % 400 == 0) {
        for (int attempt = 0; attempt < 20; attempt++) {
            int tx = random(1, HILL_START_X - 1);
            int ty = random(1, MAP_H - 1);
            if (mapGet(tx, ty) == TILE_GRASS) {
                mapSet(tx, ty, TILE_TREE);
                mapMarkDirty(tx, ty);
                break;
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

// Total workshop rects (7 workshops + 3 farms + barracks + corridor)
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
    n += fortCountRemainingDig(FORT_BAR_X1,   FORT_BAR_Y1,   FORT_BAR_X2,   FORT_BAR_Y2);
    return n;
}

// Designate workshop corridor only — rooms are opened incrementally
// as corridor tiles become passable (see progressWorkshopDig).
static void designateAllWorkshops() {
    fortDesignateRect(FORT_WCORR_X1, FORT_WCORR_Y1, FORT_WCORR_X2, FORT_WCORR_Y2);
}

// Each tick during FS_WORKSHOPS: designate any wall tile inside a room that
// has at least one passable neighbour.  Called every tick so newly-dug tiles
// propagate reachability inward row-by-row with no permanent stalls.
static void progressWorkshopDig() {
    static const struct { int x1,y1,x2,y2; } kRooms[] = {
        {FORT_WS_WOOD_X1,  FORT_WS_WOOD_Y1,  FORT_WS_WOOD_X2,  FORT_WS_WOOD_Y2 },
        {FORT_WS_STILL_X1, FORT_WS_STILL_Y1, FORT_WS_STILL_X2, FORT_WS_STILL_Y2},
        {FORT_WS_KITCH_X1, FORT_WS_KITCH_Y1, FORT_WS_KITCH_X2, FORT_WS_KITCH_Y2},
        {FORT_FARM1_X1,    FORT_FARM1_Y1,    FORT_FARM1_X2,    FORT_FARM1_Y2   },
        {FORT_FARM2_X1,    FORT_FARM2_Y1,    FORT_FARM2_X2,    FORT_FARM2_Y2   },
        {FORT_WS_STONE_X1, FORT_WS_STONE_Y1, FORT_WS_STONE_X2, FORT_WS_STONE_Y2},
        {FORT_WS_SMELT_X1, FORT_WS_SMELT_Y1, FORT_WS_SMELT_X2, FORT_WS_SMELT_Y2},
        {FORT_WS_FORGE_X1, FORT_WS_FORGE_Y1, FORT_WS_FORGE_X2, FORT_WS_FORGE_Y2},
        {FORT_WS_FARM_X1,  FORT_WS_FARM_Y1,  FORT_WS_FARM_X2,  FORT_WS_FARM_Y2 },
        {FORT_FARM3_X1,    FORT_FARM3_Y1,    FORT_FARM3_X2,    FORT_FARM3_Y2   },
        {FORT_BAR_X1,      FORT_BAR_Y1,      FORT_BAR_X2,      FORT_BAR_Y2     },
    };
    static const int kNumRooms = 11;
    static const int8_t DX[4] = {1,-1,0,0};
    static const int8_t DY[4] = {0,0,1,-1};

    for (int ri = 0; ri < kNumRooms; ri++) {
        for (int y = kRooms[ri].y1; y <= kRooms[ri].y2; y++) {
            for (int x = kRooms[ri].x1; x <= kRooms[ri].x2; x++) {
                // Skip already-dug or already-designated tiles
                if (!mapInBounds(x, y)) continue;
                if (mapGet(x, y) != TILE_WALL) continue;
                if (mapDesignated(x, y)) continue;
                // Only designate if at least one neighbour is passable
                bool ok = false;
                for (int d = 0; d < 4 && !ok; d++) {
                    int nx = x + DX[d], ny = y + DY[d];
                    if (mapInBounds(nx, ny) && mapPassable(nx, ny)) ok = true;
                }
                if (ok) {
                    mapDesignate(x, y, true);
                    taskAdd(TASK_DIG, x, y);
                }
            }
        }
    }
}

// Each tick once gTombDug is set: designate reachable tomb wall tiles.
static void progressTombDig() {
    if (!gTombDug) return;
    static const int8_t DX[4] = {1,-1,0,0};
    static const int8_t DY[4] = {0,0,1,-1};
    for (int y = TOMB_Y1; y <= TOMB_Y2; y++) {
        for (int x = TOMB_X1; x <= TOMB_X2; x++) {
            if (!mapInBounds(x, y)) continue;
            if (mapGet(x, y) != TILE_WALL) continue;
            if (mapDesignated(x, y)) continue;
            bool ok = false;
            for (int d = 0; d < 4 && !ok; d++) {
                int nx = x + DX[d], ny = y + DY[d];
                if (mapInBounds(nx, ny) && mapPassable(nx, ny)) ok = true;
            }
            if (ok) { mapDesignate(x, y, true); taskAdd(TASK_DIG, x, y); }
        }
    }
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
    // Set barracks room type
    mapSetRoomRect(FORT_BAR_X1, FORT_BAR_Y1, FORT_BAR_X2, FORT_BAR_Y2, ROOM_BARRACKS);

    // Queue initial barrels at woodworker (food/drink storage)
    {
        int bwx = woodWkX(), bwy = woodWkY();
        for (int i = 0; i < 3; i++) {
            int ti = taskAdd(TASK_CRAFT, bwx, bwy);
            if (ti >= 0) gTasks[ti].auxType = (uint8_t)CRAFT_BARREL;
        }
    }

    // Place door at workshop corridor entrance
    int doorX = sEX1 + 1;
    int doorY = (sEY1 + sEY2) / 2;
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
//  Migrants
// ----------------------------------------------------------------
static void manageMigrants() {
    if (gFortFallen) return;
    if (gTick < MIGRANT_START_TICK) return;
    // Only after entrance is dug (dwarves need somewhere to go)
    if (gFortStage < FS_MAIN_HALL) return;
    // Cap check
    int alive = 0;
    for (int i = 0; i < gNumDwarves; i++) if (!gDwarves[i].dead) alive++;
    if (alive >= MIGRANT_POP_CAP) return;

    // Wave trigger: every MIGRANT_WAVE_INTERVAL ticks after MIGRANT_START_TICK
    uint32_t elapsed = gTick - MIGRANT_START_TICK;
    if (elapsed % MIGRANT_WAVE_INTERVAL != 0) return;

    int count = MIGRANT_WAVE_MIN + random(0, MIGRANT_WAVE_MAX - MIGRANT_WAVE_MIN + 1);
    count = min(count, MIGRANT_POP_CAP - alive);
    if (count > 0) dwarfSpawnMigrants(count);
}

// ----------------------------------------------------------------
//  Barrel sync — keeps physical barrel items aligned with supply levels.
//  Each ITEM_FOOD = 1 full food barrel (BARREL_CAPACITY units).
//  Each ITEM_DRINK = 1 full drink barrel.
//  Each ITEM_BARREL = empty barrel (can be filled by either).
//  Foraging/production is capped by total available barrel space.
// ----------------------------------------------------------------
static void syncBarrels() {
    int neededFood  = gFoodSupply  / BARREL_CAPACITY;
    int neededDrink = gDrinkSupply / BARREL_CAPACITY;
    int haveFood    = mapCountItemGlobal(ITEM_FOOD);
    int haveDrink   = mapCountItemGlobal(ITEM_DRINK);

    // Convert excess food barrels to empty
    while (haveFood > neededFood) {
        if (!mapConsumeFromStockpile(ITEM_FOOD, 1)) break;
        int sx, sy;
        if (stockpileFindSlot(&sx, &sy)) mapAddItem(sx, sy, ITEM_BARREL);
        haveFood--;
    }
    // Fill empty barrels with food if needed
    while (haveFood < neededFood) {
        if (!mapConsumeFromStockpile(ITEM_BARREL, 1)) break;
        int sx, sy;
        if (stockpileFindSlot(&sx, &sy)) mapAddItem(sx, sy, ITEM_FOOD);
        haveFood++;
    }

    // Convert excess drink barrels to empty
    while (haveDrink > neededDrink) {
        if (!mapConsumeFromStockpile(ITEM_DRINK, 1)) break;
        int sx, sy;
        if (stockpileFindSlot(&sx, &sy)) mapAddItem(sx, sy, ITEM_BARREL);
        haveDrink--;
    }
    // Fill empty barrels with drink if needed
    while (haveDrink < neededDrink) {
        if (!mapConsumeFromStockpile(ITEM_BARREL, 1)) break;
        int sx, sy;
        if (stockpileFindSlot(&sx, &sy)) mapAddItem(sx, sy, ITEM_DRINK);
        haveDrink++;
    }
}

// ----------------------------------------------------------------
//  Wagon demolition — once the wagon area is cleared of items,
//  queue a chop task to break it down into wood.
// ----------------------------------------------------------------
static void manageWagonDemolition() {
    if (sWagonDemolished) return;
    // Find the cart tile
    int cartX = -1, cartY = -1;
    for (int y = 0; y < MAP_H && cartX < 0; y++)
        for (int x = 0; x < HILL_START_X && cartX < 0; x++)
            if (gMap[y][x].type == TILE_CART) { cartX = x; cartY = y; }
    if (cartX < 0) { sWagonDemolished = true; return; }

    // Check if area within 4 tiles has no items
    bool hasItems = false;
    for (int dy = -4; dy <= 4 && !hasItems; dy++)
        for (int dx = -4; dx <= 4 && !hasItems; dx++) {
            int nx = cartX+dx, ny = cartY+dy;
            if (mapInBounds(nx,ny) && gMap[ny][nx].item != ITEM_NONE) hasItems = true;
        }
    if (hasItems) return;

    // Queue chop task on the cart tile
    if (!taskExistsAt(cartX, cartY, TASK_CHOP)) {
        mapDesignate(cartX, cartY, true);
        taskAdd(TASK_CHOP, cartX, cartY);
        sWagonDemolished = true;  // prevent re-queuing
        Serial.println("Wagon ready for demolition");
    }
}

// ----------------------------------------------------------------
//  Temple management — dug after workshops complete; shrine crafted once dug
// ----------------------------------------------------------------
static void progressTempleDig() {
    if (!sTempleDug) {
        // Designate temple room and passage when reachable
        static const int8_t DX[4] = {1,-1,0,0};
        static const int8_t DY[4] = {0,0,1,-1};
        // Room tiles
        for (int y = FORT_TEMPLE_Y1; y <= FORT_TEMPLE_Y2; y++) {
            for (int x = FORT_TEMPLE_X1; x <= FORT_TEMPLE_X2; x++) {
                if (!mapInBounds(x,y) || mapGet(x,y) != TILE_WALL || mapDesignated(x,y)) continue;
                bool adj = false;
                for (int d = 0; d < 4 && !adj; d++)
                    if (mapInBounds(x+DX[d],y+DY[d]) && mapPassable(x+DX[d],y+DY[d])) adj = true;
                if (adj) { mapDesignate(x,y,true); taskAdd(TASK_DIG,x,y); }
            }
        }
        // Passage tiles
        for (int y = FORT_TEMPLE_PASS_Y1; y <= FORT_TEMPLE_PASS_Y2; y++) {
            int x = FORT_TEMPLE_PASS_X;
            if (!mapInBounds(x,y) || mapGet(x,y) != TILE_WALL || mapDesignated(x,y)) continue;
            bool adj = false;
            for (int d = 0; d < 4 && !adj; d++)
                if (mapInBounds(x+DX[d],y+DY[d]) && mapPassable(x+DX[d],y+DY[d])) adj = true;
            if (adj) { mapDesignate(x,y,true); taskAdd(TASK_DIG,x,y); }
        }
        // Done when all dug
        if (fortCountRemainingDig(FORT_TEMPLE_X1, FORT_TEMPLE_Y1, FORT_TEMPLE_X2, FORT_TEMPLE_Y2) == 0
            && fortCountRemainingDig(FORT_TEMPLE_PASS_X, FORT_TEMPLE_PASS_Y1,
                                     FORT_TEMPLE_PASS_X, FORT_TEMPLE_PASS_Y2) == 0) {
            mapSetRoomRect(FORT_TEMPLE_X1, FORT_TEMPLE_Y1, FORT_TEMPLE_X2, FORT_TEMPLE_Y2, ROOM_TEMPLE);
            sTempleDug = true;
            Serial.println("Temple complete");
        }
    }

    // Queue shrine craft + placement once temple is dug
    if (sTempleDug && !sShrineQueued) {
        int mx = (FORT_WS_STONE_X1+FORT_WS_STONE_X2)/2;
        int my = (FORT_WS_STONE_Y1+FORT_WS_STONE_Y2)/2;
        if (mapPassable(mx, my) && mapCountItemGlobal(ITEM_STONE) >= 2) {
            int ti = taskAdd(TASK_CRAFT, mx, my);
            if (ti >= 0) gTasks[ti].auxType = (uint8_t)CRAFT_SHRINE;
            // Place shrine in centre of temple
            int shrineX = (FORT_TEMPLE_X1+FORT_TEMPLE_X2)/2;
            int shrineY = (FORT_TEMPLE_Y1+FORT_TEMPLE_Y2)/2;
            if (!taskExistsPlaceFurn(shrineX, shrineY))
                queuePlace(ITEM_SHRINE, shrineX, shrineY);
            sShrineQueued = true;
        }
    }
}

// ----------------------------------------------------------------
//  Bin management — craft and place bins in stockpile
// ----------------------------------------------------------------
static void manageBins() {
    int wx, wy; getCraftLoc(&wx, &wy);
    // Count placed bins
    int placed = mapCountTileInRect(gStockX1, gStockY1, gStockX2, gStockY2, TILE_BIN);
    int inStock = mapCountItemGlobal(ITEM_BIN);
    if (placed + inStock < 6 && mapCountItemGlobal(ITEM_WOOD) >= 2
        && !taskExistsCraft(CRAFT_BIN)) {
        int ti = taskAdd(TASK_CRAFT, wx, wy);
        if (ti >= 0) gTasks[ti].auxType = (uint8_t)CRAFT_BIN;
    }
    // Place any unplaced bins in stockpile
    if (inStock > 0) {
        for (int y = gStockY1; y <= gStockY2 && inStock > 0; y++) {
            for (int x = gStockX1; x <= gStockX2 && inStock > 0; x++) {
                if (!mapInBounds(x,y)) continue;
                if (mapGet(x,y) == TILE_FLOOR && gMap[y][x].item == ITEM_NONE
                    && !taskExistsPlaceFurn(x, y)) {
                    queuePlace(ITEM_BIN, x, y);
                    inStock--;
                }
            }
        }
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
    gFarmsActive       = false;
    sWagonDemolished   = false;
    sTempleDug         = false;
    sShrineQueued      = false;
    gFortFallen        = false;
    gFortFallReason[0] = '\0';
    gFortWon   = false;
    gDoneTick  = 0;
    strncpy(gStageName, "Arriving", sizeof(gStageName) - 1);
    computeLayout();   // randomize room positions from the already-seeded RNG
}

// ----------------------------------------------------------------
//  Main tick
// ----------------------------------------------------------------
void fortPlanTick() {
    gTick++;

    // Periodic food from surface foraging, drink from stream (barrel-capped)
    if (gTick % FORAGE_FOOD_INTERVAL == 0) {
        int barrelCap = (mapCountItemGlobal(ITEM_FOOD) + mapCountItemGlobal(ITEM_BARREL))
                        * BARREL_CAPACITY;
        if (barrelCap > 0)
            gFoodSupply = min(gFoodSupply + FORAGE_FOOD_AMOUNT, barrelCap);
    }
    if (gTick % COLLECT_DRINK_INTERVAL == 0) {
        int barrelCap = (mapCountItemGlobal(ITEM_DRINK) + mapCountItemGlobal(ITEM_BARREL))
                        * BARREL_CAPACITY;
        if (barrelCap > 0)
            gDrinkSupply = min(gDrinkSupply + COLLECT_DRINK_AMOUNT, barrelCap);
    }

    // Sync physical barrel items with supply levels (every 5 ticks)
    // syncBarrels disabled: barrel items are cosmetic storage markers;
    // supply levels are managed abstractly by foraging/collect intervals.

    // Wagon demolition (once all starting items are hauled away)
    if (gTick % 20 == 0 && gFortStage >= FS_STOCKPILE) manageWagonDemolition();

    manageWoodSupply();
    manageBurials();
    manageMigrants();
    progressTombDig();  // designate reachable tomb tiles each tick
    if (gFortStage >= FS_DONE) progressTempleDig();

    // Fishing: queue tasks at stream tiles when food reserves are below half
    if (gFoodSupply < MAX_FOOD_SUPPLY / 2) {
        int fishCount = 0;
        for (int i = 0; i < gTaskCount; i++)
            if (!gTasks[i].done && gTasks[i].type == TASK_FISH) fishCount++;
        if (fishCount < MAX_CONCURRENT_FISH) {
            // Find water tiles spread across map height; add up to (limit - current) tasks
            int needed = MAX_CONCURRENT_FISH - fishCount;
            int step   = MAP_H / (MAX_CONCURRENT_FISH + 1);
            for (int slot = 0; slot < MAX_CONCURRENT_FISH && needed > 0; slot++) {
                int baseY = step * (slot + 1);
                for (int dy = 0; dy < MAP_H && needed > 0; dy++) {
                    int wy = (baseY + dy) % MAP_H;
                    bool found = false;
                    for (int wx = 0; wx < HILL_START_X && !found; wx++) {
                        if (mapGet(wx, wy) == TILE_WATER && !taskExistsAt(wx, wy, TASK_FISH)) {
                            taskAdd(TASK_FISH, wx, wy);
                            needed--;
                            found = true;
                        }
                    }
                    if (found) break; // move to next slot
                }
            }
        }
    }

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
        fortDesignateRect(sHX1, sHY1, sHX2, sHY2);
        break;

    case FS_MAIN_HALL:
        if (fortCountRemainingDig(sHX1, sHY1, sHX2, sHY2) > 0) break;
        mapSetRoomRect(sHX1, sHY1, sHX2, sHY2, ROOM_HALL);
        // Temporary stockpile in hall while real one is being dug
        gStockX1=sHX1+1; gStockY1=sHY1+1;
        gStockX2=sHX2-1; gStockY2=sHY2-1;
        requeueWoodHauls();
        gFortStage = FS_N_CORRIDOR;
        strncpy(gStageName, "N. Corridor", sizeof(gStageName)-1);
        fortDesignateRect(sNX1, sNY1, sNX2, sNY2);
        break;

    case FS_N_CORRIDOR:
        if (fortCountRemainingDig(sNX1, sNY1, sNX2, sNY2) > 0) break;
        mapSetRoomRect(sNX1, sNY1, sNX2, sNY2, ROOM_HALL);
        gFortStage = FS_STOCKPILE;
        strncpy(gStageName, "Stockpile Room", sizeof(gStageName)-1);
        fortDesignateRect(sPX1, sPY1, sPX2, sPY2);
        break;

    case FS_STOCKPILE:
        if (fortCountRemainingDig(sPX1, sPY1, sPX2, sPY2) > 0) break;
        mapSetRoomRect(sPX1, sPY1, sPX2, sPY2, ROOM_STOCKPILE);
        gStockX1=sPX1; gStockY1=sPY1;
        gStockX2=sPX2; gStockY2=sPY2;
        requeueWoodHauls();
        gFortStage = FS_S_CORRIDOR;
        strncpy(gStageName, "S. Corridor", sizeof(gStageName)-1);
        fortDesignateRect(sSX1, sSY1, sSX2, sSY2);
        break;

    case FS_S_CORRIDOR:
        if (fortCountRemainingDig(sSX1, sSY1, sSX2, sSY2) > 0) break;
        mapSetRoomRect(sSX1, sSY1, sSX2, sSY2, ROOM_HALL);
        gFortStage = FS_BEDROOMS;
        strncpy(gStageName, "Bedrooms", sizeof(gStageName)-1);
        // Connecting passage from S. Corridor (single column, aligned with hall centre)
        fortDesignateRect(sHallCx, sBY1, sHallCx, sBCY - 1);
        // East-west bedroom corridor
        fortDesignateRect(sBX1, sBCY, sBX2, sBCY);
        // 3 north rooms (2×2 interior each; dividing walls stay solid)
        fortDesignateRect(sBX1,   sBY1, sBX1+1, sBCY-1);
        fortDesignateRect(sBX1+4, sBY1, sBX1+5, sBCY-1);
        fortDesignateRect(sBX1+7, sBY1, sBX1+8, sBCY-1);
        // 3 south rooms
        fortDesignateRect(sBX1,   sBCY+1, sBX1+1, sBY2);
        fortDesignateRect(sBX1+4, sBCY+1, sBX1+5, sBY2);
        fortDesignateRect(sBX1+7, sBCY+1, sBX1+8, sBY2);
        break;

    case FS_BEDROOMS: {
        // Done when all 8 sub-rects are fully dug
        int bedLeft = 0;
        bedLeft += fortCountRemainingDig(sHallCx, sBY1, sHallCx, sBCY-1);
        bedLeft += fortCountRemainingDig(sBX1, sBCY, sBX2, sBCY);
        bedLeft += fortCountRemainingDig(sBX1,   sBY1, sBX1+1, sBCY-1);
        bedLeft += fortCountRemainingDig(sBX1+4, sBY1, sBX1+5, sBCY-1);
        bedLeft += fortCountRemainingDig(sBX1+7, sBY1, sBX1+8, sBCY-1);
        bedLeft += fortCountRemainingDig(sBX1,   sBCY+1, sBX1+1, sBY2);
        bedLeft += fortCountRemainingDig(sBX1+4, sBCY+1, sBX1+5, sBY2);
        bedLeft += fortCountRemainingDig(sBX1+7, sBCY+1, sBX1+8, sBY2);
        if (bedLeft > 0) break;
        mapSetRoomRect(sBX1, sBY1, sBX2, sBY2, ROOM_BEDROOM);
        // Skip furnishing — beds need the woodworker workshop. Dig east corridor next.
        gFortStage = FS_E_CORRIDOR;
        strncpy(gStageName, "E. Corridor", sizeof(gStageName)-1);
        fortDesignateRect(sEX1, sEY1, sEX2, sEY2);
        break;
    }

    case FS_E_CORRIDOR:
        if (fortCountRemainingDig(sEX1, sEY1, sEX2, sEY2) > 0) break;
        mapSetRoomRect(sEX1, sEY1, sEX2, sEY2, ROOM_HALL);
        gFortStage = FS_WORKSHOPS;
        strncpy(gStageName, "Workshop Wing", sizeof(gStageName)-1);
        designateAllWorkshops();
        break;

    case FS_WORKSHOPS:
        progressWorkshopDig(); // open rooms as corridor is dug
        if (workshopTotalRemainingDig() > 0) break;
        finaliseWorkshops();
        gFortStage = FS_DONE;
        gDoneTick  = gTick;
        strncpy(gStageName, "Spring", sizeof(gStageName)-1);
        break;

    case FS_DONE:
        manageFurnishBed();   // finish any remaining beds (may have timed out earlier)
        manageFurnishHall();  // hall furniture crafted at woodworker workshop
        manageFarms();
        manageMushProcessing();
        manageBins();
        // Queue bone broth at kitchen if food low and bones available
        if (gFoodSupply < MAX_FOOD_SUPPLY / 2
            && mapCountItemGlobal(ITEM_BONE) >= 2
            && !taskExistsCraft(CRAFT_BONE_BROTH)) {
            int kx = (FORT_WS_KITCH_X1+FORT_WS_KITCH_X2)/2;
            int ky = (FORT_WS_KITCH_Y1+FORT_WS_KITCH_Y2)/2;
            if (mapPassable(kx, ky)) {
                int ti = taskAdd(TASK_CRAFT, kx, ky);
                if (ti >= 0) gTasks[ti].auxType = (uint8_t)CRAFT_BONE_BROTH;
            }
        }
        // Queue stone mugs at mason if stone available
        if (mapCountItemGlobal(ITEM_STONE) >= CRAFT_STONE_MUG_COST
            && !taskExistsCraft(CRAFT_STONE_MUG)) {
            int mx = (FORT_WS_STONE_X1+FORT_WS_STONE_X2)/2;
            int my = (FORT_WS_STONE_Y1+FORT_WS_STONE_Y2)/2;
            if (mapPassable(mx, my)) {
                int ti = taskAdd(TASK_CRAFT, mx, my);
                if (ti >= 0) gTasks[ti].auxType = (uint8_t)CRAFT_STONE_MUG;
            }
        }
        // Keep crafting barrels at woodworker if wood available (for food/drink storage)
        if (mapCountItemGlobal(ITEM_WOOD) > CRAFT_WOOD_BARREL * 3
            && !taskExistsCraft(CRAFT_BARREL)) {
            int wx2 = woodWkX(), wy2 = woodWkY();
            if (mapPassable(wx2, wy2)) {
                int ti = taskAdd(TASK_CRAFT, wx2, wy2);
                if (ti >= 0) gTasks[ti].auxType = (uint8_t)CRAFT_BARREL;
            }
        }
        // Season tracking and win condition.
        // Victory requires SEASONS_TO_WIN total seasons from embark (not just post-completion),
        // so a slow build still wins once total game time crosses the threshold.
        {
            static const char* kSeasons[] = {"Spring","Summer","Autumn","Winter"};
            uint32_t totalSeasons = gTick / TICKS_PER_SEASON;
            if (totalSeasons < (uint32_t)SEASONS_TO_WIN) {
                // Show season name cycling from the post-completion tick
                uint32_t postSeason = (gTick - gDoneTick) / TICKS_PER_SEASON;
                strncpy(gStageName, kSeasons[postSeason % 4], sizeof(gStageName)-1);
            } else {
                gFortWon = true;
            }
        }
        break;
    }
}
