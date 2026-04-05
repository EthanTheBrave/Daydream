#pragma once
#include "types.h"

extern Task  gTasks[MAX_TASKS];
extern int   gTaskCount;

void tasksInit();

// Add a task; returns index, or -1 on full.
// Duplicate dig/chop tasks at the same tile are silently ignored.
int  taskAdd(TaskType type, int x, int y, int destX = -1, int destY = -1);

void taskComplete(int idx);
void taskUnclaim(int idx);

// Find nearest unclaimed, ready task to (cx,cy) in priority order.
int  taskFindBest(int cx, int cy);

// Cancel all tasks at tile (x,y)
void taskCancelAt(int x, int y);

bool taskExistsAt(int x, int y, TaskType type);
bool taskExistsCraft(CraftType ct);                   // any pending craft of this type?
bool taskExistsPlaceFurn(int destX, int destY);       // place task targeting this dest?
bool taskIsReady(int idx);                            // false if PLACE_FURN but item gone
