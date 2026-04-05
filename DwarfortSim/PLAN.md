# DwarfortSim — Implementation Plan

## Status: In Progress
Last updated: 2026-04-04

---

## Problem Summary

Three issues to fix:

1. **Screen width unused** — MAP_W=40 × FONT_W=6 = 240px, but landscape rotation gives 320px wide display. Need MAP_W=53 (318px).
2. **Rooms too big** — all rooms oversized, need to shrink.
3. **Workshops + Farms** — add 7 workshop types (3×3 each) and 1–3 mushroom farm plots with processing chain.

---

## Target Fort Layout (MAP_W=53, MAP_H=29, HILL_START_X=10)

```
x=0..9  = surface (grass/trees/water)
x=10..52 = mountain
y=0..28
Center: y=14
```

### Room Coordinates

| Room            | x1 | y1 | x2 | y2 | Size  |
|-----------------|----|----|----|----|-------|
| Entrance        | 10 | 12 | 12 | 14 | 3×3   |
| Main Hall       | 13 | 11 | 19 | 17 | 7×7   |
| N Corridor      | 15 | 8  | 17 | 10 | 3×3   |
| Stockpile       | 13 | 3  | 20 | 7  | 8×5   |
| S Corridor      | 15 | 18 | 17 | 20 | 3×3   |
| Bedrooms        | 13 | 21 | 20 | 25 | 8×5   |
| E Corridor      | 20 | 12 | 22 | 14 | 3×3   |
| Tomb            | 22 | 20 | 28 | 24 | 7×5   |

### Workshop Wing (x=23..52)

E-W Main corridor at y=14: x=23..52 (connects all workshops)

**Row North (y=11–13):**
| Workshop         | x1 | y1 | x2 | y2 |
|-----------------|----|----|----|----|
| Woodworker Shop  | 24 | 11 | 26 | 13 |
| Still            | 28 | 11 | 30 | 13 |
| Kitchen          | 32 | 11 | 34 | 13 |

**Row South (y=15–17):**
| Workshop         | x1 | y1 | x2 | y2 |
|-----------------|----|----|----|----|
| Stoneworker Shop | 24 | 15 | 26 | 17 |
| Smelter          | 28 | 15 | 30 | 17 |
| Forge            | 32 | 15 | 34 | 17 |
| Farmer Workshop  | 36 | 15 | 38 | 17 |

**Farm Plots (3×3 each, TILE_FARM):**
| Farm   | x1 | y1 | x2 | y2 |
|--------|----|----|----|----|
| Farm 1 | 36 | 11 | 38 | 13 |
| Farm 2 | 40 | 11 | 42 | 13 |
| Farm 3 | 40 | 15 | 42 | 17 |

---

## Hall Furnishing Positions (updated for new hall size)

Tables (HALL_TABLES=2): (15,14), (17,14)
Chairs (HALL_CHAIRS=4): (15,13), (17,13), (15,15), (17,15)

---

## New Types Needed

### types.h

**TileType** — add:
- `TILE_FARM = 16` — mushroom farm plot (passable, grows mushrooms)

**RoomType** — add:
- `ROOM_FARM = 6`

**ItemType** — add:
- `ITEM_MUSHROOM = 12`
- `ITEM_BEER = 13`

**CraftType** — add:
- `CRAFT_MUSHROOM_FOOD = 6` — Kitchen: 2 mushrooms → 1 food
- `CRAFT_MUSHROOM_BEER = 7` — Still: 2 mushrooms → 1 beer

### config.h — add:

```c
#define MUSHROOM_GROW_INTERVAL  30   // ticks between mushroom spawns per farm tile
#define MUSHROOM_PER_FARM        1   // mushrooms spawned per grow tick
#define CRAFT_MUSH_FOOD_COST     2   // mushrooms per food
#define CRAFT_MUSH_BEER_COST     2   // mushrooms per beer
```

---

## FortStage Changes

New stages inserted after FS_WORKSHOP:
```
FS_WORKSHOPS   = 10  // build all 7 workshops (replaces single FS_WORKSHOP)
FS_FARMS       = 11  // dig + designate farm plots
FS_DONE        = 12
```

Workshop build order within FS_WORKSHOPS:
1. Dig E corridor (x=20-22, y=12-14)
2. Dig main E-W workshop corridor (x=23-52, y=14)  ← single row of floor
3. Dig all 7 workshop rooms
4. Dig 3 farm plots (as TILE_FARM instead of TILE_FLOOR)
5. Place TILE_FORGE at center of each workshop (visual)
6. Set room types

---

## Mushroom Farm Logic (in fortPlanTick / FS_DONE)

Every `MUSHROOM_GROW_INTERVAL` ticks:
- For each TILE_FARM tile with no item on it:
  - Spawn ITEM_MUSHROOM on that tile
  - Queue TASK_HAUL to stockpile

Kitchen crafting (when ≥ CRAFT_MUSH_FOOD_COST mushrooms in stockpile):
- Queue TASK_CRAFT at Kitchen center with auxType=CRAFT_MUSHROOM_FOOD

Still crafting (when ≥ CRAFT_MUSH_BEER_COST mushrooms in stockpile):
- Queue TASK_CRAFT at Still center with auxType=CRAFT_MUSHROOM_BEER

---

## File Change Order

1. **config.h** — MAP_W=53, all new coordinates, mushroom constants
2. **types.h** — new tile/item/craft/room/stage enums
3. **renderer.cpp** — render TILE_FARM, room tints for workshops
4. **fortplan.cpp** — new stages, mushroom grow logic, workshop placement
5. **map.cpp** — TILE_FARM passability (passable but not growable through walls)
6. **tasks.cpp** — craft cost/product for CRAFT_MUSHROOM_FOOD/BEER
7. **pc_sim/main.cpp** — verify build still compiles

---

## Notes / Gotchas

- `int8_t` coordinates in Task struct: max value 127. All new coords (x≤52, y≤28) fit fine.
- Tomb was at x=29-37, y=19-23. NEW tomb: x=22-28, y=20-24 (east of bedrooms, west of workshop corridor).
- Workshop corridor digs a single tile-wide row at y=14 connecting x=20 to x=52 — this is just floor tiles.
- TILE_FARM tiles should NOT drop stone when dug (they're soil). Need to handle in dwarvesTick or tasks.cpp.
- Beer goes into ITEM_DRINK slots, food into ITEM_FOOD slots (or just their own slots but consumed same way as food/drink).
  → Simplest: ITEM_BEER acts exactly like ITEM_DRINK (add to gDrinkSupply when hauled to stockpile).
  → ITEM_MUSHROOM_FOOD acts exactly like ITEM_FOOD. OR just have crafting add directly to supply counters.
- Surface stockpile initial zone: (2,8)-(8,20) in old code. With MAP_W=53 this still fits on surface.
- fortPlaceCart puts wood/stone items; those still haul to initial stockpile zone. Fine.
