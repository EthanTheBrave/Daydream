#include "logger.h"
#include "dwarves.h"
#include "fortplan.h"
#include "config.h"
#include <string.h>

#ifdef ARDUINO
#  include <SD.h>
#  include <Arduino.h>
#  define LOGIDX_PATH  "/logidx.bin"
#  define HIST_PATH    "/history.bin"
#  define LOG_PATH(n)  (n == 0 ? "/log0.csv" : \
                        n == 1 ? "/log1.csv" : \
                        n == 2 ? "/log2.csv" : \
                        n == 3 ? "/log3.csv" : \
                        n == 4 ? "/log4.csv" : \
                        n == 5 ? "/log5.csv" : \
                        n == 6 ? "/log6.csv" : \
                        n == 7 ? "/log7.csv" : \
                        n == 8 ? "/log8.csv" : "/log9.csv")
#else
#  include <stdio.h>
static char sLogPathBuf[32];
#  define LOG_PATH(n)  (snprintf(sLogPathBuf, sizeof(sLogPathBuf), "log%d.csv", (n)), sLogPathBuf)
#  define LOGIDX_PATH  "logidx.bin"
#  define HIST_PATH    "history.bin"
#endif

#define LOG_MAX_FILES  10

// ----------------------------------------------------------------
//  History record (binary, ~64 bytes)
// ----------------------------------------------------------------
static const uint32_t HIST_MAGIC   = 0x48535452; // 'HSTR'
static const uint8_t  HIST_VERSION = 1;

struct HistoryRecord {
    uint32_t magic;
    uint8_t  version;
    uint32_t totalRuns;
    uint32_t bestTicks;
    uint16_t bestPop;
    uint16_t bestSeasons;
    char     lastCause[48];
};

// ----------------------------------------------------------------
//  Module state
// ----------------------------------------------------------------
static uint8_t sSlot    = 0;   // log file slot for the current run (0-9)
static int     sMaxPop  = 0;   // peak alive count this run
static int     sPrevAlive = -1;

// ----------------------------------------------------------------
//  Helpers
// ----------------------------------------------------------------
static const char* seasonName() {
    switch (gSeason) {
        case 0: return "Spring";
        case 1: return "Summer";
        case 2: return "Autumn";
        case 3: return "Winter";
        default: return "?";
    }
}

static int countAlive() {
    int n = 0;
    for (int i = 0; i < gNumDwarves; i++)
        if (!gDwarves[i].dead) n++;
    return n;
}

// ----------------------------------------------------------------
//  SD / file wrappers for log (stays open per run)
// ----------------------------------------------------------------
#ifdef ARDUINO
static File sLogFile;
static void logWrite(const char* line) {
    if (sLogFile) { sLogFile.print(line); sLogFile.flush(); }
}
static void logClose() { if (sLogFile) sLogFile.close(); }
static bool logOpen(const char* path) {
    if (SD.exists(path)) SD.remove(path);
    sLogFile = SD.open(path, FILE_WRITE);
    return (bool)sLogFile;
}
#else
static FILE* sLogFile = nullptr;
static void logWrite(const char* line) {
    if (sLogFile) { fputs(line, sLogFile); fflush(sLogFile); }
}
static void logClose() { if (sLogFile) { fclose(sLogFile); sLogFile = nullptr; } }
static bool logOpen(const char* path) {
    sLogFile = fopen(path, "w");
    return sLogFile != nullptr;
}
#endif

// ----------------------------------------------------------------
//  Read/write the slot index (1-byte file)
// ----------------------------------------------------------------
static uint8_t readSlot() {
#ifdef ARDUINO
    File f = SD.open(LOGIDX_PATH, FILE_READ);
    if (!f) return 0;
    uint8_t v = 0;
    f.read(&v, 1);
    f.close();
    return v % LOG_MAX_FILES;
#else
    FILE* f = fopen(LOGIDX_PATH, "rb");
    if (!f) return 0;
    uint8_t v = 0;
    fread(&v, 1, 1, f);
    fclose(f);
    return v % LOG_MAX_FILES;
#endif
}

static void writeSlot(uint8_t slot) {
    uint8_t v = slot % LOG_MAX_FILES;
#ifdef ARDUINO
    if (SD.exists(LOGIDX_PATH)) SD.remove(LOGIDX_PATH);
    File f = SD.open(LOGIDX_PATH, FILE_WRITE);
    if (f) { f.write(&v, 1); f.close(); }
#else
    FILE* f = fopen(LOGIDX_PATH, "wb");
    if (f) { fwrite(&v, 1, 1, f); fclose(f); }
#endif
}

// ----------------------------------------------------------------
//  Read/write history record
// ----------------------------------------------------------------
static bool readHistory(HistoryRecord* out) {
#ifdef ARDUINO
    File f = SD.open(HIST_PATH, FILE_READ);
    if (!f) return false;
    bool ok = (f.read((uint8_t*)out, sizeof(*out)) == sizeof(*out));
    f.close();
    return ok && out->magic == HIST_MAGIC && out->version == HIST_VERSION;
#else
    FILE* f = fopen(HIST_PATH, "rb");
    if (!f) return false;
    bool ok = (fread(out, 1, sizeof(*out), f) == sizeof(*out));
    fclose(f);
    return ok && out->magic == HIST_MAGIC && out->version == HIST_VERSION;
#endif
}

static void writeHistory(const HistoryRecord* in) {
#ifdef ARDUINO
    if (SD.exists(HIST_PATH)) SD.remove(HIST_PATH);
    File f = SD.open(HIST_PATH, FILE_WRITE);
    if (f) { f.write((const uint8_t*)in, sizeof(*in)); f.close(); }
#else
    FILE* f = fopen(HIST_PATH, "wb");
    if (f) { fwrite(in, 1, sizeof(*in), f); fclose(f); }
#endif
}

// ----------------------------------------------------------------
void loggerInit() {
    sSlot = readSlot();
}

// ----------------------------------------------------------------
void loggerBeginRun() {
    logClose(); // safety: close any previously open log
    sMaxPop   = countAlive();
    sPrevAlive = sMaxPop;

    const char* path = LOG_PATH(sSlot);
    if (!logOpen(path)) return;

    // Advance and persist the slot for the NEXT run
    writeSlot((sSlot + 1) % LOG_MAX_FILES);

    // Header
    char buf[80];
    snprintf(buf, sizeof(buf), "# Run %u\n", (unsigned)(sSlot + 1));
    logWrite(buf);
    logWrite("tick,event,alive,food,drink,season,stage,note\n");

    // Tick-0 snapshot
    snprintf(buf, sizeof(buf), "%lu,start,%d,%d,%d,%s,%s,\n",
             (unsigned long)gTick, sMaxPop, gFoodSupply, gDrinkSupply,
             seasonName(), gStageName);
    logWrite(buf);
}

// ----------------------------------------------------------------
void logSnapshot() {
    int alive = countAlive();
    if (alive > sMaxPop) sMaxPop = alive;

    // Note any deaths since last snapshot
    char note[24] = "";
    if (sPrevAlive > 0 && alive < sPrevAlive) {
        snprintf(note, sizeof(note), "%d died", sPrevAlive - alive);
    }
    sPrevAlive = alive;

    char buf[120];
    snprintf(buf, sizeof(buf), "%lu,snap,%d,%d,%d,%s,%s,%s\n",
             (unsigned long)gTick, alive, gFoodSupply, gDrinkSupply,
             seasonName(), gStageName, note);
    logWrite(buf);
}

// ----------------------------------------------------------------
void logStageChange(const char* stage) {
    int alive = countAlive();
    char buf[120];
    snprintf(buf, sizeof(buf), "%lu,stage,%d,%d,%d,%s,%s,\n",
             (unsigned long)gTick, alive, gFoodSupply, gDrinkSupply,
             seasonName(), stage);
    logWrite(buf);
}

// ----------------------------------------------------------------
void logFortFall() {
    int alive = countAlive();

    // Final log line
    char buf[120];
    snprintf(buf, sizeof(buf), "%lu,fallen,%d,%d,%d,%s,%s,%s\n",
             (unsigned long)gTick, alive, gFoodSupply, gDrinkSupply,
             seasonName(), gStageName, gFortFallReason);
    logWrite(buf);
    logClose();

    // Update history record
    HistoryRecord h;
    if (!readHistory(&h)) {
        h.magic       = HIST_MAGIC;
        h.version     = HIST_VERSION;
        h.totalRuns   = 0;
        h.bestTicks   = 0;
        h.bestPop     = 0;
        h.bestSeasons = 0;
        h.lastCause[0] = '\0';
    }

    h.totalRuns++;
    if (gTick > h.bestTicks)   h.bestTicks   = gTick;
    if (sMaxPop > h.bestPop)   h.bestPop     = (uint16_t)sMaxPop;

    // Count seasons: each season is TICKS_PER_SEASON ticks after FS_DONE
    uint16_t seasons = (gTick > gDoneTick)
                       ? (uint16_t)((gTick - gDoneTick) / TICKS_PER_SEASON)
                       : 0;
    if (seasons > h.bestSeasons) h.bestSeasons = seasons;

    strncpy(h.lastCause, gFortFallReason, sizeof(h.lastCause) - 1);
    h.lastCause[sizeof(h.lastCause) - 1] = '\0';

    writeHistory(&h);
}
