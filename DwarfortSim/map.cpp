#include "map.h"
#include "tasks.h"
#include <Arduino.h>
#include <string.h>

Tile gMap[MAP_H][MAP_W];
static bool gDirty[MAP_H][MAP_W];

int gStockX1 = 2, gStockY1 = 8, gStockX2 = 8, gStockY2 = 20; // surface pile

// ----------------------------------------------------------------
void mapInit(uint32_t seed) {
    randomSeed(seed);

    // Fill everything as solid wall, unrevealed
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            gMap[y][x] = { TILE_WALL, false, ITEM_NONE, 0, ROOM_NONE, false };
            gDirty[y][x] = true;
        }
    }

    // Surface: left of HILL_START_X is grass
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < HILL_START_X; x++) {
            gMap[y][x].type = TILE_GRASS;
        }
    }

    // Uneven hillface: vary the boundary by 1–2 tiles
    for (int y = 0; y < MAP_H; y++) {
        int jag = random(0, 3); // 0, 1, or 2 extra grass tiles
        for (int x = HILL_START_X; x < HILL_START_X + jag && x < MAP_W; x++) {
            gMap[y][x].type = TILE_GRASS;
        }
    }

    // Scatter trees on the surface
    for (int i = 0; i < 10; i++) {
        int tx = random(1, HILL_START_X - 1);
        int ty = random(1, MAP_H - 1);
        gMap[ty][tx].type = TILE_TREE;
    }

    // Scatter shrubs and flowers on grass tiles
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < HILL_START_X; x++) {
            if (gMap[y][x].type != TILE_GRASS) continue;
            int r = random(0, 12);
            if      (r == 0) gMap[y][x].type = TILE_SHRUB;
            else if (r == 1) gMap[y][x].type = TILE_FLOWER;
        }
    }

    // Small stream (optional visual detail)
    {
        int sx = 4;
        for (int y = 0; y < MAP_H; y++) {
            gMap[y][sx].type = TILE_WATER;
            if (random(0, 3) == 0) sx += (random(0, 2) ? 1 : -1);
            sx = constrain(sx, 1, HILL_START_X - 2);
        }
    }

    // Reveal all surface tiles and the first column of hill wall
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            if (gMap[y][x].type != TILE_WALL) {
                gMap[y][x].revealed = true;
            }
        }
        // Reveal the hillface wall tiles (adjacent to grass)
        for (int x = 0; x < MAP_W; x++) {
            if (gMap[y][x].type == TILE_WALL) {
                // Check if any 4-neighbour is non-wall
                bool adj = false;
                if (x > 0       && gMap[y][x-1].type != TILE_WALL) adj = true;
                if (x < MAP_W-1 && gMap[y][x+1].type != TILE_WALL) adj = true;
                if (y > 0       && gMap[y-1][x].type != TILE_WALL) adj = true;
                if (y < MAP_H-1 && gMap[y+1][x].type != TILE_WALL) adj = true;
                if (adj) gMap[y][x].revealed = true;
            }
        }
    }

    memset(gDirty, true, sizeof(gDirty));
}

// ----------------------------------------------------------------
bool mapInBounds(int x, int y) {
    return x >= 0 && x < MAP_W && y >= 0 && y < MAP_H;
}

bool mapPassable(int x, int y) {
    if (!mapInBounds(x, y)) return false;
    TileType t = gMap[y][x].type;
    return t == TILE_FLOOR  || t == TILE_GRASS  || t == TILE_DOOR
        || t == TILE_RAMP   || t == TILE_STAIR
        || t == TILE_SHRUB  || t == TILE_FLOWER || t == TILE_FARM;
}

bool mapPassableOrTarget(int x, int y) {
    if (!mapInBounds(x, y)) return false;
    return mapPassable(x, y) || gMap[y][x].type == TILE_WALL;
}

TileType mapGet(int x, int y) {
    if (!mapInBounds(x, y)) return TILE_WALL;
    return gMap[y][x].type;
}

void mapSet(int x, int y, TileType t) {
    if (!mapInBounds(x, y)) return;
    gMap[y][x].type = t;
    gMap[y][x].revealed = true;
    gDirty[y][x] = true;
    // When a wall is dug out, reveal the 4 neighbours so tunnel walls show
    if (t == TILE_FLOOR) {
        mapRevealAdjacent(x, y);
    }
}

bool mapDesignated(int x, int y) {
    if (!mapInBounds(x, y)) return false;
    return gMap[y][x].designated;
}

void mapDesignate(int x, int y, bool v) {
    if (!mapInBounds(x, y)) return;
    gMap[y][x].designated = v;
    gDirty[y][x] = true;
}

// ----------------------------------------------------------------
//  Items
// ----------------------------------------------------------------
bool mapHasItem(int x, int y, ItemType it) {
    if (!mapInBounds(x, y)) return false;
    return gMap[y][x].item == it && gMap[y][x].itemCount > 0;
}

void mapAddItem(int x, int y, ItemType it) {
    if (!mapInBounds(x, y)) return;
    if (gMap[y][x].item == ITEM_NONE) {
        gMap[y][x].item      = it;
        gMap[y][x].itemCount = 1;
    } else if (gMap[y][x].item == it) {
        gMap[y][x].itemCount++;
    }
    gDirty[y][x] = true;
}

bool mapRemoveItem(int x, int y, ItemType it) {
    if (!mapInBounds(x, y)) return false;
    if (gMap[y][x].item != it || gMap[y][x].itemCount == 0) return false;
    gMap[y][x].itemCount--;
    if (gMap[y][x].itemCount == 0) gMap[y][x].item = ITEM_NONE;
    gDirty[y][x] = true;
    return true;
}

bool mapFindItem(int cx, int cy, ItemType it, int* ox, int* oy) {
    int bestDist = 9999;
    bool found = false;
    for (int r = 1; r < MAP_W; r++) {
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if (abs(dx) != r && abs(dy) != r) continue; // ring only
                int nx = cx + dx, ny = cy + dy;
                if (!mapInBounds(nx, ny)) continue;
                if (gMap[ny][nx].item == it && gMap[ny][nx].itemCount > 0) {
                    int d = abs(dx) + abs(dy);
                    if (d < bestDist) {
                        bestDist = d;
                        *ox = nx; *oy = ny;
                        found = true;
                    }
                }
            }
        }
        if (found) return true;
    }
    return false;
}

// ----------------------------------------------------------------
//  Dirty flags
// ----------------------------------------------------------------
void mapMarkDirty(int x, int y) {
    if (mapInBounds(x, y)) gDirty[y][x] = true;
}

bool mapIsDirty(int x, int y) {
    if (!mapInBounds(x, y)) return false;
    return gDirty[y][x];
}

void mapClearAllDirty() {
    memset(gDirty, false, sizeof(gDirty));
}

// ----------------------------------------------------------------
//  Stockpile helpers
// ----------------------------------------------------------------
bool stockpileFindSlot(int* ox, int* oy) {
    for (int y = gStockY1; y <= gStockY2; y++) {
        for (int x = gStockX1; x <= gStockX2; x++) {
            if (!mapInBounds(x, y)) continue;
            if (!mapPassable(x, y) || gMap[y][x].item != ITEM_NONE) continue;
            // Skip slots already targeted by an active haul/place task
            bool reserved = false;
            for (int i = 0; i < gTaskCount; i++) {
                if (gTasks[i].done) continue;
                if ((gTasks[i].type == TASK_HAUL || gTasks[i].type == TASK_PLACE_FURN)
                    && gTasks[i].destX == x && gTasks[i].destY == y) {
                    reserved = true; break;
                }
            }
            if (!reserved) { *ox = x; *oy = y; return true; }
        }
    }
    return false;
}

// ----------------------------------------------------------------
//  Crafting material helpers
// ----------------------------------------------------------------
int mapCountItemGlobal(ItemType it) {
    int n = 0;
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            if (gMap[y][x].item == it) n += gMap[y][x].itemCount;
    return n;
}

int mapCountItemInZone(int x1, int y1, int x2, int y2, ItemType it) {
    int n = 0;
    for (int y = y1; y <= y2; y++)
        for (int x = x1; x <= x2; x++)
            if (mapInBounds(x,y) && gMap[y][x].item == it) n += gMap[y][x].itemCount;
    return n;
}

bool mapConsumeFromStockpile(ItemType it, int count) {
    // Check global availability first
    if (mapCountItemGlobal(it) < count) return false;
    int remaining = count;
    // Prefer consuming from stockpile zone
    for (int y = gStockY1; y <= gStockY2 && remaining > 0; y++)
        for (int x = gStockX1; x <= gStockX2 && remaining > 0; x++)
            while (remaining > 0 && mapRemoveItem(x, y, it)) remaining--;
    // Fall back to global search
    for (int y = 0; y < MAP_H && remaining > 0; y++)
        for (int x = 0; x < MAP_W && remaining > 0; x++)
            while (remaining > 0 && mapRemoveItem(x, y, it)) remaining--;
    return remaining == 0;
}

int mapCountTileInRect(int x1, int y1, int x2, int y2, TileType t) {
    int n = 0;
    for (int y = y1; y <= y2; y++)
        for (int x = x1; x <= x2; x++)
            if (mapInBounds(x,y) && gMap[y][x].type == t) n++;
    return n;
}

// ----------------------------------------------------------------
//  Room marking
// ----------------------------------------------------------------
// ----------------------------------------------------------------
//  Fog of war
// ----------------------------------------------------------------
bool mapIsRevealed(int x, int y) {
    if (!mapInBounds(x, y)) return false;
    return gMap[y][x].revealed;
}

void mapReveal(int x, int y) {
    if (!mapInBounds(x, y)) return;
    if (gMap[y][x].revealed) return;
    gMap[y][x].revealed = true;
    gDirty[y][x] = true;
}

void mapRevealAdjacent(int x, int y) {
    mapReveal(x,   y);
    mapReveal(x-1, y);
    mapReveal(x+1, y);
    mapReveal(x,   y-1);
    mapReveal(x,   y+1);
}

void mapSetRoom(int x, int y, RoomType r) {
    if (!mapInBounds(x, y)) return;
    gMap[y][x].roomType = r;
    gDirty[y][x] = true;
}

void mapSetRoomRect(int x1, int y1, int x2, int y2, RoomType r) {
    for (int y = y1; y <= y2; y++)
        for (int x = x1; x <= x2; x++)
            mapSetRoom(x, y, r);
}
