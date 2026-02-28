# Falling Sand Simulator

A cellular automaton falling sand simulator for the Casio Classpad calculator!

## Features

- **Multiple particle types**: Sand, Water, Stone, Wall, Lava, Plant, and Air (eraser)
- **Realistic physics**: Particles fall under gravity with different behaviors
  - Sand: Falls and settles in piles; displaces water
  - Water: Falls and flows horizontally in both directions
  - Stone: Falls straight down, no sideways movement
  - Wall: Static, used to build structures; cannot be displaced
  - Lava: Flows like water (but slower); converts adjacent sand to stone, evaporates adjacent water, and burns adjacent plants
  - Plant: Static; grows into adjacent empty cells when touching water; burns to air when touching lava
  - Air: Eraser — removes particles from the grid
- **Adjustable brush size**: Sizes 1–9, controlled via a UI slider or the **+**/**−** keys; persisted across sessions via MCS
- **On-screen FPS counter**: Averaged over the last 30 rendered frames
- **Interactive UI bar**: Tap particle swatches to select type; visual highlight shows the active selection
- **Frame skipping**: Optional performance optimization — physics runs at full speed while rendering is throttled
- **MCS persistence**: Brush size is automatically saved to and restored from calculator memory
- **Performance optimized**: 192×96 simulation grid with direct VRAM writes at 2×2 px per cell

## Controls

- **Touch screen**: Draw particles on the grid
- **Bottom UI bar**: Tap a particle swatch to select that particle type
- **Brush size slider**: Drag (or tap) the slider in the UI bar to set brush size
- **+ / − keys**: Increase / decrease brush size (also responds to key-hold)
- **CLEAR (AC) key**: Clear the entire grid and reset to walls only
- **EXE key**: Exit the simulator

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

Copy `dist/CPapp.hh3` to the root of the calculator when connected in USB storage mode, then select and run from the launcher.

## Technical Details

- Grid size: 192×96 cells
- Display resolution: 384×192 pixels (2×2 pixel cells per grid cell)
- Update algorithm: Bottom-to-top scan with alternating horizontal direction each row
- Particle interactions:
  - Sand displaces water (density-based)
  - Lava converts adjacent sand→stone, water→air, plant→air
  - Plant grows into empty cells when water is adjacent (1-in-10 chance per frame)
- Temperature system: Each cell carries a temperature value (0–255); lava cells start at 255, water at 20, all others at 50 (ambient)
- PRNG: XorShift32 for fast, lightweight random number generation
- Brush size range: 1–9, default 3; saved/loaded via MCS folder `FSandSim` / variable `BrushSz`

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

**Note:** The FPS counter shows *rendered* frames per second, not physics updates.

### Particle Fall Speeds

Fall speeds are configured in `src/config.h` and control how often each particle updates:

| Particle | `FALL_SPEED` | Behaviour |
|----------|-------------|-----------|
| Stone    | 1           | Updates every frame (fastest fall) |
| Sand     | 2           | Updates ~50% of frames |
| Water    | 1           | Updates every frame (fast flow) |
| Lava     | 2           | Updates ~50% of frames (slower than water) |

## Future Enhancements

- More particle types (oil, acid, fire, etc.)
- Save/load simulation states
- Color variations / aging for particles
- Active temperature propagation and heat-based interactions
