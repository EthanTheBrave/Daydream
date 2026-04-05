#include "pathfind.h"
#include "map.h"
#include <string.h>

// ----------------------------------------------------------------
//  Static BFS state (shared; single-threaded so this is safe)
// ----------------------------------------------------------------
static bool    sVisited[MAP_H][MAP_W];
static int16_t sParent[MAP_H][MAP_W];  // encoded as y*MAP_W+x, -1 = none

struct QNode { int8_t x, y; };
static QNode   sQueue[MAP_W * MAP_H];

// Temporary path buffer for reverse reconstruction
static int8_t  sTmpX[MAX_PATH_LEN + 8];
static int8_t  sTmpY[MAX_PATH_LEN + 8];

static const int8_t DX[4] = { 0,  0,  1, -1 };
static const int8_t DY[4] = { 1, -1,  0,  0 };

// ----------------------------------------------------------------
static int bfs(int sx, int sy, int tx, int ty,
               int8_t* pathX, int8_t* pathY, int maxLen) {
    if (sx == tx && sy == ty) return 0;

    memset(sVisited, false, sizeof(sVisited));
    memset(sParent,  -1,    sizeof(sParent));

    int head = 0, tail = 0;
    sQueue[tail++] = { (int8_t)sx, (int8_t)sy };
    sVisited[sy][sx] = true;

    bool found = false;
    while (head < tail && !found) {
        QNode cur = sQueue[head++];
        for (int d = 0; d < 4; d++) {
            int nx = cur.x + DX[d];
            int ny = cur.y + DY[d];
            if (!mapInBounds(nx, ny))   continue;
            if (sVisited[ny][nx])        continue;
            if (!mapPassable(nx, ny))    continue;
            sVisited[ny][nx] = true;
            sParent[ny][nx]  = (int16_t)(cur.y * MAP_W + cur.x);
            if (nx == tx && ny == ty) { found = true; break; }
            if (tail < MAP_W * MAP_H) sQueue[tail++] = { (int8_t)nx, (int8_t)ny };
        }
    }
    if (!found) return 0;

    // Reconstruct (backwards) into sTmpX/Y
    int len = 0;
    int8_t cx = (int8_t)tx, cy = (int8_t)ty;
    while (!(cx == sx && cy == sy) && len < (int)sizeof(sTmpX)) {
        sTmpX[len] = cx;
        sTmpY[len] = cy;
        len++;
        int16_t p = sParent[cy][cx];
        if (p < 0) { len = 0; break; }
        cx = (int8_t)(p % MAP_W);
        cy = (int8_t)(p / MAP_W);
    }

    int outLen = len < maxLen ? len : maxLen;
    for (int i = 0; i < outLen; i++) {
        pathX[i] = sTmpX[len - 1 - i];
        pathY[i] = sTmpY[len - 1 - i];
    }
    return outLen;
}

// ----------------------------------------------------------------
int pathFind(int sx, int sy, int tx, int ty,
             int8_t* pathX, int8_t* pathY, int maxLen) {
    return bfs(sx, sy, tx, ty, pathX, pathY, maxLen);
}

// ----------------------------------------------------------------
int pathFindAdj(int sx, int sy, int tx, int ty,
                int8_t* pathX, int8_t* pathY, int maxLen) {
    // Already adjacent — no movement required; return 0 (in position)
    for (int d = 0; d < 4; d++) {
        int ax = tx + DX[d], ay = ty + DY[d];
        if (ax == sx && ay == sy) return 0;
    }

    int bestLen = 9999;
    int bestOut = 0;

    for (int d = 0; d < 4; d++) {
        int ax = tx + DX[d];
        int ay = ty + DY[d];
        if (!mapInBounds(ax, ay) || !mapPassable(ax, ay)) continue;

        int8_t tmpX[MAX_PATH_LEN], tmpY[MAX_PATH_LEN];
        int len = bfs(sx, sy, ax, ay, tmpX, tmpY, maxLen);
        if (len > 0 && len < bestLen) {
            bestLen = len;
            bestOut = len;
            memcpy(pathX, tmpX, len * sizeof(int8_t));
            memcpy(pathY, tmpY, len * sizeof(int8_t));
        }
    }
    return bestOut;
}
