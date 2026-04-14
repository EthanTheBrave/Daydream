// ================================================================
//  DWARFORT SIMULATOR — PC Debug Build
//  Compiles the sketch sources natively for Windows/Linux.
//  Renders to the terminal using ANSI 256-colour escape codes.
//
//  Controls (press in terminal window):
//    q   — quit
//    +   — speed up (halve tick interval)
//    -   — slow down (double tick interval)
//    d   — toggle dwarf debug overlay
// ================================================================

#include "Arduino.h"
#include "TFT_eSPI.h"

#include "../config.h"
#include "../types.h"
#include "../map.h"
#include "../tasks.h"
#include "../dwarves.h"
#include "../fortplan.h"
#include "../animals.h"
#include "../goblins.h"
#include "../renderer.h"
#include "../pathfind.h"
#include "../save.h"
#include "../logger.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#  include <windows.h>
#  include <conio.h>
#else
#  include <unistd.h>
#  include <termios.h>
#  include <fcntl.h>
#endif

// ----------------------------------------------------------------
//  TFT instance — renderer.cpp references this via extern
// ----------------------------------------------------------------
TFT_eSPI tft;

// ----------------------------------------------------------------
//  setup() / loop() — mirrors DwarfortSim.ino exactly
// ----------------------------------------------------------------
static void setup() {
    Serial.begin(115200);
    tft.init();
    tft.setRotation(2);
    tft.fillScreen(0x0000);
    tft.setTextSize(1);
    tft.setTextFont(1);

    fprintf(stderr, "DWARFORT SIMULATOR — PC Debug Build\n");

    loggerInit();
    bool resuming = hasSave();
    if (resuming) {
        fprintf(stderr, "Resuming from save...\n");
        if (!loadGame()) {
            fprintf(stderr, "[save] load failed, starting new game\n");
            deleteSave();
            resuming = false;
        }
    }

    if (!resuming) {
        uint32_t seed = MAP_SEED ? MAP_SEED : (uint32_t)time(NULL);
        fprintf(stderr, "Generating world (seed %u)...\n", (unsigned)seed);
        mapInit(seed);
        tasksInit();
        fortPlanInit();
        animalsInit();
        goblinsInit();

        int spawnX = HILL_START_X - 2;
        int spawnY = MAP_H / 2;
        dwarfInit(NUM_DWARVES, spawnX, spawnY);
        fortPlaceCart(spawnX, spawnY - 5);
    }

    rendererInit();
    renderAll();
    loggerBeginRun();
}

// ----------------------------------------------------------------
//  Console setup
// ----------------------------------------------------------------
static void consoleInit() {
#ifdef _WIN32
    // Enable ANSI escape processing on Windows 10+
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    SetConsoleOutputCP(65001);  // UTF-8 — needed for Unicode glyph output
    // Hide cursor
    CONSOLE_CURSOR_INFO ci = {1, FALSE};
    SetConsoleCursorInfo(hOut, &ci);
#endif
    // Clear screen, hide cursor via ANSI
    printf("\033[2J\033[?25l");
    fflush(stdout);
}

static void consoleCleanup() {
    printf("\033[?25h\033[0m\n"); // show cursor, reset colour
    fflush(stdout);
}

// ----------------------------------------------------------------
//  Non-blocking key check — returns 0 if no key
// ----------------------------------------------------------------
static int keyCheck() {
#ifdef _WIN32
    if (_kbhit()) return _getch();
    return 0;
#else
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    int oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    int ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    return (ch == EOF) ? 0 : ch;
#endif
}

// ----------------------------------------------------------------
//  Debug overlay — printed below the map in the terminal
// ----------------------------------------------------------------
static bool sShowDebug = true;

static const char* stateName(DwarfState s) {
    switch(s) {
        case DS_IDLE:     return "IDLE    ";
        case DS_MOVING:   return "MOVING  ";
        case DS_WORKING:  return "WORKING ";
        case DS_EATING:   return "EATING  ";
        case DS_DRINKING: return "DRINKING";
        case DS_SLEEPING: return "SLEEPING";
        case DS_HAULING:  return "HAULING ";
        case DS_WANDER:   return "WANDER  ";
        case DS_FETCHING: return "FETCHING";
        case DS_TRAINING: return "TRAINING";
        case DS_PRAYING:  return "PRAYING ";
        default:          return "???     ";
    }
}

static void printDebug(uint32_t tickInterval) {
    if (!sShowDebug) {
        // Clear debug area
        printf("\033[0m\033[%d;1H\033[J", MAP_H + 4);
        return;
    }

    // Position cursor below the map + status bar (2 extra blank lines)
    printf("\033[0m\033[%d;1H", MAP_H + 4);

    // Count tasks
    int totalTasks = 0, doneTasks = 0, digTasks = 0, haulTasks = 0, claimedTasks = 0;
    for (int i = 0; i < gTaskCount; i++) {
        totalTasks++;
        if (gTasks[i].done)    doneTasks++;
        if (!gTasks[i].done && gTasks[i].type == TASK_DIG)  digTasks++;
        if (!gTasks[i].done && gTasks[i].type == TASK_HAUL) haulTasks++;
        if (!gTasks[i].done && gTasks[i].claimed)           claimedTasks++;
    }
    int activeTasks = totalTasks - doneTasks;

    printf("--- DEBUG (d=toggle, +/- speed, q=quit) ---\n");
    printf("Tick:%-6u  Stage:%-20s  TickMs:%-4u\n",
           (unsigned)gTick, gStageName, (unsigned)tickInterval);
    printf("Tasks: active=%-3d (dig=%-2d haul=%-2d claimed=%-2d)  slots=%d/%d\n",
           activeTasks, digTasks, haulTasks, claimedTasks, totalTasks, MAX_TASKS);
    printf("Food:%-4d  Drink:%-4d\n", gFoodSupply, gDrinkSupply);

    for (int i = 0; i < gNumDwarves; i++) {
        const Dwarf& d = gDwarves[i];
        printf("  %-8s  %s  pos(%2d,%2d)  task=%-3d  "
               "H:%-3d T:%-3d F:%-3d  carry=%d  path=%d/%d\n",
               d.name, stateName(d.state),
               d.x, d.y, d.taskIdx,
               d.hunger, d.thirst, d.fatigue,
               (int)d.carrying, d.pathPos, d.pathLen);
    }
    printf("\033[J"); // clear to end of screen
    fflush(stdout);
}

// ----------------------------------------------------------------
//  Headless fast-forward mode (--headless N)
// ----------------------------------------------------------------
static void runHeadless(int maxTicks) {
    uint32_t seed = MAP_SEED ? MAP_SEED : (uint32_t)time(NULL);
    printf("Seed: %u\n", seed); fflush(stdout);
    mapInit(seed);
    tasksInit();
    fortPlanInit();
    animalsInit();
    goblinsInit();
    dwarfInit(NUM_DWARVES, HILL_START_X - 2, MAP_H / 2);
    fortPlaceCart(HILL_START_X - 2, MAP_H / 2 - 5);

    char prevStage[24] = "";
    strncpy(prevStage, gStageName, sizeof(prevStage)-1);
    printf("T=%4u  Stage: %s\n", (unsigned)gTick, gStageName); fflush(stdout);

    for (int i = 0; i < maxTicks; i++) {
        fortPlanTick();
        dwarvesTick();
        animalsTick();
        goblinsTick();

        if (strcmp(gStageName, prevStage) != 0) {
            strncpy(prevStage, gStageName, sizeof(prevStage)-1);
            int alive = 0;
            for (int j = 0; j < gNumDwarves; j++)
                if (!gDwarves[j].dead) alive++;
            printf("T=%4u  Stage: %-22s  alive=%d\n",
                   (unsigned)gTick, gStageName, alive); fflush(stdout);
        }

        // Periodic diagnostics every 500 ticks
        if (gTick % 500 == 0) {
            int alive = 0, dig = 0, haul = 0, craft = 0, chop = 0, idle = 0;
            for (int j = 0; j < gNumDwarves; j++) if (!gDwarves[j].dead) alive++;
            for (int j = 0; j < gTaskCount; j++) {
                if (gTasks[j].done) continue;
                if (gTasks[j].type == TASK_DIG)   dig++;
                if (gTasks[j].type == TASK_HAUL)  haul++;
                if (gTasks[j].type == TASK_CRAFT) craft++;
                if (gTasks[j].type == TASK_CHOP)  chop++;
            }
            int training = 0; int maxSkill = 0; int minSkill = 255;
            for (int j = 0; j < gNumDwarves; j++) {
                if (gDwarves[j].dead) continue;
                if (gDwarves[j].state == DS_IDLE) idle++;
                if (gDwarves[j].state == DS_TRAINING) training++;
                if (gDwarves[j].combatSkill > maxSkill) maxSkill = gDwarves[j].combatSkill;
                if (gDwarves[j].combatSkill < minSkill) minSkill = gDwarves[j].combatSkill;
            }
            // Dump dig task coords when count has been stuck (same as previous sample)
            static int sPrevDig = -1;
            if (dig > 0 && dig == sPrevDig && gTick % 2000 == 0) {
                printf("  [T=%4u] DIG STALL — %d tasks stuck:\n", (unsigned)gTick, dig);
                // Print each stuck dig task and its adjacency passability
                for (int j = 0; j < gTaskCount; j++) {
                    if (gTasks[j].done || gTasks[j].type != TASK_DIG) continue;
                    int tx = gTasks[j].x, ty = gTasks[j].y;
                    printf("    task(%d,%d) claimed=%d  adj: N(%d,%d)=%d E(%d,%d)=%d S(%d,%d)=%d W(%d,%d)=%d\n",
                           tx, ty, (int)gTasks[j].claimed,
                           tx,   ty-1, (int)mapPassable(tx,   ty-1),
                           tx+1, ty,   (int)mapPassable(tx+1, ty  ),
                           tx,   ty+1, (int)mapPassable(tx,   ty+1),
                           tx-1, ty,   (int)mapPassable(tx-1, ty  ));
                    if (gTasks[j].claimed) {
                        int who = gTasks[j].claimedBy;
                        if (who >= 0 && who < gNumDwarves) {
                            const Dwarf& dw = gDwarves[who];
                            printf("      Claimed by %s at (%d,%d) state=%d\n",
                                   dw.name, dw.x, dw.y, (int)dw.state);
                        }
                    }
                }
                // Print all dwarf positions
                for (int j = 0; j < gNumDwarves; j++) {
                    if (gDwarves[j].dead) continue;
                    printf("    %s (%d,%d) state=%d task=%d\n",
                           gDwarves[j].name, gDwarves[j].x, gDwarves[j].y,
                           (int)gDwarves[j].state, gDwarves[j].taskIdx);
                }
            }
            sPrevDig = dig;
            printf("  [T=%4u] alive=%d idle=%d train=%d  dig=%d chop=%d haul=%d craft=%d  "
                   "wood=%d stone=%d food=%d drink=%d  skill=%d-%d\n",
                   (unsigned)gTick, alive, idle, training, dig, chop, haul, craft,
                   mapCountItemGlobal(ITEM_WOOD), mapCountItemGlobal(ITEM_STONE),
                   gFoodSupply, gDrinkSupply, minSkill, maxSkill);
            fflush(stdout);
        }

        if (gFortFallen) {
            printf("T=%4u  *** FORTRESS FALLEN: %s ***\n",
                   (unsigned)gTick, gFortFallReason);
            fflush(stdout);
            break;
        }
    }

    int alive = 0;
    for (int j = 0; j < gNumDwarves; j++) if (!gDwarves[j].dead) alive++;
    printf("T=%4u  End. alive=%d/%d  stage=%s\n",
           (unsigned)gTick, alive, gNumDwarves, gStageName); fflush(stdout);
}

// ----------------------------------------------------------------
//  main
// ----------------------------------------------------------------
int main(int argc, char** argv) {
#ifdef HEADLESS
    (void)argc; (void)argv;
    runHeadless(HEADLESS);
    return 0;
#endif
    // --headless N  runs N ticks without display and exits
    if (argc >= 3 && strcmp(argv[1], "--headless") == 0) {
        runHeadless(atoi(argv[2]));
        return 0;
    }

    consoleInit();

    setup();
    tft.flush(MAP_W, MAP_H + 2);

    uint32_t tickInterval = SIM_TICK_MS;
    uint32_t lastTick     = millis();
    char     prevStage[24] = "";
    strncpy(prevStage, gStageName, sizeof(prevStage) - 1);

    while (true) {
        // Key input
        int key = keyCheck();
        if (key == 'q' || key == 'Q') break;
        if (key == '+' || key == '=') tickInterval = (tickInterval > 50) ? tickInterval/2 : 50;
        if (key == '-')               tickInterval = (tickInterval < 4000) ? tickInterval*2 : 4000;
        if (key == 'd' || key == 'D') sShowDebug = !sShowDebug;

        // Tick simulation at the configured interval
        uint32_t now = millis();
        if (now - lastTick >= tickInterval) {
            lastTick = now;

            fortPlanTick();
            dwarvesTick();
            animalsTick();
            goblinsTick();
            tickerTick();

            if (strcmp(gStageName, prevStage) != 0) {
                logStageChange(gStageName);
                strncpy(prevStage, gStageName, sizeof(prevStage) - 1);
            }

            if (gTick > 0 && gTick % 500 == 0) {
                logSnapshot();
            }

            if (gFortFallen) {
                logFortFall();
                deleteSave();
                renderFailure(gFortFallReason);
                tft.flush(MAP_W, MAP_H + 2);
                // Move cursor below map and wait — without this the window
                // closes instantly and the player never sees what happened.
                printf("\033[%d;1H\033[0m  Press any key to exit.\n", MAP_H + 4);
                fflush(stdout);
                while (!keyCheck()) delay(50);
                break;
            }

            renderFrame();
            if (gTick > 0 && gTick % 500 == 0) saveGame();



            tft.flush(MAP_W, MAP_H + 2);
            printDebug(tickInterval);
        }

        // Don't spin at 100% CPU
        delay(10);
    }

    consoleCleanup();
    return 0;
}
