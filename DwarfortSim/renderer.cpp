#include "renderer.h"
#include "map.h"
#include "dwarves.h"
#include "fortplan.h"
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

// Room background tints (very dark, just a hint of colour)
#define BG_HALL      0x0821  // dim blue-grey
#define BG_STOCKPILE 0x1800  // dim red-brown
#define BG_BEDROOM   0x0010  // dim blue
#define BG_WORKSHOP  0x2000  // dim red
#define BG_FARM      0x0200  // dim green (soil)
#define BG_TOMB      0x1010  // dim purple

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

// ----------------------------------------------------------------
static uint16_t roomBg(RoomType r) {
    switch (r) {
        case ROOM_HALL:      return BG_HALL;
        case ROOM_STOCKPILE: return BG_STOCKPILE;
        case ROOM_BEDROOM:   return BG_BEDROOM;
        case ROOM_WORKSHOP:  return BG_WORKSHOP;
        case ROOM_FARM:      return BG_FARM;
        case ROOM_TOMB:      return BG_TOMB;
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

    // Dwarf overrides everything (CP437 char 1 = smiley)
    if (dwarfAt(x, y)) {
        *ch = '\x01'; *fg = C_YELLOW; return;
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
            case ITEM_CORPSE:   *ch = 'X'; *fg = C_RED;        return;
            case ITEM_BED_I:    *ch = '['; *fg = C_CYAN;       return;
            case ITEM_TABLE_I:  *ch = 'n'; *fg = C_BROWN;      return;
            case ITEM_DOOR_I:   *ch = '+'; *fg = C_BROWN;      return;
            case ITEM_MUSHROOM: *ch = 'm'; *fg = C_PURPLE;     return;
            case ITEM_BEER:     *ch = 'U'; *fg = C_CYAN;       return;
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
            *ch = '.'; *fg = C_TAN;
            break;

        case TILE_GRASS:
            *ch = ','; *fg = C_GREEN;      *bg = C_BLACK;     break;

        case TILE_SHRUB:
            *ch = '"'; *fg = C_DARK_GREEN; *bg = C_BLACK;     break;

        case TILE_FLOWER:
            *ch = '*'; *fg = C_PINK;       *bg = C_BLACK;     break;

        case TILE_TREE:
            *ch = 'T'; *fg = C_DARK_GREEN; *bg = C_BLACK;     break;

        case TILE_DOOR:
            *ch = '+'; *fg = C_BROWN;      *bg = C_BLACK;     break;

        case TILE_WATER:
            *ch = '~'; *fg = C_BLUE;       *bg = C_DARK_BLUE; break;

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

        default:
            *ch = ' '; *fg = C_BLACK;      *bg = C_BLACK;     break;
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
static void drawStatus() {
    char buf[80];
    snprintf(buf, sizeof(buf), "T:%-5u @%d F:%d Dr:%d  %s",
             (unsigned int)gTick, gNumDwarves,
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

void renderFrame() {
    for (int i = 0; i < gNumDwarves; i++) {
        if (!gDwarves[i].active) continue;
        mapMarkDirty(gDwarves[i].x,    gDwarves[i].y);
        mapMarkDirty(gDwarves[i].lastX, gDwarves[i].lastY);
    }

    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            if (mapIsDirty(x, y)) drawCell(x, y);

    mapClearAllDirty();
    drawStatus();
}
