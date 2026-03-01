#ifndef OVERCLOCK_H
#define OVERCLOCK_H

// ---------------------------------------------------------------------------
// CPU overclock for the Casio ClassPad / fx-CP400 (SH7305 / SH4AL-DSP).
//
// Technique (same as Ptune4):
//   The SH7305 clock chain is:
//     OSC (32.768 kHz) → FLL → PLL → dividers → CPU / Bus / Peripheral clocks
//   The FLL Frequency Register (FLLFRQ at 0xA415003C) sets the FLL multiplier
//   (FLF, bits [13:0]).  Increasing FLF raises every downstream clock
//   proportionally, with no need to touch the PLL or bus-divider registers.
//
// Safety:
//   • Level 0: OS default — FLLFRQ is never written, only read & stored.
//   • Levels 1-4: FLF is incremented by a conservative delta over the default.
//     The cap of 1023 (SELXM=0 range) is respected to avoid instability.
//   • BSC wait-state registers are deliberately NOT modified, so ROM/SDRAM
//     timing stays conservative (safe for high bus clocks).
//
// Expected results (assuming stock FLF ≈ 900, PLL×8, IFC÷2 → 118 MHz CPU):
//   Level 0:  ~118 MHz  (default)
//   Level 1:  ~124 MHz  (+5.6%)
//   Level 2:  ~135 MHz  (+16.7%)
//   Level 3:  ~149 MHz  (+26.1%)
//   Level 4:  ~163 MHz  (+38.3%)
// ---------------------------------------------------------------------------

// Overclock level range
constexpr int OC_LEVEL_MIN     = 0;
constexpr int OC_LEVEL_MAX     = 4;
constexpr int OC_LEVEL_DEFAULT = 0;  // start at OS default (no writes on level 0)

// Human-readable name for each level
extern const char* const overclock_level_names[OC_LEVEL_MAX + 1];

// Initialise: snapshot the hardware-default CPG registers.
// MUST be called once before oclock_apply().
void oclock_init();

// Apply 'level' (0 = OS default, 1-4 = progressively faster).
// Safe to call multiple times (re-applying the same level is a no-op in terms
// of visible effect, though it does issue the FLL write + lock wait).
void oclock_apply(int level);

// Return the estimated CPU speed as a percentage of the OS default (100 = default).
int oclock_speed_percent(int level);

#endif // OVERCLOCK_H
