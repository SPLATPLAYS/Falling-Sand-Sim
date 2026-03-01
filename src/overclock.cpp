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

// ---------------------------------------------------------------------------
// SH7305 BSC (Bus State Controller) — SDRAM timing on CS3 (Area 3)
// BSC base address: 0xFEC10000 (P4 always-accessible mapping)
// CS3WCR is at offset 0x2C within the BSC register block.
//
// CS3WCR SDRAM-mode bit layout (SH7730-compatible, big-endian 32-bit):
//   [31:15] reserved
//   [14:13] TRP   — row precharge time
//   [12]    reserved
//   [11:10] TRCD  — RAS-to-CAS delay
//     [9]   reserved
//     [8:7] A3CL  — CAS latency  (0b01=CL2, 0b10=CL3; 0b00 and 0b11 invalid)
//     [6:5] reserved
//     [4:3] TRWL  — write precharge time
//     [2]   reserved
//     [1:0] TRC   — row cycle time (smaller value = fewer wait cycles)
//
// SDMR3 is a "ghost" address range where a write (any value) issues a
// Mode Register Set (MRS) command to the SDRAM chip.  The CAS latency is
// encoded in the upper address bits, so the target address selects CL:
//   0xFEC15040 → MRS with CL=2
//   0xFEC15060 → MRS with CL=3
// This must be issued after every A3CL change so the chip stays in sync.
// ---------------------------------------------------------------------------
static volatile uint32_t* const BSC_CS3WCR =
    reinterpret_cast<volatile uint32_t*>(0xFEC1002Cu);
static volatile uint16_t* const SDMR3_CL2 =
    reinterpret_cast<volatile uint16_t*>(0xFEC15040u);
static volatile uint16_t* const SDMR3_CL3 =
    reinterpret_cast<volatile uint16_t*>(0xFEC15060u);

// Snapshot of the values the OS set at boot.  Safe to restore unconditionally.
static uint32_t default_fllfrq = 0u;
static uint32_t default_frqcr  = 0u;
static uint32_t default_cs3wcr = 0u;
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
// Level 5 uses SELXM=1 rather than an FLF offset, so its entry is 0 (unused).
static const int flf_increment[OC_LEVEL_MAX + 1] = {
    0,    // Level 0 — OS default   (no write)
    50,   // Level 1 — LIGHT        (+5.6% at FLF=900)
    150,  // Level 2 — MEDIUM       (+16.7%)
    235,  // Level 3 — FAST         (+26.1%)
    345,  // Level 4 — TURBO        (+38.3%)
    0,    // Level 5 — TURBO+       handled via SELXM=1 (not an FLF delta)
};

const char* const overclock_level_names[OC_LEVEL_MAX + 1] = {
    "DEFAULT",
    "LIGHT",
    "MEDIUM",
    "FAST",
    "TURBO",
    "TURBO+",
};

// ---------------------------------------------------------------------------
// CS3WCR value for TURBO+ (SELXM=1, ~2× bus clock).
// Mirrors the Ptune4 alpha-F5 preset for fx-CG50/CG100:
//   TRP=2  → bits[14:13] = 0b10  (0x4000)
//   TRCD=2 → bits[11:10] = 0b10  (0x0800)
//   A3CL=1 → bits[ 8: 7] = 0b01  (0x0080)  ← CL=2
//   TRWL=2 → bits[ 4: 3] = 0b10  (0x0010)
//   TRC=2  → bits[ 1: 0] = 0b10  (0x0002)  ← 6 bus cycles
// The SDRAM chip must also be updated via an MRS write to SDMR3_CL2.
// ---------------------------------------------------------------------------
static const uint32_t CS3WCR_TURBO_PLUS = 0x4892u;

// ---------------------------------------------------------------------------
// BSC helpers — called from oclock_apply(), not exposed in the header
// ---------------------------------------------------------------------------

// Apply the tightest safe SDRAM timing for the OS-default bus clock:
//   • A3CL: reduce CL=3 → CL=2 if the OS left it at CL=3; issue MRS to sync chip.
//   • TRC:  set to the minimum safe field value for the actual bus frequency,
//           derived from Ptune4's raW_TRC default table (mem_test.h) with a
//           5 % margin — replacing the previous fixed "TRC − 1" heuristic.
//
// IMPORTANT: must only be called when the CPU/bus clock is at the OS-default
// frequency.  The memory bus scales linearly with the FLL; any overclock level
// must call bsc_restore_default() before raising the clock.
//
// Ptune4 raW_TRC defaults and their meaning (mem_test.h):
//   TRC field 0 (3 bus cyc): safe when Bphi <  50 MHz  (× 0.95 = 47.5 MHz)
//   TRC field 1 (4 bus cyc): safe when Bphi <  80 MHz  (× 0.95 = 76.0 MHz)
//   TRC field 2 (6 bus cyc): safe when Bphi < 120 MHz  (× 0.95 = 114.0 MHz)
//   TRC field 3 (9 bus cyc): required  when Bphi ≥ 120 MHz × 0.95
//
// Bus frequency computation (no 64-bit arithmetic needed):
//   Bphi = FLF × (SELXM+1) × (STC+1) × 32768 Hz / BFC_div
//
// FRQCR field layout (SH7305, confirmed from Ptune4 express.c):
//   bits[29:24] = STC  → PLL multiplier = STC+1  (max 64)
//   bits[22:20] = IFC divider field  (unused here)
//   bits[10:8]  = BFC divider field  → BFC_div = 2^(field+1)
static void bsc_apply_fast() {
    // -----------------------------------------------------------------------
    // Step 1: compute the minimum safe TRC field for the stock bus clock.
    //
    // Work in units of 32768 Hz to keep all arithmetic in uint32_t:
    //   bphi_units = FLF × (SELXM+1) × (STC+1) / BFC_div
    //              = FLF × (SELXM+1) × (STC+1) >> (BFC_fld+1)
    //
    // Thresholds from Ptune4 raW_TRC (5 % margin, rounded up for safety):
    //   47.5 MHz / 32768 Hz = 1449.7 → 1450
    //   76.0 MHz / 32768 Hz = 2319.4 → 2320
    //  114.0 MHz / 32768 Hz = 3478.9 → 3479
    //
    // Max bphi_units at realistic values (FLF=1023, SELXM=0, STC=7, BFC_div=4):
    //   1023 × 1 × 8 >> 2 = 2046 — well within uint32_t.
    // -----------------------------------------------------------------------
    uint32_t flf     = default_fllfrq & 0x3FFFu;
    uint32_t selxm   = (default_fllfrq >> 14) & 1u;
    uint32_t stc     = (default_frqcr  >> 24) & 0x3Fu;  // PLL multiplier − 1
    uint32_t bfc_fld = (default_frqcr  >>  8) & 0x7u;   // log2(BFC_div) − 1

    // BFC_div is always a power of two → right-shift replaces the division.
    uint32_t bphi_units = (flf * (1u + selxm) * (stc + 1u)) >> (bfc_fld + 1u);

    uint32_t min_trc;
    if      (bphi_units >= 3479u) min_trc = 3u;  // ≥ 114 MHz → need 9-cycle TRC
    else if (bphi_units >= 2320u) min_trc = 2u;  // ≥  76 MHz → need 6-cycle TRC
    else if (bphi_units >= 1450u) min_trc = 1u;  // ≥  47 MHz → need 4-cycle TRC
    else                          min_trc = 0u;  //  < 47 MHz → 3-cycle TRC sufficient

    uint32_t wcr = default_cs3wcr;

    // -----------------------------------------------------------------------
    // Step 2: CAS latency — reduce CL=3 → CL=2.
    // A3CL encoding: 0b01=CL2, 0b10=CL3 (0b00 and 0b11 are reserved/invalid).
    // -----------------------------------------------------------------------
    uint32_t a3cl = (wcr >> 7) & 0x3u;
    if (a3cl == 2u) {
        // CL=3 → CL=2: update CS3WCR then issue MRS so the SDRAM chip latches it.
        wcr = (wcr & ~0x00000180u) | (1u << 7);
        *BSC_CS3WCR = wcr;
        *SDMR3_CL2 = 0;  // MRS address encodes CL=2; any write value works
    }
    // A3CL=1 (CL=2) already — nothing to do.
    // Leave 0/3 (reserved) untouched rather than risk corrupting the controller.

    // -----------------------------------------------------------------------
    // Step 3: TRC — tighten to the dynamic minimum.
    // Only write if the OS value is looser than min_trc; never relax below it.
    // No MRS command needed — TRC only affects the BSC state machine, not
    // anything the SDRAM chip tracks internally.
    // -----------------------------------------------------------------------
    uint32_t trc = wcr & 0x3u;
    if (trc > min_trc) {
        wcr = (wcr & ~0x3u) | min_trc;
        *BSC_CS3WCR = wcr;
    }
}

// Restore the OS-default BSC timing and re-issue MRS so the SDRAM chip
// re-latches the original CAS latency.  Must be called before raising bus
// frequency so the chip is always operating within its rated timing margins.
static void bsc_restore_default() {
    *BSC_CS3WCR = default_cs3wcr;
    uint32_t a3cl = (default_cs3wcr >> 7) & 0x3u;
    if      (a3cl == 1u) *SDMR3_CL2 = 0;
    else if (a3cl == 2u) *SDMR3_CL3 = 0;
}

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
    default_cs3wcr = *BSC_CS3WCR;
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
        // Back at default bus speed: safe to apply tighter SDRAM timing.
        bsc_apply_fast();
        return;
    }

    // Restore OS BSC timing before raising the bus clock so SDRAM margins
    // remain within spec during and after the frequency transition.
    bsc_restore_default();

    if (level == 5) {
        // TURBO+: switch SELXM to 1, keeping the OS FLF value unchanged.
        // SELXM=1 uses XTAL (32.768 kHz) instead of XTAL/2 as the FLL
        // reference, doubling every downstream clock at the same FLF.
        // This mirrors the Ptune4 alpha-F5 preset (fx-CG50/CG100).
        //
        // CS3WCR is set to alpha-F5 SDRAM timing before the frequency jump
        // so the SDRAM is never accessed outside its rated margins.
        *BSC_CS3WCR = CS3WCR_TURBO_PLUS;
        *SDMR3_CL2 = 0;  // Re-latch CL=2 into the SDRAM chip (MRS command)

        uint32_t base_flf = default_fllfrq & 0x3FFFu; // bits[13:0]
        // Clear bits [14:0] of default_fllfrq (SELXM + FLF), then set
        // SELXM=1 (bit 14) and restore the original FLF in bits [13:0].
        *CPG_FLLFRQ = (default_fllfrq & ~0x7FFFu) | (1u << 14) | base_flf;
        fll_lock_wait();
        return;
    }

    // Levels 1-4: increment FLF within the SELXM=0 range.
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

    if (level >= 5) {
        // TURBO+: SELXM=1 doubles the effective FLL multiplier.  The
        // effective FLF is 2×base_flf, so percentage = 200 regardless of
        // the actual base_flf value.
        return 200;
    }

    uint32_t selxm   = (default_fllfrq >> 14) & 1u;
    uint32_t new_flf = base_flf + static_cast<uint32_t>(flf_increment[level]);
    if (selxm == 0u && new_flf > 1023u) new_flf = 1023u;

    // Percentage = new_flf / base_flf × 100, rounded to nearest integer.
    return static_cast<int>((new_flf * 100u + base_flf / 2u) / base_flf);
}
