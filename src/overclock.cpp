#include "overclock.h"
#include <cstdint>

// ---------------------------------------------------------------------------
// SH7305 CPG MMIO — these addresses are identical across all SH7305-based
// Casio calculators (fx-CG50, fx-CGx0, fx-CP400 / ClassPad).
// ---------------------------------------------------------------------------
static volatile uint32_t* const CPG_FRQCR  =
    reinterpret_cast<volatile uint32_t*>(0xA4150000u); // Frequency Control Register
static volatile uint32_t* const CPG_FLLFRQ =
    reinterpret_cast<volatile uint32_t*>(0xA415003Cu); // FLL Frequency Register

// Snapshot of the values the OS set at boot.  Safe to restore unconditionally.
static uint32_t default_fllfrq = 0u;
static uint32_t default_frqcr  = 0u;
static bool     initialized    = false;

// ---------------------------------------------------------------------------
// FLF increment table
// FLF = bits[13:0] of FLLFRQ.  All downstream clocks scale linearly with FLF,
// so +50 on FLF ≈ +5.6% on every clock in the system (at FLF default ≈ 900).
// Conservative ceiling: 1023 (SELXM=0 single-range, avoids mode switching).
// Ptune4 F5/alpha preset for CG50/CG100 uses FLF=900 SELXM=1 ≈ FLF_eff=1800,
// but that requires also tuning BSC wait-states.  We stay firmly within the
// SELXM=0 headroom for maximum compatibility and no BSC adjustments needed.
// ---------------------------------------------------------------------------
static const int flf_increment[OC_LEVEL_MAX + 1] = {
    0,    // Level 0 — OS default   (no write)
    50,   // Level 1 — LIGHT        (+5.6% at FLF=900)
    150,  // Level 2 — MEDIUM       (+16.7%)
    235,  // Level 3 — FAST         (+26.1%)
    345,  // Level 4 — TURBO        (+38.3%)
};

const char* const overclock_level_names[OC_LEVEL_MAX + 1] = {
    "DEFAULT",
    "LIGHT",
    "MEDIUM",
    "FAST",
    "TURBO",
};

// ---------------------------------------------------------------------------
// Busy-wait for FLL relock after a FLLFRQ write.
// SH7305 data-sheet specifies a maximum FLL lock time of 16384 FLL cycles.
// At the lowest FLL output (~6.5 MHz) that is ~2.5 ms.  We spin for ~500 k
// iterations which is well over 2.5 ms at default CPU speed.  At overclocked
// speed the iterations execute faster in wall-time but remain safely above the
// minimum lock time even at the highest level.
// ---------------------------------------------------------------------------
[[gnu::noinline]] static void fll_lock_wait() {
    int i = 500000;
    while (i > 0) {
        i--; /* spin — wait for FLL to relock */
        __asm__ volatile(""); // prevent optimisation of the loop body
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void oclock_init() {
    // Read current (OS-set) register values and save them.
    // We intentionally do NOT write anything here: the calc starts at its
    // normal speed regardless of the previously saved overclock level; the
    // caller (main) must then call oclock_apply(savedLevel) if it wants to
    // apply a non-default speed.
    default_frqcr  = *CPG_FRQCR;
    default_fllfrq = *CPG_FLLFRQ;
    initialized    = true;
}

void oclock_apply(int level) {
    if (!initialized) return;
    if (level < OC_LEVEL_MIN) level = OC_LEVEL_MIN;
    if (level > OC_LEVEL_MAX) level = OC_LEVEL_MAX;

    // Restore the OS default FRQCR first (bus/peripheral dividers stay stock).
    *CPG_FRQCR = default_frqcr;

    if (level == OC_LEVEL_MIN) {
        // Full restore — write back the OS FLLFRQ and wait for lock.
        *CPG_FLLFRQ = default_fllfrq;
        fll_lock_wait();
        return;
    }

    // Compute new FLF, clamped to the safe SELXM=0 ceiling of 1023.
    uint32_t base_flf = default_fllfrq & 0x3FFFu; // bits[13:0]
    uint32_t selxm    = (default_fllfrq >> 14) & 1u;

    uint32_t new_flf = base_flf + static_cast<uint32_t>(flf_increment[level]);
    if (selxm == 0u && new_flf > 1023u) new_flf = 1023u;
    if (new_flf > 0x3FFFu)             new_flf  = 0x3FFFu;

    // Write new FLF, preserving all other FLLFRQ bits (SELXM, reserved).
    *CPG_FLLFRQ = (default_fllfrq & ~0x3FFFu) | new_flf;
    fll_lock_wait();
}

int oclock_speed_percent(int level) {
    if (!initialized) return 100;
    if (level <= 0)   return 100;

    uint32_t base_flf = default_fllfrq & 0x3FFFu;
    if (base_flf == 0u) return 100; // avoid divide by zero

    uint32_t selxm   = (default_fllfrq >> 14) & 1u;
    uint32_t new_flf = base_flf + static_cast<uint32_t>(flf_increment[level]);
    if (selxm == 0u && new_flf > 1023u) new_flf = 1023u;

    // Percentage = new_flf / base_flf × 100, rounded to nearest integer.
    return static_cast<int>((new_flf * 100u + base_flf / 2u) / base_flf);
}
