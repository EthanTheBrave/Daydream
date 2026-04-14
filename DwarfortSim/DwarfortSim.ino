// ================================================================
//  DWARFORT SIMULATOR
//  A single-Z-level autonomous dwarf fort simulation for the
//  "Cheap Yellow Display" ESP32 (ESP32-2432S028 / ILI9341 320x240)
//
//  Required library: TFT_eSPI
//    → Install via Arduino Library Manager
//    → Copy User_Setup.h from this sketch folder into:
//        Arduino/libraries/TFT_eSPI/User_Setup.h
//      (overwrite the existing file)
//
//  All gameplay tuning is in config.h
// ================================================================

#include <TFT_eSPI.h>
#include "config.h"
#include "types.h"
#include "map.h"
#include "tasks.h"
#include "dwarves.h"
#include "fortplan.h"
#include "animals.h"
#include "goblins.h"
#include "renderer.h"
#include "save.h"

// ----------------------------------------------------------------
//  TFT instance (used by renderer.cpp via extern)
// ----------------------------------------------------------------
TFT_eSPI tft = TFT_eSPI();

// ----------------------------------------------------------------
//  Timing
// ----------------------------------------------------------------
static uint32_t sLastTick = 0;

// ----------------------------------------------------------------
void setup() {
    Serial.begin(115200);

    // Backlight on
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    // Init display
    tft.init();
    tft.setRotation(1);       // landscape: 320x240
    tft.fillScreen(0x0000);
    tft.setTextSize(1);
    tft.setTextFont(1);

    saveInit();
    bool resuming = hasSave();

    // Splash
    tft.setTextColor(0xFFE0, 0x0000);
    tft.setCursor(76, 108);
    tft.print("DWARFORT SIMULATOR");
    tft.setTextColor(0x8410, 0x0000);
    tft.setCursor(resuming ? 118 : 106, 120);
    tft.print(resuming ? "Resuming..." : "Generating...");
    delay(800);

    if (resuming) {
        if (!loadGame()) {
            // Corrupt or incompatible save — start fresh
            Serial.println("[save] load failed, starting new game");
            deleteSave();
            resuming = false;
        }
    }

    if (!resuming) {
        uint32_t seed = MAP_SEED ? MAP_SEED : esp_random();
        Serial.print("Map seed: "); Serial.println(seed);
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

    sLastTick = millis();
    Serial.println("DwarfortSim ready.");
}

// ----------------------------------------------------------------
void loop() {
    if (gFortFallen) {
        deleteSave();
        renderFailure(gFortFallReason);
        while (true) delay(1000);
    }

    uint32_t now = millis();
    if (now - sLastTick >= SIM_TICK_MS) {
        sLastTick = now;
        fortPlanTick();
        dwarvesTick();
        animalsTick();
        goblinsTick();
        tickerTick();
        renderFrame();
        if (gTick > 0 && gTick % 500 == 0) saveGame();
    }
}
