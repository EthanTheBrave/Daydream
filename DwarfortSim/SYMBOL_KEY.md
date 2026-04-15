# DwarfortSim — Symbol Key

All glyphs use CP437 encoding on a 320×240 TFT display (53 columns × 29 rows, 6×8 px font).

---

## Entities

| Glyph | Colour | Meaning |
|-------|--------|---------|
| `☺` (CP437 01) | Yellow | Live dwarf |
| `☻` (CP437 02) | Dark grey | Dwarf corpse |
| `g` | Green | Goblin |
| `s` | White | Sheep |
| `c` | Orange | Cat |

---

## Terrain

| Glyph | Foreground | Background | Meaning |
|-------|-----------|------------|---------|
| ` ` | — | Black | Unrevealed / fog of war |
| `#` | Grey | Dark grey | Solid wall |
| `#` | **Yellow** | Dark grey | Wall designated for digging |
| `.` | Tan | (room tint) | Excavated floor |
| `~` | **Orange** | Red tint | Workshop floor (heat shimmer) |
| `+` | **Cyan** | Teal tint | Barracks floor (training mat) |
| `,` | Green | Black | Grass (surface) |
| `"` | Dark green | Black | Shrub |
| `*` | Pink | Black | Flower |
| `T` | Dark green | Black | Tree (not passable; can be chopped) |
| `+` | Brown | Black | Placed door |
| `~` | Blue | Dark blue | Water (passable) |
| `/` | Grey | Dark grey | Ramp |
| `<` | White | Black | Stair |
| `W` | Brown | Black | Embark supply cart |
| `:` | Dark brown | Dark green | Farm plot (grows mushrooms) |

---

## Placed furniture

| Glyph | Foreground | Background | Meaning |
|-------|-----------|------------|---------|
| `[` | Cyan | Blue tint | Bed (placed in bedroom) |
| `n` | Brown | Blue-grey tint | Table (placed in hall) |
| `h` | Brown | Blue-grey tint | Chair (placed in hall) |
| `&` | Orange | Red tint | Workshop / forge |
| `\|` | White | Purple tint | Coffin tomb (buried dwarf) |

---

## Items on the ground / in stockpile

| Glyph | Colour | Item |
|-------|--------|------|
| `*` | Orange | Stone boulder |
| `/` | Brown | Wood log |
| `%` | Red | Food |
| `u` | Cyan | Drink (water barrel) |
| `U` | Cyan | Beer |
| `m` | Purple | Mushroom |
| `O` | Brown | Barrel |
| `[` | Cyan | Bed (unplaced, in stockpile) |
| `n` | Brown | Table (unplaced) |
| `h` | Brown | Chair (unplaced) |
| `+` | Brown | Door (unplaced) |
| `\|` | Grey | Coffin (unplaced) |
| `☻` (CP437 02) | Dark grey | Corpse item (awaiting burial) |

---

## Room background tints

Excavated floor tiles inherit a subtle background colour from their room type:

| Colour | Room type |
|--------|-----------|
| Dim blue-grey | Main hall |
| Dim red-brown | Stockpile |
| Dim blue | Bedroom |
| Red | Workshop |
| Dim green | Farm |
| Dim purple | Tomb |
| Dim teal | Barracks |

---

## Status bar (bottom row)

```
@N  d:N  h:N  c:N  t:N    F:N  Dr:N    <Stage>
```

| Field | Meaning |
|-------|---------|
| `@N` | Alive dwarves |
| `d:N` | Active dig tasks |
| `h:N` | Active haul tasks |
| `c:N` | Active craft tasks |
| `t:N` | Dwarves currently training |
| `F:N` | Food supply units |
| `Dr:N` | Drink supply units |
| `<Stage>` | Current fort construction stage |
