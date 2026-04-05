#pragma once

void rendererInit();

// Redraw only dirty tiles + status bar
void renderFrame();

// Force a full redraw of everything (call once after init)
void renderAll();
