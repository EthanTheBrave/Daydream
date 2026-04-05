#pragma once
#include "config.h"
#include <stdint.h>

// BFS from (sx,sy) to (tx,ty) over passable tiles.
// Fills pathX/pathY (up to maxLen steps, not including start).
// Returns step count, or 0 if unreachable.
int pathFind(int sx, int sy, int tx, int ty,
             int8_t* pathX, int8_t* pathY, int maxLen);

// BFS to any passable tile adjacent to (tx,ty).
// Used so a dwarf can stand next to a wall and dig it.
int pathFindAdj(int sx, int sy, int tx, int ty,
                int8_t* pathX, int8_t* pathY, int maxLen);
