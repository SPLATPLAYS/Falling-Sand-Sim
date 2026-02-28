# Falling Sand Simulator

A cellular automaton falling sand simulator for the Casio Classpad calculator!

## Features

- **Multiple particle types**: Sand, Water, Stone, Wall, Lava, and Plant
- **Realistic physics**: Particles fall under gravity with different behaviors
  - Sand: Falls and settles in piles
  - Water: Falls and flows horizontally
  - Stone: Falls straight down
  - Wall: Static, used to build structures
  - Lava: Flows like water, converts sand to stone and evaporates water
  - Plant: Static, grows when touching water, burns when touching lava
- **Touch screen controls**: Draw particles directly on screen
- **Interactive UI**: Select particle types from the bottom menu
- **Frame skipping**: Optional performance optimization for smoother physics
- **Performance optimized**: 192x96 simulation grid running in real-time

## Controls

- **Touch screen**: Draw particles on the grid
- **Bottom UI bar**: Tap particle swatches to select particle type
- **Clear button**: Clear the screen
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

- Grid size: 192x96 cells
- Display resolution: 384x192 pixels (2x2 pixel cells)
- Update algorithm: Bottom-to-top scan with alternating horizontal direction
- Particle interactions: Density-based displacement (sand displaces water), lava converts sand to stone and evaporates water

### Performance Optimization: Frame Skipping

Frame skipping allows the physics simulation to run at full speed while reducing rendering overhead. This can significantly improve performance on the calculator.

**How to enable:**
1. Open `src/config.h`
2. Set `FRAME_SKIP_ENABLED` to `true`
3. Adjust `FRAME_SKIP_AMOUNT`:
   - `0` = No skipping (60 FPS target, default)
   - `1` = Skip 1 frame (30 FPS rendering, 60 FPS physics)
   - `2` = Skip 2 frames (20 FPS rendering, 60 FPS physics)
   - `3` = Skip 3 frames (15 FPS rendering, 60 FPS physics)

**Benefits:**
- Physics simulation continues at full speed regardless of rendering
- Input handling remains responsive every frame
- Useful for complex simulations with many particles

**Note:** The FPS counter shows *rendered* frames per second, not physics updates.

## Future Enhancements

- More particle types (oil, acid, etc.)
- Adjustable brush sizes
- Save/load states
- Color variations for particles
- Temperature simulation
