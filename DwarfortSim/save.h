#pragma once
#include <stdint.h>
#include <stdbool.h>

// ----------------------------------------------------------------
//  Save / resume support
//  Arduino: binary file on SD card (/save.dat, CS=GPIO5)
//  PC sim:  binary file in working directory (DwarfortSim.sav)
// ----------------------------------------------------------------

void saveInit();      // call in setup() — mounts SD on Arduino, no-op on PC
bool hasSave();       // true if a valid save file exists
bool saveGame();      // write current state; returns false on I/O error
bool loadGame();      // restore state from save; returns false on error/corrupt
void deleteSave();    // remove save file (call on fortress fall)
