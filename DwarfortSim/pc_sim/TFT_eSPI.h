#pragma once
#include <stdint.h>
#include <string.h>
#include <stdio.h>

// ----------------------------------------------------------------
//  TFT_eSPI stub for PC simulation.
//  drawChar(px,py,...) converts pixel coords → cell coords
//  using a fixed 6×8 font (same as the real GLCD font 1).
//  Call tft.flush() to render the framebuffer to the console.
// ----------------------------------------------------------------

#define TFT_ESPI_VERSION "sim"

// Framebuffer size — big enough for any rotation
#define SIM_FB_COLS 56
#define SIM_FB_ROWS 32

struct TFT_eSprite {}; // stub to avoid compile errors if referenced

class TFT_eSPI {
public:
    struct Cell { char ch; uint16_t fg; uint16_t bg; };

    Cell     fb[SIM_FB_ROWS][SIM_FB_COLS];
    int      curX, curY;          // pixel-space cursor for print()
    uint16_t textFg, textBg;

    TFT_eSPI() { reset(); }

    void reset() {
        memset(fb, 0, sizeof(fb));
        for (int r = 0; r < SIM_FB_ROWS; r++)
            for (int c = 0; c < SIM_FB_COLS; c++)
                fb[r][c] = {' ', 0x0000, 0x0000};
        curX = curY = 0;
        textFg = 0xFFFF; textBg = 0x0000;
    }

    void init()              { reset(); }
    void setRotation(int)    {}
    void setTextFont(int)    {}
    void setTextSize(int)    {}

    void setTextColor(uint16_t fg, uint16_t bg) { textFg = fg; textBg = bg; }
    void setTextColor(uint16_t fg)              { textFg = fg; }

    void fillScreen(uint16_t col) {
        for (int r = 0; r < SIM_FB_ROWS; r++)
            for (int c = 0; c < SIM_FB_COLS; c++)
                fb[r][c] = {' ', col, col};
    }

    void fillRect(int px, int py, int pw, int ph, uint16_t col) {
        int c0 = px/6,         r0 = py/8;
        int c1 = (px+pw-1)/6,  r1 = (py+ph-1)/8;
        for (int r = r0; r <= r1 && r < SIM_FB_ROWS; r++)
            for (int c = c0; c <= c1 && c < SIM_FB_COLS; c++)
                if (r >= 0 && c >= 0) fb[r][c] = {' ', col, col};
    }

    void drawChar(int px, int py, char ch, uint16_t fg, uint16_t bg, int /*size*/) {
        int col = px / 6;
        int row = py / 8;
        if (row < 0 || row >= SIM_FB_ROWS || col < 0 || col >= SIM_FB_COLS) return;
        fb[row][col] = {ch, fg, bg};
    }

    void setCursor(int x, int y) { curX = x; curY = y; }

    void print(const char* s) {
        int col = curX / 6;
        int row = curY / 8;
        if (row < 0 || row >= SIM_FB_ROWS) return;
        for (int i = 0; s[i] && col < SIM_FB_COLS; i++, col++)
            fb[row][col] = {s[i], textFg, textBg};
    }

    // ----------------------------------------------------------------
    //  Render framebuffer to Windows/ANSI console
    // ----------------------------------------------------------------
    void flush(int mapCols, int mapRows) const {
        // Build output in a buffer — avoid thousands of tiny writes
        static char out[SIM_FB_ROWS * SIM_FB_COLS * 32];
        int n = 0;

        // Home cursor
        n += sprintf(out + n, "\033[H");

        for (int r = 0; r < mapRows + 1; r++) {  // +1 for status bar row
            uint16_t lastFg = 0xFFFF, lastBg = 0xFFFF;
            for (int c = 0; c < mapCols; c++) {
                const Cell& cell = fb[r][c];
                uint16_t fg = cell.fg, bg = cell.bg;
                if (fg != lastFg || bg != lastBg) {
                    n += sprintf(out + n, "\033[38;5;%dm\033[48;5;%dm",
                                 rgb565_ansi(fg), rgb565_ansi(bg));
                    lastFg = fg; lastBg = bg;
                }
                char ch = cell.ch;
                // Replace CP437 special chars that don't render in most terminals
                if ((uint8_t)ch < 32) ch = '@';
                out[n++] = ch;
            }
            n += sprintf(out + n, "\033[0m\n");
        }
        // Reset color
        n += sprintf(out + n, "\033[0m");
        out[n] = '\0';
        fputs(out, stdout);
        fflush(stdout);
    }

private:
    // Convert RGB565 to ANSI 256-colour index (6×6×6 cube)
    static int rgb565_ansi(uint16_t c) {
        if (c == 0x0000) return 0;   // black
        if (c == 0xFFFF) return 15;  // bright white
        int r = ((c >> 11) & 0x1F) * 5 / 31;
        int g = ((c >> 5)  & 0x3F) * 5 / 63;
        int b = ((c)       & 0x1F) * 5 / 31;
        // Don't return index 16 (very dark) for non-black — nudge up
        int idx = 16 + 36*r + 6*g + b;
        return idx;
    }
};
