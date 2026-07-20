# Gabbro Platform Support - Known Compromises & Future Work

Initial gabbro (260x260 round, color) support was added by scaling from the chalk
(180x180 round) layout using a 1.444x scale factor. Several areas need revisiting
with proper design work and on-device testing.

## Rainbow Hand Image

**Current state:** `rainbow_hand~gabbro.png` is a copy of the emery image (11x163px).
The emery image was chosen because its height (163px) is close to the ideal gabbro
minute hand length (~159px for a 130px screen radius).

**What needs to be done:** Create a purpose-built rainbow hand image for the 260x260
round display. The ideal dimensions would be approximately 14x159px (width scaled
proportionally from chalk's 10x110px). The pivot point offset is currently set to
(5, 81) matching the emery image - this should be recalculated for the new image.

**Relevant code:** `src/c/consts.h` defines `RAINBOW_HAND_OFFSET_X` and
`RAINBOW_HAND_OFFSET_Y` for the gabbro platform block.

## Tick and Time Point Coordinates

**Current state:** All 12 tick point pairs and 12 time display points were computed
mathematically by scaling chalk's coordinates by 260/180. Tick positions were
calculated from circle geometry (outer radius 130, inner radius 121).

**What needs to be done:** Test on an actual gabbro device or emulator to verify
visual alignment. The time text positions may need manual tweaking for optimal
readability, especially at diagonal positions (1, 2, 4, 5, 7, 8, 10, 11 o'clock)
where text blocks interact with hand collision detection.

**Relevant code:** `src/c/consts.h` gabbro `ticks_points[12][2]` and
`time_points[12]` arrays.

## Quadrant Info Center Positions

**Current state:** The four info center positions (North, South, East, West) were
scaled from chalk by 1.444x.

**What needs to be done:** Verify on-device that weather, date, steps, and
battery/bluetooth info blocks are positioned correctly and don't overlap with the
larger text from the 33pt font.

**Relevant code:** `src/c/quadrant.c` gabbro `SOUTH_INFO_CENTER`,
`NORTH_INFO_CENTER`, `EAST_INFO_CENTER`, `WEST_INFO_CENTER`.

## Text Block Render + Collision Size (RESOLVED)

**Was:** Both the render frame `TEXT_BLOCK_SIZE` (`src/c/text_block.h`) and the
collision rectangle `BLOCK_SIZE` (`src/c/quadrant.c`) were fixed at `GSize(70, 23)`
and `GSize(38, 20)` for all platforms. With the 33pt font on gabbro, the render
frame was too small: `graphics_draw_text` word-wrapped the trailing glyph onto a
clipped second line, so temperature/steps showed a stray "-" (e.g. `2.2-`, `6-`).

**Fix:** Both are now platform-conditional, scaled to match the font
(`GSize(105, 34)` render / `GSize(55, 29)` collision on gabbro; intermediate values
for emery's 28pt). Verify on-device that the widened blocks still position well and
don't overlap at diagonal hand positions.

**Relevant code:** `src/c/text_block.h` `TEXT_BLOCK_SIZE`, `src/c/quadrant.c`
`BLOCK_SIZE`.

## Font Size

**Current state:** Added `FONT_NUPE_33` (33pt) for gabbro, computed from chalk's
23pt scaled by 260/180. This is a new resource entry in package.json.

**What needs to be done:** Verify 33pt looks good on the 260x260 display. May need
adjustment up or down depending on the Nupe font's specific metrics at this size.
