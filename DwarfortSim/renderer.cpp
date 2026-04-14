#include "renderer.h"
#include "map.h"
#include "dwarves.h"
#include "tasks.h"
#include "fortplan.h"
#include "animals.h"
#include "goblins.h"
#include "config.h"
#include "types.h"
#include <TFT_eSPI.h>
#include <Arduino.h>
#include <stdio.h>

extern TFT_eSPI tft;

// ----------------------------------------------------------------
//  DF-style colour palette (RGB565)
// ----------------------------------------------------------------
#define C_BLACK      0x0000
#define C_DARK_GRAY  0x2104
#define C_GRAY       0x8410
#define C_BROWN      0x8400
#define C_DARK_GREEN 0x0380
#define C_GREEN      0x07E0
#define C_YELLOW     0xFFE0
#define C_WHITE      0xFFFF
#define C_BLUE       0x001F
#define C_DARK_BLUE  0x000D
#define C_RED        0xF800
#define C_CYAN       0x07FF
#define C_MAGENTA    0xF81F
#define C_TAN        0xAD55
#define C_ORANGE     0xFD20
#define C_PINK       0xFC18  // flower
#define C_PURPLE     0x780F
#define C_DARK_BROWN 0x4200

// Seasonal colours
#define C_SNOW_FG    0xFFFF   // pure white (snow / frost)
#define C_SNOW_BG    0x2104   // same as C_DARK_GRAY — frozen ground shadow
#define C_AUTUMN_LEF 0xFD20   // C_ORANGE — autumn foliage / shrubs

// Blood
#define C_BLOOD_FRESH 0x8000  // bright dark-red  (blood > half of BLOOD_FADE_TICKS)
#define C_BLOOD_DRY   0x4000  // dark maroon      (blood <= half, drying)

// Room background tints (very dark, just a hint of colour)
#define BG_HALL      0x0821  // dim blue-grey
#define BG_STOCKPILE 0x1800  // dim red-brown
#define BG_BEDROOM   0x0010  // dim blue
#define BG_WORKSHOP  0x8000  // red — workshops clearly distinct from floor
#define BG_FARM      0x0200  // dim green (soil)
#define BG_TOMB      0x1010  // dim purple
#define BG_BARRACKS  0x0421  // dim teal — training area
#define BG_TEMPLE    0x3010  // dim gold — holy ground

// ----------------------------------------------------------------
//  Per-cell display cache
// ----------------------------------------------------------------
static char     sDispCh[MAP_H][MAP_W];
static uint16_t sDispFg[MAP_H][MAP_W];
static uint16_t sDispBg[MAP_H][MAP_W];

// ----------------------------------------------------------------
static bool dwarfAt(int x, int y) {
    for (int i = 0; i < gNumDwarves; i++)
        if (gDwarves[i].active && gDwarves[i].x == x && gDwarves[i].y == y)
            return true;
    return false;
}

static Animal* animalAt(int x, int y) {
    for (int i = 0; i < gNumAnimals; i++)
        if (gAnimals[i].active && gAnimals[i].x == x && gAnimals[i].y == y)
            return &gAnimals[i];
    return nullptr;
}

// ----------------------------------------------------------------
static uint16_t roomBg(RoomType r) {
    switch (r) {
        case ROOM_HALL:      return BG_HALL;
        case ROOM_STOCKPILE: return BG_STOCKPILE;
        case ROOM_BEDROOM:   return BG_BEDROOM;
        case ROOM_WORKSHOP:  return BG_WORKSHOP;
        case ROOM_FARM:      return BG_FARM;
        case ROOM_TOMB:      return BG_TOMB;
        case ROOM_BARRACKS:  return BG_BARRACKS;
        case ROOM_TEMPLE:    return BG_TEMPLE;
        default:             return C_BLACK;
    }
}

// ----------------------------------------------------------------
static void tileVisual(int x, int y, char* ch, uint16_t* fg, uint16_t* bg) {
    *bg = C_BLACK;

    Tile& t = gMap[y][x];

    // Fog of war
    if (!t.revealed) {
        *ch = ' '; *fg = C_BLACK; return;
    }

    // Goblin — rendered above dwarves (threat visibility)
    for (int i = 0; i < gNumGoblins; i++) {
        if (gGoblins[i].active && gGoblins[i].x == x && gGoblins[i].y == y) {
            *ch = 'g'; *fg = C_GREEN; return;
        }
    }

    // Dwarf — count how many are on this tile (pass-through allows stacking)
    {
        int dwCount = 0;
        for (int i = 0; i < gNumDwarves; i++)
            if (gDwarves[i].active && gDwarves[i].x == x && gDwarves[i].y == y)
                dwCount++;
        if (dwCount > 0) {
            *ch = '\x01';
            // Slow flash (every 4 ticks) when multiple dwarves share a tile
            *fg = (dwCount > 1 && (gTick % 4) < 2) ? C_WHITE : C_YELLOW;
            return;
        }
    }

    // Animal rendering
    Animal* an = animalAt(x, y);
    if (an) {
        *bg = roomBg(t.roomType);
        if (an->type == ANIMAL_SHEEP) { *ch = 's'; *fg = C_WHITE;  return; }
        if (an->type == ANIMAL_CAT)   { *ch = 'c'; *fg = C_ORANGE; return; }
    }

    // Item on a passable tile — show item, use room bg
    if (t.item != ITEM_NONE && mapPassable(x, y)) {
        *bg = roomBg(t.roomType);
        switch (t.item) {
            case ITEM_STONE:    *ch = '*'; *fg = C_ORANGE;     return;
            case ITEM_WOOD:     *ch = '/'; *fg = C_BROWN;      return;
            case ITEM_FOOD:     *ch = '%'; *fg = C_RED;        return;
            case ITEM_DRINK:    *ch = 'u'; *fg = C_CYAN;       return;
            case ITEM_CHAIR:    *ch = 'h'; *fg = C_BROWN;      return;
            case ITEM_COFFIN:   *ch = '|'; *fg = C_GRAY;       return;
            case ITEM_BARREL:   *ch = 'O'; *fg = C_BROWN;      return;
            case ITEM_CORPSE:   *ch = '\x02'; *fg = C_DARK_GRAY; return;
            case ITEM_BED_I:    *ch = '['; *fg = C_CYAN;       return;
            case ITEM_TABLE_I:  *ch = 'n'; *fg = C_BROWN;      return;
            case ITEM_DOOR_I:   *ch = '+'; *fg = C_BROWN;      return;
            case ITEM_MUSHROOM: *ch = 'm'; *fg = C_PURPLE;     return;
            case ITEM_BEER:     *ch = 'U'; *fg = C_CYAN;       return;
            case ITEM_BONE:     *ch = 'b'; *fg = C_WHITE;      return;
            case ITEM_BIN:      *ch = 'B'; *fg = C_BROWN;      return;
            case ITEM_SHRINE:   *ch = '\x0F'; *fg = C_YELLOW;  return;  // sun glyph (CP437 ☼)
            default: break;
        }
    }

    // Designated wall
    if (t.designated && t.type == TILE_WALL) {
        *ch = '#'; *fg = C_YELLOW; *bg = C_DARK_GRAY; return;
    }

    switch (t.type) {
        case TILE_WALL:
            *ch = '#'; *fg = C_GRAY;       *bg = C_DARK_GRAY; break;

        case TILE_FLOOR:
            *bg = roomBg(t.roomType);
            if (t.roomType == ROOM_WORKSHOP) {
                *ch = '~'; *fg = C_ORANGE;   // forge-heat shimmer
            } else if (t.roomType == ROOM_BARRACKS) {
                *ch = '+'; *fg = C_CYAN;     // training mat crosshatch
            } else {
                *ch = '.'; *fg = C_TAN;
            }
            break;

        case TILE_GRASS:
            *bg = C_BLACK;
            if (gSeason == 3) { *ch = ','; *fg = C_SNOW_FG; *bg = C_SNOW_BG; }
            else if (gSeason == 2) { *ch = ','; *fg = C_BROWN; }
            else { *ch = ','; *fg = C_GREEN; }
            break;

        case TILE_SHRUB:
            *bg = C_BLACK;
            if (gSeason == 3) { *ch = '"'; *fg = C_SNOW_FG; *bg = C_SNOW_BG; }
            else if (gSeason == 2) { *ch = '"'; *fg = C_AUTUMN_LEF; }
            else { *ch = '"'; *fg = C_DARK_GREEN; }
            break;

        case TILE_FLOWER:
            *bg = C_BLACK;
            if (gSeason == 3) { *ch = ','; *fg = C_SNOW_FG; *bg = C_SNOW_BG; }  // buried
            else if (gSeason == 2) { *ch = '*'; *fg = C_BROWN; }  // wilted
            else { *ch = '*'; *fg = C_PINK; }
            break;

        case TILE_TREE:
            *bg = C_BLACK;
            if (gSeason == 3) { *ch = 'T'; *fg = C_GRAY; *bg = C_SNOW_BG; }   // bare / snowy
            else if (gSeason == 2) { *ch = 'T'; *fg = C_AUTUMN_LEF; }         // autumn foliage
            else { *ch = 'T'; *fg = C_DARK_GREEN; }
            break;

        case TILE_DOOR:
            *ch = '+'; *fg = C_BROWN;      *bg = C_BLACK;     break;

        case TILE_WATER:
            if (gSeason == 3) { *ch = '~'; *fg = C_GRAY; *bg = C_SNOW_BG; }   // frozen
            else { *ch = '~'; *fg = C_BLUE; *bg = C_DARK_BLUE; }
            break;

        case TILE_RAMP:
            *ch = '/'; *fg = C_GRAY;       *bg = C_DARK_GRAY; break;

        case TILE_STAIR:
            *ch = '<'; *fg = C_WHITE;      *bg = C_BLACK;     break;

        case TILE_BED:
            *ch = '['; *fg = C_CYAN;       *bg = BG_BEDROOM;  break;

        case TILE_FORGE:
            *ch = '&'; *fg = C_ORANGE;     *bg = BG_WORKSHOP; break;

        case TILE_TABLE:
            *ch = 'n'; *fg = C_BROWN;      *bg = BG_HALL;     break;

        case TILE_CART:
            *ch = 'W'; *fg = C_BROWN;      *bg = C_BLACK;     break;

        case TILE_CHAIR:
            *ch = 'h'; *fg = C_BROWN;      *bg = BG_HALL;     break;

        case TILE_COFFIN_T:
            *ch = '|'; *fg = C_WHITE;      *bg = BG_TOMB;     break;

        case TILE_FARM:
            *ch = ':'; *fg = C_DARK_BROWN; *bg = BG_FARM;     break;

        case TILE_SHRINE:
            *ch = '\x0F'; *fg = C_YELLOW; *bg = BG_TEMPLE;    break;  // ☼ shrine

        case TILE_BIN:
            *bg = roomBg(t.roomType);
            if (t.item != ITEM_NONE) {
                // Show contained item when bin has contents
                switch (t.item) {
                    case ITEM_STONE:    *ch = '*'; *fg = C_ORANGE; break;
                    case ITEM_WOOD:     *ch = '/'; *fg = C_BROWN;  break;
                    case ITEM_BONE:     *ch = 'b'; *fg = C_WHITE;  break;
                    case ITEM_MUSHROOM: *ch = 'm'; *fg = C_PURPLE; break;
                    default:            *ch = 'B'; *fg = C_BROWN;  break;
                }
            } else {
                *ch = 'B'; *fg = C_TAN;   // empty bin
            }
            break;

        default:
            *ch = ' '; *fg = C_BLACK;      *bg = C_BLACK;     break;
    }

    // Blood overlay: tint background red on tiles with spilled blood.
    // Walls don't show blood (no visible floor surface).
    if (t.blood > 0 && t.type != TILE_WALL) {
        *bg = (t.blood > BLOOD_FADE_TICKS / 2) ? C_BLOOD_FRESH : C_BLOOD_DRY;
    }
}

// ----------------------------------------------------------------
static void drawCell(int x, int y) {
    char ch; uint16_t fg, bg;
    tileVisual(x, y, &ch, &fg, &bg);

    if (ch == sDispCh[y][x] && fg == sDispFg[y][x] && bg == sDispBg[y][x]) return;

    sDispCh[y][x] = ch;
    sDispFg[y][x] = fg;
    sDispBg[y][x] = bg;

    tft.drawChar(x * FONT_W, y * FONT_H, ch, fg, bg, 1);
}

// ----------------------------------------------------------------
//  Ticker — scrolling event log (one line above the status bar)
// ----------------------------------------------------------------
#define TICKER_QUEUE_CAP  8
#define TICKER_MSG_LEN   53    // max chars (= MAP_W tiles)
#define TICKER_REVEAL_RATE 3   // chars revealed per game tick
#define TICKER_HOLD_TICKS 200  // ticks to hold a fully-revealed message

static char  sTickerQueue[TICKER_QUEUE_CAP][TICKER_MSG_LEN + 1];
static int   sTickerQHead  = 0;
static int   sTickerQTail  = 0;
static char  sTickerCurrent[TICKER_MSG_LEN + 1];
static int   sTickerReveal = 0;   // chars shown so far
static int   sTickerHold   = 0;   // ticks spent at full reveal

void tickerPush(const char* msg) {
    int next = (sTickerQTail + 1) % TICKER_QUEUE_CAP;
    if (next == sTickerQHead) return; // queue full — drop oldest? just drop new
    strncpy(sTickerQueue[sTickerQTail], msg, TICKER_MSG_LEN);
    sTickerQueue[sTickerQTail][TICKER_MSG_LEN] = 0;
    sTickerQTail = next;
}

static void tickerLoadNext() {
    if (sTickerQHead == sTickerQTail) return; // nothing queued
    strncpy(sTickerCurrent, sTickerQueue[sTickerQHead], TICKER_MSG_LEN + 1);
    sTickerQHead = (sTickerQHead + 1) % TICKER_QUEUE_CAP;
    sTickerReveal = 0;
    sTickerHold   = 0;
}

void tickerTick() {
    int msgLen = (int)strlen(sTickerCurrent);
    if (msgLen == 0) { tickerLoadNext(); return; }

    if (sTickerReveal < msgLen) {
        sTickerReveal += TICKER_REVEAL_RATE;
        if (sTickerReveal > msgLen) sTickerReveal = msgLen;
        return;
    }
    // Fully revealed — cut hold short if a new event is waiting; otherwise hold
    if (sTickerQHead != sTickerQTail) {
        sTickerCurrent[0] = 0;
        tickerLoadNext();
        return;
    }
    if (++sTickerHold >= TICKER_HOLD_TICKS) {
        sTickerCurrent[0] = 0;
        tickerLoadNext();
    }
}

static void drawTicker() {
    tft.fillRect(0, TICKER_BAR_Y, MAP_W * FONT_W, TICKER_BAR_H, C_BLACK);
    if (sTickerReveal > 0 && sTickerCurrent[0]) {
        char buf[TICKER_MSG_LEN + 1];
        int n = sTickerReveal < TICKER_MSG_LEN ? sTickerReveal : TICKER_MSG_LEN;
        int msgLen = (int)strlen(sTickerCurrent);
        if (n > msgLen) n = msgLen;
        strncpy(buf, sTickerCurrent, n);
        buf[n] = 0;
        tft.setTextColor(C_CYAN, C_BLACK);
        tft.setTextSize(1);
        tft.setCursor(2, TICKER_BAR_Y + 1);
        tft.print(buf);
    }
}

// ----------------------------------------------------------------
static void drawStatus() {
    char buf[80];
    int alive = 0, dig = 0, haul = 0, craft = 0, train = 0;
    for (int i = 0; i < gNumDwarves; i++) {
        if (gDwarves[i].dead) continue;
        alive++;
        if (gDwarves[i].state == DS_TRAINING) train++;
    }
    for (int i = 0; i < gTaskCount; i++) {
        if (gTasks[i].done) continue;
        if (gTasks[i].type == TASK_DIG)   dig++;
        if (gTasks[i].type == TASK_HAUL)  haul++;
        if (gTasks[i].type == TASK_CRAFT) craft++;
    }
    snprintf(buf, sizeof(buf), "@%d d:%d h:%d c:%d t:%d  F:%d Dr:%d  %s",
             alive, dig, haul, craft, train,
             gFoodSupply, gDrinkSupply,
             gStageName);

    tft.fillRect(0, STATUS_BAR_Y, MAP_W * FONT_W, STATUS_BAR_H, 0x1082);
    tft.setTextColor(C_WHITE, 0x1082);
    tft.setTextSize(1);
    tft.setCursor(2, STATUS_BAR_Y + 1);
    tft.print(buf);
}

// ----------------------------------------------------------------
void rendererInit() {
    memset(sDispCh, 0, sizeof(sDispCh));
    memset(sDispFg, 0, sizeof(sDispFg));
    memset(sDispBg, 0, sizeof(sDispBg));
}

void renderAll() {
    tft.fillScreen(C_BLACK);
    memset(sDispCh, 1, sizeof(sDispCh));
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            drawCell(x, y);
    drawTicker();
    drawStatus();
    mapClearAllDirty();
}

// ----------------------------------------------------------------
void renderFailure(const char* reason) {
    tft.fillScreen(C_BLACK);

    // Big red title
    tft.setTextSize(2);
    tft.setTextColor(C_RED, C_BLACK);
    tft.setCursor(28, 60);
    tft.print("FORTRESS FALLEN");

    // Separator
    tft.fillRect(10, 82, MAP_W * FONT_W - 20, 2, C_RED);

    // Reason line
    char buf[64];
    snprintf(buf, sizeof(buf), "Cause: %s", reason);
    tft.setTextSize(1);
    tft.setTextColor(C_YELLOW, C_BLACK);
    tft.setCursor(80, 100);
    tft.print(buf);

    // Dwarf count
    tft.setTextColor(C_GRAY, C_BLACK);
    tft.setCursor(70, 116);
    tft.print("All dwarves have perished.");

    // Flavour
    tft.setTextColor(C_DARK_GREEN, C_BLACK);
    tft.setCursor(50, 140);
    tft.print("Their story will be remembered.");

    // Tick
    snprintf(buf, sizeof(buf), "Survived %u ticks.", (unsigned int)gTick);
    tft.setTextColor(C_GRAY, C_BLACK);
    tft.setCursor(76, 160);
    tft.print(buf);
}

// ----------------------------------------------------------------
void renderFrame() {
    for (int i = 0; i < gNumDwarves; i++) {
        if (!gDwarves[i].active) continue;
        mapMarkDirty(gDwarves[i].x,    gDwarves[i].y);
        mapMarkDirty(gDwarves[i].lastX, gDwarves[i].lastY);
    }
    for (int i = 0; i < gNumAnimals; i++) {
        if (!gAnimals[i].active) continue;
        mapMarkDirty(gAnimals[i].x,    gAnimals[i].y);
        mapMarkDirty(gAnimals[i].lastX, gAnimals[i].lastY);
    }
    for (int i = 0; i < gNumGoblins; i++) {
        if (!gGoblins[i].active) continue;
        mapMarkDirty(gGoblins[i].x,    gGoblins[i].y);
        mapMarkDirty(gGoblins[i].lastX, gGoblins[i].lastY);
    }

    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            if (mapIsDirty(x, y)) drawCell(x, y);

    mapClearAllDirty();
    drawTicker();
    drawStatus();
}
