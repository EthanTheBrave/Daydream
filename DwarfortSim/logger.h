#pragma once

// ----------------------------------------------------------------
//  Run logger + history
//
//  Writes one CSV log per run (rolling 10-file window) and a
//  persistent history.bin with all-time records.
//
//  Arduino: files on SD card root  (logN.csv, history.bin, logidx.bin)
//  PC sim:  files in working directory
// ----------------------------------------------------------------

void loggerInit();                      // call after saveInit()
void loggerBeginRun();                  // call after world is initialised
void logSnapshot();                     // call every 500 ticks
void logStageChange(const char* stage); // call when gStageName changes
void logFortFall();                     // call before deleteSave/renderFailure
