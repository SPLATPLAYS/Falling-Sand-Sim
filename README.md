# Falling Sand Simulator

A cellular automaton falling sand simulator for the Casio ClassPad / fx-CG series calculator!

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
- **Start menu**: Title screen with PLAY, SETTINGS, and EXIT buttons; navigable by touch or keyboard
- **Settings menu**: In-app settings screen with two sub-menus:
  - **CPU Speed**: Overclock the SH7305 CPU through 5 levels (DEFAULT → LIGHT → MEDIUM → FAST → TURBO); live preview as you navigate; estimated speed percentage shown per level
  - **Sim Speed**: Choose one of 5 simulation speed modes (NORMAL, X2, X3, X5, X9) controlling how many physics ticks run per rendered frame
- **Overclock support**: FLL-based CPU frequency scaling from ~118 MHz (default) up to ~163 MHz (+38%); no BSC wait-state changes needed; safe register restore on exit
- **Frame skipping**: Physics always runs at full speed; rendering is throttled according to the selected sim speed mode for a higher physics tick rate
- **MCS persistence**: Brush size, CPU speed level, and sim speed mode are automatically saved to and restored from calculator memory (MCS folder `FSandSim`)
- **Performance optimized**: 192×96 simulation grid with direct VRAM writes at 2×2 px per cell; hot functions placed in ILRAM; grid split across on-chip X/Y RAM

## Controls

### Start Menu
- **Touch**: Tap PLAY, SETTINGS, or EXIT buttons
- **EXE**: Start the simulation (same as PLAY)
- **CLEAR / Action bar ESC**: Exit the application

### Settings Menu
- **Up / Down**: Navigate between CPU SPEED and SIM SPEED rows
- **EXE**: Enter the highlighted sub-menu
- **CLEAR / Action bar ESC**: Return to the start menu

### CPU Speed / Sim Speed Sub-Menus
- **Up / Down**: Navigate levels (CPU Speed sub-menu applies the level immediately as a live preview)
- **EXE**: Confirm and save the selection
- **CLEAR / Action bar ESC**: Cancel and return to the settings menu

### In-Game
- **Touch screen**: Draw particles on the grid
- **Bottom UI bar**: Tap a particle swatch to select that particle type
- **Brush size slider**: Drag (or tap) the slider track in the UI bar to set brush size
- **+ / − keys**: Increase / decrease brush size (also responds to key-hold)
- **0 key**: Toggle the temperature heat-map overlay
- **CLEAR (AC) key**: Clear the entire grid and reset to walls only
- **EXE key / Action bar ESC**: Return to the start menu

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
- MCS persistence: brush size (`BrushSz`), CPU overclock level (`OCLevel`), and sim speed mode (`SimSpd`) all saved/loaded under MCS folder `FSandSim`
- FPS timing: `gettimeofday()` backed by TMU2 at Phi/16 on the SH7305, giving sub-microsecond resolution; when frame skipping is active the FPS counter reflects rendered frames per second (physics still runs at full tick rate)

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

### CPU Overclock

The overclock system adjusts the SH7305 FLL (Frequency-Lock Loop) multiplier register (`FLLFRQ`) to scale all downstream clocks proportionally. No PLL, bus-divider, or BSC wait-state registers are modified, so SDRAM/ROM timing remains conservative and compatible across devices.

The OS-default register values are snapshotted at startup (`oclock_init()`). Level 0 never writes to the hardware. On exit the hardware is left at the last applied level (restoring to default would require writing the register again, which is unnecessary given the OS resets clocks on app exit).

| Level | Name    | FLF delta | Est. CPU speed |
|-------|---------|-----------|----------------|
| 0     | DEFAULT | +0        | ~118 MHz       |
| 1     | LIGHT   | +50       | ~124 MHz (+6%) |
| 2     | MEDIUM  | +150      | ~135 MHz (+17%)|
| 3     | FAST    | +235      | ~149 MHz (+26%)|
| 4     | TURBO   | +345      | ~163 MHz (+38%)|

After each `FLLFRQ` write a busy-wait of ~500 k iterations is performed to allow the FLL to relock (the SH7305 datasheet specifies a maximum lock time of 16,384 FLL cycles ≈ 2.5 ms at minimum FLL output).

### Simulation Speed (Frame Skipping)

The sim speed setting controls how many physics ticks execute between rendered frames. Physics input handling runs every tick regardless; only the VRAM write and `LCD_Refresh()` call are skipped.

| Mode   | Skip amount | Physics ticks per frame |
|--------|-------------|------------------------|
| NORMAL | 0           | 1                       |
| X2     | 1           | 2                       |
| X3     | 2           | 3                       |
| X5     | 4           | 5                       |
| X9     | 8           | 9                       |

The setting is persisted via MCS (`SimSpd` variable in folder `FSandSim`).

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
