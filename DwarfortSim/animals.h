#pragma once
#include "config.h"
#include <stdint.h>

enum AnimalType : uint8_t {
    ANIMAL_NONE  = 0,
    ANIMAL_SHEEP = 1,  // 's'  wanders surface, purely cosmetic
    ANIMAL_CAT   = 2,  // 'c'  wanders fort, boosts nearby dwarf happiness
};

struct Animal {
    AnimalType type;
    int8_t     x, y;
    int8_t     lastX, lastY;
    uint8_t    wanderTimer;
    bool       active;
};

extern Animal gAnimals[MAX_ANIMALS];
extern int    gNumAnimals;

void animalsInit();
void animalsTick();

// Returns happiness bonus (0 or 1) if a cat is within CAT_HAPPINESS_RADIUS of (x,y)
int  animalCatNearby(int x, int y);
