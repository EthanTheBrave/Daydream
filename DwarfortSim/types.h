#pragma once
#include <stdint.h>
#include "config.h"

// ----------------------------------------------------------------
//  Tile
// ----------------------------------------------------------------
enum TileType : uint8_t {
    TILE_WALL    = 0,   // '#'  unexcavated rock
    TILE_FLOOR   = 1,   // '.'  excavated floor
    TILE_GRASS   = 2,   // ','  surface grass
    TILE_TREE    = 3,   // 'T'  tree (choppable)
    TILE_DOOR    = 4,   // '+'  placed door
    TILE_WATER   = 5,   // '~'  water
    TILE_RAMP    = 6,   // '/'  hill ramp (cosmetic)
    TILE_STAIR   = 7,   // '<'  stairway marker (cosmetic)
    TILE_SHRUB   = 8,   // '"'  bush/shrub (passable)
    TILE_FLOWER  = 9,   // '*'  flower (passable)
    TILE_BED     = 10,  // '['  placed bed
    TILE_FORGE   = 11,  // '&'  workshop machine (impassable)
    TILE_TABLE   = 12,  // 'n'  placed table (impassable)
    TILE_CART    = 13,  // 'W'  embark wagon glyph (passable, demolishable)
    TILE_CHAIR   = 14,  // 'h'  placed chair (passable)
    TILE_COFFIN_T = 15, // '|'  placed coffin/sarcophagus (impassable)
    TILE_FARM    = 16,  // ':'  underground mushroom farm plot (passable)
    TILE_SHRINE  = 17,  // 'Omega' stone shrine (passable, prayer destination)
    TILE_BIN     = 18,  // 'B'  storage bin (passable, stacks up to BIN_CAPACITY items)
    // NOTE: Animals are not tile types — they are entities in the animal array
};

// ----------------------------------------------------------------
//  Room types (painted on floor tiles to affect rendering)
// ----------------------------------------------------------------
enum RoomType : uint8_t {
    ROOM_NONE      = 0,
    ROOM_HALL      = 1,
    ROOM_STOCKPILE = 2,
    ROOM_BEDROOM   = 3,
    ROOM_WORKSHOP  = 4,
    ROOM_TOMB      = 5,
    ROOM_FARM      = 6,
    ROOM_BARRACKS  = 7,
    ROOM_TEMPLE    = 8,
};

// ----------------------------------------------------------------
//  Items (stacked on a tile)
// ----------------------------------------------------------------
enum ItemType : uint8_t {
    ITEM_NONE     = 0,
    ITEM_STONE    = 1,   // '*'  loose stone from digging
    ITEM_WOOD     = 2,   // '/'  log from chopping
    ITEM_FOOD     = 3,   // '%'  food barrel (1 item = BARREL_CAPACITY food units)
    ITEM_DRINK    = 4,   // 'u'  drink barrel (1 item = BARREL_CAPACITY drink units)
    ITEM_CHAIR    = 5,   // 'h'  crafted chair (before placing)
    ITEM_COFFIN   = 6,   // '|'  crafted coffin
    ITEM_BARREL   = 7,   // 'O'  empty barrel (fills to become ITEM_FOOD or ITEM_DRINK)
    ITEM_CORPSE   = 8,   // 'X'  dead dwarf's body
    ITEM_BED_I    = 9,   // '['  crafted bed (before placing)
    ITEM_TABLE_I  = 10,  // 'n'  crafted table (before placing)
    ITEM_DOOR_I   = 11,  // '+'  crafted door (before placing)
    ITEM_MUSHROOM = 12,  // 'm'  fresh mushroom (farmable ingredient)
    ITEM_BEER     = 13,  // 'U'  brewed beer (acts as drink supply)
    ITEM_BIN      = 15,  // 'B'  storage bin item (placed → TILE_BIN)
    ITEM_SHRINE   = 16,  // shrine item (placed → TILE_SHRINE)
};

// ----------------------------------------------------------------
//  Map tile (7 bytes)
// ----------------------------------------------------------------
struct Tile {
    TileType type;
    bool     designated;
    ItemType item;
    uint8_t  itemCount;
    RoomType roomType;
    bool     revealed;
    uint8_t  blood;      // >0: blood on tile, counts down to 0 (fades)
};

// ----------------------------------------------------------------
//  Crafting
// ----------------------------------------------------------------
enum CraftType : uint8_t {
    CRAFT_TABLE          = 0,
    CRAFT_CHAIR          = 1,
    CRAFT_DOOR           = 2,
    CRAFT_COFFIN         = 3,
    CRAFT_BARREL         = 4,
    CRAFT_BED            = 5,
    CRAFT_MUSHROOM_FOOD  = 6,  // Kitchen: 2 mushrooms → food supply (+3)
    CRAFT_MUSHROOM_BEER  = 7,  // Still:   2 mushrooms → drink supply (+3)
    CRAFT_STONE_MUG      = 9,  // Mason:   1 stone → 1 empty barrel
    CRAFT_BIN            = 10, // Woodworker: 1 wood → 1 storage bin
    CRAFT_SHRINE         = 11, // Mason:   2 stone → 1 shrine (placed → TILE_SHRINE)
};

inline uint8_t craftWoodCost(CraftType ct) {
    switch (ct) {
        case CRAFT_TABLE:  return CRAFT_WOOD_TABLE;
        case CRAFT_CHAIR:  return CRAFT_WOOD_CHAIR;
        case CRAFT_DOOR:   return CRAFT_WOOD_DOOR;
        case CRAFT_COFFIN: return CRAFT_WOOD_COFFIN;
        case CRAFT_BARREL: return CRAFT_WOOD_BARREL;
        case CRAFT_BED:    return CRAFT_WOOD_BED;
        case CRAFT_BIN:    return 1;
        default:           return 0;
    }
}

// Mushroom cost (for CRAFT_MUSHROOM_FOOD / CRAFT_MUSHROOM_BEER)
inline uint8_t craftMushroomCost(CraftType ct) {
    if (ct == CRAFT_MUSHROOM_FOOD) return CRAFT_MUSH_FOOD_COST;
    if (ct == CRAFT_MUSHROOM_BEER) return CRAFT_MUSH_BEER_COST;
    return 0;
}

// Stone cost (for CRAFT_STONE_MUG and CRAFT_SHRINE)
inline uint8_t craftStoneCost(CraftType ct) {
    if (ct == CRAFT_STONE_MUG)  return CRAFT_STONE_MUG_COST;
    if (ct == CRAFT_SHRINE)     return 2;
    return 0;
}

inline ItemType craftProduct(CraftType ct) {
    switch (ct) {
        case CRAFT_TABLE:         return ITEM_TABLE_I;
        case CRAFT_CHAIR:         return ITEM_CHAIR;
        case CRAFT_DOOR:          return ITEM_DOOR_I;
        case CRAFT_COFFIN:        return ITEM_COFFIN;
        case CRAFT_BARREL:        return ITEM_BARREL;
        case CRAFT_BED:           return ITEM_BED_I;
        case CRAFT_MUSHROOM_FOOD: return ITEM_FOOD;   // not actually placed; supply added directly
        case CRAFT_MUSHROOM_BEER: return ITEM_BEER;   // not actually placed; supply added directly
        case CRAFT_STONE_MUG:     return ITEM_BARREL;
        case CRAFT_BIN:           return ITEM_BIN;
        case CRAFT_SHRINE:        return ITEM_SHRINE;
        default:                  return ITEM_STONE;
    }
}

inline TileType furnitureItemToTile(ItemType it) {
    switch (it) {
        case ITEM_TABLE_I: return TILE_TABLE;
        case ITEM_CHAIR:   return TILE_CHAIR;
        case ITEM_DOOR_I:  return TILE_DOOR;
        case ITEM_BED_I:   return TILE_BED;
        case ITEM_COFFIN:  return TILE_COFFIN_T;
        case ITEM_BIN:     return TILE_BIN;
        case ITEM_SHRINE:  return TILE_SHRINE;
        default:           return TILE_FLOOR;
    }
}

inline bool isFurnitureItem(ItemType it) {
    return it == ITEM_TABLE_I || it == ITEM_CHAIR || it == ITEM_DOOR_I
        || it == ITEM_BED_I   || it == ITEM_COFFIN
        || it == ITEM_BIN     || it == ITEM_SHRINE;
}

// ----------------------------------------------------------------
//  Tasks
// ----------------------------------------------------------------
enum TaskType : uint8_t {
    TASK_NONE       = 0,
    TASK_DIG        = 1,   // excavate a wall tile
    TASK_CHOP       = 2,   // chop a tree tile
    TASK_HAUL       = 3,   // move item from (x,y) to (destX,destY)
    TASK_EAT        = 4,
    TASK_DRINK      = 5,
    TASK_SLEEP      = 6,
    TASK_CRAFT      = 7,   // craft item at workshop; auxType=CraftType
    TASK_PLACE_FURN = 8,   // pick up item at stockpile, place at dest; auxType=ItemType
    TASK_BURY       = 9,   // carry corpse from (x,y) to (destX,destY)
    TASK_FISH       = 10,  // fish at a river tile (bare-handed, only when food < 50%)
};

struct Task {
    TaskType type;
    int8_t   x,    y;       // work site / item source
    int8_t   destX, destY;  // haul / placement destination
    bool     claimed;
    int8_t   claimedBy;
    bool     done;
    uint8_t  workNeeded;
    uint8_t  auxType;       // CraftType for CRAFT, ItemType for PLACE_FURN
};

// ----------------------------------------------------------------
//  Dwarves
// ----------------------------------------------------------------
enum DwarfState : uint8_t {
    DS_IDLE     = 0,
    DS_MOVING   = 1,
    DS_WORKING  = 2,
    DS_EATING   = 3,
    DS_DRINKING = 4,
    DS_SLEEPING = 5,
    DS_HAULING  = 6,
    DS_WANDER   = 7,
    DS_DEAD     = 8,
    DS_FETCHING = 9,   // moving to stockpile to pick up craft material
    DS_TRAINING = 10,  // moving to/training in barracks
    DS_PRAYING  = 11,  // walking to / praying at shrine
};

struct Dwarf {
    int8_t     x,  y;
    int8_t     lastX, lastY;
    DwarfState state;
    int16_t    taskIdx;
    uint8_t    hunger;
    uint8_t    thirst;
    uint8_t    fatigue;
    uint8_t    workLeft;
    uint8_t    idleTicks;
    uint8_t    starveCount;   // consecutive ticks at hunger=100
    uint8_t    dehydCount;    // consecutive ticks at thirst=100
    ItemType   carrying;
    bool       placeFurn;     // true when hauling to place as tile, not drop
    uint8_t    combatSkill;   // 0–COMBAT_SKILL_MAX, trained in barracks
    uint8_t    happiness;     // 0–MAX_HAPPINESS; gained by prayer, slows starvation
    int8_t     pathX[MAX_PATH_LEN];
    int8_t     pathY[MAX_PATH_LEN];
    uint8_t    pathLen;
    uint8_t    pathPos;
    uint8_t    blockedCount; // ticks spent blocked on current path step; triggers re-path at 5
    bool       active;
    bool       dead;
    char       name[10];
};

// ----------------------------------------------------------------
//  Fort planning stages
// ----------------------------------------------------------------
enum FortStage : uint8_t {
    FS_ARRIVAL      = 0,
    FS_ENTRANCE     = 1,
    FS_MAIN_HALL    = 2,
    FS_FURNISH_HALL = 3,   // craft + place tables/chairs
    FS_N_CORRIDOR   = 4,
    FS_STOCKPILE    = 5,
    FS_S_CORRIDOR   = 6,
    FS_BEDROOMS     = 7,
    FS_FURNISH_BED  = 8,   // unused — bed furnishing deferred to FS_DONE (needs workshops)
    FS_E_CORRIDOR   = 9,
    FS_WORKSHOPS    = 10,  // dig workshop wing (corridor + 7 workshops + 3 farms)
    FS_DONE         = 11,
};
