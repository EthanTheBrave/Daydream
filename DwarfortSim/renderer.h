#pragma once

void rendererInit();

// Redraw only dirty tiles + status bar + ticker
void renderFrame();

// Force a full redraw of everything (call once after init)
void renderAll();

// Show fortress-fallen screen
void renderFailure(const char* reason);

// ---- Ticker (scrolling event log) --------------------------------
// Push a message onto the ticker queue; displayed character-by-character
void tickerPush(const char* msg);

// Advance ticker state one simulation tick (call every game tick)
void tickerTick();
