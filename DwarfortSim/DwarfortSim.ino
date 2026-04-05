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
#include "renderer.h"

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

    // Splash
    tft.setTextColor(0xFFE0, 0x0000);
    tft.setCursor(76, 108);
    tft.print("DWARFORT SIMULATOR");
    tft.setTextColor(0x8410, 0x0000);
    tft.setCursor(106, 120);
    tft.print("Generating...");
    delay(800);

    // Init subsystems
    mapInit(MAP_SEED);
    tasksInit();
    fortPlanInit();

    // Dwarves spawn on the surface, just left of the hillface, vertically centred
    int spawnX = HILL_START_X - 2;
    int spawnY = MAP_H / 2;
    dwarfInit(NUM_DWARVES, spawnX, spawnY);

    // Place embark cart with starting materials
    fortPlaceCart(spawnX, spawnY - 5);

    rendererInit();
    renderAll();

    sLastTick = millis();
    Serial.println("DwarfortSim ready.");
}

// ----------------------------------------------------------------
void loop() {
    if (gFortFallen) {
        // Show failure screen once and stop
        renderFailure(gFortFallReason);
        while (true) delay(1000);
    }

    uint32_t now = millis();
    if (now - sLastTick >= SIM_TICK_MS) {
        sLastTick = now;
        fortPlanTick();
        dwarvesTick();
        renderFrame();
    }
}
