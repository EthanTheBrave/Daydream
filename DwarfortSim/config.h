#pragma once

// ================================================================
//  DWARFORT SIMULATOR — CONFIGURATION
//  Edit these values before uploading to change behaviour.
//  No other files need to be touched for basic tuning.
// ================================================================

// --- Dwarves -------------------------------------------------------
#define NUM_DWARVES        7     // Starting population (1–10)

// --- Timing --------------------------------------------------------
#define SIM_TICK_MS      400     // ms between simulation steps
                                 // Lower = faster simulation

// --- Map -----------------------------------------------------------
#define MAP_SEED           0     // 0 = random each run; set a number for a fixed layout
#define HILL_START_X      10     // Column where the hill face begins
                                 // (surface grass to the left)

// --- Starting supplies (placed as items on the surface) ------------
#define START_FOOD       120     // Food units at embark
#define START_DRINK      120     // Drink units at embark

// --- Dwarf needs ---------------------------------------------------
#define HUNGER_RATE        1     // Hunger added per tick
#define THIRST_RATE        2     // Thirst added per tick
#define FATIGUE_RATE       1     // Fatigue added per tick
#define HUNGER_THRESH     65     // Hunger level that triggers eating
#define THIRST_THRESH     60     // Thirst level that triggers drinking
#define FATIGUE_THRESH    75     // Fatigue that triggers sleeping
#define EAT_RESTORE       40     // Hunger removed per eat action
#define DRINK_RESTORE     40     // Thirst removed per drink action
#define SLEEP_RESTORE      5     // Fatigue removed per sleep tick

// --- Work speeds (ticks to complete) -------------------------------
#define DIG_TICKS          3     // Ticks to excavate one wall tile
#define CHOP_TICKS         4     // Ticks to chop one tree
#define HAUL_TICKS         2     // Ticks to deposit a carried item
#define EAT_TICKS          3     // Ticks to eat a meal
#define DRINK_TICKS        2     // Ticks to drink
#define SLEEP_TICKS       15     // Ticks of sleep per rest session

// --- Display (do not change unless using a different font) ---------
#define MAP_W             53     // Tile columns  (53 * 6 = 318px — fills ~320px landscape)
#define MAP_H             28     // Tile rows     (28 * 8 = 224px; +8 ticker +8 status = 240px)
#define FONT_W             6     // Pixels per character (TFT_eSPI font 1)
#define FONT_H             8
#define TICKER_BAR_Y     224     // Pixel Y of scrolling event ticker (MAP_H * FONT_H)
#define TICKER_BAR_H       8
#define STATUS_BAR_Y     232     // Pixel Y of status bar (TICKER_BAR_Y + TICKER_BAR_H)
#define STATUS_BAR_H       8     // Pixels tall for status bar — 232+8=240px total

// --- Crafting ------------------------------------------------------
#define CRAFT_TICKS       10     // Ticks to craft one item at workshop
#define CRAFT_WOOD_TABLE   2     // Wood cost per table
#define CRAFT_WOOD_CHAIR   1
#define CRAFT_WOOD_BED     2
#define CRAFT_WOOD_DOOR    1
#define CRAFT_WOOD_COFFIN  3
#define CRAFT_WOOD_BARREL  2
#define CRAFT_MUSH_FOOD_COST  2  // Mushrooms consumed per food batch (Kitchen)
#define CRAFT_MUSH_BEER_COST  2  // Mushrooms consumed per beer batch (Still)
#define CRAFT_STONE_MUG_COST  1  // Stone consumed per mug (Mason)

// --- Death ---------------------------------------------------------
#define STARVE_TICKS      25     // Consecutive max-hunger ticks before death (longer — dwarves must walk to food)
#define DEHYDRATE_TICKS   18     // Consecutive max-thirst ticks before death

// --- Embark cart (items placed on surface at spawn) ----------------
#define CART_WOOD          5     // Starting wood pieces in cart
#define CART_STONE         3     // Starting stone pieces in cart

// --- Supply replenishment (surface foraging / stream collection) ---
// Periodic additions to food and drink supply, simulating dwarves
// hunting/foraging and collecting water off-screen.
#define FORAGE_FOOD_INTERVAL  15   // ticks between food top-ups
#define FORAGE_FOOD_AMOUNT    15   // food added per top-up
#define COLLECT_DRINK_INTERVAL 10  // ticks between drink top-ups
#define COLLECT_DRINK_AMOUNT  15   // drink added per top-up
// No supply cap — food and drink can accumulate without limit

// --- Wood supply (surface trees) -----------------------------------
#define LOW_WOOD_THRESHOLD     4   // designate trees when wood < this
#define TREE_REGROW_INTERVAL 800   // ticks between tree regrowth attempts
#define MAX_SURFACE_TREES     12   // hard cap on trees on the surface at once

// --- Mushroom farms ------------------------------------------------
#define MUSHROOM_GROW_INTERVAL 15  // ticks between mushroom spawns per farm plot
#define MAX_MUSHROOMS_STOCKPILE 20 // stop growing if this many mushrooms in stockpile

// --- Tomb (dug when a dwarf dies) ----------------------------------
// Placed south of workshop corridor (y=14) at the east end, accessible
// from corridor tiles x=44-50, y=14.
#define TOMB_X1           44
#define TOMB_Y1           15
#define TOMB_X2           50
#define TOMB_Y2           18

// --- Furnishing counts ---------------------------------------------
#define HALL_TABLES        2
#define HALL_CHAIRS        4
#define BED_COUNT  6           // one bed per small room (3 north + 3 south)

// --- Season tracking -----------------------------------------------
#define TICKS_PER_SEASON  1000   // ticks per in-game season (Spring/Summer/Autumn/Winter)
// No victory condition — fortress runs until all dwarves die

// --- Combat / Barracks (4×4 room at end of workshop wing) ----------
#define FORT_BAR_X1  44
#define FORT_BAR_Y1  10
#define FORT_BAR_X2  47
#define FORT_BAR_Y2  13

#define COMBAT_TRAIN_INTERVAL   3    // ticks between +1 skill while training
#define COMBAT_TRAIN_DURATION  40    // ticks per barracks training session
#define COMBAT_SKILL_MAX       80    // max combat skill

// --- Barrel & bin storage ------------------------------------------
#define BARREL_CAPACITY       10   // food/drink units per barrel
#define BIN_CAPACITY           5   // items per storage bin

// --- Temple --------------------------------------------------------
// Small 3×3 prayer room east of barracks, connected via short passage
#define FORT_TEMPLE_X1        48
#define FORT_TEMPLE_Y1         7
#define FORT_TEMPLE_X2        50
#define FORT_TEMPLE_Y2         9
// Passage: 2-wide column (x=49-50) from corridor (y=13) to temple (y=10)
#define FORT_TEMPLE_PASS_X    49
#define FORT_TEMPLE_PASS_X2   50
#define FORT_TEMPLE_PASS_Y1   10
#define FORT_TEMPLE_PASS_Y2   13

// --- Prayer --------------------------------------------------------
#define PRAYER_TICKS          30   // ticks per prayer session at shrine
#define MAX_HAPPINESS         40   // max happiness value
#define HAPPINESS_DECAY       80   // ticks between happiness -1
#define EAT_HALL_HAPPINESS     2   // happiness gained per meal eaten at a furnished hall table

// --- Fishing -------------------------------------------------------
#define FISH_TICKS            15   // ticks per fishing attempt
#define FISH_FOOD_AMOUNT       2   // food supply added per catch
#define MAX_CONCURRENT_FISH    3   // max simultaneous fish tasks active at once

// --- Animals -------------------------------------------------------
#define SHEEP_HARVEST_INTERVAL 700  // ticks between sheep harvests (600 = spawn interval)
#define SHEEP_MEAT_FOOD          3  // food supply added per sheep harvested
#define MAX_ANIMALS         8    // max sheep + cats on map at once
#define ANIMAL_WANDER_TICKS 3    // ticks between animal moves
#define CAT_HAPPINESS_RADIUS 3   // tiles away from cat that dwarves feel happy
#define SHEEP_SPAWN_INTERVAL 600 // ticks between new sheep spawning (surface)
#define CAT_SPAWN_INTERVAL   900 // ticks between new cats spawning (inside fort)
#define MAX_SHEEP            4
#define MAX_CATS             4

// --- Migrants ------------------------------------------------------
#define MIGRANT_WAVE_INTERVAL  800   // ticks between migrant waves
#define MIGRANT_WAVE_MIN         1   // min migrants per wave
#define MIGRANT_WAVE_MAX         3   // max migrants per wave
#define MIGRANT_START_TICK     400   // first wave not before this tick
#define MIGRANT_POP_CAP         20   // hard cap on total dwarves

// --- Blood ---------------------------------------------------------
#define BLOOD_FADE_TICKS    200   // blood starts at this value and counts down
#define BLOOD_DECAY_INTERVAL  3   // ticks between each -1 decay step (~600 ticks total)

// --- Entity limits -------------------------------------------------
#define MAX_DWARVES       24     // Must be >= MIGRANT_POP_CAP + NUM_DWARVES
#define MAX_TASKS        300
#define MAX_PATH_LEN     200     // Increased for wider map

// --- Fort rooms (relative to map; edit carefully) ------------------
// MAP is 53 wide x 29 tall. Hill starts at x=10. Vertical centre: y=14
// Surface: x=0..9    Mountain: x=10..52

// Entrance tunnel (6×4 — wide enough to feel like a real gate passage)
#define FORT_ENTRANCE_X1  10
#define FORT_ENTRANCE_Y1  12
#define FORT_ENTRANCE_X2  15
#define FORT_ENTRANCE_Y2  15

// Main hall (7×7)
#define FORT_HALL_X1      13
#define FORT_HALL_Y1      11
#define FORT_HALL_X2      19
#define FORT_HALL_Y2      17

// North corridor (3×3)
#define FORT_NCORR_X1     15
#define FORT_NCORR_Y1      8
#define FORT_NCORR_X2     17
#define FORT_NCORR_Y2     10

// Stockpile room (8×5)
#define FORT_STOCK_X1     13
#define FORT_STOCK_Y1      3
#define FORT_STOCK_X2     20
#define FORT_STOCK_Y2      7

// South corridor (3×3)
#define FORT_SCORR_X1     15
#define FORT_SCORR_Y1     18
#define FORT_SCORR_X2     17
#define FORT_SCORR_Y2     20

// Bedrooms — 6 small 2×2 rooms off a central corridor
// Layout: 3 rooms north (y=21..22) + corridor (y=23) + 3 rooms south (y=24..25)
// Connecting passage at x=16 links to S. Corridor above.
// Dividing walls remain at x=15, x=19 (un-designated).
#define FORT_BED_X1       13
#define FORT_BED_Y1       21
#define FORT_BED_X2       22
#define FORT_BED_Y2       25
#define FORT_BED_CORR_Y   23   // east-west bedroom corridor row

// East antechamber — wide junction room before workshop wing.
// Height matches the main hall (set dynamically in computeLayout).
// Provides space for future side rooms to branch north/south.
#define FORT_ECORR_X1     20
#define FORT_ECORR_Y1     12   // fallback only — overridden by sHY1/sHY2
#define FORT_ECORR_X2     30
#define FORT_ECORR_Y2     14   // fallback only — overridden by sHY1/sHY2

// Workshop main corridor — 2-tile-wide E-W spine at y=13-14
#define FORT_WCORR_X1     23
#define FORT_WCORR_Y1     13
#define FORT_WCORR_X2     52
#define FORT_WCORR_Y2     14

// --- Individual workshops (3×3 each) ---
// Row North (y=11..13)
#define FORT_WS_WOOD_X1   24
#define FORT_WS_WOOD_Y1   11
#define FORT_WS_WOOD_X2   26
#define FORT_WS_WOOD_Y2   13

#define FORT_WS_STILL_X1  28
#define FORT_WS_STILL_Y1  11
#define FORT_WS_STILL_X2  30
#define FORT_WS_STILL_Y2  13

#define FORT_WS_KITCH_X1  32
#define FORT_WS_KITCH_Y1  11
#define FORT_WS_KITCH_X2  34
#define FORT_WS_KITCH_Y2  13

// Row South (y=15..17)
#define FORT_WS_STONE_X1  24
#define FORT_WS_STONE_Y1  15
#define FORT_WS_STONE_X2  26
#define FORT_WS_STONE_Y2  17

#define FORT_WS_SMELT_X1  28
#define FORT_WS_SMELT_Y1  15
#define FORT_WS_SMELT_X2  30
#define FORT_WS_SMELT_Y2  17

#define FORT_WS_FORGE_X1  32
#define FORT_WS_FORGE_Y1  15
#define FORT_WS_FORGE_X2  34
#define FORT_WS_FORGE_Y2  17

#define FORT_WS_FARM_X1   36
#define FORT_WS_FARM_Y1   15
#define FORT_WS_FARM_X2   38
#define FORT_WS_FARM_Y2   17

// --- Workshop galleries (1-tile lateral corridors above/below the workshop rows)
// Connects all north workshop rooms (y=10) and south workshop rooms (y=18)
// so dwarves can reach any workshop from either direction.
#define FORT_NGALLERY_X1   23    // above the north workshop row
#define FORT_NGALLERY_X2   43    // reaches east end of farm rows
#define FORT_NGALLERY_Y    10

#define FORT_SGALLERY_X1   23    // below the south workshop row
#define FORT_SGALLERY_X2   43
#define FORT_SGALLERY_Y    18

// --- Farm plots (3×3 each, become TILE_FARM) ---
#define FORT_FARM1_X1     36
#define FORT_FARM1_Y1     11
#define FORT_FARM1_X2     38
#define FORT_FARM1_Y2     13

#define FORT_FARM2_X1     40
#define FORT_FARM2_Y1     11
#define FORT_FARM2_X2     42
#define FORT_FARM2_Y2     13

#define FORT_FARM3_X1     40
#define FORT_FARM3_Y1     15
#define FORT_FARM3_X2     42
#define FORT_FARM3_Y2     17
