#include "bl_piece_id.hpp"
#include "bl_piece_id_freq_calc.hpp"
#include "peripheral/tc/plib_tc0.h"
#include "peripheral/tcc/plib_tcc0.h"
#include "pic32cm6408jh00048.h"

// The Clock chain is:
//
// 48MHz OSC -> (Gclkdiv) GCLK8 (dynamic control to get target freq)
// GCLK8     -> PLL  (48Mhz -> 96Mhz)  Div 1
// PLL       -> TCC
// TCC count and period determine the waveform
//
// GClk notes
//   - "Before a Generator is enabled, the corresponding clock source should be enabled."
//   - "The Generator must be enabled (GENCTRLn.GENEN = 1) and the division factor must be
//      set (GENTRLn.DIVSEL and GENCTRLn.DIV) by performing a single 32-bit write to the
//      Generator Control register (GENCTRLn)."
//   - "Before switching the Generic Clock Generator 0 (GCLKGEN0) from a clock source A to
//      another clock source B, enable the ONDEMAND feature of the clock source A to ensure
//      a proper transition from clock source A to clock source B."
//      (This shouldn't affect us, as the SOURCE is not changing)
//   - "When dividing a clock with an odd division factor, the duty-cycle will not be 50/50.
//      Setting the Improve Duty Cycle bit of the Generator Control register (GENCTRLn.IDC)
//      will result in a 50/50 duty cycle."
//      (Maybe need to consider this for PLL behavior?  Probably not...)_
//
// PLL notes
//   - "The frequency of the DPLL output clock CK is stable when the module is enabled and
//      when the Lock bit in the DPLL Status register is set (DPLLSTATUS.LOCK)"
//   - "The lock time timer uses GCLK_DPLL_32 as a source clock, which must be enabled and
//      configured as a 32.768 kHz clock"
//   - "When a software operation requires reference clock switching, the recommended
//      procedure is to turn the DPLL into the Standby mode, modify the DPLLCTRLB.REFCLK to
//      select the desired reference source, and activate the DPLL again."
//      (I believe this is the SOURCE, not just changing reference frequency)
//   - "The DPLL Controller supports on-the-fly update of the DPLL Ratio Control (DPLLRATIO)
//      register only when the GCLK_DPLL_32K (GCLK.PCHCTRL1) is configured and enabled.  The
//      FDPLL reference clock (DPLLCTRLB.REFCLK) can still be set to one of the three
//      options (XOSC, XOSC32K, GCLK) as long as GCLK_DPLL_32K is active.
//   - "The following bits need synchronization when written:
//        • Enable bit in control register A (DPLLCTRLA.ENABLE)
//        • DPLL Ratio register (DPLLRATIO)
//        • DPLL Prescaler register (DPLLPRESC)"
//   - "The Pattern (PATT), Period (PER) and Compare Channels (CCx) registers are all double
//      buffered.  ... the double buffering feature is not mandatory. The double buffering
//      is disabled by writing a '1' to CTRLSET.LUPD."
//
//  TCC ... no notes?
//
//
//
enum GenClkSrc {
  XOSC        = 0x0,  // XOSC oscillator output
  GCLKIN      = 0x1,  // Generator input pad (GCLK_IO)
  GCLKGEN1    = 0x2,  // Generic clock generator 1 output
  OSCULP32K   = 0x3,  // OSCULP32K oscillator output
  OSC32K      = 0x4,  // OSC32K oscillator output
  XOSC32K     = 0x5,  // XOSC32K oscillator output
  OSC48M      = 0x6,  // OSC48M oscillator output
  FDPLL96M    = 0x7,  // DPLL96M output
};

void disableGclk(uint8_t gclkNum) {
    uint32_t val = GCLK_REGS->GCLK_GENCTRL[gclkNum];
    val &= ~(GCLK_GENCTRL_GENEN_Msk);
    GCLK_REGS->GCLK_GENCTRL[gclkNum] = val;

    // Synchronize disable
    while((GCLK_REGS->GCLK_SYNCBUSY & GCLK_SYNCBUSY_GENCTRL8_Msk) == GCLK_SYNCBUSY_GENCTRL8_Msk) {
    }
}

void setGclkDiv(uint8_t gclkNum, uint8_t divider) {
    uint32_t val = GCLK_REGS->GCLK_GENCTRL[gclkNum];

    // Set divider
    val &= ~GCLK_GENCTRL_DIV_Msk;
    val |= GCLK_GENCTRL_DIV(divider) | GCLK_GENCTRL_GENEN_Msk;

    // Set enable bit and divider in the same write
    GCLK_REGS->GCLK_GENCTRL[gclkNum] = val;

    // Synchronize enable
    while((GCLK_REGS->GCLK_SYNCBUSY & GCLK_SYNCBUSY_GENCTRL8_Msk) == GCLK_SYNCBUSY_GENCTRL8_Msk) {
    }
}

void initPll() {
    // Enable GClk 8
    GCLK_REGS->GCLK_PCHCTRL[0] = GCLK_PCHCTRL_GEN(0x8UL)  | GCLK_PCHCTRL_CHEN_Msk;
    while ((GCLK_REGS->GCLK_PCHCTRL[0] & GCLK_PCHCTRL_CHEN_Msk) != GCLK_PCHCTRL_CHEN_Msk)
    {
        /* Wait for synchronization */
    }

    // Configure filter to High-Damping to reduce jitter from fractional PLL
    //   Also default lock time and Gclk reference
    OSCCTRL_REGS->OSCCTRL_DPLLCTRLB = OSCCTRL_DPLLCTRLB_FILTER(0UL) | OSCCTRL_DPLLCTRLB_LTIME(0UL)| OSCCTRL_DPLLCTRLB_REFCLK(2UL) ;

    /* Selection of the DPLL Pre-Scalar */
   OSCCTRL_REGS->OSCCTRL_DPLLPRESC = (uint8_t)OSCCTRL_DPLLPRESC_PRESC(0UL);

    while((OSCCTRL_REGS->OSCCTRL_DPLLSYNCBUSY & OSCCTRL_DPLLSYNCBUSY_DPLLPRESC_Msk) == OSCCTRL_DPLLSYNCBUSY_DPLLPRESC_Msk )
    {
        /* Waiting for the synchronization */
    }
}

void setPulseCount(uint8_t pulses) {
    if (pulses > 0) {
        pulses -= 1;
    }
    TC0_REGS->COUNT8.TC_COUNT = pulses;
}

void disablePll() {
    OSCCTRL_REGS->OSCCTRL_DPLLCTRLA = (uint8_t)(0);

    while((OSCCTRL_REGS->OSCCTRL_DPLLSYNCBUSY & OSCCTRL_DPLLSYNCBUSY_ENABLE_Msk) == OSCCTRL_DPLLSYNCBUSY_ENABLE_Msk )
    {
        /* Waiting for the DPLL enable synchronization */
    }
}

void enablePll() {
    OSCCTRL_REGS->OSCCTRL_DPLLCTRLA = (uint8_t)(OSCCTRL_DPLLCTRLA_ENABLE_Msk);

    while((OSCCTRL_REGS->OSCCTRL_DPLLSYNCBUSY & OSCCTRL_DPLLSYNCBUSY_ENABLE_Msk) == OSCCTRL_DPLLSYNCBUSY_ENABLE_Msk )
    {
        /* Waiting for the DPLL enable synchronization */
    }
}

void setPllMult(uint16_t whole, uint8_t frac) {
    OSCCTRL_REGS->OSCCTRL_DPLLRATIO = OSCCTRL_DPLLRATIO_LDRFRAC(frac) | OSCCTRL_DPLLRATIO_LDR(whole);

    while((OSCCTRL_REGS->OSCCTRL_DPLLSYNCBUSY & OSCCTRL_DPLLSYNCBUSY_DPLLRATIO_Msk) == OSCCTRL_DPLLSYNCBUSY_DPLLRATIO_Msk)
    {
        /* Waiting for the synchronization */
    }
}

uint32_t frequencies[] = {
  1'000'000,
  1'500'000,
  2'000'000,
  2'500'000,
  3'000'000,
  3'500'000,
  4'000'000,
};

void initPieceId() {
    initPll();
}

void setPieceId(unsigned idx) {
    const PllTccConfig config = getPllTccConfigForFreq(frequencies[idx]);
#if 0
    PllTccConfig config = {
        .gclkDiv = 24,
        .ldr     = ((44 + 1) * 16) + 12,
        .tccCnt  = 61,
    };
#endif

    TCC0_PWMStop();
    TC0_CompareStop();
    disablePll();
    disableGclk(8);

    setGclkDiv(8, config.gclkDiv);

    const uint32_t whole = config.ldr / 16 - 1;
    const uint32_t frac = config.ldr % 16;

    setPllMult(whole, frac);
    enablePll();

    TCC0_PWMInitialize();

    uint32_t period = config.tccCnt;
    if (period & 1) {
        period++;
    }

    TCC0_REGS->TCC_CC[1] = period / 2;
    TCC0_REGS->TCC_PER = period;

    while (TCC0_REGS->TCC_SYNCBUSY != 0U)
    {
        /* Wait for sync */
    }

    TC0_CompareInitialize();
    setPulseCount(10);
    TC0_CompareStart();

    TCC0_PWMStart();
    // "Retrigger" TCC0 to start after stop
    //TCC0_REGS->TCC_CTRLBSET = TCC_CTRLBSET_CMD(1);
}

unsigned freqIdx = 0;

void switchPieceId() {
  freqIdx++;
  if (freqIdx >= 7) {
    freqIdx = 0;
  }
  setPieceId(freqIdx);
}

void startPieceId() {
  freqIdx = 0;
  setPieceId(freqIdx);
}
