#pragma once
#include "types.h"

extern Dwarf gDwarves[MAX_DWARVES];
extern int   gNumDwarves;

// Global food/drink supplies (simplified — not stored on map tiles)
extern int   gFoodSupply;
extern int   gDrinkSupply;   // stream water
extern int   gBeerSupply;    // brewed beer (drunk preferentially; boosts happiness)

void dwarfInit(int count, int startX, int startY);

// Spawn N new migrants arriving from the left edge
void dwarfSpawnMigrants(int count);

// Advance all dwarves one simulation tick
void dwarvesTick();
