#include "animals.h"
#include "map.h"
#include "fortplan.h"
#include "config.h"
#include <Arduino.h>
#include <string.h>

Animal gAnimals[MAX_ANIMALS];
int    gNumAnimals = 0;

// ----------------------------------------------------------------
void animalsInit() {
    memset(gAnimals, 0, sizeof(gAnimals));
    gNumAnimals = 0;

    // Start with 2 sheep on the surface
    for (int i = 0; i < 2; i++) {
        if (gNumAnimals >= MAX_ANIMALS) break;
        int tries = 0;
        while (tries++ < 30) {
            int x = random(1, HILL_START_X - 1);
            int y = random(1, MAP_H - 1);
            if (!mapPassable(x, y)) continue;
            Animal& a = gAnimals[gNumAnimals++];
            a.type   = ANIMAL_SHEEP;
            a.x      = (int8_t)x;
            a.y      = (int8_t)y;
            a.lastX  = a.x;
            a.lastY  = a.y;
            a.wanderTimer = random(1, ANIMAL_WANDER_TICKS + 1);
            a.active = true;
            mapMarkDirty(x, y);
            break;
        }
    }
}

// ----------------------------------------------------------------
static bool animalAt(int x, int y, int exceptIdx = -1) {
    for (int i = 0; i < gNumAnimals; i++) {
        if (i == exceptIdx) continue;
        if (gAnimals[i].active && gAnimals[i].x == x && gAnimals[i].y == y) return true;
    }
    return false;
}

static void spawnAnimal(AnimalType type) {
    if (gNumAnimals >= MAX_ANIMALS) return;

    // Count existing of this type
    int count = 0;
    for (int i = 0; i < gNumAnimals; i++)
        if (gAnimals[i].active && gAnimals[i].type == type) count++;

    int cap = (type == ANIMAL_SHEEP) ? MAX_SHEEP : MAX_CATS;
    if (count >= cap) return;

    // Find spawn location
    int tries = 0;
    while (tries++ < 40) {
        int x, y;
        if (type == ANIMAL_SHEEP) {
            // Sheep spawn on grass surface
            x = random(0, HILL_START_X);
            y = random(1, MAP_H - 1);
            if (!mapPassable(x, y)) continue;
            if (mapGet(x, y) == TILE_FLOOR) continue; // not underground
        } else {
            // Cats spawn near the hall entrance or stockpile if dug
            x = random(FORT_ENTRANCE_X1, FORT_HALL_X2 + 1);
            y = random(FORT_HALL_Y1, FORT_HALL_Y2 + 1);
            if (!mapPassable(x, y)) continue;
        }
        if (animalAt(x, y)) continue;

        // Find a free slot
        int slot = gNumAnimals;
        for (int i = 0; i < MAX_ANIMALS; i++) {
            if (!gAnimals[i].active) { slot = i; break; }
        }
        if (slot >= MAX_ANIMALS) return;

        Animal& a = gAnimals[slot];
        a.type   = type;
        a.x      = (int8_t)x;
        a.y      = (int8_t)y;
        a.lastX  = a.x;
        a.lastY  = a.y;
        a.wanderTimer = random(1, ANIMAL_WANDER_TICKS + 1);
        a.active = true;
        if (slot == gNumAnimals) gNumAnimals++;
        mapMarkDirty(x, y);
        return;
    }
}

// ----------------------------------------------------------------
static void wanderAnimal(int idx) {
    Animal& a = gAnimals[idx];
    if (a.wanderTimer > 0) { a.wanderTimer--; return; }
    a.wanderTimer = ANIMAL_WANDER_TICKS;

    // Pick a random adjacent passable tile
    static const int8_t dx[4] = {1,-1,0,0};
    static const int8_t dy[4] = {0,0,1,-1};
    int order[4] = {0,1,2,3};
    // Shuffle
    for (int i = 3; i > 0; i--) {
        int j = random(0, i+1);
        int tmp = order[i]; order[i] = order[j]; order[j] = tmp;
    }

    for (int i = 0; i < 4; i++) {
        int nx = a.x + dx[order[i]];
        int ny = a.y + dy[order[i]];
        if (!mapInBounds(nx, ny)) continue;
        if (!mapPassable(nx, ny)) continue;

        // Sheep stay on surface; cats prefer inside but can roam
        if (a.type == ANIMAL_SHEEP && nx >= HILL_START_X) continue;
        if (a.type == ANIMAL_CAT   && nx < HILL_START_X - 1) continue;

        if (animalAt(nx, ny, idx)) continue;

        mapMarkDirty(a.x, a.y);
        a.lastX = a.x; a.lastY = a.y;
        a.x = (int8_t)nx; a.y = (int8_t)ny;
        mapMarkDirty(a.x, a.y);
        return;
    }
}

// ----------------------------------------------------------------
void animalsTick() {
    // Periodic spawning
    if (gTick % SHEEP_SPAWN_INTERVAL == 0) spawnAnimal(ANIMAL_SHEEP);
    // Cats appear only once fort has a hall
    if (gFortStage >= FS_FURNISH_HALL && gTick % CAT_SPAWN_INTERVAL == 0)
        spawnAnimal(ANIMAL_CAT);

    for (int i = 0; i < gNumAnimals; i++) {
        if (!gAnimals[i].active) continue;
        wanderAnimal(i);
    }
}

// ----------------------------------------------------------------
int animalCatNearby(int x, int y) {
    for (int i = 0; i < gNumAnimals; i++) {
        if (!gAnimals[i].active) continue;
        if (gAnimals[i].type != ANIMAL_CAT) continue;
        int dist = abs(gAnimals[i].x - x) + abs(gAnimals[i].y - y);
        if (dist <= CAT_HAPPINESS_RADIUS) return 1;
    }
    return 0;
}
