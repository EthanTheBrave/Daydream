#pragma once
#include "types.h"

extern Tile gMap[MAP_H][MAP_W];

void mapInit(uint32_t seed);

bool mapInBounds(int x, int y);
bool mapPassable(int x, int y);          // can a dwarf walk here?
bool mapPassableOrTarget(int x, int y);  // passable, or the target of a dig

TileType mapGet(int x, int y);
void     mapSet(int x, int y, TileType t);

bool     mapDesignated(int x, int y);
void     mapDesignate(int x, int y, bool v);

// Items
bool     mapHasItem(int x, int y, ItemType it);
void     mapAddItem(int x, int y, ItemType it);
bool     mapRemoveItem(int x, int y, ItemType it);
// Find nearest tile that has 'it', searching a radius; returns true and sets ox/oy
bool     mapFindItem(int cx, int cy, ItemType it, int* ox, int* oy);

// Dirty-flag system (for incremental rendering)
void     mapMarkDirty(int x, int y);
bool     mapIsDirty(int x, int y);
void     mapClearAllDirty();

// Stockpile zone (set by fort planner)
extern int gStockX1, gStockY1, gStockX2, gStockY2;
// Find an empty floor tile inside the stockpile zone; returns true on success
bool     stockpileFindSlot(int* ox, int* oy);

// Room type marking
void     mapSetRoom(int x, int y, RoomType r);
void     mapSetRoomRect(int x1, int y1, int x2, int y2, RoomType r);

// Crafting material helpers
int      mapCountItemGlobal(ItemType it);             // total on entire map
int      mapCountItemInZone(int x1, int y1, int x2, int y2, ItemType it);
bool     mapConsumeFromStockpile(ItemType it, int count); // remove N from stockpile

// Tile counts in a rect (for furnishing checks)
int      mapCountTileInRect(int x1, int y1, int x2, int y2, TileType t);

// Fog of war
bool     mapIsRevealed(int x, int y);
void     mapReveal(int x, int y);        // reveal one tile
void     mapRevealAdjacent(int x, int y); // reveal tile + 4 neighbours
