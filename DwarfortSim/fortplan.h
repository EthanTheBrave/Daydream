#pragma once
#include "types.h"

extern FortStage gFortStage;
extern uint32_t  gTick;
extern char      gStageName[24];

// Death tracking — fort planner checks these each tick
extern int  gDeadUnburied;    // number of corpses awaiting burial
extern int  gTombSlotUsed;    // coffin slots placed in tomb so far
extern bool gTombDug;         // has the tomb room been dug?

// Fortress failure
extern bool gFortFallen;
extern char gFortFallReason[48];  // e.g. "starvation", "dehydration"

extern uint32_t gDoneTick;   // gTick when FS_DONE was entered
extern uint8_t  gSeason;     // 0=Spring 1=Summer 2=Autumn 3=Winter

void fortPlanInit();
void fortPlanTick();

// Used by map init to place cart items and create initial haul tasks
void fortPlaceCart(int cartX, int cartY);

int  fortCountRemainingDig(int x1, int y1, int x2, int y2);
void fortDesignateRect(int x1, int y1, int x2, int y2);

// Called by dwarves.cpp when a dwarf dies — triggers burial pipeline
void fortNotifyDeath(int corpseX, int corpseY);
