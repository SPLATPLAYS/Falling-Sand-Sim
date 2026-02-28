# Falling Sand Simulator

A cellular automaton falling sand simulator for the Casio Classpad calculator!

## Features

- **Multiple particle types**: Sand, Water, Stone, Wall, Lava, Plant, Ice, and Air (eraser)
- **Realistic physics**: Particles fall under gravity with different behaviors
  - Sand: Falls and settles in piles; displaces water; sustained heat converts it to stone
  - Water: Falls and flows sideways in a randomized direction; freezes to ice when cold; evaporates when hot
  - Stone: Falls straight down, no sideways movement; melts back to lava at extreme heat
  - Wall: Static, used to build structures; cannot be displaced or overwritten
  - Lava: Flows like water but slower; converts adjacent sand→stone, evaporates adjacent water, melts adjacent ice to water, and burns adjacent plants; isolated lava slowly solidifies into stone; continuous heat source
  - Plant: Static; grows into adjacent empty cells when touching water (1-in-10 chance per frame); burns to air when adjacent to lava or under sustained heat
  - Ice: Falls like sand and can displace water; pins surrounding temperature to near-freezing, converting nearby water to ice; melts to water when warm
  - Air: Eraser — removes particles from the grid
- **Temperature system**: Coarse 48×24 heat grid propagated every physics tick; drives particle phase changes (freezing, evaporation, melting, scorching) and provides a toggle-able heat-map overlay
- **Adjustable brush size**: Sizes 1–9, default 3; controlled via a UI slider or the **+**/**−** keys (with key-hold repeat); persisted across sessions via MCS
- **On-screen FPS counter**: 5×7 pixel font, up to 5 digits with leading-zero suppression, averaged over the last 30 physics frames
- **Interactive UI bar**: Tap particle swatches to select type; visual white-border highlight shows the active selection; Air shown in bright pink for visibility
- **Frame skipping**: Optional performance optimization — physics runs at full speed while rendering is throttled
- **MCS persistence**: Brush size is automatically saved to and restored from calculator memory
- **Performance optimized**: 192×96 simulation grid with direct VRAM writes at 2×2 px per cell; hot functions placed in ILRAM; grid split across on-chip X/Y RAM

## Controls

- **Touch screen**: Draw particles on the grid
- **Bottom UI bar**: Tap a particle swatch to select that particle type
- **Brush size slider**: Drag (or tap) the slider track in the UI bar to set brush size
- **+ / − keys**: Increase / decrease brush size (also responds to key-hold)
- **0 key**: Toggle the temperature heat-map overlay
- **CLEAR (AC) key**: Clear the entire grid and reset to walls only
- **EXE key / Action bar ESC**: Exit the simulator

## How to Setup

1. Clone the project
2. Enter the devcontainer
3. `make compile_commands.json` or use the vscode task
4. Restart clangd (e.g. via CTRL+SHIFT+P in vscode)

## How to Build

```sh
make -j
```

Or execute the default vscode build task with CTRL+SHIFT+B

## How to Run

Copy `dist/FallingSandSim.hh3` to the root of the calculator when connected in USB storage mode, then select and run from the launcher.

## Technical Details

- Grid size: 192×96 cells
- Display resolution: 384×192 pixels (2×2 pixel cells per grid cell)
- Update algorithm: Bottom-to-top scan with alternating left/right direction each row; bitset tracks which cells have already moved this tick to prevent double-updates
- On-chip RAM layout: rows 0–41 in X RAM, rows 42–83 in Y RAM, rows 84–95 in regular RAM (fits within the 8 KB per bank limit)
- ILRAM: `simulate()` and `drawGrid()` are placed in the SH7305's internal instruction RAM for faster fetch/execute
- PRNG: XorShift32 for fast, lightweight random number generation
- Brush size range: 1–9, default 3; saved/loaded via MCS folder `FSandSim` / variable `BrushSz`
- FPS timing: `gettimeofday()` backed by TMU2 at Phi/16 on the SH7305, giving sub-microsecond resolution

### Temperature System

The temperature system uses a coarse 48×24 grid (each coarse cell covers a 4×4 block of fine cells), requiring only 1,152 bytes instead of 18 KB for a full-resolution grid. The system runs every physics tick.

**Each tick:**
1. **Diffusion** — 4 passes of weighted 4-neighbour blending (`self×4 + neighbours×1, >>3`), spreading heat up to 4 coarse cells per tick
2. **Source injection** — coarse tiles are pinned or nudged based on their contents:
   - Lava tile → pinned to 255 (max heat)
   - All-wall tile → pinned to 50 (ambient, thermal insulator)
   - Ice tile → pinned to 5 (near-freezing cold source)
   - Water present → aggressively cooled toward 20 (cold) at 3 units/tick
   - Air exposed → slow drift toward ambient (1 unit/tick)
   - Buried (solid, no air) → very slow drift (1-in-16 chance per tick)

**Phase-change thresholds (0–255 scale):**

| Threshold | Value | Effect |
|-----------|-------|--------|
| `TEMP_ICE_SURFACE` | 5 | Temperature ice pins its coarse tile to |
| `TEMP_FREEZE_WATER` | 12 | Water converts to ice at or below this |
| `TEMP_COLD` | 20 | Cold baseline (water resting temperature) |
| `TEMP_AMBIENT` | 50 | Room temperature |
| `TEMP_ICE_MELT` | 65 | Ice melts to water at or above this |
| `TEMP_HOT` | 200 | Sand scorches to stone; water evaporates; plant burns |
| `TEMP_STONE_MELT` | 230 | Stone (surrounded by lava) melts back to lava |
| `TEMP_LAVA` | 255 | Lava source temperature |

The UI-zone coarse rows are always pinned to ambient so heat cannot bleed behind the particle selector bar.

**Heat-map overlay**: Press **0** to toggle a false-colour view where every cell is coloured by its coarse-tile temperature instead of its particle type (WALL cells show ambient; all others show the heat map).

### Performance Optimization: Frame Skipping

Frame skipping allows the physics simulation to run at full speed while reducing rendering overhead. This can significantly improve performance on the calculator.

**How to enable:**
1. Open `src/config.h`
2. Set `FRAME_SKIP_ENABLED` to `true`
3. Adjust `FRAME_SKIP_AMOUNT`:
   - `0` = No skipping (max FPS, default)
   - `1` = Skip 1 render frame (physics still runs every frame)
   - `2` = Skip 2 render frames
   - `3` = Skip 3 render frames

**Benefits:**
- Physics simulation continues at full speed regardless of rendering frequency
- Input handling remains responsive every frame
- Useful for complex simulations with many particles

**Note:** The FPS counter always shows *physics* frames per second (one count per `simulate()` call), regardless of whether frame skipping is enabled.

### Particle Fall Speeds

Fall speeds are configured in `src/config.h` and control how often each particle updates per physics tick. Values must be powers of 2 so the modulo check compiles to a cheap bitwise AND on the SH4 (which has no hardware divide instruction).

| Particle | `FALL_SPEED` | Update frequency |
|----------|-------------|-----------------|
| Stone    | 1           | Every frame (fastest fall) |
| Water    | 1           | Every frame (fast flow) |
| Sand     | 2           | ~50% of frames |
| Lava     | 2           | ~50% of frames (slower than water) |
| Ice      | 2           | ~50% of frames (falls like sand) |

## Future Enhancements

- More particle types (oil, acid, fire, smoke, etc.)
- Save/load simulation states
- Color aging / weathering for particles
