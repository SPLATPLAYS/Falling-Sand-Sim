#ifndef OVERCLOCK_H
#define OVERCLOCK_H

// ---------------------------------------------------------------------------
// CPU overclock for the Casio ClassPad / fx-CP400 (SH7305 / SH4AL-DSP).
//
// Technique (same as Ptune4):
//   The SH7305 clock chain is:
//     OSC (32.768 kHz) → FLL → PLL → dividers → CPU / Bus / Peripheral clocks
//   The FLL Frequency Register (FLLFRQ at 0xA415003C) has two controls:
//     • FLF (bits [13:0]): FLL integer multiplier.
//     • SELXM (bit 14): selects the FLL reference.  At 0 (default) the
//       reference is XTAL/2; at 1 it is XTAL directly, doubling the FLL
//       output at the same FLF value.
//   Levels 1-4 only increment FLF within the SELXM=0 range (max 1023).
//   Level 5 (TURBO+) switches SELXM to 1, keeping the OS FLF unchanged —
//   this doubles every downstream clock at once, matching Ptune4's
//   alpha-F5 preset for the fx-CG50 / fx-CG100.
//
// Safety:
//   • Level 0: OS default — FLLFRQ is never written, only read & stored.
//   • Levels 1-4: FLF is incremented by a conservative delta over the
//     default; cap of 1023 (SELXM=0 range) respected.
//   • Level 5 (TURBO+): SELXM switched to 1 (doubles FLL at same FLF).
//     CS3WCR SDRAM timing is set to the Ptune4 alpha-F5 values to keep
//     SDRAM stable at the higher bus clock.  CS3WCR is fully restored to
//     the OS default when returning to any lower level.
//
// Expected results (assuming stock FLF ≈ 900, PLL×8, IFC÷2 → 118 MHz CPU):
//   Level 0:  ~118 MHz  (default)
//   Level 1:  ~124 MHz  (+5.6%)
//   Level 2:  ~135 MHz  (+16.7%)
//   Level 3:  ~149 MHz  (+26.1%)
//   Level 4:  ~163 MHz  (+38.3%)
//   Level 5:  ~236 MHz  (+100%)   ← SELXM=1 "TURBO+"
// ---------------------------------------------------------------------------

// Overclock level range
constexpr int OC_LEVEL_MIN     = 0;
constexpr int OC_LEVEL_MAX     = 5;
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
