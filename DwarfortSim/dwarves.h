#pragma once
#include "types.h"

extern Dwarf gDwarves[MAX_DWARVES];
extern int   gNumDwarves;

// Global food/drink supplies (simplified — not stored on map tiles)
extern int   gFoodSupply;
extern int   gDrinkSupply;

void dwarfInit(int count, int startX, int startY);

// Advance all dwarves one simulation tick
void dwarvesTick();
